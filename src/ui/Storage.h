#pragma once
#include "../core/Game.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace compintel::ui
{
struct ProgramVersion
{
    std::string id;
    std::wstring name;
    GameKind game{GameKind::Gomoku};
    std::string source;
    std::filesystem::path executable;
    std::wstring created;
};

struct Replay
{
    std::string id;
    GameKind game{GameKind::Gomoku};
    std::wstring first;
    std::wstring second;
    std::wstring created;
    std::vector<Move> moves;
    std::vector<uint64_t> milliseconds;
    int winner{-1};
    std::string reason;
};

class Storage
{
public:
    explicit Storage(std::filesystem::path dataDirectory = {});
    void Load();
    void Save() const;
    void ExportReplay(Replay const& replay, std::filesystem::path const& path) const;
    Replay ImportReplay(std::filesystem::path const& path) const;
    static std::string NewId();
    static std::wstring Now();
    static std::string Example(GameKind game);
    static std::wstring GameName(GameKind game);

    std::filesystem::path root;
    std::vector<ProgramVersion> programs;
    std::vector<Replay> replays;
    std::string gomokuDraft;
    std::string quoridorDraft;
    std::wstring draftName{L"我的第一个 Player"};
    std::wstring compiler;
    int turnTimeout{1000};
    bool lightTheme{false};
    bool followSystemTheme{true};
    std::wstring warning;
};
}
