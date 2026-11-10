#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "PlayerProcess.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace compintel::runtime {
namespace {

class Handle {
public:
    Handle() = default;
    explicit Handle(HANDLE value) : value_(value) {}
    ~Handle() { Reset(); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
    Handle(Handle&& other) noexcept : value_(other.Release()) {}
    Handle& operator=(Handle&& other) noexcept {
        if (this != &other) Reset(other.Release());
        return *this;
    }
    HANDLE Get() const { return value_; }
    explicit operator bool() const { return value_ && value_ != INVALID_HANDLE_VALUE; }
    HANDLE Release() { return std::exchange(value_, nullptr); }
    void Reset(HANDLE value = nullptr) {
        if (*this) CloseHandle(value_);
        value_ = value;
    }
private:
    HANDLE value_ = nullptr;
};

std::string Utf8(std::wstring_view value) {
    if (value.empty()) return {};
    int count = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    std::string result(count, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), count, nullptr, nullptr);
    return result;
}

std::wstring Wide(std::string_view value, UINT encoding = CP_UTF8) {
    if (value.empty()) return {};
    int count = MultiByteToWideChar(encoding, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(count, L'\0');
    MultiByteToWideChar(encoding, 0, value.data(), static_cast<int>(value.size()), result.data(), count);
    return result;
}

std::string DiagnosticText(std::string_view value) {
    if (value.empty()) return {};
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0))
        return std::string(value);
    return Utf8(Wide(value, CP_OEMCP));
}

std::string WinError(std::string_view operation, DWORD code = GetLastError()) {
    wchar_t* message = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t*>(&message), 0, nullptr);
    std::string result(operation);
    result += " (" + std::to_string(code) + ")";
    if (message) {
        result += ": " + Utf8(message);
        LocalFree(message);
    }
    return result;
}

std::wstring Quote(std::wstring_view value) {
    std::wstring result = L"\"";
    unsigned slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(slashes * (c == L'\"' ? 2 : 1), L'\\');
        slashes = 0;
        if (c == L'\"') result += L'\\';
        result += c;
    }
    result.append(slashes * 2, L'\\');
    return result + L'\"';
}

bool Pipe(Handle& read, Handle& write) {
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
    HANDLE r = nullptr, w = nullptr;
    if (!CreatePipe(&r, &w, &attributes, 4096)) return false;
    read.Reset(r);
    write.Reset(w);
    return true;
}

Handle Job(std::size_t memoryMiB, DWORD processes) {
    Handle result(CreateJobObjectW(nullptr, nullptr));
    if (!result) return {};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
        JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_JOB_MEMORY;
    info.BasicLimitInformation.ActiveProcessLimit = processes;
    info.ProcessMemoryLimit = memoryMiB * 1024ULL * 1024ULL;
    info.JobMemoryLimit = info.ProcessMemoryLimit;
    if (!SetInformationJobObject(result.Get(), JobObjectExtendedLimitInformation, &info, sizeof(info))) return {};
    return result;
}

// Only the explicit pipe handles are inherited, even if another task is spawning
// a child concurrently. The child cannot inherit unrelated application handles.
bool Spawn(const std::filesystem::path& application, std::wstring command, const std::filesystem::path& cwd,
           HANDLE input, HANDLE output, HANDLE error, const wchar_t* environment,
           Handle& process, Handle& thread, std::string& diagnostic) {
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<std::byte> storage(bytes);
    auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &bytes)) {
        diagnostic = WinError("InitializeProcThreadAttributeList");
        return false;
    }
    HANDLE handles[] = {input, output, error};
    std::vector<HANDLE> inherited;
    for (HANDLE handle : handles) {
        if (std::find(inherited.begin(), inherited.end(), handle) == inherited.end()) inherited.push_back(handle);
    }
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherited.data(),
                                   inherited.size() * sizeof(HANDLE), nullptr, nullptr)) {
        diagnostic = WinError("UpdateProcThreadAttribute");
        DeleteProcThreadAttributeList(attributes);
        return false;
    }
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = input;
    startup.StartupInfo.hStdOutput = output;
    startup.StartupInfo.hStdError = error;
    startup.lpAttributeList = attributes;
    PROCESS_INFORMATION info{};
    BOOL ok = CreateProcessW(application.c_str(), command.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
        const_cast<wchar_t*>(environment), cwd.empty() ? nullptr : cwd.c_str(), &startup.StartupInfo, &info);
    DWORD code = GetLastError();
    DeleteProcThreadAttributeList(attributes);
    if (!ok) { diagnostic = WinError("CreateProcess", code); return false; }
    process.Reset(info.hProcess);
    thread.Reset(info.hThread);
    return true;
}

