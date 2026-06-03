// GUID Generator page: generate one or many random version-4 GUIDs with the
// usual formatting options. Generation/formatting live in GuidLogic
// (superwin_core, unit-tested); this is the WinUI wiring.
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/clipboard/ClipText.h"
#include "modules/guid/GuidLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

class GuidPage : public IModulePage {
public:
    GuidPage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }

private:
    void Build() {
        output_ = winrt::TextBox();
        output_.IsReadOnly(true);
        output_.AcceptsReturn(true);
        output_.TextWrapping(winrt::TextWrapping::NoWrap);
        output_.Height(260);
        output_.FontFamily(winrt::Microsoft::UI::Xaml::Media::FontFamily(L"Consolas"));

        uppercase_ = MakeToggle(L"Uppercase", false);
        hyphens_ = MakeToggle(L"Hyphens", true);
        braces_ = MakeToggle(L"Braces { }", false);

        count_ = winrt::NumberBox();
        count_.Minimum(1);
        count_.Maximum(1000);
        count_.Value(1);
        count_.SmallChange(1);
        count_.SpinButtonPlacementMode(winrt::NumberBoxSpinButtonPlacementMode::Inline);
        count_.Width(140);

        auto opts = ui::HStack(16);
        opts.VerticalAlignment(winrt::VerticalAlignment::Center);
        opts.Children().Append(uppercase_);
        opts.Children().Append(hyphens_);
        opts.Children().Append(braces_);

        auto countRow = ui::HStack(10);
        countRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        countRow.Children().Append(ui::Text(L"How many", 13));
        countRow.Children().Append(count_);

        auto gen = winrt::Button();
        gen.Content(winrt::box_value(winrt::hstring(L"Generate")));
        gen.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { Generate(); });

        auto copy = winrt::Button();
        copy.Content(winrt::box_value(winrt::hstring(L"Copy")));
        copy.Click([this, copy](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            WriteClipboardText(std::wstring(output_.Text()));
            ui::FlashCopied(copy);
        });

        auto buttons = ui::HStack(8);
        buttons.Children().Append(gen);
        buttons.Children().Append(copy);

        auto card = ui::VStack(14);
        card.Children().Append(opts);
        card.Children().Append(countRow);
        card.Children().Append(buttons);
        card.Children().Append(output_);

        root_ = ui::Page(L"GUID Generator", ui::Card(card));
        Generate();  // show one immediately
    }

    winrt::ToggleSwitch MakeToggle(winrt::hstring header, bool on) {
        winrt::ToggleSwitch t;
        t.Header(winrt::box_value(header));
        t.IsOn(on);
        return t;
    }

    void Generate() {
        GuidOptions opt;
        opt.uppercase = uppercase_.IsOn();
        opt.hyphens = hyphens_.IsOn();
        opt.braces = braces_.IsOn();

        int n = static_cast<int>(count_.Value());
        if (n < 1) n = 1;

        std::wstring out;
        for (int i = 0; i < n; ++i) {
            if (i) out += L"\r\n";
            out += Utf8ToWide(NewGuid(opt));
        }
        output_.Text(winrt::hstring(out));
    }

    winrt::Microsoft::UI::Xaml::Controls::TextBox output_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch uppercase_{nullptr}, hyphens_{nullptr}, braces_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::NumberBox count_{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeGuidPage() {
    return std::make_unique<GuidPage>();
}

}  // namespace superwin
