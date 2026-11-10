#include "ui/MainWindow.h"
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <Windows.h>

using namespace winrt;
using namespace Microsoft::UI::Xaml;

struct App : ApplicationT<App, Markup::IXamlMetadataProvider>
{
    XamlTypeInfo::XamlControlsXamlMetaDataProvider metadata;
    std::shared_ptr<compintel::ui::MainWindow> mainWindow;

    App()
    {
        UnhandledException([](auto const&, UnhandledExceptionEventArgs const& args) {
            args.Handled(true);
            MessageBoxW(nullptr, args.Message().c_str(), L"CompIntel3", MB_OK | MB_ICONERROR);
        });
    }
    Markup::IXamlType GetXamlType(Windows::UI::Xaml::Interop::TypeName const& type) { return metadata.GetXamlType(type); }
    Markup::IXamlType GetXamlType(hstring const& name) { return metadata.GetXamlType(name); }
    com_array<Markup::XmlnsDefinition> GetXmlnsDefinitions() { return metadata.GetXmlnsDefinitions(); }
    void OnLaunched(LaunchActivatedEventArgs const&)
    {
        try
        {
            Resources().MergedDictionaries().Append(Controls::XamlControlsResources());
            mainWindow = std::make_shared<compintel::ui::MainWindow>();
            mainWindow->Show();
        }
        catch (hresult_error const& error)
        {
            MessageBoxW(nullptr, error.message().c_str(), L"CompIntel3 启动失败", MB_OK | MB_ICONERROR);
            Exit();
        }
        catch (std::exception const& error)
        {
            MessageBoxW(nullptr, to_hstring(error.what()).c_str(), L"CompIntel3 启动失败", MB_OK | MB_ICONERROR);
            Exit();
        }
    }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    try
    {
        handle instance(CreateMutexW(nullptr, FALSE, L"Local\\CompIntel3.Desktop"));
        if (!instance) throw_last_error();
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            if (auto existing=FindWindowW(nullptr,L"CompIntel3 · 算法对弈工作台"))
            {
                ShowWindow(existing,SW_RESTORE);
                SetForegroundWindow(existing);
            }
            return 0;
        }
        init_apartment(apartment_type::single_threaded);
        Application::Start([](auto&&) { make<App>(); });
        return 0;
    }
    catch (hresult_error const& error)
    {
        MessageBoxW(nullptr, error.message().c_str(), L"CompIntel3 启动失败", MB_OK | MB_ICONERROR);
    }
    catch (std::exception const& error)
    {
        MessageBoxW(nullptr, to_hstring(error.what()).c_str(), L"CompIntel3 启动失败", MB_OK | MB_ICONERROR);
    }
    return 1;
}
