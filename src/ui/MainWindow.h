#pragma once
#include "Storage.h"
#include "../runtime/PlayerProcess.h"
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.h>
#include <array>
#include <memory>
#include <atomic>

namespace compintel::ui
{
class MainWindow : public std::enable_shared_from_this<MainWindow>
{
public:
    explicit MainWindow(std::filesystem::path const& dataDirectory = {});
    void Show();
    ~MainWindow();

private:
    friend struct UiSmokeTests;
    template<typename T> T Find(wchar_t const* name) const { return m_root.FindName(name).as<T>(); }
    void Navigate(int page);
    void Notify(std::wstring const& message, bool error = false);
    bool Save();
    void Bind();
    void PopulatePlayers(bool preserveSelection = false);
    void PopulatePrograms();
    void PopulateReplays();
    void DrawBoard(winrt::Microsoft::UI::Xaml::Controls::Canvas const& canvas, Game const& game, bool interactive);
    void RefreshArena();
    void StartMatch();
    void StopMatch(bool archive = true);
    void FinishMatch();
    void Advance();
    void HumanMove(int x, int y);
    void ApplyMove(Move const& move, uint64_t milliseconds = 0);
    void Undo();
    void Archive(std::string const& reason);
    winrt::fire_and_forget ThinkAsync(int generation);
    winrt::fire_and_forget CompileAsync();
    winrt::fire_and_forget ImportSource();
    winrt::fire_and_forget ExportSource();
    winrt::fire_and_forget ImportReplay();
    winrt::fire_and_forget ExportReplay();
    void LoadEditorGame();
    void StoreDraft();
    void RenderReplay();
    void OpenDataDirectory();
    void SetWallMode(int mode);

    winrt::Microsoft::UI::Xaml::Window m_window{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Grid m_root{nullptr};
    winrt::Microsoft::UI::Dispatching::DispatcherQueue m_dispatcher{nullptr};
    winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_saveTimer{nullptr};
    winrt::Microsoft::UI::Dispatching::DispatcherQueueTimer m_replayTimer{nullptr};
    Storage m_storage;
    Game m_game;
    GameKind m_editorGame{GameKind::Gomoku};
    std::array<int,2> m_playerChoice{0,1}; // 0 human, 1 builtin, 2+ immutable versions.
    std::vector<ProgramVersion> m_available;
    std::array<std::shared_ptr<runtime::PlayerProcess>,2> m_process;
    std::shared_ptr<std::atomic_bool> m_cancelled{std::make_shared<std::atomic_bool>(false)};
    std::array<bool,2> m_initialized{};
    std::array<std::wstring,2> m_names;
    std::vector<uint64_t> m_timings;
    std::vector<int> m_programRows;
    bool m_running{false};
    bool m_busy{false};
    bool m_compiling{false};
    bool m_updating{false};
    bool m_dirty{false};
    bool m_archived{false};
    bool m_closed{false};
    int m_generation{0};
    int m_matchTimeout{1000};
    int m_wallMode{0}; // 0 pawn; 1 horizontal; 2 vertical.
    int m_page{0};
    int m_replayIndex{-1};
    int m_replayStep{0};
    std::wstring m_matchMessage;
};
}
