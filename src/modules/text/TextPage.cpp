// Text Tools page: paste text, transform it (case, Base64), and see a live
// character / word / line count. Transforms live in TextLogic (superwin_core,
// unit-tested); this file is the WinUI wiring.
#include <Windows.h>

#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/clipboard/ClipText.h"
#include "modules/text/TextLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

class TextPage : public IModulePage {
public:
    TextPage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }

private:
    void Build() {
        input_ = winrt::TextBox();
        input_.PlaceholderText(L"Type or paste text here");
        input_.AcceptsReturn(true);
        input_.TextWrapping(winrt::TextWrapping::Wrap);
        input_.Height(150);
        input_.TextChanged([this](winrt::IInspectable const&, winrt::IInspectable const&) {
            UpdateStats();
        });

        stats_ = ui::Caption(L"");

        // Transform buttons.
        auto row1 = ui::HStack(8);
        row1.Children().Append(MakeOp(L"UPPERCASE", [](const std::string& s) { return ToUpperAscii(s); }));
        row1.Children().Append(MakeOp(L"lowercase", [](const std::string& s) { return ToLowerAscii(s); }));
        row1.Children().Append(MakeOp(L"Title Case", [](const std::string& s) { return ToTitleCase(s); }));
        auto row2 = ui::HStack(8);
        row2.Children().Append(MakeOp(L"Base64 encode", [](const std::string& s) { return Base64Encode(s); }));
        row2.Children().Append(MakeOp(L"Base64 decode", [](const std::string& s) {
            auto d = Base64Decode(s);
            return d ? *d : std::string("(not valid Base64)");
        }));

        auto copy = winrt::Button();
        copy.Content(winrt::box_value(winrt::hstring(L"Copy result")));
        copy.Click([this, copy](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            WriteClipboardText(std::wstring(input_.Text()));
            ui::FlashCopied(copy);
        });
        row2.Children().Append(copy);

        auto card = ui::VStack(12);
        card.Children().Append(input_);
        card.Children().Append(stats_);
        card.Children().Append(row1);
        card.Children().Append(row2);

        root_ = ui::Page(L"Text Tools", ui::Card(card));
        UpdateStats();
    }

    template <typename Fn>
    winrt::Button MakeOp(winrt::hstring label, Fn op) {
        winrt::Button b;
        b.Content(winrt::box_value(label));
        b.Click([this, op](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            const std::string in = WideToUtf8(std::wstring(input_.Text()));
            input_.Text(winrt::hstring(Utf8ToWide(op(in))));
            UpdateStats();
        });
        return b;
    }

    void UpdateStats() {
        const TextStats st = Analyze(WideToUtf8(std::wstring(input_.Text())));
        stats_.Text(winrt::hstring(
            std::to_wstring(st.characters) + L" characters  \x2022  " +
            std::to_wstring(st.words) + L" words  \x2022  " +
            std::to_wstring(st.lines) + L" lines"));
    }

    winrt::Microsoft::UI::Xaml::Controls::TextBox input_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock stats_{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeTextPage() {
    return std::make_unique<TextPage>();
}

}  // namespace superwin
