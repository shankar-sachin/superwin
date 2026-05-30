// SuperWin entry point.
//
// Stage 0b is a minimal Win32 tray host: a hidden message window that owns the
// tray icon, single-instance coordination and global hotkeys -- the shared
// backbone every module hangs off. Stage 0c (SUPERWIN_ENABLE_WINUI) opens the
// WinUI 3 dashboard from here. Keeping the host as a normal (never-shown)
// window -- rather than HWND_MESSAGE -- means SetForegroundWindow works for the
// tray context menu and RegisterHotKey / clipboard listeners attach cleanly.

#include <Windows.h>
#include <commctrl.h>
#include <objbase.h>  // CoInitializeEx / CoUninitialize

#include <memory>

#include "core/Autostart.h"
#include "core/Hotkeys.h"
#include "core/Settings.h"
#include "core/SingleInstance.h"
#include "core/TrayIcon.h"
#include "core/Updater.h"

using namespace superwin;

#if SUPERWIN_ENABLE_WINUI
// ===========================================================================
// Stage 0c: code-first WinUI 3 entry point.
//
// The Windows App SDK Foundation package's auto-initializers (enabled via the
// WindowsAppSdkBootstrapInitialize / UndockedRegFreeWinRT MSBuild properties)
// bootstrap the runtime before main(), so we can construct WinUI types in an
// unpackaged process. The App is built entirely in code -- no XAML markup --
// which keeps the CMake build free of the XAML compiler.
// ===========================================================================
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>  // full IVector<T>::Append definitions
#include <winrt/Windows.UI.Xaml.Interop.h>          // TypeName
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>          // IXamlMetadataProvider, IXamlType
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>    // XamlControlsXamlMetaDataProvider

#include <memory>
#include <string>

#include "app/Shell.h"

namespace winrt {
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Markup;
using namespace winrt::Microsoft::UI::Xaml::XamlTypeInfo;
}  // namespace winrt

namespace {

// A code-first (no-XAML) WinUI 3 Application must still answer the framework's
// requests for XAML type metadata. We implement IXamlMetadataProvider and
// delegate to WinUI's built-in provider, which knows every stock control type
// referenced by XamlControlsResources' theme dictionaries. Without this the
// framework faults with E_UNEXPECTED inside Microsoft.UI.Xaml.dll.
struct App : winrt::ApplicationT<App, winrt::IXamlMetadataProvider> {
    App() {
        UnhandledException([](winrt::Windows::Foundation::IInspectable const&,
                              winrt::Microsoft::UI::Xaml::UnhandledExceptionEventArgs const& e) {
            ::OutputDebugStringW((std::wstring(L"SuperWin XAML unhandled: ") + e.Message().c_str() + L"\n").c_str());
        });
    }

    void OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const&) {
        // Load the default WinUI control styles. This must happen here, not in
        // the App constructor -- doing it before the framework is fully up
        // faults with E_UNEXPECTED.
        Resources().MergedDictionaries().Append(winrt::XamlControlsResources());

        shell_ = std::make_unique<Shell>();
        auto window = shell_->Create();
        window.Activate();
    }

    // --- IXamlMetadataProvider (delegated to the WinUI controls provider) ---
    winrt::IXamlType GetXamlType(winrt::Windows::UI::Xaml::Interop::TypeName const& type) {
        return Provider().GetXamlType(type);
    }
    winrt::IXamlType GetXamlType(winrt::hstring const& fullName) {
        return Provider().GetXamlType(fullName);
    }
    winrt::com_array<winrt::XmlnsDefinition> GetXmlnsDefinitions() {
        return Provider().GetXmlnsDefinitions();
    }

private:
    winrt::XamlControlsXamlMetaDataProvider Provider() {
        if (!provider_) provider_ = winrt::XamlControlsXamlMetaDataProvider();
        return provider_;
    }

    std::unique_ptr<Shell> shell_;
    winrt::XamlControlsXamlMetaDataProvider provider_{nullptr};
};

}  // namespace

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // One instance only.
    SingleInstance single;
    if (single.AlreadyRunning()) {
        SingleInstance::NotifyExisting();
        return 0;
    }

    winrt::init_apartment(winrt::apartment_type::single_threaded);
    Updater::Initialize();  // background auto-update checks via WinSparkle
    winrt::Microsoft::UI::Xaml::Application::Start([](auto&&) { winrt::make<App>(); });
    Updater::Shutdown();
    return 0;
}

