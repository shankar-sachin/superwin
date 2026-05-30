#include <Windows.h>

#include <chrono>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>

#include "app/Ui.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

void WriteClipboardText(const std::wstring& s) {
    if (!::OpenClipboard(nullptr)) return;
    ::EmptyClipboard();
    const size_t bytes = (s.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL h = ::GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        std::memcpy(::GlobalLock(h), s.c_str(), bytes);
        ::GlobalUnlock(h);
        ::SetClipboardData(CF_UNICODETEXT, h);
    }
    ::CloseClipboard();
}

std::wstring HexOf(winrt::Windows::UI::Color c) {
    wchar_t buf[16];
    swprintf_s(buf, L"#%02X%02X%02X", c.R, c.G, c.B);
    return buf;
}
std::wstring RgbOf(winrt::Windows::UI::Color c) {
    wchar_t buf[32];
    swprintf_s(buf, L"rgb(%d, %d, %d)", c.R, c.G, c.B);
    return buf;
}

class ColorPickerPage : public IModulePage {
public:
    ColorPickerPage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }
    void OnHidden() override { if (timer_) timer_.Stop(); }

private:
    void Build() {
        picker_ = winrt::ColorPicker();
        picker_.IsAlphaEnabled(false);
        picker_.IsHexInputVisible(true);
        picker_.ColorChanged([this](winrt::ColorPicker const&, winrt::ColorChangedEventArgs const&) {
            UpdateReadout();
        });

        hex_ = ui::Text(L"#FFFFFF", 18, true);
        rgb_ = ui::Caption(L"rgb(255, 255, 255)");

        auto copyHex = winrt::Button();
        copyHex.Content(winrt::box_value(winrt::hstring(L"Copy hex")));
        copyHex.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            WriteClipboardText(HexOf(picker_.Color()));
        });
        auto copyRgb = winrt::Button();
        copyRgb.Content(winrt::box_value(winrt::hstring(L"Copy rgb")));
        copyRgb.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            WriteClipboardText(RgbOf(picker_.Color()));
        });

        eyedropper_ = winrt::Button();
        eyedropper_.Content(winrt::box_value(winrt::hstring(L"Pick from screen (3s)")));
        eyedropper_.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { StartPick(); });

        auto readoutCol = ui::VStack(6);
        readoutCol.Children().Append(hex_);
        readoutCol.Children().Append(rgb_);
        auto btns = ui::HStack(8);
        btns.Children().Append(copyHex);
        btns.Children().Append(copyRgb);
        readoutCol.Children().Append(btns);
        readoutCol.Children().Append(eyedropper_);

        auto row = ui::HStack(24);
        row.Children().Append(picker_);
        row.Children().Append(ui::Card(readoutCol));

        timer_ = winrt::DispatcherTimer();
        timer_.Interval(std::chrono::milliseconds(1000));
        timer_.Tick([this](winrt::IInspectable const&, winrt::IInspectable const&) { Countdown(); });

        root_ = ui::Page(L"Color Picker", row);
    }

    void UpdateReadout() {
        auto c = picker_.Color();
        hex_.Text(winrt::hstring(HexOf(c)));
        rgb_.Text(winrt::hstring(RgbOf(c)));
    }

    void StartPick() {
        countdown_ = 3;
        eyedropper_.IsEnabled(false);
        eyedropper_.Content(winrt::box_value(winrt::hstring(L"Move cursor… 3")));
        timer_.Start();
    }

    void Countdown() {
        if (--countdown_ > 0) {
            eyedropper_.Content(winrt::box_value(
                winrt::hstring(L"Move cursor… " + std::to_wstring(countdown_))));
            return;
        }
        timer_.Stop();
        POINT pt{};
        ::GetCursorPos(&pt);
        HDC dc = ::GetDC(nullptr);
        COLORREF c = ::GetPixel(dc, pt.x, pt.y);
        ::ReleaseDC(nullptr, dc);
        winrt::Windows::UI::Color color{255, GetRValue(c), GetGValue(c), GetBValue(c)};
        picker_.Color(color);
        eyedropper_.IsEnabled(true);
        eyedropper_.Content(winrt::box_value(winrt::hstring(L"Pick from screen (3s)")));
    }

    winrt::Microsoft::UI::Xaml::Controls::ColorPicker picker_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock hex_{nullptr}, rgb_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Button eyedropper_{nullptr};
    winrt::DispatcherTimer timer_{nullptr};
    int countdown_ = 0;
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeColorPickerPage() {
    return std::make_unique<ColorPickerPage>();
}

}  // namespace superwin
