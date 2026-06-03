// Small WinUI 3 layout helpers shared by every page, giving SuperWin a
// consistent PowerToys-style look (rounded "cards" on a Mica surface, page
// titles, vertical/horizontal stacks). Header-only.
#pragma once

#include <chrono>
#include <functional>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>  // IMap/IVector method definitions
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>  // ButtonBase::Click etc.
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Text.h>  // FontWeights

#include "app/Shell.h"

namespace superwin::ui {

namespace mux = winrt::Microsoft::UI::Xaml;
namespace muxc = winrt::Microsoft::UI::Xaml::Controls;

inline mux::Media::Brush ThemeBrush(winrt::hstring key) {
    auto res = mux::Application::Current().Resources();
    if (res.HasKey(winrt::box_value(key))) {
        return res.Lookup(winrt::box_value(key)).try_as<mux::Media::Brush>();
    }
    return nullptr;
}

inline muxc::TextBlock Text(winrt::hstring s, double size = 14, bool semibold = false) {
    muxc::TextBlock t;
    t.Text(s);
    t.FontSize(size);
    if (semibold) t.FontWeight(winrt::Windows::UI::Text::FontWeights::SemiBold());
    t.TextWrapping(mux::TextWrapping::Wrap);
    return t;
}

inline muxc::TextBlock Title(winrt::hstring s) {
    auto t = Text(s, 28, true);
    return t;
}

inline muxc::TextBlock Caption(winrt::hstring s) {
    auto t = Text(s, 12.5);
    if (auto b = ThemeBrush(L"TextFillColorSecondaryBrush")) t.Foreground(b);
    return t;
}

inline muxc::StackPanel Stack(mux::Controls::Orientation o, double spacing) {
    muxc::StackPanel p;
    p.Orientation(o);
    p.Spacing(spacing);
    return p;
}

inline muxc::StackPanel VStack(double spacing = 8) {
    return Stack(muxc::Orientation::Vertical, spacing);
}
inline muxc::StackPanel HStack(double spacing = 8) {
    return Stack(muxc::Orientation::Horizontal, spacing);
}

// Rounded card surface with subtle border, matching WinUI's Fluent cards.
inline muxc::Border Card(mux::UIElement content, double padding = 16) {
    muxc::Border b;
    b.CornerRadius(mux::CornerRadius{8, 8, 8, 8});
    b.Padding(mux::Thickness{padding, padding, padding, padding});
    if (auto bg = ThemeBrush(L"CardBackgroundFillColorDefaultBrush")) b.Background(bg);
    if (auto st = ThemeBrush(L"CardStrokeColorDefaultBrush")) {
        b.BorderBrush(st);
        b.BorderThickness(mux::Thickness{1, 1, 1, 1});
    }
    b.Child(content);
    return b;
}

// Briefly flip a copy button to "Copied" after a successful copy, then restore
// its original content (label or icon). The real original is stashed in the
// button's Tag so rapid repeat clicks never leave it stuck on "Copied". The
// timer stops itself via the sender, so there is no capture cycle keeping it
// (or the page) alive.
inline void FlashCopied(muxc::Button button) {
    if (!button.Tag()) button.Tag(button.Content());
    button.Content(winrt::box_value(winrt::hstring(L"Copied")));
    mux::DispatcherTimer timer;
    timer.Interval(std::chrono::milliseconds(1200));
    timer.Tick([button](winrt::Windows::Foundation::IInspectable const& sender,
                        winrt::Windows::Foundation::IInspectable const&) {
        if (auto orig = button.Tag()) {
            button.Content(orig);
            button.Tag(nullptr);
        }
        sender.as<mux::DispatcherTimer>().Stop();
    });
    timer.Start();
}

// Scrollable page body with comfortable padding and a title at the top.
inline muxc::ScrollViewer Page(winrt::hstring title, mux::UIElement body) {
    auto col = VStack(16);
    col.Margin(mux::Thickness{36, 24, 36, 36});
    col.Children().Append(Title(title));
    col.Children().Append(body);

    muxc::ScrollViewer sv;
    sv.VerticalScrollBarVisibility(muxc::ScrollBarVisibility::Auto);
    sv.Content(col);
    return sv;
}

}  // namespace superwin::ui

namespace superwin {

// Trivial IModulePage backed by a prebuilt element, with optional show/hide hooks.
class SimplePage : public IModulePage {
public:
    explicit SimplePage(winrt::Microsoft::UI::Xaml::UIElement root,
                        std::function<void()> onShown = {},
                        std::function<void()> onHidden = {})
        : root_(root), onShown_(std::move(onShown)), onHidden_(std::move(onHidden)) {}

    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }
    void OnShown() override { if (onShown_) onShown_(); }
    void OnHidden() override { if (onHidden_) onHidden_(); }

private:
    winrt::Microsoft::UI::Xaml::UIElement root_;
    std::function<void()> onShown_;
    std::function<void()> onHidden_;
};

}  // namespace superwin
