#include "ui/Storage.h"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace compintel;
using namespace compintel::ui;
using namespace winrt;
using namespace Windows::Data::Json;
namespace fs = std::filesystem;

namespace
{
void Require(bool condition, std::string const& message)
{
    if (!condition) throw std::runtime_error(message);
}

void Throws(std::function<void()> const& action, std::string const& message)
{
    bool thrown = false;
    try { action(); }
    catch (...) { thrown = true; }
    Require(thrown, message);
}

std::string Read(fs::path const& path)
{
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), {}};
}

void Write(fs::path const& path, std::string const& text)
{
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << text;
    Require(static_cast<bool>(file), "Test file write failed");
}

Replay WinningReplay()
{
    Game game;
    for (int x = 0; x < 4; ++x)
    {
        Require(game.Play({x, 3}), "Winning fixture first move");
        Require(game.Play({x * 2, 8}), "Winning fixture second move");
    }
    Require(game.Play({4, 3}), "Winning fixture final move");
    Replay result;
    result.id = Storage::NewId();
    result.game = game.Kind();
    result.first = L"白棋 · Player";
    result.second = L"黑棋 · 人类";
    result.created = Storage::Now();
    result.winner = game.Winner();
    result.reason = game.Result();
    for (auto const& entry : game.History())
    {
        result.moves.push_back(entry.move);
        result.milliseconds.push_back(result.moves.size() * 3);
    }
    return result;
}

