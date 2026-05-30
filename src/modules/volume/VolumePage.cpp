#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "app/Ui.h"
#include "modules/volume/AudioSessions.h"
#include "modules/volume/VolumeMath.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Xaml::Controls::Primitives;
}  // namespace winrt

namespace superwin {
namespace {

// One row: name, slider (0-100), live "%  (dB)" label, and a mute checkbox.
winrt::Border BuildRow(winrt::hstring name, float scalar, bool muted,
                       std::function<void(float)> onVolume,
                       std::function<void(bool)> onMute) {
    auto readout = ui::Text(L"", 12.5);
    auto updateReadout = [readout](double pct) {
        const float s = static_cast<float>(pct / 100.0);
        readout.Text(winrt::hstring(std::to_wstring(static_cast<int>(pct + 0.5)) + L"%   " +
                                    std::wstring(winrt::to_hstring(FormatDb(DbFromScalar(s))))));
    };

    winrt::Slider slider;
    slider.Minimum(0);
    slider.Maximum(100);
    slider.Value(PercentFromScalar(scalar));
    slider.MinWidth(260);
    slider.ValueChanged([onVolume, updateReadout](winrt::IInspectable const&,
                          winrt::RangeBaseValueChangedEventArgs const& e) {
        updateReadout(e.NewValue());
        if (onVolume) onVolume(static_cast<float>(e.NewValue() / 100.0));
    });
    updateReadout(PercentFromScalar(scalar));

    winrt::CheckBox mute;
    mute.Content(winrt::box_value(winrt::hstring(L"Mute")));
    mute.IsChecked(muted);
    mute.Checked([onMute](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
        if (onMute) onMute(true);
    });
    mute.Unchecked([onMute](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
        if (onMute) onMute(false);
    });

    auto nameText = ui::Text(name, 14, true);
    nameText.Width(160);
    nameText.TextTrimming(winrt::TextTrimming::CharacterEllipsis);

    auto row = ui::HStack(16);
    row.VerticalAlignment(winrt::VerticalAlignment::Center);
    row.Children().Append(nameText);
    row.Children().Append(slider);
    auto right = ui::VStack(0);
    right.Width(120);
    right.Children().Append(readout);
    row.Children().Append(right);
    row.Children().Append(mute);
    return ui::Card(row, 14);
}

class VolumePage : public IModulePage {
public:
    VolumePage() {
        body_ = ui::VStack(16);
        root_ = ui::Page(L"Volume Customizer", body_);
    }

    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }
    void OnShown() override { Rebuild(); }

private:
    void Rebuild() {
        body_.Children().Clear();
        if (!audio_.Valid()) {
            body_.Children().Append(ui::Card(ui::Text(L"No audio endpoint available.")));
            return;
        }

        // Master.
        auto masterSection = ui::VStack(8);
        masterSection.Children().Append(ui::Text(L"Master output", 15, true));
        masterSection.Children().Append(BuildRow(
            L"Speakers", audio_.MasterVolume(), audio_.MasterMuted(),
            [this](float v) { audio_.SetMasterVolume(v); },
            [this](bool m) { audio_.SetMasterMuted(m); }));
        body_.Children().Append(masterSection);

        // Per-app.
        auto appsSection = ui::VStack(8);
        auto header = ui::HStack(12);
        header.Children().Append(ui::Text(L"Apps", 15, true));
        winrt::Button refresh;
        refresh.Content(winrt::box_value(winrt::hstring(L"Refresh")));
        refresh.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { Rebuild(); });
        header.Children().Append(refresh);
        appsSection.Children().Append(header);

        auto sessions = audio_.Refresh();
        if (sessions.empty()) {
            appsSection.Children().Append(ui::Caption(L"No apps are currently playing audio."));
        }
        for (size_t i = 0; i < sessions.size(); ++i) {
            const auto& s = sessions[i];
            appsSection.Children().Append(BuildRow(
                winrt::hstring(s.name), s.volume, s.muted,
                [this, i](float v) { audio_.SetSessionVolume(i, v); },
                [this, i](bool m) { audio_.SetSessionMuted(i, m); }));
        }
        body_.Children().Append(appsSection);
    }

    AudioController audio_;
    winrt::Microsoft::UI::Xaml::Controls::StackPanel body_{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeVolumePage() {
    return std::make_unique<VolumePage>();
}

}  // namespace superwin