std::uint64_t CpuMicros(HANDLE process) {
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(process, &created, &exited, &kernel, &user)) return 0;
    ULARGE_INTEGER k{}, u{};
    k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
    u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
    return (k.QuadPart + u.QuadPart) / 10;
}

bool Drain(HANDLE pipe, std::string& destination, std::size_t maximum, bool& truncated) {
    DWORD available = 0;
    std::size_t consumed = 0;
    while (PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) && available) {
        char buffer[4096];
        DWORD read = 0;
        if (!ReadFile(pipe, buffer, std::min<DWORD>(available, sizeof(buffer)), &read, nullptr)) return false;
        const std::size_t keep = std::min<std::size_t>(read, maximum > destination.size() ? maximum - destination.size() : 0);
        destination.append(buffer, keep);
        if (keep < read) truncated = true;
        consumed += read;
        // Bound each polling pass; a noisy child must not starve timeout checks.
        if (available <= sizeof(buffer) || consumed >= 64 * 1024) break;
    }
    return available != 0;
}

struct CaptureResult { bool started = false; DWORD exitCode = ~0u; std::string output; };

CaptureResult Capture(const std::filesystem::path& executable, std::wstring command,
                      const std::filesystem::path& cwd = {}, const wchar_t* environment = nullptr,
                      std::chrono::milliseconds timeout = std::chrono::seconds(45)) {
    CaptureResult result;
    Handle read, write, nullInput(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!nullInput || !Pipe(read, write)) { result.output = WinError("Create compiler pipes"); return result; }
    SetHandleInformation(nullInput.Get(), HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
    SetHandleInformation(read.Get(), HANDLE_FLAG_INHERIT, 0);
    Handle process, thread;
    if (!Spawn(executable, std::move(command), cwd, nullInput.Get(), write.Get(), write.Get(), environment,
               process, thread, result.output)) return result;
    Handle job = Job(2048, 32);
    if (!job || !AssignProcessToJobObject(job.Get(), process.Get())) {
        result.output = WinError("Create compiler job");
        TerminateProcess(process.Get(), 1);
        return result;
    }
    result.started = true;
    write.Reset();
    ResumeThread(thread.Get());
    auto started = std::chrono::steady_clock::now();
    bool truncated = false;
    while (true) {
        Drain(read.Get(), result.output, 256 * 1024, truncated);
        if (WaitForSingleObject(process.Get(), 2) == WAIT_OBJECT_0) break;
        if (std::chrono::steady_clock::now() - started > timeout) {
            TerminateJobObject(job.Get(), 1);
            result.output += "\nCompiler timed out.\n";
            break;
        }
    }
    WaitForSingleObject(process.Get(), 2000);
    Drain(read.Get(), result.output, 256 * 1024, truncated);
    GetExitCodeProcess(process.Get(), &result.exitCode);
    if (truncated) result.output += "\n[compiler output truncated]\n";
    return result;
}

std::filesystem::path EnvironmentPath(const wchar_t* name) {
    DWORD length = GetEnvironmentVariableW(name, nullptr, 0);
    if (!length) return {};
    std::wstring value(length, L'\0');
    GetEnvironmentVariableW(name, value.data(), length);
    value.resize(length - 1);
    return value;
}

std::filesystem::path FindVcvars() {
    auto current = EnvironmentPath(L"VSINSTALLDIR") / L"VC/Auxiliary/Build/vcvars64.bat";
    if (std::filesystem::exists(current)) return current;
    auto installer = EnvironmentPath(L"ProgramFiles(x86)") / L"Microsoft Visual Studio/Installer/vswhere.exe";
    if (std::filesystem::exists(installer)) {
        auto located = Capture(installer, Quote(installer.wstring()) +
            L" -latest -products * -utf8 -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath");
        if (located.exitCode == 0) {
            auto text = Wide(located.output);
            while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n')) text.pop_back();
            auto path = std::filesystem::path(text) / L"VC/Auxiliary/Build/vcvars64.bat";
            if (std::filesystem::exists(path)) return path;
        }
    }
    return {};
}

struct CompilerEnvironment { std::vector<wchar_t> block; std::filesystem::path compiler; std::string error; };

CompilerEnvironment LoadMsvcEnvironment() {
    CompilerEnvironment result;
    auto vcvars = FindVcvars();
    if (vcvars.empty()) {
        result.error = "MSVC C++ tools were not found. Install Visual Studio Build Tools with Desktop development with C++, or specify clang++/g++.";
        return result;
    }
    auto commandInterpreter = EnvironmentPath(L"ComSpec");
    if (commandInterpreter.empty()) commandInterpreter = L"C:\\Windows\\System32\\cmd.exe";
    // vcvars is a verified Microsoft installation path. Source and build paths
    // never enter this shell command; compiler arguments use CreateProcess directly.
    auto setup = Capture(commandInterpreter, Quote(commandInterpreter.wstring()) +
        L" /d /u /s /c \"\"" + vcvars.wstring() + L"\" >nul && set\"");
    if (setup.exitCode != 0) {
        result.error = "Cannot initialize the MSVC x64 environment.";
        return result;
    }
    if (setup.output.size() % sizeof(wchar_t)) {
        result.error = "MSVC returned an invalid environment block.";
        return result;
    }
    std::wstring environment(setup.output.size() / sizeof(wchar_t), L'\0');
    memcpy(environment.data(), setup.output.data(), setup.output.size());
    std::wistringstream input(environment);
    std::wstring line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty() || line.find(L'=') == std::wstring::npos) continue;
        if (line.starts_with(L"VCToolsInstallDir="))
            result.compiler = std::filesystem::path(line.substr(18)) / L"bin/Hostx64/x64/cl.exe";
        result.block.insert(result.block.end(), line.begin(), line.end());
        result.block.push_back(L'\0');
    }
    result.block.push_back(L'\0');
    if (result.compiler.empty() || !std::filesystem::exists(result.compiler))
        result.error = "MSVC environment did not provide a working x64 cl.exe.";
    return result;
}

