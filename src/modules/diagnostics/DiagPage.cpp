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

// A "label : value" row for the detail cards.
winrt::Microsoft::UI::Xaml::UIElement InfoRow(winrt::hstring label, winrt::hstring value) {
    auto row = ui::HStack(12);
    auto l = ui::Caption(label);
    l.Width(160);
    auto v = ui::Text(value.empty() ? winrt::hstring(L"\x2014") : value, 13.5);
    row.Children().Append(l);
    row.Children().Append(v);
    return row;
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
    static winrt::hstring CpuLine(const SystemSpecs& s) {
        std::wstring out = s.cpuName.empty() ? L"Unknown CPU" : s.cpuName;
        out += L"  (" + std::to_wstring(s.physicalCores) + L" cores / " +
               std::to_wstring(s.logicalCores) + L" threads";
        if (s.cpuBaseMHz > 0) {
            wchar_t mhz[32];
            swprintf_s(mhz, L", %.2f GHz", s.cpuBaseMHz / 1000.0);
            out += mhz;
        }
        out += L")";
        return winrt::hstring(out);
    }

    void Build() {
        auto body = ui::VStack(18);

        // --- Headline spec cards ---
        auto specsWrap = ui::HStack(12);
        specsWrap.Children().Append(SpecCard(L"Processor", CpuLine(specs_)));
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

        // --- System details ---
        auto sys = ui::VStack(8);
        sys.Children().Append(ui::Text(L"System", 15, true));
        sys.Children().Append(InfoRow(L"OS build", winrt::hstring(specs_.osBuild)));
        sys.Children().Append(InfoRow(L"Architecture", winrt::hstring(specs_.osArch)));
        sys.Children().Append(InfoRow(L"Device name", winrt::hstring(specs_.computerName)));
        sys.Children().Append(InfoRow(L"Signed-in user", winrt::hstring(specs_.userName)));
        sys.Children().Append(InfoRow(L"Manufacturer", winrt::hstring(specs_.systemManufacturer)));
        sys.Children().Append(InfoRow(L"Model", winrt::hstring(specs_.systemProduct)));
        sys.Children().Append(InfoRow(L"BIOS", winrt::hstring(specs_.biosVersion)));
        sys.Children().Append(InfoRow(L"Motherboard", winrt::hstring(specs_.baseBoard)));
        sys.Children().Append(InfoRow(L"CPU vendor", winrt::hstring(specs_.cpuVendor)));
        sys.Children().Append(InfoRow(L"Displays", winrt::hstring(specs_.displayInfo)));
        body.Children().Append(ui::Card(sys));

        // --- Graphics adapters ---
        if (!specs_.gpus.empty()) {
            auto gfx = ui::VStack(8);
            gfx.Children().Append(ui::Text(L"Graphics adapters", 15, true));
            for (const auto& g : specs_.gpus) gfx.Children().Append(ui::Text(winrt::hstring(g), 13.5));
            body.Children().Append(ui::Card(gfx));
        }

        // --- Storage ---
        if (!specs_.disks.empty()) {
            auto disk = ui::VStack(8);
            disk.Children().Append(ui::Text(L"Storage", 15, true));
            for (const auto& d : specs_.disks) disk.Children().Append(ui::Text(winrt::hstring(d), 13.5));
            body.Children().Append(ui::Card(disk));
        }

        // --- Live section ---
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

        auto liveDetails = ui::VStack(6);
        commitLabel_ = ui::Text(L"Committed  \x2014", 13);
        cacheLabel_ = ui::Text(L"Cached  \x2014", 13);
        procLabel_ = ui::Text(L"Processes  \x2014", 13);
        threadLabel_ = ui::Text(L"Threads  \x2014", 13);
        handleLabel_ = ui::Text(L"Handles  \x2014", 13);
        uptimeLabel_ = ui::Text(L"Uptime  \x2014", 13);
        liveDetails.Children().Append(commitLabel_);
        liveDetails.Children().Append(cacheLabel_);
        liveDetails.Children().Append(procLabel_);
        liveDetails.Children().Append(threadLabel_);
        liveDetails.Children().Append(handleLabel_);
        liveDetails.Children().Append(uptimeLabel_);
        live.Children().Append(liveDetails);

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
        const LiveStats st = SampleLive();
        cpuBar_.Value(st.cpuPercent);
        ramBar_.Value(st.ramPercent);
        cpuLabel_.Text(winrt::hstring(L"CPU  " + std::to_wstring(static_cast<int>(st.cpuPercent + 0.5)) + L"%"));
        ramLabel_.Text(winrt::hstring(L"RAM  " + std::to_wstring(static_cast<int>(st.ramPercent + 0.5)) + L"%   (" +
                                      FormatBytes(st.ramUsedBytes) + L" / " + FormatBytes(st.ramTotalBytes) + L")"));

        commitLabel_.Text(winrt::hstring(L"Committed  " + FormatBytes(st.commitUsedBytes) + L" / " +
                                         FormatBytes(st.commitLimitBytes)));
        cacheLabel_.Text(winrt::hstring(L"Cached  " + FormatBytes(st.cachedBytes)));
        procLabel_.Text(winrt::hstring(L"Processes  " + std::to_wstring(st.processCount)));
        threadLabel_.Text(winrt::hstring(L"Threads  " + std::to_wstring(st.threadCount)));
        handleLabel_.Text(winrt::hstring(L"Handles  " + std::to_wstring(st.handleCount)));
        uptimeLabel_.Text(winrt::hstring(L"Uptime  " + FormatDuration(st.uptimeSeconds)));

        if (miniCpu_) {
            miniCpu_.Text(cpuLabel_.Text());
            miniRam_.Text(winrt::hstring(L"RAM  " + std::to_wstring(static_cast<int>(st.ramPercent + 0.5)) + L"%"));
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
    winrt::Microsoft::UI::Xaml::Controls::TextBlock commitLabel_{nullptr}, cacheLabel_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock procLabel_{nullptr}, threadLabel_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock handleLabel_{nullptr}, uptimeLabel_{nullptr};
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
