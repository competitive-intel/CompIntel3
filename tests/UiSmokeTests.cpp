// Integration checks inside the real WinUI dispatcher. This harness does not
// inject input, inspect other applications, or use OS UI automation.
#include "../src/ui/MainWindow.h"
#include "../src/ui/UiMarkup.h"

#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <coroutine>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace winrt;
using namespace Microsoft::UI::Xaml;
namespace fs = std::filesystem;
using namespace std::chrono_literals;

int exitCode = 1;
int checkCount = 0;
std::ofstream logFile;
fs::path testDataDirectory;

void Log(std::string_view message)
{
    logFile << message << '\n';
    logFile.flush();
}

void Check(bool condition, std::string const& description)
{
    ++checkCount;
    Log(std::string(condition ? "PASS " : "FAIL ") + description);
    if (!condition) throw std::runtime_error(description);
}

struct Foreground
{
    Microsoft::UI::Dispatching::DispatcherQueue queue;
    bool await_ready() const noexcept { return queue.HasThreadAccess(); }
    void await_resume() const noexcept {}
    void await_suspend(std::coroutine_handle<> continuation) const
    {
        if (!queue.TryEnqueue([continuation] { continuation.resume(); })) continuation.destroy();
    }
};

void ExitAfterUnhandled(std::string const& message)
{
    exitCode = 1;
    Log("FAIL unhandled: " + message);
    Application::Current().Exit();
}
}

namespace compintel::ui {
using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Controls::Primitives;

struct UiSmokeTests
{
    static void Attach(std::shared_ptr<MainWindow> const& window)
    {
        auto ran = std::make_shared<bool>(false);
        std::weak_ptr<MainWindow> weak = window;
        window->m_root.Loaded([weak, ran](auto&&, auto&&) {
            if (std::exchange(*ran, true)) return;
            if (auto strong = weak.lock()) Run(std::move(strong));
        });
    }

    // Capture our own live XAML visual tree. This does not depend on a desktop
    // automation helper, and renders the production controls, not a mockup.
    static Windows::Foundation::IAsyncAction Capture(std::shared_ptr<MainWindow> window, std::wstring name)
    {
        using namespace Microsoft::UI::Xaml::Media::Imaging;
        using namespace Windows::Graphics::Imaging;
        using namespace Windows::Storage;
        using namespace Windows::Storage::Streams;
        co_await resume_after(250ms);
        co_await Foreground{window->m_dispatcher};
        window->m_root.UpdateLayout();
        if (name.ends_with(L"settings"))
        {
            auto content = window->Find<StackPanel>(L"SettingsContent");
            auto bounds = content.TransformToVisual(window->m_root).TransformBounds(
                Windows::Foundation::Rect{0, 0, static_cast<float>(content.ActualWidth()), 1});
            Log("Settings content bounds: " + std::to_string(bounds.X) + " + " + std::to_string(bounds.Width));
            Check(bounds.X >= 0 && bounds.X + bounds.Width <= window->m_root.ActualWidth() - 16,
                  "settings content stays inside the window: " + to_string(name));
        }
        if (name == L"quoridor-active")
        {
            auto list = window->Find<ListView>(L"MoveList");
            auto container = list.ContainerFromIndex(0).try_as<FrameworkElement>();
            Log("Move list: items=" + std::to_string(list.Items().Size()) + " height=" +
                std::to_string(list.ActualHeight()) + " container=" + std::to_string(static_cast<bool>(container)));
            if (container) Log("Move container: " + std::to_string(container.ActualWidth()) + " x " + std::to_string(container.ActualHeight()));
            Check(list.Items().Size() > 0 && container && container.ActualHeight() > 0,
                  "move history realizes native list rows on the active board");
        }
        RenderTargetBitmap bitmap;
        co_await bitmap.RenderAsync(window->m_root);
        auto buffer = co_await bitmap.GetPixelsAsync();
        Check(bitmap.PixelWidth() > 0 && bitmap.PixelHeight() > 0 && buffer.Length() > 0,
              "live XAML screenshot has pixels: " + to_string(name));
        auto directory = fs::absolute(L"artifacts/ui-review");
        fs::create_directories(directory);
        auto folder = co_await StorageFolder::GetFolderFromPathAsync(directory.wstring());
        auto file = co_await folder.CreateFileAsync(name + L".png", CreationCollisionOption::ReplaceExisting);
        auto stream = co_await file.OpenAsync(FileAccessMode::ReadWrite);
        auto encoder = co_await BitmapEncoder::CreateAsync(BitmapEncoder::PngEncoderId(), stream);
        com_array<uint8_t> pixels(buffer.Length());
        DataReader::FromBuffer(buffer).ReadBytes(pixels);
        encoder.SetPixelData(BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied,
                             bitmap.PixelWidth(), bitmap.PixelHeight(), 96, 96, pixels);
        co_await encoder.FlushAsync();
        Log("SCREENSHOT " + to_string(file.Path()));
    }