using NativeProcessControl = LONG (NTAPI*)(HANDLE);
NativeProcessControl NativeControl(const char* name) {
    return reinterpret_cast<NativeProcessControl>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), name));
}

} // namespace

CompileResult Compiler::Compile(std::string_view source, const std::filesystem::path& outputDirectory,
                                const std::filesystem::path& compiler) {
    CompileResult result;
    try {
        if (source.empty() || source.size() > 256 * 1024) {
            result.diagnostics = "Source must contain 1 to 262144 UTF-8 bytes.";
            return result;
        }
        static std::atomic_uint64_t sequence{0};
        const auto directory = std::filesystem::absolute(outputDirectory) /
            (L"build-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64()) +
             L"-" + std::to_wstring(sequence.fetch_add(1)));
        std::filesystem::create_directories(directory);
        const auto sourcePath = directory / L"player.cpp";
        const auto executablePath = directory / L"player.exe";
        // WinUI TextBox exposes line breaks as bare CR. MSVC does not treat
        // those as ordinary source newlines, so an initial // comment can
        // accidentally consume the entire file. Canonicalize all three forms.
        std::string normalizedSource;
        normalizedSource.reserve(source.size());
        for (std::size_t i = 0; i < source.size(); ++i) {
            if (source[i] == '\r') {
                normalizedSource += '\n';
                if (i + 1 < source.size() && source[i + 1] == '\n') ++i;
            } else normalizedSource += source[i];
        }
        std::ofstream file(sourcePath, std::ios::binary);
        file.write(normalizedSource.data(), static_cast<std::streamsize>(normalizedSource.size()));
        file.close();
        if (!file) { result.diagnostics = "Cannot save player.cpp."; return result; }
        std::filesystem::path selected = compiler;
        const wchar_t* environment = nullptr;
        std::optional<CompilerEnvironment> msvc;
        std::wstring command;
        bool isMsvc = selected.empty() || _wcsicmp(selected.filename().c_str(), L"cl.exe") == 0 ||
                      _wcsicmp(selected.filename().c_str(), L"cl") == 0;
        if (isMsvc) {
            msvc = LoadMsvcEnvironment();
            if (!msvc->error.empty()) { result.diagnostics = msvc->error; return result; }
            if (selected.empty() || !selected.has_parent_path()) selected = msvc->compiler;
            environment = msvc->block.data();
            command = Quote(selected.wstring()) + L" /nologo /std:c++20 /EHsc /utf-8 /O2 /MD " +
                      Quote(L"/Fe:" + executablePath.wstring()) + L" " +
                      Quote(L"/Fo:" + (directory / L"player.obj").wstring()) + L" " +
                      Quote(sourcePath.wstring()) + L" /link /SUBSYSTEM:CONSOLE";
        } else {
            if (!selected.has_parent_path()) {
                std::wstring found(32768, L'\0');
                DWORD size = SearchPathW(nullptr, selected.c_str(), L".exe", static_cast<DWORD>(found.size()), found.data(), nullptr);
                if (size && size < found.size()) { found.resize(size); selected = found; }
            }
            command = Quote(selected.wstring()) + L" -std=c++20 -O2 -o " + Quote(executablePath.wstring()) + L" " + Quote(sourcePath.wstring());
        }
        auto compiled = Capture(selected, command, directory, environment);
        result.diagnostics = DiagnosticText(compiled.output);
        result.success = compiled.started && compiled.exitCode == 0 && std::filesystem::exists(executablePath);
        if (result.success) result.executable = executablePath;
        else if (result.diagnostics.empty()) result.diagnostics = "Compilation failed (exit code " + std::to_string(compiled.exitCode) + ").";
    } catch (const std::exception& error) {
        result.diagnostics = error.what();
    }
    return result;
}

