// PowerToys-style application shell: a Mica window hosting a left NavigationView
// with a Home page and one page per module. Pages are created lazily and cached.
#pragma once

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <functional>
#include <memory>
#include <unordered_map>

namespace superwin {

class AppHost;

// Every page (Home + modules) implements this. Root() returns the element shown
// in the content frame; OnShown/OnHidden bracket visibility so pages can start
// and stop live work (timers, audio enumeration, ...).
struct IModulePage {
    virtual ~IModulePage() = default;
    virtual winrt::Microsoft::UI::Xaml::UIElement Root() = 0;
    virtual void OnShown() {}
    virtual void OnHidden() {}
};

// Module page factories (defined in each module's .cpp).
std::unique_ptr<IModulePage> MakeVolumePage();
std::unique_ptr<IModulePage> MakeClipboardPage();
std::unique_ptr<IModulePage> MakeDiagnosticsPage();
std::unique_ptr<IModulePage> MakeNotepadPage();
std::unique_ptr<IModulePage> MakeColorPickerPage();
std::unique_ptr<IModulePage> MakeKeepAwakePage();
std::unique_ptr<IModulePage> MakeHashPage();
std::unique_ptr<IModulePage> MakeNetInfoPage();
std::unique_ptr<IModulePage> MakeConvertPage();
std::unique_ptr<IModulePage> MakePasswordPage();
std::unique_ptr<IModulePage> MakeTextPage();
// Home gets a navigate callback so its tiles can jump to a section by tag.
std::unique_ptr<IModulePage> MakeHomePage(std::function<void(winrt::hstring)> navigate);
// Settings needs the shell (theme, navigation) and host (hotkey re-registration).
class Shell;
std::unique_ptr<IModulePage> MakeSettingsPage(Shell* shell, AppHost* host);

class Shell {
public:
    // Build the window + navigation. Does not activate it.
    winrt::Microsoft::UI::Xaml::Window Create();

    // Select a section by tag ("home","volume","clipboard","diagnostics","notepad").
    void Navigate(winrt::hstring tag);

    winrt::Microsoft::UI::Xaml::Window Window() const { return window_; }

    // Give the settings page access to the host for hotkey re-registration.
    void SetAppHost(AppHost* host) { host_ = host; }

    // Apply a theme to the whole window ("light" / "dark" / "system").
    void SetTheme(winrt::hstring mode);

private:
    IModulePage* EnsurePage(winrt::hstring const& tag);
    void OnSelectionChanged(winrt::hstring tag);
    winrt::Microsoft::UI::Xaml::Controls::Grid BuildTitleBar();

    winrt::Microsoft::UI::Xaml::Window window_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::NavigationView nav_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Grid root_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Frame contentHost_{nullptr};

    AppHost* host_ = nullptr;
    std::unordered_map<winrt::hstring, std::unique_ptr<IModulePage>> pages_;
    IModulePage* current_ = nullptr;
};

}  // namespace superwin
