#include "Storage.h"
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <Windows.h>
#include <shlobj.h>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

#ifdef GetObject
#undef GetObject
#endif

namespace compintel::ui
{
using namespace winrt;
using namespace Windows::Data::Json;
namespace fs = std::filesystem;

namespace
{
constexpr size_t MaxFileBytes = 32 * 1024 * 1024;
constexpr size_t MaxSourceBytes = 256 * 1024;
constexpr uint32_t MaxLibraryEntries = 1000;

void CheckSource(std::string_view source)
{
    if (source.size() > MaxSourceBytes) throw std::runtime_error("源代码超过 256 KiB 限制");
}

void CheckText(std::wstring_view value, size_t maximum = 512)
{
    if (value.size() > maximum) throw std::runtime_error("文字字段超过长度限制");
}

bool ValidId(std::string_view value)
{
    if (value.size() != 36) return false;
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (i == 8 || i == 13 || i == 18 || i == 23)
        {
            if (value[i] != '-') return false;
        }
        else if (!((value[i] >= '0' && value[i] <= '9') || (value[i] >= 'a' && value[i] <= 'f') ||
                   (value[i] >= 'A' && value[i] <= 'F'))) return false;
    }
    return true;
}

std::string CanonicalId(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

std::string Read(fs::path const& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("无法读取文件");
    if (fs::file_size(path) > MaxFileBytes) throw std::runtime_error("文件超过 32 MiB 限制");
    std::string contents{std::istreambuf_iterator<char>(stream), {}};
    if (contents.size() > MaxFileBytes) throw std::runtime_error("文件超过 32 MiB 限制");
    return contents;
}

void Write(fs::path const& path, std::string const& contents)
{
    if (contents.size() > MaxFileBytes) throw std::runtime_error("文件超过 32 MiB 限制");
    auto temporary = path;
    temporary += L".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) throw std::runtime_error("无法写入文件");
        stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        stream.flush();
        if (!stream) throw std::runtime_error("保存失败，请检查磁盘空间");
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("无法替换保存文件");
}

void Set(JsonObject const& object, wchar_t const* key, std::wstring_view value)
{
    object.SetNamedValue(key, JsonValue::CreateStringValue(value));
}
void Set(JsonObject const& object, wchar_t const* key, std::string_view value)
{
    object.SetNamedValue(key, JsonValue::CreateStringValue(to_hstring(value)));
}
void Set(JsonObject const& object, wchar_t const* key, double value)
{
    object.SetNamedValue(key, JsonValue::CreateNumberValue(value));
}
GameKind Kind(JsonObject const& object)
{
    auto name = object.GetNamedString(L"game");
    if (name == L"gomoku") return GameKind::Gomoku;
    if (name == L"quoridor") return GameKind::Quoridor;
    throw std::runtime_error("未知游戏类型");
}
JsonObject SerializeReplay(Replay const& replay)
{
    JsonObject result;
    Set(result, L"format", std::string_view("compintel3.replay.v1"));
    Set(result, L"id", replay.id);
    Set(result, L"game", std::string_view(replay.game == GameKind::Gomoku ? "gomoku" : "quoridor"));
    Set(result, L"first", replay.first);
    Set(result, L"second", replay.second);
    Set(result, L"created", replay.created);
    Set(result, L"winner", static_cast<double>(replay.winner));
    Set(result, L"reason", replay.reason);
    JsonArray moves;
    Game game(replay.game);
    for (size_t i = 0; i < replay.moves.size(); ++i)
    {
        JsonObject move;
        Set(move, L"action", game.FormatMove(replay.moves[i]));
        Set(move, L"ms", static_cast<double>(i < replay.milliseconds.size() ? replay.milliseconds[i] : 0));
        moves.Append(move);
    }
    result.SetNamedValue(L"moves", moves);
    return result;
}
Replay DeserializeReplay(JsonObject const& object)
{
    if (object.GetNamedString(L"format") != L"compintel3.replay.v1")
        throw std::runtime_error("不是 CompIntel3 v1 回放文件");
    Replay result;
    result.game = Kind(object);
    result.id = to_string(object.GetNamedString(L"id", to_hstring(Storage::NewId())));
    if (!ValidId(result.id)) throw std::runtime_error("回放编号无效");
    result.first = object.GetNamedString(L"first", L"先手");
    result.second = object.GetNamedString(L"second", L"后手");
    result.created = object.GetNamedString(L"created", L"");
    CheckText(result.first);
    CheckText(result.second);
    CheckText(result.created, 128);
    double winner = object.GetNamedNumber(L"winner", -1);
    if (winner != -1 && winner != 0 && winner != 1) throw std::runtime_error("回放胜负字段无效");
    result.winner = static_cast<int>(winner);
    result.reason = to_string(object.GetNamedString(L"reason", L""));
    // A retained 16 KiB stderr stream may expand when converted to UTF-8.
    if (result.reason.size() > 64 * 1024) throw std::runtime_error("回放诊断字段超过长度限制");
    Game game(result.game);
    auto moves = object.GetNamedArray(L"moves");
    if (moves.Size() > (result.game == GameKind::Gomoku ? 225u : 200u))
        throw std::runtime_error("回放步数超过限制");
    for (auto const& item : moves)
    {
        auto entry = item.GetObject();
        Move move;
        std::string error;
        if (!game.ParseMove(to_string(entry.GetNamedString(L"action")), move, &error) || !game.Play(move, &error))
            throw std::runtime_error("回放包含非法行动：" + error);
        result.moves.push_back(move);
        auto milliseconds = entry.GetNamedNumber(L"ms", 0);
        if (!std::isfinite(milliseconds) || milliseconds < 0 || milliseconds > 3600000 ||
            std::floor(milliseconds) != milliseconds) throw std::runtime_error("回放耗时无效");
        result.milliseconds.push_back(static_cast<uint64_t>(milliseconds));
    }
    if (game.Finished() && game.Winner() != result.winner)
        throw std::runtime_error("回放结果与棋盘结果不一致");
    if (!game.Finished() && result.winner >= 0 && result.reason.empty())
        throw std::runtime_error("未在棋盘上结束的回放须提供判负原因");
    return result;
}
}

Storage::Storage(fs::path dataDirectory)
{
    if (dataDirectory.empty())
    {
        PWSTR local = nullptr;
        check_hresult(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local));
        std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> folder(local, &CoTaskMemFree);
        root = fs::path(local) / L"CompIntel3";
    }
    else root = std::move(dataDirectory);
    fs::create_directories(root / L"programs");
    fs::create_directories(root / L"exports");
    gomokuDraft = Example(GameKind::Gomoku);
    quoridorDraft = Example(GameKind::Quoridor);
}