struct PlayerProcess::State {
    Handle process, thread, job, input, output, error;
    std::atomic_bool stopped{false};
    std::mutex turnMutex;
    bool firstResume = true;
    std::string stderrText;
    bool stderrTruncated = false;
    NativeProcessControl suspend = NativeControl("NtSuspendProcess");
    NativeProcessControl resume = NativeControl("NtResumeProcess");
    ~State() {
        if (job) TerminateJobObject(job.Get(), 1);
        // Assignment to the job can fail after CreateProcess succeeded. In that
        // case the still-suspended primary process is not in the job to kill.
        if (process) TerminateProcess(process.Get(), 1);
    }
    void Stop() noexcept {
        stopped.store(true);
        if (job) TerminateJobObject(job.Get(), 1);
    }
};

PlayerProcess::PlayerProcess(std::filesystem::path executable, ProcessLimits limits)
    : executable_(std::move(executable)), limits_(limits) {}
PlayerProcess::~PlayerProcess() { Stop(); }

bool PlayerProcess::Start(std::string* error) {
    std::shared_ptr<State> previous;
    std::uint64_t generation;
    {
        std::lock_guard lock(mutex_);
        generation = ++generation_;
        previous = std::exchange(state_, {});
    }
    if (previous) previous->Stop();
    auto state = std::make_shared<State>();
    auto fail = [&](std::string diagnostic) {
        if (error) *error = std::move(diagnostic);
        return false;
    };
    if (!state->suspend || !state->resume) return fail("Windows process suspension is unavailable.");
    Handle childInput, childOutput, childError;
    if (!Pipe(childInput, state->input) || !Pipe(state->output, childOutput) || !Pipe(state->error, childError))
        return fail(WinError("Create player pipes"));
    for (HANDLE handle : {state->input.Get(), state->output.Get(), state->error.Get()})
        SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0);
    std::string diagnostic;
    auto executable = std::filesystem::absolute(executable_);
    if (!Spawn(executable, Quote(executable.wstring()), executable.parent_path(), childInput.Get(),
               childOutput.Get(), childError.Get(), nullptr, state->process, state->thread, diagnostic))
        return fail(std::move(diagnostic));
    state->job = Job(limits_.memoryMiB, 8);
    if (!state->job || !AssignProcessToJobObject(state->job.Get(), state->process.Get()))
        return fail(WinError("Assign player resource limits"));
    {
        std::lock_guard lock(mutex_);
        if (generation != generation_) return fail("Player startup was cancelled.");
        state_ = std::move(state);
    }
    return true;
}

bool PlayerProcess::Running() const {
    std::lock_guard lock(mutex_);
    return state_ && !state_->stopped.load() && WaitForSingleObject(state_->process.Get(), 0) == WAIT_TIMEOUT;
}

void PlayerProcess::Stop() noexcept {
    std::shared_ptr<State> state;
    {
        std::lock_guard lock(mutex_);
        ++generation_;
        state = std::exchange(state_, {});
    }
    if (state) state->Stop();
}