    static Windows::Foundation::IAsyncAction ReviewAppearance(std::shared_ptr<MainWindow> window)
    {
        const wchar_t* names[] = {L"arena", L"editor", L"replay", L"rules", L"settings"};
        auto originalSize = window->m_window.AppWindow().Size();
        auto originalTheme = window->m_root.RequestedTheme();
        const auto scale = window->m_root.XamlRoot().RasterizationScale();
        window->m_window.AppWindow().Resize(Windows::Graphics::SizeInt32{static_cast<int>(1440 * scale), static_cast<int>(1000 * scale)});
        for (auto theme : {ElementTheme::Light, ElementTheme::Dark})
        {
            window->m_root.RequestedTheme(theme);
            for (int page = 0; page < 5; ++page)
            {
                window->Navigate(page);
                window->m_root.UpdateLayout();
                co_await resume_after(150ms);
                co_await Foreground{window->m_dispatcher};
                Check(window->Find<NavigationView>(L"AppNavigation").SelectedItem() ==
                      window->Find<NavigationViewItem>(page == 0 ? L"NavArena" : page == 1 ? L"NavEditor" :
                          page == 2 ? L"NavReplay" : page == 3 ? L"NavRules" : L"NavSettings"),
                      "native navigation selection follows page " + std::to_string(page));
                co_await Capture(window, std::wstring(theme == ElementTheme::Light ? L"light-" : L"dark-") + names[page]);
            }
        }
        window->m_window.AppWindow().Resize(Windows::Graphics::SizeInt32{static_cast<int>(1000 * scale), static_cast<int>(800 * scale)});
        for (int page = 0; page < 5; ++page)
        {
            window->Navigate(page);
            co_await resume_after(150ms);
            co_await Foreground{window->m_dispatcher};
            window->m_root.UpdateLayout();
            Check(window->Find<NavigationView>(L"AppNavigation").DisplayMode() == NavigationViewDisplayMode::Compact,
                  "navigation compacts at a smaller desktop width");
            co_await Capture(window, std::wstring(L"compact-") + names[page]);
        }
        window->m_window.AppWindow().Resize(originalSize);
        window->m_root.RequestedTheme(originalTheme);
        window->Navigate(0);
    }

