// JSON Formatter page: paste JSON, then pretty-print or minify it. Parsing and
// formatting live in JsonLogic (superwin_core, unit-tested); this is the WinUI
// wiring.
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/clipboard/ClipText.h"
#include "modules/jsonfmt/JsonLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

class JsonPage : public IModulePage {
public:
    JsonPage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }

private:
    void Build() {
        input_ = winrt::TextBox();
        input_.PlaceholderText(L"Paste JSON here");
        input_.AcceptsReturn(true);
        input_.TextWrapping(winrt::TextWrapping::Wrap);
        input_.Height(300);
        input_.FontFamily(winrt::Microsoft::UI::Xaml::Media::FontFamily(L"Consolas"));

        status_ = ui::Caption(L"");

        auto row = ui::HStack(8);
        row.Children().Append(MakeOp(L"Format", 2));
        row.Children().Append(MakeOp(L"Minify", -1));

        auto copy = winrt::Button();
        copy.Content(winrt::box_value(winrt::hstring(L"Copy")));
        copy.Click([this, copy](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            WriteClipboardText(std::wstring(input_.Text()));
            ui::FlashCopied(copy);
        });
        row.Children().Append(copy);

        auto card = ui::VStack(12);
        card.Children().Append(input_);
        card.Children().Append(status_);
        card.Children().Append(row);

        root_ = ui::Page(L"JSON Formatter", ui::Card(card));
    }

    winrt::Button MakeOp(winrt::hstring label, int indent) {
        winrt::Button b;
        b.Content(winrt::box_value(label));
        b.Click([this, indent](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            const std::string in = WideToUtf8(std::wstring(input_.Text()));
            const JsonResult r = FormatJson(in, indent);
            if (r.ok) {
                input_.Text(winrt::hstring(Utf8ToWide(r.text)));
                status_.Text(L"Valid JSON");
            } else {
                status_.Text(winrt::hstring(L"\x26A0  " + Utf8ToWide(r.error)));
            }
        });
        return b;
    }

    winrt::Microsoft::UI::Xaml::Controls::TextBox input_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock status_{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeJsonPage() {
    return std::make_unique<JsonPage>();
}

}  // namespace superwin
