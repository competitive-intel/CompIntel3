#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace compintel::runtime {

struct CompileResult {
    bool success = false;
    std::filesystem::path executable;
    std::string diagnostics;
};

// Compile runs synchronously. Call it from a worker thread, never the UI thread.
class Compiler {
public:
    static CompileResult Compile(std::string_view source,
                                 const std::filesystem::path& outputDirectory,
                                 const std::filesystem::path& compiler = {});
};

struct TurnResult {
    bool success = false;
    std::string line;
    std::string diagnostics;
    std::uint64_t wallMilliseconds = 0;
    std::uint64_t cpuMicroseconds = 0;
};

struct ProcessLimits {
    std::chrono::milliseconds turnCpu{100};
    std::chrono::milliseconds totalCpu{5000};
    std::size_t outputLineBytes = 64;
    std::size_t memoryMiB = 256;
};

// A local development runner with resource limits, not a security sandbox.
// The child stays alive for the match and is suspended between Turn calls.
class PlayerProcess {
public:
    explicit PlayerProcess(std::filesystem::path executable, ProcessLimits limits = {});
    ~PlayerProcess();
    PlayerProcess(const PlayerProcess&) = delete;
    PlayerProcess& operator=(const PlayerProcess&) = delete;

    bool Start(std::string* error = nullptr);
    TurnResult Turn(std::string_view input,
                    std::chrono::milliseconds timeout = std::chrono::milliseconds{1000});
    void Stop() noexcept;
    bool Running() const;

private:
    struct State;
    std::filesystem::path executable_;
    ProcessLimits limits_;
    mutable std::mutex mutex_;
    std::shared_ptr<State> state_;
    std::uint64_t generation_ = 0;
};

} // namespace compintel::runtime
