#include "MainWindow.h"
#include "UiMarkup.h"
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.FileProperties.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.UI.Text.h>
#include <microsoft.ui.xaml.window.h>
#include <shobjidl_core.h>
#include <shellapi.h>
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <chrono>
#include <coroutine>
#include <fstream>
#include <sstream>

namespace compintel::ui
{
using namespace winrt;
using namespace Microsoft::UI;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Controls::Primitives;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Shapes;
using namespace Microsoft::UI::Xaml::Input;
using namespace Windows::Foundation;
namespace fs = std::filesystem;

namespace
{
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

// Bind to XAML ThemeResource expressions rather than copying resolved colors.
// This also updates the game surface when Windows switches contrast themes.
void ThemeFill(FrameworkElement const& element, DependencyProperty const& property, Shapes::Rectangle const& source)
{
    Data::Binding binding;
    binding.Source(source); binding.Path(PropertyPath(L"Fill")); binding.Mode(Data::BindingMode::OneWay);
    element.SetBinding(property, binding);
}
void At(Canvas const& canvas, FrameworkElement const& element, double x, double y)
{
    Canvas::SetLeft(element, x);
    Canvas::SetTop(element, y);
    canvas.Children().Append(element);
}
void Dot(Canvas const& canvas, double x, double y, double diameter, Shapes::Rectangle const& source)
{
    Shapes::Ellipse dot;
    dot.Width(diameter); dot.Height(diameter);
    ThemeFill(dot, Shapes::Shape::FillProperty(), source);
    dot.IsHitTestVisible(false);
    At(canvas, dot, x - diameter / 2, y - diameter / 2);
}
void Text(Canvas const& canvas, std::wstring const& value, double x, double y, Shapes::Rectangle const& source,
          double width = 28, double fontSize = 12)
{
    TextBlock text;
    text.Text(value); text.FontSize(fontSize); text.Width(width);
    text.TextAlignment(TextAlignment::Center); text.IsHitTestVisible(false);
    ThemeFill(text, TextBlock::ForegroundProperty(), source);
    At(canvas, text, x, y);
}
void RectangleAt(Canvas const& canvas, double x, double y, double width, double height,
                 Shapes::Rectangle const& source, double radius = 0)
{
    Shapes::Rectangle rectangle;
    rectangle.Width(width); rectangle.Height(height);
    ThemeFill(rectangle, Shapes::Shape::FillProperty(), source);
    rectangle.RadiusX(radius); rectangle.RadiusY(radius); rectangle.IsHitTestVisible(false);
    At(canvas, rectangle, x, y);
}
ElementTheme ThemeForChoice(int index)
{
    return index == 1 ? ElementTheme::Light : index == 2 ? ElementTheme::Dark : ElementTheme::Default;
}
HWND Handle(Window const& window)
{
    HWND handle{};
    check_hresult(window.as<IWindowNative>()->get_WindowHandle(&handle));
    return handle;
}
std::wstring ErrorText()
{
    try { throw; }
    catch (hresult_error const& error) { return std::wstring(error.message()); }
    catch (std::exception const& error) { return std::wstring(to_hstring(error.what())); }
    catch (...) { return L"操作未能完成。"; }
}
std::string SourceText(TextBox const& editor)
{
    // WinUI's text engine can expose paragraph breaks as lone CR characters.
    auto raw=to_string(editor.Text());
    std::string source;
    source.reserve(raw.size());
    for(size_t i=0;i<raw.size();++i)
    {
        if(raw[i]!='\r') { source.push_back(raw[i]); continue; }
        source.push_back('\n');
        if(i+1<raw.size()&&raw[i+1]=='\n') ++i;
    }
    return source;
}
std::wstring Reason(std::string const& reason)
{
    if (reason == "win" || reason == "five_in_a_row") return L"五子连珠";
    if (reason == "goal_reached") return L"抵达目标边";
    if (reason == "draw" || reason == "board_full") return L"棋盘已满，和棋";
    if (reason == "move_limit") return L"达到 200 手上限";
    if (reason == "stopped") return L"手动结束";
    auto text=std::wstring(to_hstring(reason));
    return text.size()>300 ? text.substr(0,300)+L"…" : text;
}
StackPanel ListEntry(std::wstring const& title, std::wstring const& detail, Shapes::Rectangle const& secondary)
{
    StackPanel panel;
    panel.Spacing(4); panel.Margin({0,8,0,8});
    TextBlock first; first.Text(title); first.Style(Application::Current().Resources().Lookup(box_value(L"BodyTextBlockStyle")).as<Style>()); first.TextTrimming(TextTrimming::CharacterEllipsis);
    TextBlock second; second.Text(detail); second.Style(Application::Current().Resources().Lookup(box_value(L"CaptionTextBlockStyle")).as<Style>());
    ThemeFill(second, TextBlock::ForegroundProperty(), secondary); second.TextTrimming(TextTrimming::CharacterEllipsis);
    panel.Children().Append(first); panel.Children().Append(second);
    return panel;
}
LRESULT CALLBACK MinimumSize(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR)
{
    if (message == WM_GETMINMAXINFO)
    {
        auto info = reinterpret_cast<MINMAXINFO*>(lparam);
        auto dpi = GetDpiForWindow(hwnd);
        info->ptMinTrackSize = {MulDiv(960, dpi, 96), MulDiv(720, dpi, 96)};
    }
    return DefSubclassProc(hwnd, message, wparam, lparam);
}
}

MainWindow::MainWindow(std::filesystem::path const& dataDirectory) : m_storage(dataDirectory)
{
    m_storage.Load();
    m_window = Window();
    m_window.Title(L"CompIntel3 · 算法对弈工作台");
    m_root = Markup::XamlReader::Load(UiMarkup).as<Grid>();
    m_root.RequestedTheme(ThemeForChoice(m_storage.followSystemTheme ? 0 : m_storage.lightTheme ? 1 : 2));
    m_window.SystemBackdrop(MicaBackdrop());
    m_window.Content(m_root);
    m_window.ExtendsContentIntoTitleBar(true);
    m_window.SetTitleBar(Find<TitleBar>(L"AppTitleBar"));
    m_dispatcher = m_window.DispatcherQueue();
    auto hwnd = Handle(m_window);
    SetWindowSubclass(hwnd, MinimumSize, 1, 0);
    auto dpi = GetDpiForWindow(hwnd);
    auto monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO info{sizeof(info)};
    GetMonitorInfoW(monitor, &info);
    int width = std::min(MulDiv(1420, dpi, 96), static_cast<int>(info.rcWork.right - info.rcWork.left - 36));
    int height = std::min(MulDiv(940, dpi, 96), static_cast<int>(info.rcWork.bottom - info.rcWork.top - 36));
    m_window.AppWindow().Resize(Windows::Graphics::SizeInt32{width, height});
    m_window.AppWindow().Move(Windows::Graphics::PointInt32{info.rcWork.left + (info.rcWork.right - info.rcWork.left - width)/2, info.rcWork.top + (info.rcWork.bottom - info.rcWork.top - height)/2});
    Find<TextBox>(L"ProgramName").Text(m_storage.draftName);
    Find<TextBox>(L"SourceEditor").Text(to_hstring(m_storage.gomokuDraft));
    Find<TextBox>(L"CompilerPath").Text(m_storage.compiler);
    Find<NumberBox>(L"TurnTimeout").Value(m_storage.turnTimeout);
    Find<ComboBox>(L"ThemeSelector").SelectedIndex(m_storage.followSystemTheme ? 0 : m_storage.lightTheme ? 1 : 2);
    Find<TextBlock>(L"DataPath").Text(m_storage.root.wstring());
    Bind();
    PopulatePlayers();
    PopulatePrograms();
    PopulateReplays();
    Navigate(0);
    RefreshArena();
    RenderReplay();
    if (!m_storage.warning.empty()) Notify(m_storage.warning, true);
}

MainWindow::~MainWindow()
{
    for (auto& process : m_process) if (process) process->Stop();
}

void MainWindow::Show() { m_window.Activate(); }

void MainWindow::Bind()
{
    Find<NavigationView>(L"AppNavigation").SelectionChanged([this](auto&&, NavigationViewSelectionChangedEventArgs const& args) {
        if (auto item = args.SelectedItem().try_as<NavigationViewItem>())
        {
            auto page = std::stoi(std::wstring(unbox_value<hstring>(item.Tag())));
            if (page != m_page) Navigate(page);
        }
    });
    Find<ComboBox>(L"ThemeSelector").SelectionChanged([this](auto&&, auto&&) {
        m_root.RequestedTheme(ThemeForChoice(Find<ComboBox>(L"ThemeSelector").SelectedIndex()));
    });
    Find<ComboBox>(L"GameSelector").SelectionChanged([this](auto&&,auto&&) {
        if (m_updating || m_running) return;
        m_game = Game(Find<ComboBox>(L"GameSelector").SelectedIndex() == 0 ? GameKind::Gomoku : GameKind::Quoridor);
        m_matchMessage.clear(); m_timings.clear(); SetWallMode(0); PopulatePlayers(); RefreshArena();
    });
    Find<Button>(L"StartButton").Click([this](auto&&,auto&&) { StartMatch(); });
    Find<Button>(L"StopButton").Click([this](auto&&,auto&&) { StopMatch(); });
    Find<Button>(L"UndoButton").Click([this](auto&&,auto&&) { Undo(); });
    ToolTipService::SetToolTip(Find<Button>(L"UndoButton"),box_value(L"悔棋适用于内置棋手和双人练习。自定义程序的内部状态无法回退。"));
    Find<CheckBox>(L"ShowNumbers").Click([this](auto&&,auto&&) { RefreshArena(); RenderReplay(); });
    Find<ToggleButton>(L"PawnMode").Click([this](auto&&,auto&&) { SetWallMode(0); });
    Find<ToggleButton>(L"HorizontalMode").Click([this](auto&&,auto&&) { SetWallMode(1); });
    Find<ToggleButton>(L"VerticalMode").Click([this](auto&&,auto&&) { SetWallMode(2); });
    Find<Canvas>(L"BoardCanvas").PointerPressed([this](auto&&, PointerRoutedEventArgs const& args) {
        if (m_game.Kind() != GameKind::Quoridor || m_wallMode == 0) return;
        auto point = args.GetCurrentPoint(Find<Canvas>(L"BoardCanvas")).Position();
        // Internal wall intersections lie halfway between neighboring cell centers.
        int x = static_cast<int>(std::round((point.X - 48.0) / 56.0));
        int y = static_cast<int>(std::round((point.Y - 48.0) / 56.0));
        if (x >= 1 && x <= 8 && y >= 1 && y <= 8) HumanMove(x,y);
    });
    Find<ComboBox>(L"EditorGame").SelectionChanged([this](auto&&,auto&&) { if (!m_updating) LoadEditorGame(); });
    Find<TextBox>(L"SourceEditor").TextChanged([this](auto&&,auto&&) {
        if (m_updating) return;
        StoreDraft();
        auto text = SourceText(Find<TextBox>(L"SourceEditor"));
        Find<TextBlock>(L"EditorStatus").Text(L"草稿 · " + std::to_wstring(std::count(text.begin(),text.end(),'\n')+1) + L" 行 · " + std::to_wstring(text.size()) + L" 字节");
    });
    Find<TextBox>(L"ProgramName").TextChanged([this](auto&&,auto&&) { if (!m_updating) StoreDraft(); });
    Find<TextBox>(L"SourceEditor").KeyDown([this](auto&&, KeyRoutedEventArgs const& args) {
        if (args.Key() == Windows::System::VirtualKey::Tab)
        {
            Find<TextBox>(L"SourceEditor").SelectedText(L"    "); args.Handled(true);
        }
    });
    auto accelerator = [this](Windows::System::VirtualKey key, auto action) {
        KeyboardAccelerator item;
        item.Key(key); item.Modifiers(Windows::System::VirtualKeyModifiers::Control);
        item.Invoked([action](auto&&, KeyboardAcceleratorInvokedEventArgs const& args) { action(); args.Handled(true); });
        m_root.KeyboardAccelerators().Append(item);
    };
    accelerator(Windows::System::VirtualKey::S, [this] { StoreDraft(); if(Save()) Notify(L"草稿已保存。"); });
    accelerator(Windows::System::VirtualKey::B, [this] { if (m_page == 1) CompileAsync(); });
    Find<AppBarButton>(L"LoadExample").Click([this](auto&&,auto&&) {
        StoreDraft(); Save();
        // Preserve a named copy before replacing a draft with the bundled example.
        auto source = SourceText(Find<TextBox>(L"SourceEditor"));
        if (!source.empty())
        {
            auto path = m_storage.root / (L"draft-" + std::wstring(to_hstring(Storage::NewId())) + L".cpp");
            std::ofstream file(path, std::ios::binary); file << source;
            if (!file) { Notify(L"草稿备份失败，未替换编辑器。", true); return; }
        }
        Find<TextBox>(L"SourceEditor").Text(to_hstring(Storage::Example(m_editorGame)));
        Notify(L"已载入示例；之前的草稿已备份到数据目录。");
    });
    Find<Button>(L"CompileButton").Click([this](auto&&,auto&&) { CompileAsync(); });
    Find<AppBarButton>(L"ImportSource").Click([this](auto&&,auto&&) { ImportSource(); });
    Find<AppBarButton>(L"ExportSource").Click([this](auto&&,auto&&) { ExportSource(); });
    Find<ListView>(L"ProgramList").SelectionChanged([this](auto&&,auto&&) {
        if (m_updating) return;
        int row = Find<ListView>(L"ProgramList").SelectedIndex();
        if (row < 0 || row >= static_cast<int>(m_programRows.size())) return;
        auto const& version = m_storage.programs[m_programRows[row]];
        Find<TextBox>(L"ProgramName").Text(version.name);
        Find<TextBox>(L"SourceEditor").Text(to_hstring(version.source));
        Notify(L"已载入版本源码。后续编译会创建新版本。");
    });
    Find<ListView>(L"ReplayList").SelectionChanged([this](auto&&,auto&&) {
        if (m_updating) return;
        m_replayIndex = Find<ListView>(L"ReplayList").SelectedIndex(); m_replayStep = 0;
        if (m_replayTimer) m_replayTimer.Stop();
        RenderReplay();
    });
    Find<Slider>(L"ReplaySlider").ValueChanged([this](auto&&, RangeBaseValueChangedEventArgs const& args) {
        if (!m_updating) { m_replayStep = static_cast<int>(args.NewValue()); RenderReplay(); }
    });
    auto replayStep = [this](int step) {
        if (m_replayIndex < 0) return;
        m_replayTimer.Stop();
        m_replayStep = std::clamp(step, 0, static_cast<int>(m_storage.replays[m_replayIndex].moves.size())); RenderReplay();
    };
    Find<Button>(L"ReplayFirst").Click([replayStep](auto&&,auto&&) { replayStep(0); });
    Find<Button>(L"ReplayPrev").Click([this,replayStep](auto&&,auto&&) { replayStep(m_replayStep-1); });
    Find<Button>(L"ReplayNext").Click([this,replayStep](auto&&,auto&&) { replayStep(m_replayStep+1); });
    Find<Button>(L"ReplayLast").Click([replayStep](auto&&,auto&&) { replayStep(10000); });
    Find<Button>(L"ReplayPlay").Click([this](auto&&,auto&&) {
        if (m_replayIndex < 0) return;
        if (m_replayTimer.IsRunning()) m_replayTimer.Stop();
        else
        {
            if (m_replayStep >= static_cast<int>(m_storage.replays[m_replayIndex].moves.size())) m_replayStep=0;
            m_replayTimer.Start();
        }
        RenderReplay();
    });
    Find<Button>(L"ImportReplay").Click([this](auto&&,auto&&) { ImportReplay(); });
    Find<Button>(L"ExportReplay").Click([this](auto&&,auto&&) { ExportReplay(); });
    Find<Button>(L"GomokuExample").Click([this](auto&&,auto&&) { Find<ComboBox>(L"EditorGame").SelectedIndex(0); Navigate(1); });
    Find<Button>(L"QuoridorExample").Click([this](auto&&,auto&&) { Find<ComboBox>(L"EditorGame").SelectedIndex(1); Navigate(1); });
    Find<Button>(L"OpenData").Click([this](auto&&,auto&&) { OpenDataDirectory(); });
    Find<Button>(L"SaveSettings").Click([this](auto&&,auto&&) {
        double timeout = Find<NumberBox>(L"TurnTimeout").Value();
        if (!std::isfinite(timeout) || timeout < 100 || timeout > 10000) { Notify(L"每步时限请输入 100～10000 毫秒。", true); return; }
        m_storage.compiler = Find<TextBox>(L"CompilerPath").Text();
        m_storage.turnTimeout = static_cast<int>(timeout);
        int theme = Find<ComboBox>(L"ThemeSelector").SelectedIndex();
        m_storage.followSystemTheme = theme == 0;
        m_storage.lightTheme = theme == 1;
        m_dirty = true; if(Save()) Notify(L"设置已保存，将用于下次编译和新对局。");
    });
    m_saveTimer = m_dispatcher.CreateTimer(); m_saveTimer.Interval(std::chrono::seconds(4));
    m_saveTimer.Tick([this](auto&&,auto&&) { if (m_dirty) Save(); }); m_saveTimer.Start();
    m_replayTimer = m_dispatcher.CreateTimer(); m_replayTimer.Interval(std::chrono::milliseconds(450));
    m_replayTimer.Tick([this](auto&&,auto&&) {
        if (m_replayIndex < 0) { m_replayTimer.Stop(); return; }
        ++m_replayStep;
        if (m_replayStep >= static_cast<int>(m_storage.replays[m_replayIndex].moves.size())) m_replayTimer.Stop();
        RenderReplay();
    });
    m_window.Closed([this](auto&&,auto&&) {
        StopMatch(); StoreDraft(); Save(); m_closed=true; ++m_generation;
        m_saveTimer.Stop(); m_replayTimer.Stop();
    });
}

void MainWindow::Navigate(int page)
{
    if (page < 0 || page >= 5) return;
    if (m_page == 1) { StoreDraft(); Save(); }
    m_page=page;
    if (m_replayTimer) m_replayTimer.Stop();
    const wchar_t* pages[] = {L"ArenaPage",L"EditorPage",L"ReplayPage",L"RulesPage",L"SettingsPage"};
    const wchar_t* nav[] = {L"NavArena",L"NavEditor",L"NavReplay",L"NavRules",L"NavSettings"};
    const wchar_t* titles[] = {L"对弈工作台",L"程序实验室",L"对局回放",L"规则与协议",L"设置"};
    const wchar_t* subtitles[] = {L"选择游戏与棋手，开始本地对局。",L"编写、编译和管理你的 C++ Player。",L"按手数回顾已保存的对局。",L"查看游戏规则、输入输出格式与运行限制。",L"管理应用外观、编译器与本地数据。"};
    for(int i=0;i<5;++i)
    {
        Find<FrameworkElement>(pages[i]).Visibility(i==page ? Visibility::Visible : Visibility::Collapsed);
    }
    Find<NavigationView>(L"AppNavigation").SelectedItem(Find<NavigationViewItem>(nav[page]));
    Find<TextBlock>(L"PageTitle").Text(titles[page]); Find<TextBlock>(L"PageSubtitle").Text(subtitles[page]);
    Find<InfoBar>(L"Notice").IsOpen(false);
    if(page==2) RenderReplay();
}

void MainWindow::Notify(std::wstring const& message, bool error)
{
    if(m_closed) return;
    auto notice=Find<InfoBar>(L"Notice");
    notice.Severity(error ? InfoBarSeverity::Error : InfoBarSeverity::Informational);
    notice.Title(error ? L"需要处理" : L""); notice.Message(message); notice.IsOpen(true);
}

bool MainWindow::Save()
{
    try { m_storage.Save(); m_dirty=false; return true; }
    catch(...) { m_dirty=true; Notify(L"保存失败："+ErrorText(),true); return false; }
}

void MainWindow::StoreDraft()
{
    auto source=SourceText(Find<TextBox>(L"SourceEditor"));
    if(m_editorGame==GameKind::Gomoku) m_storage.gomokuDraft=source; else m_storage.quoridorDraft=source;
    m_storage.draftName=Find<TextBox>(L"ProgramName").Text(); m_dirty=true;
}

void MainWindow::LoadEditorGame()
{
    StoreDraft();
    m_editorGame=Find<ComboBox>(L"EditorGame").SelectedIndex()==0 ? GameKind::Gomoku : GameKind::Quoridor;
    m_updating=true;
    Find<TextBox>(L"SourceEditor").Text(to_hstring(m_editorGame==GameKind::Gomoku ? m_storage.gomokuDraft : m_storage.quoridorDraft));
    m_updating=false; PopulatePrograms();
}

void MainWindow::PopulatePlayers(bool preserveSelection)
{
    m_updating=true;
    auto first=Find<ComboBox>(L"FirstPlayer"), second=Find<ComboBox>(L"SecondPlayer");
    int firstSelection=preserveSelection ? std::max(0,first.SelectedIndex()) : 0;
    int secondSelection=preserveSelection && second.SelectedIndex()>=0 ? second.SelectedIndex() : 1;
    first.Items().Clear(); second.Items().Clear(); m_available.clear();
    for(auto const* name : {L"我来下棋",L"内置棋手"}) { first.Items().Append(box_value(name)); second.Items().Append(box_value(name)); }
    for(auto const& version : m_storage.programs) if(version.game==m_game.Kind())
    {
        m_available.push_back(version);
        auto label=version.name+L" · "+std::wstring(to_hstring(version.id.substr(0,6)));
        first.Items().Append(box_value(label)); second.Items().Append(box_value(label));
    }
    first.SelectedIndex(firstSelection<static_cast<int>(first.Items().Size()) ? firstSelection : 0);
    second.SelectedIndex(secondSelection<static_cast<int>(second.Items().Size()) ? secondSelection : 1);
    m_updating=false;
}

void MainWindow::PopulatePrograms()
{
    m_updating=true; m_programRows.clear();
    auto list=Find<ListView>(L"ProgramList"); list.Items().Clear();
    for(int i=static_cast<int>(m_storage.programs.size())-1;i>=0;--i)
    {
        auto const& version=m_storage.programs[i]; if(version.game!=m_editorGame) continue;
        m_programRows.push_back(i);
        list.Items().Append(ListEntry(version.name, version.created+L"  · "+std::wstring(to_hstring(version.id.substr(0,6))), Find<Shapes::Rectangle>(L"BoardText")));
    }
    Find<TextBlock>(L"NoPrograms").Visibility(m_programRows.empty()?Visibility::Visible:Visibility::Collapsed);
    m_updating=false;
}

void MainWindow::PopulateReplays()
{
    m_updating=true;
    auto list=Find<ListView>(L"ReplayList"); list.Items().Clear();
    for(auto const& replay : m_storage.replays)
    {
        std::wstring outcome=replay.winner==0 ? L"先手胜" : replay.winner==1 ? L"后手胜" : Reason(replay.reason);
        list.Items().Append(ListEntry(Storage::GameName(replay.game)+L" · "+outcome, replay.created+L"  /  "+std::to_wstring(replay.moves.size())+L" 手", Find<Shapes::Rectangle>(L"BoardText")));
    }
    Find<TextBlock>(L"NoReplays").Visibility(m_storage.replays.empty()?Visibility::Visible:Visibility::Collapsed);
    if(!m_storage.replays.empty())
    {
        m_replayIndex=std::clamp(m_replayIndex,0,static_cast<int>(m_storage.replays.size())-1);
        list.SelectedIndex(m_replayIndex);
    }
    m_updating=false;
}

void MainWindow::DrawBoard(Canvas const& canvas, Game const& game, bool interactive)
{
    canvas.Children().Clear();
    auto line = Find<Shapes::Rectangle>(L"BoardLine");
    auto text = Find<Shapes::Rectangle>(L"BoardText");
    auto cell = Find<Shapes::Rectangle>(L"BoardCell");
    auto edge = Find<Shapes::Rectangle>(L"BoardEdge");
    auto accent = Find<Shapes::Rectangle>(L"BoardAccent");
    auto white = Find<Shapes::Rectangle>(L"BoardWhite");
    auto black = Find<Shapes::Rectangle>(L"BoardBlack");
    bool gomoku=game.Kind()==GameKind::Gomoku;
    double start=gomoku ? 48 : 76, spacing=gomoku ? 36 : 56;
    int size=game.Size();
    bool canPlay=interactive&&m_running&&!m_busy&&!game.Finished()&&m_playerChoice[game.NextSeat()]==0;
    auto legal=(!gomoku&&canPlay&&m_wallMode==0) ? game.LegalPawnMoves(game.NextSeat()) : std::vector<Move>{};
    for(int n=0;n<size;++n)
    {
        Text(canvas,std::to_wstring(n),start+n*spacing-14,16,text);
        Text(canvas,std::to_wstring(n),13,start+n*spacing-8,text);
        if(gomoku)
        {
            RectangleAt(canvas,start,start+n*spacing,spacing*(size-1),1,line);
            RectangleAt(canvas,start+n*spacing,start,1,spacing*(size-1),line);
        }
    }
    if(gomoku) for(int x : {3,7,11}) for(int y : {3,7,11}) Dot(canvas,start+x*spacing,start+y*spacing,5,line);
    for(int y=0;y<size;++y) for(int x=0;x<size;++x)
    {
        double cx=start+x*spacing, cy=start+y*spacing;
        if(!gomoku)
        {
            RectangleAt(canvas,cx-23,cy-23,46,46,edge,4);
            RectangleAt(canvas,cx-22,cy-23,44,43,cell,4);
        }
        int seat=gomoku ? game.At(x,y) : (game.Pawn(0)==Position{x,y} ? 0 : game.Pawn(1)==Position{x,y} ? 1 : -1);
        if(seat>=0)
        {
            double diameter=gomoku?29:30;
            Shapes::Ellipse stone; stone.Width(diameter); stone.Height(diameter);
            ThemeFill(stone, Shapes::Shape::FillProperty(), seat==0 ? white : black);
            ThemeFill(stone, Shapes::Shape::StrokeProperty(), seat==0 ? black : white);
            stone.StrokeThickness(1);
            stone.IsHitTestVisible(false); At(canvas,stone,cx-diameter/2,cy-diameter/2);
            if(gomoku&&Find<CheckBox>(L"ShowNumbers").IsChecked().Value())
            {
                int number=0; for(size_t i=0;i<game.History().size();++i) if(game.History()[i].move.x==x&&game.History()[i].move.y==y) { number=static_cast<int>(i)+1; break; }
                Text(canvas,std::to_wstring(number),cx-15,cy-9,seat==0 ? black : white,30,11);
            }
        }
        if(!gomoku&&canPlay&&m_wallMode==0&&std::find(legal.begin(),legal.end(),Move{x,y})!=legal.end()) Dot(canvas,cx,cy,10,accent);
        if(interactive)
        {
            Button target;
            target.Width(gomoku?34:46); target.Height(gomoku?34:46);
            target.Style(Application::Current().Resources().Lookup(box_value(L"SubtleButtonStyle")).as<Style>());
            target.MinWidth(0); target.MinHeight(0); target.Padding({0});
            target.IsHitTestVisible(canPlay&&(gomoku||m_wallMode==0));
            target.IsTabStop(canPlay&&(gomoku||m_wallMode==0));
            Automation::AutomationProperties::SetName(target,L"棋格 "+std::to_wstring(x)+L", "+std::to_wstring(y));
            ToolTipService::SetToolTip(target,box_value(std::to_wstring(x)+L", "+std::to_wstring(y)));
            target.Click([this,x,y](auto&&,auto&&) { HumanMove(x,y); });
            At(canvas,target,cx-target.Width()/2,cy-target.Height()/2);
        }
    }
    if(!gomoku)
    {
        for(auto const& wall : game.Walls())
        {
            double wx=48+wall.x*56, wy=48+wall.y*56;
            if(wall.orientation==1) RectangleAt(canvas,wx-53,wy-5,106,10,accent,4);
            else RectangleAt(canvas,wx-5,wy-53,10,106,accent,4);
        }
        Text(canvas,L"↓ 先手目标边",370,573,text,170,12);
        Text(canvas,L"↑ 后手目标边",370,37,text,170,12);
        if(interactive&&canPlay&&m_wallMode!=0)
        {
            for(int x=1;x<=8;++x) for(int y=1;y<=8;++y)
            {
                Move wall{x,y,true,m_wallMode==1?1:0};
                if(game.IsLegal(wall)) Dot(canvas,48+x*56,48+y*56,7,accent);
            }
        }
    }
    if(!game.History().empty())
    {
        auto last=game.History().back().move;
        if(!last.wall && (!gomoku || !Find<CheckBox>(L"ShowNumbers").IsChecked().Value()))
        {
            // A contrasting surround keeps the last-move marker visible on both stones.
            auto surround = game.History().back().seat == 0 ? black : white;
            Dot(canvas,start+last.x*spacing,start+last.y*spacing,10,surround);
            Dot(canvas,start+last.x*spacing,start+last.y*spacing,6,accent);
        }
    }
}

void MainWindow::SetWallMode(int mode)
{
    m_wallMode=mode;
    Find<ToggleButton>(L"PawnMode").IsChecked(mode==0);
    Find<ToggleButton>(L"HorizontalMode").IsChecked(mode==1);
    Find<ToggleButton>(L"VerticalMode").IsChecked(mode==2);
    RefreshArena();
}

void MainWindow::RefreshArena()
{
    bool gomoku=m_game.Kind()==GameKind::Gomoku;
    Find<TextBlock>(L"BoardDescription").Text(gomoku?L"15 × 15 · 自由规则":L"9 × 9 · 抵达对岸");
    Find<TextBlock>(L"MoveCounter").Text(L"第 "+std::to_wstring(m_game.History().size())+L" 手");
    Find<TextBlock>(L"FirstSeatLabel").Text(gomoku?L"○  先手 · 白方":L"○  先手 · 向下抵达 Y=8");
    Find<TextBlock>(L"SecondSeatLabel").Text(gomoku?L"●  后手 · 黑方":L"●  后手 · 向上抵达 Y=0");
    Find<StackPanel>(L"WallTools").Visibility(gomoku?Visibility::Collapsed:Visibility::Visible);
    Find<ComboBox>(L"GameSelector").IsEnabled(!m_running);
    Find<ComboBox>(L"FirstPlayer").IsEnabled(!m_running); Find<ComboBox>(L"SecondPlayer").IsEnabled(!m_running);
    Find<Button>(L"StartButton").IsEnabled(!m_running); Find<Button>(L"StopButton").IsEnabled(m_running);
    bool canUndo=m_running&&!m_busy&&!m_game.History().empty()&&m_playerChoice[0]<2&&m_playerChoice[1]<2&&(m_playerChoice[0]==0||m_playerChoice[1]==0);
    Find<Button>(L"UndoButton").IsEnabled(canUndo);
    Find<ProgressRing>(L"ThinkingRing").IsActive(m_busy);
    Find<ProgressRing>(L"ThinkingRing").Visibility(m_busy?Visibility::Visible:Visibility::Collapsed);
    std::wstring status=L"等待开始", detail=L"你可以亲自下场，也可以让两个程序交锋。";
    if(m_running)
    {
        status=std::wstring(m_game.NextSeat()==0?L"先手":L"后手")+(m_busy?L"思考中…":L"回合");
        detail=m_names[m_game.NextSeat()]+(m_playerChoice[m_game.NextSeat()]==0?L" · 请选择你的下一步":L" · 正在计算下一步");
        if(!gomoku) detail+=L"\n剩余墙体：先手 "+std::to_wstring(m_game.WallsRemaining(0))+L" / 后手 "+std::to_wstring(m_game.WallsRemaining(1));
    }
    else if(m_game.Finished())
    {
        status=m_game.Winner()<0?L"对局结束":std::wstring(m_game.Winner()==0?L"先手获胜":L"后手获胜");
        detail=m_matchMessage.empty()?Reason(m_game.FinishReason()):m_matchMessage.substr(0,300);
    }
    else if(!m_matchMessage.empty()) { status=L"对局已结束"; detail=m_matchMessage; }
    Find<TextBlock>(L"MatchStatus").Text(status); Find<TextBlock>(L"MatchDetail").Text(detail);
    Find<TextBlock>(L"BoardHint").Text(m_running ? (m_busy?L"等待棋手回应…":gomoku?L"点击交叉点落子":m_wallMode==0?L"圆点表示合法移动":L"点击标记的交叉点放墙") : L"程序互战 · 人机对战 · 双人练习");
    Find<TextBlock>(L"LastTiming").Text(m_timings.empty()?L"— ms":std::to_wstring(m_timings.back())+L" ms");
    DrawBoard(Find<Canvas>(L"BoardCanvas"),m_game,true);
    Find<TextBlock>(L"NoMoves").Visibility(m_game.History().empty() ? Visibility::Visible : Visibility::Collapsed);
    auto list=Find<ListView>(L"MoveList");
    // RefreshArena also runs when the thinking state changes. Keep existing rows
    // so repeated refreshes do not restart native item animations or scrolling.
    bool historyChanged = list.Items().Size() != m_game.History().size();
    while (list.Items().Size() > m_game.History().size()) list.Items().RemoveAtEnd();
    for(size_t i=list.Items().Size();i<m_game.History().size();++i)
    {
        auto const& move=m_game.History()[i];
        auto label=std::to_wstring(i+1)+L"   "+(move.seat==0?L"○  ":L"●  ")+std::wstring(to_hstring(m_game.FormatMove(move.move)));
        if(!label.empty()&&label.back()==L'\n') label.pop_back();
        list.Items().Append(box_value(label));
    }
    if(historyChanged && list.Items().Size()>0) list.ScrollIntoView(list.Items().GetAt(list.Items().Size()-1));
}

void MainWindow::StartMatch()
{
    if(m_running) return;
    m_game=Game(Find<ComboBox>(L"GameSelector").SelectedIndex()==0?GameKind::Gomoku:GameKind::Quoridor);
    m_playerChoice={Find<ComboBox>(L"FirstPlayer").SelectedIndex(),Find<ComboBox>(L"SecondPlayer").SelectedIndex()};
    for(int seat=0;seat<2;++seat)
    {
        int choice=m_playerChoice[seat];
        if(choice<0) { Notify(L"请为双方选择棋手。",true); return; }
        m_names[seat]=choice==0?L"我来下棋":choice==1?L"内置棋手":m_available.at(choice-2).name;
        if(choice>=2)
        {
            auto const& version=m_available.at(choice-2);
            if(!fs::exists(version.executable)) { Notify(L"找不到程序编译产物，请在实验室重新编译该版本。",true); return; }
            m_process[seat]=std::make_shared<runtime::PlayerProcess>(version.executable);
        }
        else m_process[seat].reset();
    }
    ++m_generation; m_running=true; m_busy=false; m_archived=false; m_initialized={false,false}; m_matchTimeout=m_storage.turnTimeout;
    m_cancelled=std::make_shared<std::atomic_bool>(false);
    m_timings.clear(); m_matchMessage.clear(); SetWallMode(0);
    Find<InfoBar>(L"Notice").IsOpen(false); RefreshArena(); Advance();
}

void MainWindow::StopMatch(bool archive)
{
    if(!m_running) return;
    m_cancelled->store(true);
    ++m_generation; m_running=false; m_busy=false;
    for(auto& process:m_process) if(process) process->Stop();
    if(archive) Archive("stopped");
    m_matchMessage=m_archived ? L"本局已结束，可在对局回放中查看行棋记录。" : L"本局已结束。";
    PopulatePlayers(true); RefreshArena();
}

void MainWindow::FinishMatch()
{
    m_cancelled->store(true);
    m_running=false; m_busy=false;
    for(auto& process:m_process) if(process) process->Stop();
    Archive(m_game.FinishReason().empty()?m_game.Result():m_game.FinishReason());
    PopulatePlayers(true); RefreshArena();
}

void MainWindow::Archive(std::string const& reason)
{
    if(m_archived || (m_game.History().empty()&&!m_game.Finished())) return;
    m_replayTimer.Stop();
    Replay replay;
    replay.id=Storage::NewId(); replay.game=m_game.Kind(); replay.first=m_names[0]; replay.second=m_names[1];
    replay.created=Storage::Now(); replay.winner=m_game.Winner(); replay.reason=reason;
    for(auto const& action:m_game.History()) replay.moves.push_back(action.move);
    replay.milliseconds=m_timings;
    m_storage.replays.insert(m_storage.replays.begin(),std::move(replay)); m_archived=true;
    m_replayIndex=0; m_replayStep=0; m_dirty=true; Save(); PopulateReplays();
    if(m_page==2) RenderReplay();
}

void MainWindow::Advance()
{
    if(!m_running||m_busy) return;
    if(m_game.Finished()) { FinishMatch(); return; }
    if(m_playerChoice[m_game.NextSeat()]==0) { RefreshArena(); return; }
    m_busy=true; RefreshArena(); ThinkAsync(m_generation);
}

fire_and_forget MainWindow::ThinkAsync(int generation)
{
    auto lifetime=shared_from_this();
    auto game=m_game;
    int seat=game.NextSeat(); bool builtin=m_playerChoice[seat]==1;
    auto process=m_process[seat]; bool initialized=m_initialized[seat];
    auto cancelled=m_cancelled;
    auto timeout=m_matchTimeout;
    Move move; runtime::TurnResult result; std::string error;
    auto dispatcher=m_dispatcher;
    co_await resume_background();
    try
    {
        if(cancelled->load())
        {
            if(process) process->Stop();
        }
        else if(builtin)
        {
            auto begin=std::chrono::steady_clock::now(); move=game.SuggestMove();
            result.wallMilliseconds=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
            co_await resume_after(std::chrono::milliseconds(240));
        }
        else
        {
            std::string input;
            if(!initialized)
            {
                if(!process->Start(&error)) throw std::runtime_error(error);
                input=game.Initialization(seat);
            }
            if(!game.History().empty()) input+=game.FormatMove(game.History().back().move);
            if(cancelled->load()) process->Stop();
            else
            {
                result=process->Turn(input,std::chrono::milliseconds(timeout));
                if(!result.success) error=result.diagnostics.empty()?"Player process failed":result.diagnostics;
                else if(!game.ParseMove(result.line,move,&error)) error="INVALID_MOVE: "+error;
            }
        }
    }
    catch(...) { error=to_string(ErrorText()); }
    co_await Foreground{dispatcher};
    if(m_closed||generation!=m_generation||!m_running) { if(process) process->Stop(); co_return; }
    m_busy=false;
    if(!error.empty())
    {
        m_matchMessage=std::wstring(to_hstring(error)); m_game.Forfeit(seat,error);
        Find<TextBox>(L"CompilerOutput").Text(to_hstring(error));
        Notify(L"程序未能完成合法行动，已判负。诊断："+m_matchMessage.substr(0,300),true); FinishMatch(); co_return;
    }
    if(!builtin)
    {
        m_initialized[seat]=true;
        if(!result.diagnostics.empty()) Find<TextBox>(L"CompilerOutput").Text(to_hstring(result.diagnostics));
    }
    ApplyMove(move,result.wallMilliseconds);
}

void MainWindow::HumanMove(int x,int y)
{
    if(!m_running||m_busy||m_game.Finished()||m_playerChoice[m_game.NextSeat()]!=0) return;
    Move move{x,y,m_game.Kind()==GameKind::Quoridor&&m_wallMode!=0,m_wallMode==1?1:0};
    if(!m_game.IsLegal(move)) { Notify(L"这一步不符合规则，请选择合法棋格或墙位。",true); return; }
    Find<InfoBar>(L"Notice").IsOpen(false); ApplyMove(move);
}

void MainWindow::ApplyMove(Move const& move,uint64_t milliseconds)
{
    std::string error;
    if(!m_game.Play(move,&error))
    {
        m_matchMessage=L"非法行动："+std::wstring(to_hstring(error));
        m_game.Forfeit(m_game.NextSeat(),"INVALID_MOVE: "+error); FinishMatch(); return;
    }
    m_timings.push_back(milliseconds); RefreshArena();
    if(m_game.Finished()) FinishMatch(); else Advance();
}

void MainWindow::Undo()
{
    if(!m_running||m_busy||m_game.History().empty()||m_playerChoice[0]>=2||m_playerChoice[1]>=2) return;
    ++m_generation;
    do
    {
        if(!m_game.Undo()) break;
        if(!m_timings.empty()) m_timings.pop_back();
    } while(!m_game.History().empty()&&m_playerChoice[m_game.NextSeat()]!=0);
    RefreshArena(); Advance();
}

fire_and_forget MainWindow::CompileAsync()
{
    auto lifetime=shared_from_this();
    if(m_compiling) co_return;
    auto source=SourceText(Find<TextBox>(L"SourceEditor"));
    auto name=std::wstring(Find<TextBox>(L"ProgramName").Text());
    if(source.empty()||source.size()>256*1024) { Notify(L"源码不能为空，且不能超过 256 KiB。",true); co_return; }
    if(name.find_first_not_of(L" \t\r\n")==std::wstring::npos) { Notify(L"请填写程序名称。",true); co_return; }
    StoreDraft(); Save();
    ProgramVersion version;
    version.id=Storage::NewId(); version.name=name; version.game=m_editorGame; version.source=source; version.created=Storage::Now();
    auto output=m_storage.root/L"programs"/std::wstring(to_hstring(version.id));
    auto compiler=fs::path(m_storage.compiler);
    auto dispatcher=m_dispatcher;
    m_compiling=true;
    Find<Button>(L"CompileButton").IsEnabled(false); Find<ProgressBar>(L"CompileProgress").Visibility(Visibility::Visible);
    Find<TextBox>(L"CompilerOutput").Text(L"正在使用 C++20 编译，请稍候…");
    runtime::CompileResult result;
    co_await resume_background();
    try
    {
        result=runtime::Compiler::Compile(source,output,compiler);
        if(result.success)
        {
            auto immutableExecutable=output/L"player.exe";
            fs::copy_file(result.executable,immutableExecutable,fs::copy_options::none);
            result.executable=immutableExecutable;
        }
    }
    catch(...) { result.success=false; result.diagnostics=to_string(ErrorText()); }
    co_await Foreground{dispatcher};
    if(m_closed) co_return;
    m_compiling=false; Find<Button>(L"CompileButton").IsEnabled(true); Find<ProgressBar>(L"CompileProgress").Visibility(Visibility::Collapsed);
    Find<TextBox>(L"CompilerOutput").Text(to_hstring(result.diagnostics));
    if(!result.success) { Notify(L"编译失败，请根据输出修正源码。",true); co_return; }
    version.executable=result.executable; m_storage.programs.push_back(std::move(version)); m_dirty=true; bool saved=Save();
    PopulatePrograms(); if(!m_running) PopulatePlayers(true);
    if(saved) Notify(L"编译成功，已保存新版本。可在工作台的棋手列表中选择它。");
}

fire_and_forget MainWindow::ImportSource()
{
    auto lifetime=shared_from_this();
    try
    {
        Windows::Storage::Pickers::FileOpenPicker picker;
        picker.FileTypeFilter().Append(L".cpp"); picker.FileTypeFilter().Append(L".cc"); picker.FileTypeFilter().Append(L".cxx"); picker.FileTypeFilter().Append(L".txt");
        check_hresult(picker.as<IInitializeWithWindow>()->Initialize(Handle(m_window)));
        auto file=co_await picker.PickSingleFileAsync(); if(!file||m_closed) co_return;
        auto properties=co_await file.GetBasicPropertiesAsync();
        if(properties.Size()>256*1024) { Notify(L"源码超过 256 KiB 限制。",true); co_return; }
        auto text=co_await Windows::Storage::FileIO::ReadTextAsync(file,Windows::Storage::Streams::UnicodeEncoding::Utf8);
        if(m_closed) co_return;
        StoreDraft(); Save();
        Find<TextBox>(L"SourceEditor").Text(text); Find<TextBox>(L"ProgramName").Text(file.DisplayName());
        Notify(L"已导入 UTF-8 源码。");
    }
    catch(...) { Notify(L"导入失败："+ErrorText(),true); }
}

fire_and_forget MainWindow::ExportSource()
{
    auto lifetime=shared_from_this();
    try
    {
        Windows::Storage::Pickers::FileSavePicker picker;
        picker.FileTypeChoices().Insert(L"C++ 源码",single_threaded_vector<hstring>({L".cpp"}));
        picker.SuggestedFileName(L"player"); check_hresult(picker.as<IInitializeWithWindow>()->Initialize(Handle(m_window)));
        auto file=co_await picker.PickSaveFileAsync(); if(!file||m_closed) co_return;
        co_await Windows::Storage::FileIO::WriteTextAsync(file,to_hstring(SourceText(Find<TextBox>(L"SourceEditor"))),Windows::Storage::Streams::UnicodeEncoding::Utf8);
        Notify(L"源码已导出。");
    }
    catch(...) { Notify(L"导出失败："+ErrorText(),true); }
}

void MainWindow::RenderReplay()
{
    Game game;
    bool selected=m_replayIndex>=0&&m_replayIndex<static_cast<int>(m_storage.replays.size());
    if(selected)
    {
        auto const& replay=m_storage.replays[m_replayIndex]; game=Game(replay.game);
        m_replayStep=std::clamp(m_replayStep,0,static_cast<int>(replay.moves.size()));
        for(int i=0;i<m_replayStep;++i) game.Play(replay.moves[i]);
        Find<TextBlock>(L"ReplayTitle").Text(Storage::GameName(replay.game)+L" · "+replay.first+L" vs "+replay.second);
        Find<TextBlock>(L"ReplayDetail").Text(replay.created+L"  /  "+Reason(replay.reason)+(replay.winner>=0 ? (replay.winner==0?L" · 先手胜":L" · 后手胜") : L""));
        Find<TextBlock>(L"ReplayStep").Text(std::to_wstring(m_replayStep)+L" / "+std::to_wstring(replay.moves.size()));
        m_updating=true; Find<Slider>(L"ReplaySlider").Maximum(std::max(1.0,static_cast<double>(replay.moves.size())));
        Find<Slider>(L"ReplaySlider").Value(m_replayStep); m_updating=false;
    }
    int steps = selected ? static_cast<int>(m_storage.replays[m_replayIndex].moves.size()) : 0;
    Find<Slider>(L"ReplaySlider").IsEnabled(steps > 0);
    Find<Button>(L"ExportReplay").IsEnabled(selected);
    Find<Button>(L"ReplayPlay").IsEnabled(steps > 0);
    for(auto name : {L"ReplayFirst",L"ReplayPrev"}) Find<Button>(name).IsEnabled(selected && m_replayStep > 0);
    for(auto name : {L"ReplayNext",L"ReplayLast"}) Find<Button>(name).IsEnabled(selected && m_replayStep < steps);
    Find<Button>(L"ReplayPlay").Content(box_value(m_replayTimer&&m_replayTimer.IsRunning()?L"暂停":L"播放"));
    DrawBoard(Find<Canvas>(L"ReplayCanvas"),game,false);
}

fire_and_forget MainWindow::ImportReplay()
{
    auto lifetime=shared_from_this();
    try
    {
        Windows::Storage::Pickers::FileOpenPicker picker; picker.FileTypeFilter().Append(L".json");
        check_hresult(picker.as<IInitializeWithWindow>()->Initialize(Handle(m_window)));
        auto file=co_await picker.PickSingleFileAsync(); if(!file||m_closed) co_return;
        auto replay=m_storage.ImportReplay(fs::path(file.Path().c_str())); replay.id=Storage::NewId();
        m_storage.replays.insert(m_storage.replays.begin(),std::move(replay)); m_replayIndex=0; m_replayStep=0;
        m_dirty=true; bool saved=Save(); PopulateReplays(); RenderReplay();
        if(saved) Notify(L"回放已导入，并通过逐步规则校验。");
    }
    catch(...) { Notify(L"无法导入回放："+ErrorText(),true); }
}

fire_and_forget MainWindow::ExportReplay()
{
    auto lifetime=shared_from_this();
    if(m_replayIndex<0) co_return;
    auto replay=m_storage.replays[m_replayIndex];
    try
    {
        Windows::Storage::Pickers::FileSavePicker picker;
        picker.FileTypeChoices().Insert(L"CompIntel3 回放",single_threaded_vector<hstring>({L".json"}));
        picker.SuggestedFileName(L"compintel3-"+std::wstring(to_hstring(replay.id.substr(0,8))));
        check_hresult(picker.as<IInitializeWithWindow>()->Initialize(Handle(m_window)));
        auto file=co_await picker.PickSaveFileAsync(); if(!file||m_closed) co_return;
        m_storage.ExportReplay(replay,fs::path(file.Path().c_str())); Notify(L"回放已导出。");
    }
    catch(...) { Notify(L"无法导出回放："+ErrorText(),true); }
}

void MainWindow::OpenDataDirectory()
{
    auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(Handle(m_window),L"open",m_storage.root.c_str(),nullptr,nullptr,SW_SHOWNORMAL));
    if(result<=32) Notify(L"无法打开数据目录。",true);
}
}
