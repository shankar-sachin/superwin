// PowerToys-style application shell: a Mica window hosting a left NavigationView
// with a Home page and one page per module. Pages are created lazily and cached.
#pragma once

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <functional>
#include <memory>
#include <unordered_map>

namespace superwin {

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
// Home gets a navigate callback so its tiles can jump to a section by tag.
std::unique_ptr<IModulePage> MakeHomePage(std::function<void(winrt::hstring)> navigate);

class Shell {
public:
    // Build the window + navigation. Does not activate it.
    winrt::Microsoft::UI::Xaml::Window Create();

    // Select a section by tag ("home","volume","clipboard","diagnostics","notepad").
    void Navigate(winrt::hstring tag);

    winrt::Microsoft::UI::Xaml::Window Window() const { return window_; }

private:
    IModulePage* EnsurePage(winrt::hstring const& tag);
    void OnSelectionChanged(winrt::hstring tag);

    winrt::Microsoft::UI::Xaml::Window window_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::NavigationView nav_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Frame contentHost_{nullptr};

    std::unordered_map<winrt::hstring, std::unique_ptr<IModulePage>> pages_;
    IModulePage* current_ = nullptr;
};

}  // namespace superwin