    static fire_and_forget Run(std::shared_ptr<MainWindow> window)
    {
        auto dispatcher = window->m_dispatcher;
        try
        {
            co_await resume_after(50ms);
            co_await Foreground{dispatcher};
            Log("Starting real WinUI integration checks.");
            Log("Isolated data directory: " + to_string(testDataDirectory.wstring()));
            Check(window->m_storage.root == testDataDirectory,
                  "the real window uses an isolated test data directory");
            Check(window->m_root.IsLoaded() && window->m_root.ActualWidth() > 0,
                  "the real XAML root was loaded and laid out");

            // Verify the entire namescope, including controls on collapsed pages.
            std::wstring_view markup(UiMarkup);
            std::size_t position = 0;
            std::size_t namedControls = 0;
            while ((position = markup.find(L"x:Name=\"", position)) != std::wstring_view::npos)
            {
                position += 8;
                const auto end = markup.find(L'\"', position);
                if (end == std::wstring_view::npos) throw std::runtime_error("Unterminated XAML name.");
                std::wstring name(markup.substr(position, end - position));
                Check(static_cast<bool>(window->m_root.FindName(name)), "XAML name resolves: " + to_string(name));
                ++namedControls;
                position = end + 1;
            }
            Check(namedControls > 60, "all named controls were inspected");
            // Exercise the native selection event, not just our Navigate helper.
            window->Find<NavigationView>(L"AppNavigation").SelectedItem(window->Find<NavigationViewItem>(L"NavEditor"));
            Check(window->m_page == 1, "native navigation selection opens the editor");
            window->Find<NavigationView>(L"AppNavigation").SelectedItem(window->Find<NavigationViewItem>(L"NavArena"));
            Check(window->m_page == 0, "native navigation selection returns to the arena");
            for (int choice : {1, 2, 0})
            {
                window->Find<ComboBox>(L"ThemeSelector").SelectedIndex(choice);
                Check(window->m_root.RequestedTheme() == (choice == 1 ? ElementTheme::Light :
                      choice == 2 ? ElementTheme::Dark : ElementTheme::Default), "theme selector previews choice " + std::to_string(choice));
            }
            const wchar_t* pageNames[] = {L"ArenaPage", L"EditorPage", L"ReplayPage", L"RulesPage", L"SettingsPage"};
            for (int page = 0; page < 5; ++page)
            {
                window->Navigate(page);
                window->m_root.UpdateLayout();
                for (int other = 0; other < 5; ++other)
                    Check(window->Find<FrameworkElement>(pageNames[other]).Visibility() ==
                          (other == page ? Visibility::Visible : Visibility::Collapsed),
                          "navigation page " + std::to_string(page) + ": " + to_string(pageNames[other]));
                Check(window->Find<FrameworkElement>(pageNames[page]).ActualHeight() > 0 &&
                      window->Find<FrameworkElement>(pageNames[page]).ActualWidth() > 0,
                      "visible page " + std::to_string(page) + " has a nonzero layout");
            }
            window->m_root.RequestedTheme(ElementTheme::Light);
            window->m_root.UpdateLayout();
            window->m_root.RequestedTheme(ElementTheme::Dark);
            window->m_root.UpdateLayout();
            Check(!window->Find<TextBlock>(L"PageTitle").Text().empty(), "both themes and page resources load");

            co_await ReviewAppearance(window);

            // Nine actual HumanMove calls drive the production match pipeline.
            window->Navigate(0);
            window->Find<ComboBox>(L"GameSelector").SelectedIndex(0);
            window->Find<ComboBox>(L"FirstPlayer").SelectedIndex(0);
            window->Find<ComboBox>(L"SecondPlayer").SelectedIndex(0);
            const auto replaysBeforeGomoku = window->m_storage.replays.size();
            window->StartMatch();
            Check(window->m_running && !window->m_busy, "human versus human Gomoku starts");
            const Move gomokuMoves[] = {{2,2}, {2,10}, {3,2}, {3,10}, {4,2}, {4,10}, {5,2}, {5,10}, {6,2}};
            for (auto move : gomokuMoves) window->HumanMove(move.x, move.y);
            Check(window->m_game.Finished() && window->m_game.Winner() == 0 &&
                  window->m_game.History().size() == 9 && !window->m_running,
                  "nine human moves produce a white five-in-a-row win");
            Check(window->m_archived && window->m_storage.replays.size() == replaysBeforeGomoku + 1,
                  "a finished human game is automatically archived once");
            const Replay winningReplay = window->m_storage.replays.front();
            window->Navigate(2);
            window->m_replayIndex = 0;
            window->m_replayStep = static_cast<int>(winningReplay.moves.size());
            window->RenderReplay();
            Check(window->Find<Slider>(L"ReplaySlider").Maximum() == 9 &&
                  window->Find<Canvas>(L"ReplayCanvas").Children().Size() > 0,
                  "the replay page reconstructs and renders the winning position");
            const auto exportPath = window->m_storage.root / L"exports/ui-smoke-replay.json";
            window->m_storage.ExportReplay(winningReplay, exportPath);
            const auto imported = window->m_storage.ImportReplay(exportPath);
            Check(imported.moves == winningReplay.moves && imported.winner == 0,
                  "production replay export/import preserves moves and winner");

            // Use the human wall-placement path, then the real asynchronous
            // built-in opponent while the human follows the shortest path.
            window->Navigate(0);
            window->Find<ComboBox>(L"GameSelector").SelectedIndex(1);
            window->Find<ComboBox>(L"FirstPlayer").SelectedIndex(0);
            window->Find<ComboBox>(L"SecondPlayer").SelectedIndex(1);
            const auto replaysBeforeQuoridor = window->m_storage.replays.size();
            window->StartMatch();
            window->SetWallMode(1);
            window->HumanMove(4,2);
            Check(window->m_game.Walls().size() == 1 && window->m_game.WallsRemaining(0) == 9,
                  "human Quoridor wall placement updates the board and wall inventory");
            window->SetWallMode(0);
            co_await Capture(window, L"quoridor-active");
            auto deadline = std::chrono::steady_clock::now() + 20s;
            while (window->m_running && std::chrono::steady_clock::now() < deadline)
            {
                if (!window->m_busy && window->m_game.NextSeat() == 0)
                {
                    const auto move = window->m_game.SuggestMove();
                    window->HumanMove(move.x, move.y);
                }
                co_await resume_after(20ms);
                co_await Foreground{dispatcher};
            }
            Check(window->m_game.Finished() && !window->m_running && window->m_game.Winner() >= 0,
                  "human versus asynchronous built-in Quoridor reaches a goal");
            Check(window->m_storage.replays.size() == replaysBeforeQuoridor + 1,
                  "the complete Quoridor game is archived");

            // Compile through the actual editor workflow, including copying the
            // executable to the stable version path and updating both lists.
            window->Navigate(1);
            window->Find<ComboBox>(L"EditorGame").SelectedIndex(0);
            window->Find<TextBox>(L"ProgramName").Text(L"UI smoke player");
            window->Find<TextBox>(L"SourceEditor").Text(to_hstring(Storage::Example(GameKind::Gomoku)));
            window->m_storage.compiler = L"cl.exe";
            const auto programsBefore = window->m_storage.programs.size();
            window->CompileAsync();
            Check(window->m_compiling, "the real editor enters the background compilation state");
            deadline = std::chrono::steady_clock::now() + 90s;
            while (window->m_compiling && std::chrono::steady_clock::now() < deadline)
            {
                co_await resume_after(25ms);
                co_await Foreground{dispatcher};
            }
            if (window->m_storage.programs.size() != programsBefore + 1)
                Log("Compiler output: " + to_string(window->Find<TextBox>(L"CompilerOutput").Text()));
            Check(!window->m_compiling && window->m_storage.programs.size() == programsBefore + 1,
                  "editor compilation saves exactly one successful program version");
            const auto version = window->m_storage.programs.back();
            Check(fs::exists(version.executable) && version.executable ==
                  window->m_storage.root / L"programs" / to_hstring(version.id).c_str() / L"player.exe",
                  "the version executable exists at the reloadable immutable path");
            Check(window->Find<Button>(L"CompileButton").IsEnabled() &&
                  window->Find<ListView>(L"ProgramList").Items().Size() >= 1,
                  "editor controls and program lists return to their ready state");

            // Run the compiled program in both seats through ThinkAsync, rather
            // than calling the runtime directly. Each seat receives two turns.
            window->Navigate(0);
            window->Find<ComboBox>(L"GameSelector").SelectedIndex(0);
            window->PopulatePlayers();
            Check(window->m_available.size() >= 1, "compiled versions appear in the arena player choices");
            const int customChoice = 2 + static_cast<int>(window->m_available.size()) - 1;
            for (int seat = 0; seat < 2; ++seat)
            {
                window->Find<ComboBox>(L"FirstPlayer").SelectedIndex(seat == 0 ? customChoice : 0);
                window->Find<ComboBox>(L"SecondPlayer").SelectedIndex(seat == 1 ? customChoice : 0);
                window->StartMatch();
                deadline = std::chrono::steady_clock::now() + 8s;
                while (window->m_running && window->m_game.History().size() < 4 &&
                       std::chrono::steady_clock::now() < deadline)
                {
                    if (!window->m_busy && window->m_playerChoice[window->m_game.NextSeat()] == 0)
                    {
                        auto move = window->m_game.SuggestMove();
                        window->HumanMove(move.x, move.y);
                    }
                    co_await resume_after(15ms);
                    co_await Foreground{dispatcher};
                }
                if (window->m_game.History().size() < 4) Log("Match detail: " + to_string(window->m_matchMessage));
                Check(window->m_running && window->m_game.History().size() >= 4 && window->m_initialized[seat],
                      "compiled program completes persistent turns in seat " + std::to_string(seat));
                auto process = window->m_process[seat];
                window->StopMatch();
                Check(!window->m_running && process && !process->Running(),
                      "stopping the match terminates compiled player seat " + std::to_string(seat));
            }

            window->Find<ComboBox>(L"FirstPlayer").SelectedIndex(customChoice);
            window->Find<ComboBox>(L"SecondPlayer").SelectedIndex(customChoice);
            window->StartMatch();
            deadline = std::chrono::steady_clock::now() + 8s;
            while (window->m_running && window->m_game.History().size() < 6 &&
                   std::chrono::steady_clock::now() < deadline)
            {
                co_await resume_after(15ms);
                co_await Foreground{dispatcher};
            }
            Check(window->m_game.History().size() >= 6 && window->m_initialized[0] && window->m_initialized[1],
                  "two compiled players exchange legal moves through the arena scheduler");
            window->StopMatch();
            Check(!window->m_process[0]->Running() && !window->m_process[1]->Running(),
                  "program-versus-program stop cleans up both processes");

            window->Find<ComboBox>(L"FirstPlayer").SelectedIndex(customChoice);
            window->Find<ComboBox>(L"SecondPlayer").SelectedIndex(0);
            window->StartMatch();
            auto immediatelyStopped = window->m_process[0];
            window->StopMatch(false);
            co_await resume_after(150ms);
            co_await Foreground{dispatcher};
            Check(!window->m_running && immediatelyStopped && !immediatelyStopped->Running() &&
                  window->m_game.History().empty(),
                  "immediate Start/Stop cancels background startup without a late move or surviving process");

            // Exercise the UI's protocol-failure path using a genuinely compiled
            // second version, including a zero-move forfeit and automatic replay.
            window->Navigate(1);
            window->Find<TextBox>(L"ProgramName").Text(L"UI smoke invalid output");
            window->Find<TextBox>(L"SourceEditor").Text(LR"cpp(
#include <iostream>
int main() {
    int version, height, width, seat;
    if (!(std::cin >> version >> height >> width >> seat)) return 0;
    if (seat == 1) { int x, y; if (!(std::cin >> x >> y)) return 0; }
    std::cout << "invalid" << std::endl;
    int next; std::cin >> next;
}
)cpp");
            const auto beforeInvalidCompile = window->m_storage.programs.size();
            window->CompileAsync();
            deadline = std::chrono::steady_clock::now() + 90s;
            while (window->m_compiling && std::chrono::steady_clock::now() < deadline)
            {
                co_await resume_after(25ms);
                co_await Foreground{dispatcher};
            }
            if (window->m_storage.programs.size() != beforeInvalidCompile + 1)
                Log("Invalid fixture compiler output: " + to_string(window->Find<TextBox>(L"CompilerOutput").Text()));
            Check(!window->m_compiling && window->m_storage.programs.size() == beforeInvalidCompile + 1,
                  "the editor compiles a separate invalid-output fixture version");
            window->Navigate(0);
            window->PopulatePlayers();
            const int invalidChoice = 2 + static_cast<int>(window->m_available.size()) - 1;
            window->Find<ComboBox>(L"FirstPlayer").SelectedIndex(invalidChoice);
            window->Find<ComboBox>(L"SecondPlayer").SelectedIndex(0);
            const auto beforeForfeit = window->m_storage.replays.size();
            window->StartMatch();
            deadline = std::chrono::steady_clock::now() + 8s;
            while (window->m_running && std::chrono::steady_clock::now() < deadline)
            {
                co_await resume_after(15ms);
                co_await Foreground{dispatcher};
            }
            Check(!window->m_running && window->m_game.Finished() && window->m_game.Winner() == 1 &&
                  window->m_game.FinishReason().find("INVALID_MOVE") != std::string::npos,
                  "a compiled player's malformed protocol output automatically forfeits the UI match");
            Check(window->m_storage.replays.size() == beforeForfeit + 1 &&
                  window->m_storage.replays.front().moves.empty() &&
                  window->m_storage.replays.front().winner == 1,
                  "zero-move program failures are archived with the correct opponent winner");
            Check(to_string(window->Find<TextBox>(L"CompilerOutput").Text()).find("INVALID_MOVE") != std::string::npos &&
                  !window->m_process[0]->Running(),
                  "the failure diagnostic is visible and the faulty player is terminated");

            window->Navigate(2);
            Check(!window->Find<Slider>(L"ReplaySlider").IsEnabled() &&
                  !window->Find<Button>(L"ReplayPlay").IsEnabled() &&
                  window->Find<Button>(L"ExportReplay").IsEnabled(),
                  "a zero-move forfeit can be exported without offering empty playback");
            window->StoreDraft();
            window->Save();
            Storage reopened(testDataDirectory);
            reopened.Load();
            Check(reopened.warning.empty() && reopened.programs.size() == window->m_storage.programs.size() &&
                  reopened.replays.size() == window->m_storage.replays.size(),
                  "a new Storage instance reloads all program versions and match archives");
            Check(reopened.compiler == L"cl.exe" && reopened.gomokuDraft == window->m_storage.gomokuDraft &&
                  fs::exists(reopened.programs.back().executable),
                  "reload preserves settings, source draft and executable resolution");
            window->Navigate(2);
            // The newest replay is a zero-move forfeit; select the preceding
            // program match to verify the interactive move slider as well.
            window->Find<ListView>(L"ReplayList").SelectedIndex(1);
            window->m_replayStep = 1;
            window->RenderReplay();
            Check(window->Find<Slider>(L"ReplaySlider").IsEnabled(), "the newly archived program match opens in replay");

            co_await Capture(window, L"replay-populated");
            window->Navigate(1);
            co_await Capture(window, L"editor-populated");
            window->m_saveTimer.Stop();
            window->m_replayTimer.Stop();
            Log("SUCCESS " + std::to_string(checkCount) + " real WinUI integration checks passed.");
            exitCode = 0;
        }
        catch (hresult_error const& error)
        {
            Log("FAIL HRESULT " + std::to_string(static_cast<long>(error.code())) + ": " + to_string(error.message()));
        }
        catch (std::exception const& error) { Log(std::string("FAIL exception: ") + error.what()); }
        catch (...) { Log("FAIL unknown exception."); }
        window->StopMatch(false);
        window->m_saveTimer.Stop();
        window->m_replayTimer.Stop();
        Application::Current().Exit();
    }
};
}