TurnResult PlayerProcess::Turn(std::string_view input, std::chrono::milliseconds timeout) {
    TurnResult result;
    std::shared_ptr<State> state;
    {
        std::lock_guard lock(mutex_);
        state = state_;
    }
    if (!state) { result.diagnostics = "Player is not running."; return result; }
    std::unique_lock turnLock(state->turnMutex, std::try_to_lock);
    if (!turnLock.owns_lock()) { result.diagnostics = "Another turn is already running."; return result; }
    auto started = std::chrono::steady_clock::now();
    std::uint64_t initialCpu = CpuMicros(state->process.Get());
    auto finish = [&](std::string message) {
        result.wallMilliseconds = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count());
        result.cpuMicroseconds = CpuMicros(state->process.Get()) - initialCpu;
        Drain(state->error.Get(), state->stderrText, 16 * 1024, state->stderrTruncated);
        result.diagnostics = std::move(message);
        if (!state->stderrText.empty()) result.diagnostics += "\n[stderr]\n" + DiagnosticText(state->stderrText);
        if (state->stderrTruncated) result.diagnostics += "\n[stderr truncated]";
        if (!result.success) state->Stop();
        return result;
    };
    if (state->stopped.load()) return finish("Player was stopped.");
    if (input.size() > 4096) return finish("Protocol input exceeds 4096 bytes.");
    DWORD pending = 0;
    if (PeekNamedPipe(state->output.Get(), nullptr, 0, nullptr, &pending, nullptr) && pending)
        return finish("INVALID_MOVE: player produced output outside its turn.");
    DWORD written = 0;
    if (!input.empty() && (!WriteFile(state->input.Get(), input.data(), static_cast<DWORD>(input.size()), &written, nullptr) || written != input.size()))
        return finish(WinError("Write player input"));
    if (state->firstResume) {
        if (ResumeThread(state->thread.Get()) == static_cast<DWORD>(-1)) return finish(WinError("Resume player"));
        state->firstResume = false;
    } else if (state->resume(state->process.Get()) < 0) return finish("Cannot resume player process.");
    std::string output;
    while (true) {
        if (state->stopped.load()) return finish("Player was stopped.");
        Drain(state->error.Get(), state->stderrText, 16 * 1024, state->stderrTruncated);
        auto cpu = CpuMicros(state->process.Get());
        if (cpu - initialCpu > static_cast<std::uint64_t>(limits_.turnCpu.count()) * 1000)
            return finish("TIME_LIMIT_EXCEEDED: per-turn CPU limit.");
        if (cpu > static_cast<std::uint64_t>(limits_.totalCpu.count()) * 1000)
            return finish("TIME_LIMIT_EXCEEDED: total CPU limit.");
        if (std::chrono::steady_clock::now() - started > timeout)
            return finish("TIME_LIMIT_EXCEEDED: no complete output line before the turn deadline.");
        DWORD available = 0;
        if (PeekNamedPipe(state->output.Get(), nullptr, 0, nullptr, &available, nullptr) && available) {
            char buffer[256];
            DWORD read = 0;
            if (!ReadFile(state->output.Get(), buffer, std::min<DWORD>(available, sizeof(buffer)), &read, nullptr))
                return finish(WinError("Read player output"));
            output.append(buffer, read);
            if (output.size() > limits_.outputLineBytes)
                return finish("OUTPUT_LIMIT_EXCEEDED: output line exceeds " + std::to_string(limits_.outputLineBytes) + " bytes.");
            auto newline = output.find('\n');
            if (newline != std::string::npos) {
                if (state->suspend(state->process.Get()) < 0 &&
                    WaitForSingleObject(state->process.Get(), 0) != WAIT_OBJECT_0)
                    return finish("Cannot suspend player between turns.");
                if (newline + 1 != output.size()) return finish("INVALID_MOVE: more than one output line in a turn.");
                if (PeekNamedPipe(state->output.Get(), nullptr, 0, nullptr, &pending, nullptr) && pending)
                    return finish("INVALID_MOVE: extra output after the turn line.");
                output.resize(newline);
                if (!output.empty() && output.back() == '\r') output.pop_back();
                result.line = std::move(output);
                result.success = true;
                return finish({});
            }
        }
        if (WaitForSingleObject(state->process.Get(), 2) == WAIT_OBJECT_0) {
            // A program may exit immediately after flushing its final line.
            if (PeekNamedPipe(state->output.Get(), nullptr, 0, nullptr, &available, nullptr) && available) continue;
            DWORD exitCode = 0;
            GetExitCodeProcess(state->process.Get(), &exitCode);
            return finish("RUNTIME_ERROR: player exited without a complete move (exit " + std::to_string(exitCode) + ").");
        }
    }
}

} // namespace compintel::runtime