void Storage::Load()
{
    warning.clear();
    auto path = root / L"library.json";
    if (!fs::exists(path)) return;
    try
    {
        auto object = JsonObject::Parse(to_hstring(Read(path)));
        if (object.GetNamedNumber(L"schema", 0) != 1) throw std::runtime_error("资料库版本不受支持");
        auto settings = object.GetNamedObject(L"settings", JsonObject());
        std::wstring loadedCompiler(settings.GetNamedString(L"compiler", L""));
        CheckText(loadedCompiler, 32768);
        auto timeout = settings.GetNamedNumber(L"turnTimeout", 1000);
        if (!std::isfinite(timeout)) throw std::runtime_error("回合超时设置无效");
        int loadedTimeout = static_cast<int>(std::clamp(timeout, 100.0, 10000.0));
        auto loadedTheme = settings.GetNamedBoolean(L"lightTheme", false);
        // Old libraries only store lightTheme; retain that explicit preference.
        auto loadedSystemTheme = settings.GetNamedBoolean(L"followSystemTheme", !settings.HasKey(L"lightTheme"));
        auto loadedGomokuDraft = to_string(object.GetNamedString(L"gomokuDraft", to_hstring(gomokuDraft)));
        auto loadedQuoridorDraft = to_string(object.GetNamedString(L"quoridorDraft", to_hstring(quoridorDraft)));
        std::wstring loadedDraftName(object.GetNamedString(L"draftName", draftName));
        CheckSource(loadedGomokuDraft);
        CheckSource(loadedQuoridorDraft);
        CheckText(loadedDraftName);
        std::vector<ProgramVersion> loadedPrograms;
        std::vector<Replay> loadedReplays;
        std::set<std::string> programIds;
        std::set<std::string> replayIds;
        auto programEntries = object.GetNamedArray(L"programs", JsonArray());
        auto replayEntries = object.GetNamedArray(L"replays", JsonArray());
        if (programEntries.Size() > MaxLibraryEntries || replayEntries.Size() > MaxLibraryEntries)
            throw std::runtime_error("资料库条目超过限制");
        for (auto const& item : programEntries)
        {
            auto entry = item.GetObject();
            ProgramVersion version;
            version.id = to_string(entry.GetNamedString(L"id"));
            version.name = entry.GetNamedString(L"name");
            version.game = Kind(entry);
            version.source = to_string(entry.GetNamedString(L"source"));
            // Executables are always resolved inside our own version directory.
            if (!ValidId(version.id))
                throw std::runtime_error("程序版本编号无效");
            if (!programIds.insert(CanonicalId(version.id)).second) throw std::runtime_error("程序版本编号重复");
            version.executable = root / L"programs" / fs::path(std::wstring(to_hstring(version.id))) / L"player.exe";
            version.created = entry.GetNamedString(L"created", L"");
            CheckSource(version.source);
            CheckText(version.name);
            CheckText(version.created, 128);
            loadedPrograms.push_back(std::move(version));
        }
        for (auto const& item : replayEntries)
        {
            auto replay = DeserializeReplay(item.GetObject());
            if (!replayIds.insert(CanonicalId(replay.id)).second) throw std::runtime_error("回放编号重复");
            loadedReplays.push_back(std::move(replay));
        }
        compiler = std::move(loadedCompiler);
        turnTimeout = loadedTimeout;
        lightTheme = loadedTheme;
        followSystemTheme = loadedSystemTheme;
        gomokuDraft = std::move(loadedGomokuDraft);
        quoridorDraft = std::move(loadedQuoridorDraft);
        draftName = std::move(loadedDraftName);
        programs = std::move(loadedPrograms);
        replays = std::move(loadedReplays);
    }
    catch (...)
    {
        programs.clear();
        replays.clear();
        auto backup = root / (L"library-recovery-" + std::wstring(to_hstring(NewId())) + L".json");
        std::error_code error;
        fs::copy_file(path, backup, fs::copy_options::none, error);
        warning = L"资料库读取失败。原文件" + std::wstring(error ? L"仍保留在 library.json" : L"已备份到 " + backup.filename().wstring()) + L"；当前使用空资料库。";
        if (error) throw std::runtime_error("资料库损坏且无法备份，请检查本地数据目录权限");
    }
}

