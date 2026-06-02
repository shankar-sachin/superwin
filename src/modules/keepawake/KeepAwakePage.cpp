// Keep Awake: stop Windows from sleeping or blanking the screen while a toggle
// is on, optionally for a fixed duration. Backed by SetThreadExecutionState,
// which must be re-asserted from the same thread that set it (the UI thread).
#include <Windows.h>

#include <chrono>
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "app/Ui.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

class KeepAwakePage : public IModulePage {
public:
    KeepAwakePage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }
    void OnHidden() override { /* keep the awake state running in the background */ }

private:
    void Build() {
        toggle_ = winrt::ToggleSwitch();
        toggle_.OnContent(winrt::box_value(winrt::hstring(L"Awake")));
        toggle_.OffContent(winrt::box_value(winrt::hstring(L"Normal")));
        toggle_.Toggled([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { Apply(); });

        screen_ = winrt::CheckBox();
        screen_.Content(winrt::box_value(winrt::hstring(L"Also keep the screen on")));
        screen_.IsChecked(true);
        screen_.Checked([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { Apply(); });
        screen_.Unchecked([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { Apply(); });

        duration_ = winrt::ComboBox();
        duration_.Items().Append(winrt::box_value(winrt::hstring(L"Until I turn it off")));
        duration_.Items().Append(winrt::box_value(winrt::hstring(L"15 minutes")));
        duration_.Items().Append(winrt::box_value(winrt::hstring(L"30 minutes")));
        duration_.Items().Append(winrt::box_value(winrt::hstring(L"1 hour")));
        duration_.Items().Append(winrt::box_value(winrt::hstring(L"2 hours")));
        duration_.SelectedIndex(0);

        status_ = ui::Caption(L"Your PC will sleep normally.");

        timer_ = winrt::DispatcherTimer();
        timer_.Interval(std::chrono::seconds(1));
        timer_.Tick([this](winrt::IInspectable const&, winrt::IInspectable const&) { Countdown(); });

        auto card = ui::VStack(12);
        card.Children().Append(toggle_);
        card.Children().Append(screen_);
        auto durRow = ui::HStack(10);
        durRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        durRow.Children().Append(ui::Text(L"Duration", 14));
        durRow.Children().Append(duration_);
        card.Children().Append(durRow);
        card.Children().Append(status_);

        root_ = ui::Page(L"Keep Awake", ui::Card(card));
    }

    void Apply() {
        if (toggle_.IsOn()) {
            EXECUTION_STATE es = ES_CONTINUOUS | ES_SYSTEM_REQUIRED;
            if (screen_.IsChecked().Value()) es |= ES_DISPLAY_REQUIRED;
            ::SetThreadExecutionState(es);
            remaining_ = MinutesForIndex(duration_.SelectedIndex()) * 60;
            if (remaining_ > 0) timer_.Start(); else timer_.Stop();
            UpdateStatus();
        } else {
            ::SetThreadExecutionState(ES_CONTINUOUS);
            timer_.Stop();
            remaining_ = 0;
            status_.Text(L"Your PC will sleep normally.");
        }
    }

    void Countdown() {
        if (--remaining_ <= 0) {
            toggle_.IsOn(false);  // triggers Apply() -> releases the lock
            return;
        }
        UpdateStatus();
    }

    void UpdateStatus() {
        if (remaining_ > 0) {
            const int m = remaining_ / 60, s = remaining_ % 60;
            wchar_t buf[64];
            swprintf_s(buf, L"Staying awake \x2014 %d:%02d remaining.", m, s);
            status_.Text(buf);
        } else {
            status_.Text(L"Staying awake until you turn this off.");
        }
    }

    static int MinutesForIndex(int i) {
        switch (i) { case 1: return 15; case 2: return 30; case 3: return 60; case 4: return 120; default: return 0; }
    }

    winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch toggle_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::CheckBox screen_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ComboBox duration_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock status_{nullptr};
    winrt::DispatcherTimer timer_{nullptr};
    int remaining_ = 0;
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeKeepAwakePage() {
    return std::make_unique<KeepAwakePage>();
}

}  // namespace superwin
