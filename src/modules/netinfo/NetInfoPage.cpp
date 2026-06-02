#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/netinfo/NetInfoLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

winrt::hstring Join(const std::vector<std::string>& v) {
    std::string out;
    for (size_t i = 0; i < v.size(); ++i) { if (i) out += ", "; out += v[i]; }
    return winrt::hstring(Utf8ToWide(out));
}

class NetInfoPage : public IModulePage {
public:
    NetInfoPage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }
    void OnShown() override { RefreshAdapters(); }

private:
    void Build() {
        // Ping tool.
        host_ = winrt::TextBox();
        host_.PlaceholderText(L"Host or IP (e.g. 8.8.8.8)");
        host_.Text(L"8.8.8.8");
        host_.Width(220);
        auto pingBtn = winrt::Button();
        pingBtn.Content(winrt::box_value(winrt::hstring(L"Ping")));
        pingBtn.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { DoPing(); });
        pingResult_ = ui::Caption(L"");
        auto pingRow = ui::HStack(10);
        pingRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        pingRow.Children().Append(host_);
        pingRow.Children().Append(pingBtn);
        pingRow.Children().Append(pingResult_);

        auto refresh = winrt::Button();
        refresh.Content(winrt::box_value(winrt::hstring(L"Refresh adapters")));
        refresh.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { RefreshAdapters(); });

        adapters_ = ui::VStack(12);

        auto body = ui::VStack(16);
        body.Children().Append(ui::Card(pingRow));
        body.Children().Append(refresh);
        body.Children().Append(adapters_);
        root_ = ui::Page(L"Network Info", body);
    }

    void DoPing() {
        pingResult_.Text(L"Pinging\x2026");
        auto r = Ping(WideToUtf8(std::wstring(host_.Text())), 1500);
        if (r.success) {
            pingResult_.Text(winrt::hstring(
                Utf8ToWide(r.resolvedIp) + L"  \x2022  " + std::to_wstring(r.roundTripMs) + L" ms"));
        } else {
            pingResult_.Text(winrt::hstring(Utf8ToWide(r.error.empty() ? "Failed" : r.error)));
        }
    }

    void RefreshAdapters() {
        adapters_.Children().Clear();
        auto list = EnumAdapters();
        if (list.empty()) {
            adapters_.Children().Append(ui::Caption(L"No network adapters found."));
            return;
        }
        for (const auto& a : list) {
            auto col = ui::VStack(4);
            auto header = ui::HStack(8);
            header.VerticalAlignment(winrt::VerticalAlignment::Center);
            header.Children().Append(ui::Text(winrt::hstring(Utf8ToWide(a.name)), 15, true));
            header.Children().Append(ui::Caption(a.up ? L"Up" : L"Down"));
            col.Children().Append(header);
            col.Children().Append(ui::Caption(winrt::hstring(Utf8ToWide(a.description))));
            if (!a.ipv4.empty()) col.Children().Append(ui::Text(L"IPv4: " + Join(a.ipv4), 13));
            if (!a.ipv6.empty()) col.Children().Append(ui::Text(L"IPv6: " + Join(a.ipv6), 13));
            if (!a.mac.empty())  col.Children().Append(ui::Caption(L"MAC: " + winrt::hstring(Utf8ToWide(a.mac))));
            adapters_.Children().Append(ui::Card(col));
        }
    }

    winrt::Microsoft::UI::Xaml::Controls::TextBox host_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock pingResult_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::StackPanel adapters_{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeNetInfoPage() {
    return std::make_unique<NetInfoPage>();
}

}  // namespace superwin
