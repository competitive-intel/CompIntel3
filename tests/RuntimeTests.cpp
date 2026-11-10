#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "runtime/PlayerProcess.h"

#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <string>
#include <thread>

using namespace std::chrono_literals;
using namespace compintel::runtime;

namespace {
int checks = 0;
int failures = 0;
void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}

constexpr char Fixture[] = R"cpp(
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
int main() {
    std::string mode;
    std::getline(std::cin, mode);
    if (mode == "sleep") { Sleep(INFINITE); return 0; }
    if (mode == "partial") { std::cout << "1 2" << std::flush; Sleep(INFINITE); }
    if (mode == "large") { std::cout << std::string(80, 'x') << std::endl; Sleep(INFINITE); }
    if (mode == "double") { std::cout << "1 2\n3 4\n" << std::flush; Sleep(INFINITE); }
    if (mode == "exit") return 42;
    if (mode == "spin") { for (;;) { MemoryBarrier(); } }
    if (mode == "stderr") {
        for (int i = 0; i < 30000; ++i) std::cerr << 'x';
        std::cerr << std::flush;
        std::cout << "1 2" << std::endl;
        Sleep(INFINITE);
    }
    if (mode == "echo") {
        std::string line;
        while (std::getline(std::cin, line)) std::cout << line << std::endl;
    }
}
)cpp";
}

int main() {
    const auto directory = std::filesystem::temp_directory_path() /
        (L"CompIntel3 runtime 测试 %!& " + std::to_wstring(GetCurrentProcessId()));
    const auto compiled = Compiler::Compile(Fixture, directory);
    Check(compiled.success, "MSVC compilation succeeds with Unicode, spaces and shell metacharacters in the output path");
    if (!compiled.success) { std::cerr << compiled.diagnostics << '\n'; return 1; }
    const auto invalid = Compiler::Compile("this is not valid C++", directory);
    Check(!invalid.success && !invalid.diagnostics.empty() && invalid.executable.empty(), "compile errors return diagnostics without an executable");
    Check(!Compiler::Compile("", directory).success, "empty source is rejected");
    const auto explicitMsvc = Compiler::Compile("int main() { return 0; }", directory, L"cl.exe");
    Check(explicitMsvc.success, "explicit cl.exe also initializes the MSVC x64 environment");
    const auto crOnly = Compiler::Compile("// WinUI TextBox CR-only source\rint main() { return 0; }\r", directory);
    Check(crOnly.success, "WinUI CR-only source is normalized so a leading line comment does not hide main");
    const auto mixedNewlines = Compiler::Compile("// CRLF comment\r\n#include <iostream>\rint main() { return 0; }\n", directory);
    Check(mixedNewlines.success, "mixed CRLF, CR and LF source lines compile correctly");

    {
        PlayerProcess missing(directory / L"does-not-exist.exe");
        std::string diagnostic;
        Check(!missing.Start(&diagnostic) && !diagnostic.empty(), "missing executable returns a useful error");
    }
    {
        ProcessLimits impossible;
        impossible.memoryMiB = 0;
        PlayerProcess player(compiled.executable, impossible);
        std::string diagnostic;
        Check(!player.Start(&diagnostic) && !diagnostic.empty(), "invalid job limits fail startup cleanly");
    }
    {
        PlayerProcess player(compiled.executable);
        Check(player.Start(), "start persistent fixture");
        auto first = player.Turn("echo\n1 2\n");
        Check(first.success && first.line == "1 2", "first turn accepts initialization and a move");
        std::this_thread::sleep_for(30ms);
        auto second = player.Turn("3 4\n");
        Check(second.success && second.line == "3 4", "persistent process resumes for another turn");
        Check(player.Running(), "process remains alive between turns");
        player.Stop();
        Check(!player.Running(), "stop clears running state");
    }
    for (auto mode : {"large", "double", "exit"}) {
        PlayerProcess player(compiled.executable);
        Check(player.Start(), "start invalid-output fixture");
        auto turn = player.Turn(std::string(mode) + "\n");
        Check(!turn.success && !turn.diagnostics.empty(), "oversized/multiple/absent output is rejected");
        Check(!player.Running(), "failed turn terminates the process");
    }
    for (auto mode : {"partial", "sleep"}) {
        PlayerProcess player(compiled.executable);
        Check(player.Start(), "start timeout fixture");
        auto turn = player.Turn(std::string(mode) + "\n", 150ms);
        Check(!turn.success && turn.diagnostics.find("TIME_LIMIT_EXCEEDED") != std::string::npos,
              "no newline / blocked player respects wall deadline");
        Check(turn.wallMilliseconds < 1000, "timeout returns promptly");
    }
    {
        PlayerProcess player(compiled.executable);
        Check(player.Start(), "start CPU fixture");
        auto turn = player.Turn("spin\n", 2s);
        Check(!turn.success && turn.diagnostics.find("CPU limit") != std::string::npos, "busy loop respects CPU limit");
    }
    {
        PlayerProcess player(compiled.executable);
        Check(player.Start(), "start stderr fixture");
        auto turn = player.Turn("stderr\n");
        Check(turn.success && turn.line == "1 2", "stderr is drained without blocking protocol output");
        Check(turn.diagnostics.find("stderr truncated") != std::string::npos && turn.diagnostics.size() < 17000,
              "stderr diagnostics are bounded and truncation is visible");
    }
    {
        PlayerProcess player(compiled.executable);
        Check(player.Start(), "start cancellation fixture");
        auto running = std::async(std::launch::async, [&] { return player.Turn("sleep\n", 5s); });
        std::this_thread::sleep_for(30ms);
        player.Stop();
        Check(running.wait_for(500ms) == std::future_status::ready, "Stop interrupts a blocked turn from another thread");
        Check(!running.get().success, "cancelled turn reports failure");
    }
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    std::cout << (checks - failures) << '/' << checks << " runtime checks passed\n";
    return failures ? 1 : 0;
}