void Storage::Save() const
{
    CheckSource(gomokuDraft);
    CheckSource(quoridorDraft);
    CheckText(draftName);
    CheckText(compiler, 32768);
    if (programs.size() > MaxLibraryEntries || replays.size() > MaxLibraryEntries)
        throw std::runtime_error("资料库条目超过限制");
    JsonObject object;
    Set(object, L"schema", 1.0);
    JsonObject settings;
    Set(settings, L"compiler", compiler);
    Set(settings, L"turnTimeout", static_cast<double>(turnTimeout));
    settings.SetNamedValue(L"lightTheme", JsonValue::CreateBooleanValue(lightTheme));
    settings.SetNamedValue(L"followSystemTheme", JsonValue::CreateBooleanValue(followSystemTheme));
    object.SetNamedValue(L"settings", settings);
    Set(object, L"gomokuDraft", gomokuDraft);
    Set(object, L"quoridorDraft", quoridorDraft);
    Set(object, L"draftName", draftName);
    JsonArray versions;
    std::set<std::string> programIds;
    for (auto const& version : programs)
    {
        if (!ValidId(version.id)) throw std::runtime_error("程序版本编号无效");
        if (!programIds.insert(CanonicalId(version.id)).second) throw std::runtime_error("程序版本编号重复");
        CheckSource(version.source);
        CheckText(version.name);
        CheckText(version.created, 128);
        JsonObject entry;
        Set(entry, L"id", version.id);
        Set(entry, L"name", version.name);
        Set(entry, L"game", std::string_view(version.game == GameKind::Gomoku ? "gomoku" : "quoridor"));
        Set(entry, L"source", version.source);
        Set(entry, L"created", version.created);
        versions.Append(entry);
    }
    object.SetNamedValue(L"programs", versions);
    JsonArray matches;
    std::set<std::string> replayIds;
    for (auto const& replay : replays)
    {
        auto serialized = SerializeReplay(replay);
        DeserializeReplay(serialized);
        if (!replayIds.insert(CanonicalId(replay.id)).second) throw std::runtime_error("回放编号重复");
        matches.Append(serialized);
    }
    object.SetNamedValue(L"replays", matches);
    Write(root / L"library.json", to_string(object.Stringify()));
}

void Storage::ExportReplay(Replay const& replay, fs::path const& path) const
{
    auto serialized = SerializeReplay(replay);
    DeserializeReplay(serialized);
    Write(path, to_string(serialized.Stringify()));
}

Replay Storage::ImportReplay(fs::path const& path) const
{
    return DeserializeReplay(JsonObject::Parse(to_hstring(Read(path))));
}

std::string Storage::NewId()
{
    GUID guid;
    check_hresult(CoCreateGuid(&guid));
    wchar_t text[40]{};
    StringFromGUID2(guid, text, 40);
    return CanonicalId(to_string(text).substr(1, 36));
}

std::wstring Storage::Now()
{
    SYSTEMTIME time;
    GetLocalTime(&time);
    wchar_t buffer[32];
    swprintf_s(buffer, L"%04u-%02u-%02u %02u:%02u", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
    return buffer;
}

std::string Storage::Example(GameKind game)
{
    auto resource = FindResourceW(nullptr, MAKEINTRESOURCEW(game == GameKind::Gomoku ? 101 : 102), RT_RCDATA);
    if (!resource) throw std::runtime_error("找不到示例程序资源");
    auto loaded = LoadResource(nullptr, resource);
    if (!loaded) throw std::runtime_error("无法加载示例程序资源");
    auto data = static_cast<char const*>(LockResource(loaded));
    if (!data) throw std::runtime_error("无法读取示例程序资源");
    return {data, SizeofResource(nullptr, resource)};
}

std::wstring Storage::GameName(GameKind game)
{
    return game == GameKind::Gomoku ? L"五子棋" : L"路墙棋";
}
}