void RoundTrip(fs::path const& directory)
{
    Storage first(directory);
    Require(first.gomokuDraft.find("int main") != std::string::npos &&
            first.quoridorDraft.find("int main") != std::string::npos, "Both sample resources are embedded");
    first.gomokuDraft = "// 五子棋草稿\nint main() { return 1; }\n";
    first.quoridorDraft = "// 路墙棋草稿\nint main() { return 2; }\n";
    first.draftName = L"测试 Player 中文";
    first.compiler = L"C:\\Program Files\\工具链\\cl.exe";
    first.turnTimeout = 337;
    first.lightTheme = true;
    first.followSystemTheme = false;
    ProgramVersion version;
    version.id = Storage::NewId();
    Require(version.id.find_first_of("ABCDEF") == std::string::npos, "New IDs use lowercase hex");
    std::transform(version.id.begin(), version.id.end(), version.id.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
    version.name = L"历史版本 甲";
    version.game = GameKind::Quoridor;
    version.source = first.quoridorDraft;
    version.created = Storage::Now();
    first.programs.push_back(version);
    first.replays.push_back(WinningReplay());
    first.Save();

    Storage loaded(directory);
    loaded.Load();
    Require(loaded.warning.empty(), "Uppercase legacy GUID must load without recovery");
    Require(loaded.gomokuDraft == first.gomokuDraft && loaded.quoridorDraft == first.quoridorDraft,
            "Both UTF-8 drafts survive save/load");
    Require(loaded.draftName == first.draftName && loaded.compiler == first.compiler &&
            loaded.turnTimeout == 337 && loaded.lightTheme && !loaded.followSystemTheme, "Settings survive save/load");
    Require(loaded.programs.size() == 1 && loaded.programs[0].id == version.id &&
            loaded.programs[0].name == version.name && loaded.programs[0].source == version.source &&
            loaded.programs[0].game == version.game, "Compiled version metadata survives save/load");
    Require(loaded.programs[0].executable == directory / L"programs" / fs::path(std::wstring(to_hstring(version.id))) / L"player.exe",
            "Executable path is rebuilt inside the program directory");
    Require(loaded.replays.size() == 1 && loaded.replays[0].moves == first.replays[0].moves &&
            loaded.replays[0].milliseconds == first.replays[0].milliseconds && loaded.replays[0].winner == 0,
            "Replay moves, timings and winner survive save/load");
    loaded.Load();
    Require(loaded.programs.size() == 1 && loaded.replays.size() == 1, "Repeated Load replaces existing entries");
    auto exported = directory / L"exports" / L"回放.json";
    loaded.ExportReplay(loaded.replays.front(), exported);
    auto imported = loaded.ImportReplay(exported);
    Require(imported.id == loaded.replays[0].id && imported.moves == loaded.replays[0].moves,
            "Export/import with Unicode file path round trips");

    auto json = JsonObject::Parse(to_hstring(Read(directory / L"library.json")));
    json.GetNamedObject(L"settings").SetNamedValue(L"turnTimeout", JsonValue::CreateNumberValue(1e99));
    Write(directory / L"library.json", to_string(json.Stringify()));
    loaded.Load();
    Require(loaded.warning.empty() && loaded.turnTimeout == 10000, "Huge finite timeout clamps before integer conversion");

    auto original = Read(directory / L"library.json");
    loaded.gomokuDraft.assign(256 * 1024 + 1, 'x');
    Throws([&] { loaded.Save(); }, "Oversized source must be rejected before replacing saved data");
    Require(Read(directory / L"library.json") == original, "Rejected save preserves the previous file");
}

void ThemePreferences(fs::path const& directory)
{
    Storage storage(directory);
    Require(storage.followSystemTheme, "New libraries follow Windows appearance");
    storage.Save();
    Storage reopened(directory);
    reopened.Load();
    Require(reopened.warning.empty() && reopened.followSystemTheme, "System appearance survives restart");
    for (bool light : {false, true})
    {
        auto json = JsonObject::Parse(to_hstring(Read(directory / L"library.json")));
        auto settings = json.GetNamedObject(L"settings");
        settings.Remove(L"followSystemTheme");
        settings.SetNamedValue(L"lightTheme", JsonValue::CreateBooleanValue(light));
        Write(directory / L"library.json", to_string(json.Stringify()));
        reopened.Load();
        Require(reopened.warning.empty() && !reopened.followSystemTheme && reopened.lightTheme == light,
                "Legacy explicit appearance is retained without resetting the library");
    }
}

void ReplayValidation(fs::path const& directory)
{
    Storage storage(directory);
    auto path = directory / L"exports" / L"replay.json";
    storage.ExportReplay(WinningReplay(), path);
    auto valid = JsonObject::Parse(to_hstring(Read(path)));
    auto reject = [&](std::function<void(JsonObject const&)> const& edit, char const* message)
    {
        auto changed = JsonObject::Parse(valid.Stringify());
        edit(changed);
        Write(path, to_string(changed.Stringify()));
        Throws([&] { storage.ImportReplay(path); }, message);
    };
    reject([](JsonObject const& o) { o.SetNamedValue(L"winner", JsonValue::CreateNumberValue(1)); },
           "Winner inconsistent with the final board is rejected");
    reject([](JsonObject const& o) {
        o.GetNamedArray(L"moves").GetObjectAt(0).SetNamedValue(L"action", JsonValue::CreateStringValue(L"99 0\n"));
    }, "Out of board move is rejected");
    reject([](JsonObject const& o) {
        o.GetNamedArray(L"moves").GetObjectAt(1).SetNamedValue(L"action", JsonValue::CreateStringValue(L"0 3\n"));
    }, "Repeated stone placement is rejected");
    reject([](JsonObject const& o) {
        auto moves = o.GetNamedArray(L"moves");
        moves.Append(moves.GetAt(0));
    }, "Moves after terminal position are rejected");
    reject([](JsonObject const& o) {
        o.GetNamedArray(L"moves").GetObjectAt(0).SetNamedValue(L"ms", JsonValue::CreateNumberValue(1.5));
    }, "Fractional timing is rejected");
    reject([](JsonObject const& o) {
        o.GetNamedArray(L"moves").GetObjectAt(0).SetNamedValue(L"ms", JsonValue::CreateNumberValue(1e99));
    }, "Huge timing is rejected before conversion");
    reject([](JsonObject const& o) { o.SetNamedValue(L"id", JsonValue::CreateStringValue(L"../../player.exe")); },
           "Path-like replay identifier is rejected");
    reject([](JsonObject const& o) {
        auto moves = o.GetNamedArray(L"moves");
        while (moves.Size() > 1) moves.RemoveAtEnd();
        o.SetNamedValue(L"reason", JsonValue::CreateStringValue(L""));
    }, "An unfinished board cannot declare a winner without an adjudication reason");

    Replay forfeit;
    forfeit.id = Storage::NewId();
    forfeit.moves = {{7, 7}};
    forfeit.milliseconds = {1};
    forfeit.winner = 0;
    forfeit.reason = "TIME_LIMIT_EXCEEDED\n[stderr]\n" + std::string(48 * 1024, 'x');
    storage.ExportReplay(forfeit, path);
    auto importedForfeit = storage.ImportReplay(path);
    Require(importedForfeit.winner == 0 && importedForfeit.reason == forfeit.reason,
            "A documented runtime forfeit with expanded diagnostics may end before a board win");
}

void Recovery(fs::path const& directory)
{
    Storage storage(directory);
    auto path = directory / L"library.json";
    std::string damaged = "{ malformed data that must be preserved\n";
    Write(path, damaged);
    storage.Load();
    Require(!storage.warning.empty() && storage.programs.empty() && storage.replays.empty(),
            "Damaged library reports recovery and starts with empty lists");
    Require(Read(path) == damaged, "Damaged original remains until the next explicit save");
    size_t backups = 0;
    for (auto const& entry : fs::directory_iterator(directory))
    {
        if (entry.path().filename().wstring().starts_with(L"library-recovery-"))
        {
            Require(Read(entry.path()) == damaged, "Recovery backup retains original bytes");
            ++backups;
        }
    }
    Require(backups == 1, "Exactly one recovery copy is created per failed load");
}
}

int main()
{
    init_apartment(apartment_type::single_threaded);
    auto directory = fs::temp_directory_path() / (L"CompIntel3-storage-tests-" + std::wstring(to_hstring(Storage::NewId())));
    int result = 0;
    try
    {
        RoundTrip(directory / L"roundtrip");
        std::cout << "PASS storage versions, drafts, settings and replay round trip\n";
        ThemePreferences(directory / L"themes");
        std::cout << "PASS system appearance and legacy theme migration\n";
        ReplayValidation(directory / L"validation");
        std::cout << "PASS replay terminal state, winner, moves and numeric validation\n";
        Recovery(directory / L"recovery");
        std::cout << "PASS damaged library recovery preserves original and backup\n";
    }
    catch (std::exception const& error)
    {
        std::cerr << "FAIL " << error.what() << '\n';
        result = 1;
    }
    catch (hresult_error const& error)
    {
        std::cerr << "FAIL " << to_string(error.message()) << '\n';
        result = 1;
    }
    if (fs::weakly_canonical(directory).parent_path() == fs::weakly_canonical(fs::temp_directory_path()))
    {
        std::error_code ignored;
        fs::remove_all(directory, ignored);
    }
    return result;
}
