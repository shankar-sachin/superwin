// Sleep: keep Windows awake (optionally for a fixed duration) AND schedule power
// actions -- Restart / Shut Down / Hibernate / Sleep -- after a cancelable
// countdown, plus a shortcut to Windows Update. Keep-awake is backed by
// SetThreadExecutionState (must be re-asserted from the UI thread); the power
// actions use ExitWindowsEx (with the shutdown privilege) and SetSuspendState.
#include <Windows.h>
#include <powrprof.h>
#include <shellapi.h>

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

// Grant SeShutdownPrivilege to the current process so ExitWindowsEx succeeds.
void EnableShutdownPrivilege() {
    HANDLE tok = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok)) return;
    LUID luid{};
    if (::LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &luid)) {
        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        ::AdjustTokenPrivileges(tok, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    }
    ::CloseHandle(tok);
}

enum class PowerAction { Restart, ShutDown, Hibernate, Sleep };

void ExecutePowerAction(PowerAction a) {
    switch (a) {
        case PowerAction::Restart:
            EnableShutdownPrivilege();
            ::ExitWindowsEx(EWX_REBOOT | EWX_FORCEIFHUNG,
                            SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED);
            break;
        case PowerAction::ShutDown:
            EnableShutdownPrivilege();
            ::ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCEIFHUNG,
                            SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER | SHTDN_REASON_FLAG_PLANNED);
            break;
        case PowerAction::Hibernate:
            ::SetSuspendState(TRUE, FALSE, FALSE);
            break;
        case PowerAction::Sleep:
            ::SetSuspendState(FALSE, FALSE, FALSE);
            break;
    }
}

class KeepAwakePage : public IModulePage {
public:
    KeepAwakePage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }
    void OnHidden() override { /* keep the awake state + any scheduled action running */ }