#else  // ------------------------- Win32 tray host (Stage 0b) -------------------------

namespace {

constexpr wchar_t kHostClass[] = L"SuperWin.HostWindow";

// Everything the window proc needs, stashed in GWLP_USERDATA.
struct AppContext {
    std::unique_ptr<TrayIcon>      tray;
    std::unique_ptr<HotkeyManager> hotkeys;
    HWND                           host = nullptr;
};

void OpenDashboard(AppContext& ctx) {
    // Stage 0c replaces this with the WinUI dashboard window. For now, confirm
    // the wiring works with a balloon.
    if (ctx.tray) {
        ctx.tray->ShowBalloon(L"SuperWin",
                              L"Dashboard coming online in the next milestone.");
    }
}

LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    auto* ctx = reinterpret_cast<AppContext*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == SingleInstance::ActivationMessage() && ctx) {
        OpenDashboard(*ctx);  // a second launch asked us to surface ourselves
        return 0;
    }

    switch (msg) {
        case TrayIcon::kCallbackMessage:
            if (ctx && ctx->tray && ctx->tray->HandleCallback(wParam, lParam)) return 0;
            break;
        case WM_COMMAND:
            if (ctx && ctx->tray && ctx->tray->HandleCommand(LOWORD(wParam))) return 0;
            break;
        case WM_HOTKEY:
            if (ctx && ctx->hotkeys && ctx->hotkeys->Dispatch(static_cast<int>(wParam))) return 0;
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return ::DefWindowProcW(hwnd, msg, wParam, lParam);
}

HWND CreateHostWindow(HINSTANCE inst) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HostWndProc;
    wc.hInstance = inst;
    wc.lpszClassName = kHostClass;
    ::RegisterClassExW(&wc);

    // Normal overlapped window, intentionally never shown.
    return ::CreateWindowExW(0, kHostClass, L"SuperWin", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                             nullptr, nullptr, inst, nullptr);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR /*cmdLine*/, int) {
    // One instance only; a second launch nudges the first and exits.
    SingleInstance single;
    if (single.AlreadyRunning()) {
        SingleInstance::NotifyExisting();
        return 0;
    }

    ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    ::InitCommonControlsEx(&icc);

    HWND host = CreateHostWindow(inst);
    if (!host) return 1;

    AppContext ctx;
    ctx.host = host;

    HICON icon = ::LoadIconW(nullptr, IDI_APPLICATION);  // placeholder until app .ico
    ctx.tray = std::make_unique<TrayIcon>(host, icon, L"SuperWin");
    ctx.tray->SetPrimaryAction([&ctx] { OpenDashboard(ctx); });
    ctx.tray->AddMenuItem(L"Open SuperWin", [&ctx] { OpenDashboard(ctx); });
    ctx.tray->AddSeparator();
    ctx.tray->AddMenuItem(L"Start with Windows",
        [] { Autostart::SetEnabled(!Autostart::IsEnabled()); });
    ctx.tray->AddMenuItem(L"Check for updates…", [] { Updater::CheckNow(); });
    ctx.tray->AddSeparator();
    ctx.tray->AddMenuItem(L"Quit", [host] { ::DestroyWindow(host); });

    // Global hotkeys. Win+Shift+V opens the clipboard picker (placeholder now).
    ctx.hotkeys = std::make_unique<HotkeyManager>(host);
    const std::string clipHotkey =
        Settings::Instance().GetString("clipboard.hotkey", "Win+Shift+V");
    ctx.hotkeys->Register("clipboard", ParseHotkey(clipHotkey),
        [&ctx] {
            if (ctx.tray) ctx.tray->ShowBalloon(L"Clipboard++", L"Picker arrives in a later milestone.");
        });

    ::SetWindowLongPtrW(host, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&ctx));

    Updater::Initialize();  // background auto-update checks via WinSparkle

    MSG msg;
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }

    Updater::Shutdown();
    ctx.tray.reset();
    ::CoUninitialize();
    return static_cast<int>(msg.wParam);
}

#endif  // SUPERWIN_ENABLE_WINUI