namespace {
struct SmokeApp : ApplicationT<SmokeApp, Markup::IXamlMetadataProvider>
{
    XamlTypeInfo::XamlControlsXamlMetaDataProvider metadata;
    std::shared_ptr<compintel::ui::MainWindow> mainWindow;
    SmokeApp()
    {
        UnhandledException([](auto const&, UnhandledExceptionEventArgs const& args) {
            args.Handled(true);
            ExitAfterUnhandled(to_string(args.Message()));
        });
    }
    Markup::IXamlType GetXamlType(Windows::UI::Xaml::Interop::TypeName const& type) { return metadata.GetXamlType(type); }
    Markup::IXamlType GetXamlType(hstring const& name) { return metadata.GetXamlType(name); }
    com_array<Markup::XmlnsDefinition> GetXmlnsDefinitions() { return metadata.GetXmlnsDefinitions(); }
    void OnLaunched(LaunchActivatedEventArgs const&)
    {
        try
        {
            Log("SmokeApp OnLaunched: initializing WinUI resources.");
            Resources().MergedDictionaries().Append(Controls::XamlControlsResources());
            Log("SmokeApp OnLaunched: constructing MainWindow.");
            mainWindow = std::make_shared<compintel::ui::MainWindow>(testDataDirectory);
            Log("SmokeApp OnLaunched: showing MainWindow.");
            compintel::ui::UiSmokeTests::Attach(mainWindow);
            mainWindow->Show();
        }
        catch (hresult_error const& error) { ExitAfterUnhandled("OnLaunched HRESULT: " + to_string(error.message())); }
        catch (std::exception const& error) { ExitAfterUnhandled(std::string("OnLaunched exception: ") + error.what()); }
        catch (...) { ExitAfterUnhandled("OnLaunched unknown exception."); }
    }
};
}

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    try
    {
        // The build/test entry point runs this executable from the repository.
        const auto artifacts = fs::absolute(L"artifacts");
        fs::create_directories(artifacts);
        logFile.open(artifacts / L"ui-smoke.log", std::ios::binary | std::ios::trunc);
        if (!logFile) return 2;
        init_apartment(apartment_type::single_threaded);
        testDataDirectory = artifacts / L"ui-smoke-data" / to_hstring(compintel::ui::Storage::NewId()).c_str();
        Log("CompIntel3 WinUI integration smoke test");
        Application::Start([](auto&&) { make<SmokeApp>(); });
    }
    catch (hresult_error const& error) { Log("FAIL startup HRESULT: " + to_string(error.message())); }
    catch (std::exception const& error) { Log(std::string("FAIL startup exception: ") + error.what()); }
    catch (...) { Log("FAIL unknown startup exception."); }
    return exitCode;
}
