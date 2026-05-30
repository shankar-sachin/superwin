#include <chrono>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Windowing.h>

#include "app/Ui.h"
#include "modules/diagnostics/HardwareProbe.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

winrt::Border SpecCard(winrt::hstring label, winrt::hstring value) {
    auto col = ui::VStack(2);
    col.Children().Append(ui::Caption(label));
    auto v = ui::Text(value, 15, true);
    v.TextTrimming(winrt::TextTrimming::CharacterEllipsis);
    col.Children().Append(v);
    auto card = ui::Card(col, 14);
    card.MinWidth(240);
    return card;
}

class DiagPage : public IModulePage {
public:
    DiagPage() {
        specs_ = ProbeSystem();
        Build();
        timer_ = winrt::DispatcherTimer();
        timer_.Interval(std::chrono::seconds(1));
        timer_.Tick([this](winrt::IInspectable const&, winrt::IInspectable const&) { Tick(); });
    }

    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }
    void OnShown() override { Tick(); timer_.Start(); }
    void OnHidden() override { timer_.Stop(); }

private:
    void Build() {
        auto body = ui::VStack(18);

        // Spec cards.
        auto specsWrap = ui::HStack(12);
        specsWrap.Children().Append(SpecCard(L"Processor",
            specs_.cpuName.empty() ? winrt::hstring(L"Unknown CPU")
                                   : winrt::hstring(specs_.cpuName + L"  (" +
                                       std::to_wstring(specs_.logicalCores) + L" threads)")));
        specsWrap.Children().Append(SpecCard(L"Graphics",
            specs_.gpuName.empty() ? winrt::hstring(L"Unknown GPU")
                                   : winrt::hstring(specs_.gpuName + L"  (" +
                                       FormatBytes(specs_.vramBytes) + L")")));
        auto specsWrap2 = ui::HStack(12);
        specsWrap2.Children().Append(SpecCard(L"Memory", winrt::hstring(FormatBytes(specs_.ramTotalBytes))));
        specsWrap2.Children().Append(SpecCard(L"Operating system",
            specs_.osName.empty() ? winrt::hstring(L"Windows") : winrt::hstring(specs_.osName)));

        body.Children().Append(specsWrap);
        body.Children().Append(specsWrap2);

        // Live section.
        auto live = ui::VStack(12);
        live.Children().Append(ui::Text(L"Live", 15, true));

        cpuLabel_ = ui::Text(L"CPU  0%", 13, true);
        cpuBar_ = winrt::ProgressBar(); cpuBar_.Maximum(100);
        live.Children().Append(cpuLabel_);
        live.Children().Append(cpuBar_);

        ramLabel_ = ui::Text(L"RAM  0%", 13, true);
        ramBar_ = winrt::ProgressBar(); ramBar_.Maximum(100);
        live.Children().Append(ramLabel_);
        live.Children().Append(ramBar_);

        winrt::ToggleSwitch mini;
        mini.Header(winrt::box_value(winrt::hstring(L"Mini-monitor (always on top)")));
        mini.Toggled([this](winrt::IInspectable const& s, winrt::RoutedEventArgs const&) {
            ToggleMini(s.as<winrt::ToggleSwitch>().IsOn());
        });
        live.Children().Append(mini);

        body.Children().Append(ui::Card(live));
        root_ = ui::Page(L"Diagnostics", body);
    }

    void Tick() {
        const double cpu = CpuUsagePercent();
        const double ram = RamUsagePercent();
        cpuBar_.Value(cpu);
        ramBar_.Value(ram);
        cpuLabel_.Text(winrt::hstring(L"CPU  " + std::to_wstring(static_cast<int>(cpu + 0.5)) + L"%"));
        ramLabel_.Text(winrt::hstring(L"RAM  " + std::to_wstring(static_cast<int>(ram + 0.5)) + L"%   (" +
                                      FormatBytes(RamUsedBytes()) + L" / " + FormatBytes(specs_.ramTotalBytes) + L")"));
        if (miniCpu_) {
            miniCpu_.Text(cpuLabel_.Text());
            miniRam_.Text(winrt::hstring(L"RAM  " + std::to_wstring(static_cast<int>(ram + 0.5)) + L"%"));
        }
    }

    void ToggleMini(bool on) {
        if (on && !miniWindow_) {
            miniWindow_ = winrt::Window();
            miniWindow_.Title(L"SuperWin Monitor");
            miniWindow_.SystemBackdrop(winrt::Microsoft::UI::Xaml::Media::MicaBackdrop());
            auto col = ui::VStack(4);
            col.Margin(winrt::Thickness{16, 12, 16, 12});
            miniCpu_ = ui::Text(L"CPU  0%", 16, true);
            miniRam_ = ui::Text(L"RAM  0%", 16, true);
            col.Children().Append(miniCpu_);
            col.Children().Append(miniRam_);
            miniWindow_.Content(col);
            if (auto p = miniWindow_.AppWindow().Presenter().try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>()) {
                p.IsAlwaysOnTop(true);
                p.SetBorderAndTitleBar(true, true);
                p.IsResizable(false);
                p.IsMaximizable(false);
                p.IsMinimizable(false);
            }
            miniWindow_.AppWindow().Resize({240, 130});
            miniWindow_.Activate();
        } else if (!on && miniWindow_) {
            miniWindow_.Close();
            miniWindow_ = nullptr;
            miniCpu_ = nullptr;
            miniRam_ = nullptr;
        }
    }

    SystemSpecs specs_;
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock cpuLabel_{nullptr}, ramLabel_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ProgressBar cpuBar_{nullptr}, ramBar_{nullptr};
    winrt::DispatcherTimer timer_{nullptr};
    winrt::Microsoft::UI::Xaml::Window miniWindow_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock miniCpu_{nullptr}, miniRam_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeDiagnosticsPage() {
    return std::make_unique<DiagPage>();
}

}  // namespace superwin
