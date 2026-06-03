// Password Generator page: pick length + character classes, generate a random
// password, see a live entropy estimate, and copy the result. The generation
// itself lives in PasswordLogic (superwin_core, unit-tested); this file is only
// the WinUI wiring.
#include <Windows.h>

#include <random>
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/clipboard/ClipText.h"
#include "modules/password/PasswordLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

class PasswordPage : public IModulePage {
public:
    PasswordPage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }

private:
    void Build() {
        output_ = winrt::TextBox();
        output_.IsReadOnly(true);
        output_.FontFamily(winrt::Microsoft::UI::Xaml::Media::FontFamily(L"Consolas"));
        output_.FontSize(16);
        output_.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);

        auto copy = winrt::Button();
        copy.Content(winrt::box_value(winrt::hstring(L"Copy")));
        copy.Click([this, copy](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            WriteClipboardText(std::wstring(output_.Text()));
            ui::FlashCopied(copy);
        });
        auto regen = winrt::Button();
        regen.Content(winrt::box_value(winrt::hstring(L"Regenerate")));
        regen.Style(winrt::Application::Current().Resources()
                        .Lookup(winrt::box_value(winrt::hstring(L"AccentButtonStyle")))
                        .try_as<winrt::Style>());
        regen.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { Generate(); });

        auto outRow = ui::HStack(10);
        outRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        output_.Width(440);
        outRow.Children().Append(output_);
        outRow.Children().Append(copy);
        outRow.Children().Append(regen);

        // Length slider.
        length_ = winrt::Slider();
        length_.Minimum(4);
        length_.Maximum(64);
        length_.Value(16);
        length_.Width(260);
        length_.ValueChanged([this](winrt::IInspectable const&,
                                    winrt::Controls::Primitives::RangeBaseValueChangedEventArgs const&) {
            Generate();
        });
        lengthLabel_ = ui::Text(L"Length: 16", 14, true);
        lengthLabel_.Width(110);
        auto lenRow = ui::HStack(12);
        lenRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        lenRow.Children().Append(lengthLabel_);
        lenRow.Children().Append(length_);

        // Character-class checkboxes.
        lower_ = MakeCheck(L"Lowercase (a-z)", true);
        upper_ = MakeCheck(L"Uppercase (A-Z)", true);
        digits_ = MakeCheck(L"Digits (0-9)", true);
        symbols_ = MakeCheck(L"Symbols (!@#...)", false);
        ambiguous_ = MakeCheck(L"Avoid ambiguous characters (O0 l1I...)", false);
        auto classRow = ui::HStack(18);
        classRow.Children().Append(lower_);
        classRow.Children().Append(upper_);
        classRow.Children().Append(digits_);
        classRow.Children().Append(symbols_);

        strength_ = ui::Caption(L"");

        auto card = ui::VStack(14);
        card.Children().Append(outRow);
        card.Children().Append(strength_);
        card.Children().Append(lenRow);
        card.Children().Append(classRow);
        card.Children().Append(ambiguous_);

        root_ = ui::Page(L"Password Generator", ui::Card(card));
        Generate();
    }

    winrt::CheckBox MakeCheck(winrt::hstring label, bool checked) {
        winrt::CheckBox c;
        c.Content(winrt::box_value(label));
        c.IsChecked(checked);
        c.Checked([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { Generate(); });
        c.Unchecked([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { Generate(); });
        return c;
    }

    PasswordOptions CurrentOptions() {
        PasswordOptions o;
        o.length = static_cast<int>(length_.Value());
        o.lowercase = lower_.IsChecked().Value();
        o.uppercase = upper_.IsChecked().Value();
        o.digits = digits_.IsChecked().Value();
        o.symbols = symbols_.IsChecked().Value();
        o.avoidAmbiguous = ambiguous_.IsChecked().Value();
        return o;
    }

    void Generate() {
        const PasswordOptions o = CurrentOptions();
        lengthLabel_.Text(L"Length: " + winrt::to_hstring(o.length));

        if (BuildCharset(o).empty()) {
            output_.Text(L"");
            strength_.Text(L"Select at least one character set.");
            return;
        }
        std::random_device rd;
        const std::string pw = GeneratePassword(o, rd());
        output_.Text(winrt::hstring(Utf8ToWide(pw)));

        const int bits = EstimateStrengthBits(o);
        const wchar_t* rating = bits >= 100 ? L"Very strong"
                              : bits >= 70  ? L"Strong"
                              : bits >= 40  ? L"Reasonable"
                                            : L"Weak";
        strength_.Text(winrt::hstring(L"~" + std::to_wstring(bits) + L" bits of entropy  \x2022  " + rating));
    }

    winrt::Microsoft::UI::Xaml::Controls::TextBox output_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Slider length_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock lengthLabel_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock strength_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::CheckBox lower_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::CheckBox upper_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::CheckBox digits_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::CheckBox symbols_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::CheckBox ambiguous_{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakePasswordPage() {
    return std::make_unique<PasswordPage>();
}

}  // namespace superwin