private:
    void Build() {
        auto body = ui::VStack(16);

        // ---- Keep awake card ----
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
        for (auto* s : {L"Until I turn it off", L"15 minutes", L"30 minutes", L"1 hour", L"2 hours"})
            duration_.Items().Append(winrt::box_value(winrt::hstring(s)));
        duration_.SelectedIndex(0);

        status_ = ui::Caption(L"Your PC will sleep normally.");

        timer_ = winrt::DispatcherTimer();
        timer_.Interval(std::chrono::seconds(1));
        timer_.Tick([this](winrt::IInspectable const&, winrt::IInspectable const&) { Countdown(); });

        auto awake = ui::VStack(12);
        awake.Children().Append(ui::Text(L"Keep awake", 16, true));
        awake.Children().Append(toggle_);
        awake.Children().Append(screen_);
        auto durRow = ui::HStack(10);
        durRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        durRow.Children().Append(ui::Text(L"Duration", 14));
        durRow.Children().Append(duration_);
        awake.Children().Append(durRow);
        awake.Children().Append(status_);
        body.Children().Append(ui::Card(awake));

        // ---- Power actions card ----
        actionCombo_ = winrt::ComboBox();
        for (auto* s : {L"Restart", L"Shut down", L"Hibernate", L"Sleep"})
            actionCombo_.Items().Append(winrt::box_value(winrt::hstring(s)));
        actionCombo_.SelectedIndex(0);

        whenCombo_ = winrt::ComboBox();
        for (auto* s : {L"Now", L"In 1 minute", L"In 5 minutes", L"In 10 minutes", L"In 12 minutes",
                        L"In 30 minutes", L"In 1 hour"})
            whenCombo_.Items().Append(winrt::box_value(winrt::hstring(s)));
        whenCombo_.SelectedIndex(2);  // default: 5 minutes

        scheduleBtn_ = winrt::Button();
        scheduleBtn_.Content(winrt::box_value(winrt::hstring(L"Schedule")));
        if (auto st = winrt::Application::Current().Resources()
                          .TryLookup(winrt::box_value(winrt::hstring(L"AccentButtonStyle"))).try_as<winrt::Style>())
            scheduleBtn_.Style(st);
        scheduleBtn_.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { Schedule(); });

        cancelBtn_ = winrt::Button();
        cancelBtn_.Content(winrt::box_value(winrt::hstring(L"Cancel")));
        cancelBtn_.IsEnabled(false);
        cancelBtn_.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { CancelAction(); });

        powerStatus_ = ui::Caption(L"Nothing scheduled.");

        powerTimer_ = winrt::DispatcherTimer();
        powerTimer_.Interval(std::chrono::seconds(1));
        powerTimer_.Tick([this](winrt::IInspectable const&, winrt::IInspectable const&) { PowerCountdown(); });

        auto updatesBtn = winrt::Button();
        updatesBtn.Content(winrt::box_value(winrt::hstring(L"Check for Windows Updates")));
        updatesBtn.Click([](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            ::ShellExecuteW(nullptr, L"open", L"ms-settings:windowsupdate", nullptr, nullptr, SW_SHOWNORMAL);
        });

        auto power = ui::VStack(12);
        power.Children().Append(ui::Text(L"Power actions", 16, true));
        auto row = ui::HStack(10);
        row.VerticalAlignment(winrt::VerticalAlignment::Center);
        row.Children().Append(actionCombo_);
        row.Children().Append(whenCombo_);
        row.Children().Append(scheduleBtn_);
        row.Children().Append(cancelBtn_);
        power.Children().Append(row);
        power.Children().Append(powerStatus_);
        power.Children().Append(updatesBtn);
        body.Children().Append(ui::Card(power));

        root_ = ui::Page(L"Sleep", body);
    }

    // ---- keep awake ----
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
        if (--remaining_ <= 0) { toggle_.IsOn(false); return; }
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

    // ---- power actions ----
    static int SecondsForWhen(int i) {
        switch (i) {
            case 1: return 60; case 2: return 5 * 60; case 3: return 10 * 60;
            case 4: return 12 * 60; case 5: return 30 * 60; case 6: return 60 * 60;
            default: return 5;  // "Now" still gives a 5s cancel window
        }
    }
    const wchar_t* ActionName() const {
        switch (actionCombo_.SelectedIndex()) {
            case 1: return L"Shut down"; case 2: return L"Hibernate"; case 3: return L"Sleep"; default: return L"Restart";
        }
    }
    void Schedule() {
        powerRemaining_ = SecondsForWhen(whenCombo_.SelectedIndex());
        cancelBtn_.IsEnabled(true);
        scheduleBtn_.IsEnabled(false);
        UpdatePowerStatus();
        powerTimer_.Start();
    }
    void CancelAction() {
        powerTimer_.Stop();
        powerRemaining_ = 0;
        cancelBtn_.IsEnabled(false);
        scheduleBtn_.IsEnabled(true);
        powerStatus_.Text(L"Cancelled \x2014 nothing scheduled.");
    }
    void PowerCountdown() {
        if (--powerRemaining_ <= 0) {
            powerTimer_.Stop();
            cancelBtn_.IsEnabled(false);
            scheduleBtn_.IsEnabled(true);
            powerStatus_.Text(L"Running now\x2026");
            const PowerAction a = static_cast<PowerAction>(actionCombo_.SelectedIndex());
            ExecutePowerAction(a);
            return;
        }
        UpdatePowerStatus();
    }
    void UpdatePowerStatus() {
        const int m = powerRemaining_ / 60, s = powerRemaining_ % 60;
        wchar_t buf[96];
        swprintf_s(buf, L"%s in %d:%02d \x2014 press Cancel to stop.", ActionName(), m, s);
        powerStatus_.Text(buf);
    }

    winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch toggle_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::CheckBox screen_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ComboBox duration_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock status_{nullptr};
    winrt::DispatcherTimer timer_{nullptr};
    int remaining_ = 0;

    winrt::Microsoft::UI::Xaml::Controls::ComboBox actionCombo_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ComboBox whenCombo_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Button scheduleBtn_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Button cancelBtn_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock powerStatus_{nullptr};
    winrt::DispatcherTimer powerTimer_{nullptr};
    int powerRemaining_ = 0;

    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeKeepAwakePage() {
    return std::make_unique<KeepAwakePage>();
}

}  // namespace superwin
