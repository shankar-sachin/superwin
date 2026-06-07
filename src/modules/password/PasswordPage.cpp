// Password Generator page: pick length + character classes, generate a random
// password, see a live entropy estimate, and copy the result. The generation
// itself lives in PasswordLogic (superwin_core, unit-tested); this file is only
// the WinUI wiring.
#include <Windows.h>

#include <random>
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include "app/Ui.h"
#include "core/Settings.h"
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

        // Restore the last-used options so the page (and its strength readout)
        // looks the same after a restart instead of resetting to a placeholder.
        const PasswordOptions saved = LoadOptions();

        // Length slider.
        length_ = winrt::Slider();
        length_.Minimum(4);
        length_.Maximum(64);
        length_.Value(saved.length);
        length_.Width(260);
        length_.ValueChanged([this](winrt::IInspectable const&,
                                    winrt::Controls::Primitives::RangeBaseValueChangedEventArgs const&) {
            Generate();
        });
        lengthLabel_ = ui::Text(L"Length: " + winrt::to_hstring(saved.length), 14, true);
        lengthLabel_.Width(110);
        auto lenRow = ui::HStack(12);
        lenRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        lenRow.Children().Append(lengthLabel_);
        lenRow.Children().Append(length_);

        // Character-class checkboxes.
        lower_ = MakeCheck(L"Lowercase (a-z)", saved.lowercase);
        upper_ = MakeCheck(L"Uppercase (A-Z)", saved.uppercase);
        digits_ = MakeCheck(L"Digits (0-9)", saved.digits);
        symbols_ = MakeCheck(L"Symbols (!@#...)", saved.symbols);
        ambiguous_ = MakeCheck(L"Avoid ambiguous characters (O0 l1I...)", saved.avoidAmbiguous);
        auto classRow = ui::HStack(18);
        classRow.Children().Append(lower_);
        classRow.Children().Append(upper_);
        classRow.Children().Append(digits_);
        classRow.Children().Append(symbols_);

        // Colour-coded strength meter: a rounded track with a fill whose width and
        // colour reflect the entropy estimate. Built (and populated) before the
        // first Generate() so it shows real bits from the very first frame.
        strengthFill_ = winrt::Border();
        strengthFill_.Height(8);
        strengthFill_.CornerRadius(winrt::CornerRadius{4, 4, 4, 4});
        strengthFill_.HorizontalAlignment(winrt::HorizontalAlignment::Left);
        strengthFill_.Width(0);

        strengthTrack_ = winrt::Border();
        strengthTrack_.Width(kBarWidth);
        strengthTrack_.Height(8);
        strengthTrack_.CornerRadius(winrt::CornerRadius{4, 4, 4, 4});
        strengthTrack_.HorizontalAlignment(winrt::HorizontalAlignment::Left);
        if (auto trackBg = ui::ThemeBrush(L"ControlStrongFillColorDefaultBrush"))
            strengthTrack_.Background(trackBg);
        strengthTrack_.Child(strengthFill_);

        strength_ = ui::Caption(L"");
        auto meter = ui::VStack(6);
        meter.Children().Append(strengthTrack_);
        meter.Children().Append(strength_);

        auto card = ui::VStack(14);
        card.Children().Append(outRow);
        card.Children().Append(meter);
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
        SaveOptions(o);

        if (BuildCharset(o).empty()) {
            output_.Text(L"");
            UpdateStrength(0, L"Select at least one character set.");
            return;
        }
        std::random_device rd;
        const std::string pw = GeneratePassword(o, rd());
        output_.Text(winrt::hstring(Utf8ToWide(pw)));
        UpdateStrength(EstimateStrengthBits(o), nullptr);
    }

    // Paint the strength bar + caption. `bits` drives both the fill width (capped
    // at kBarBits) and the tier colour; passing a non-null `override` replaces the
    // caption text (used for the "select a character set" state, which is 0 bits).
    void UpdateStrength(int bits, const wchar_t* overrideText) {
        const double frac = bits <= 0 ? 0.0 : (bits >= kBarBits ? 1.0 : bits / double(kBarBits));
        strengthFill_.Width(kBarWidth * frac);

        // Apple/Fluent-style tiers: red -> amber -> green -> teal.
        winrt::Windows::UI::Color c =
            bits >= 100 ? winrt::Windows::UI::Color{255, 0x4f, 0x8e, 0xf7}    // Very strong
          : bits >= 70  ? winrt::Windows::UI::Color{255, 0x34, 0xc7, 0x59}    // Strong
          : bits >= 40  ? winrt::Windows::UI::Color{255, 0xff, 0x9f, 0x0a}    // Reasonable
                        : winrt::Windows::UI::Color{255, 0xff, 0x45, 0x3a};   // Weak
        strengthFill_.Background(winrt::Microsoft::UI::Xaml::Media::SolidColorBrush(c));

        if (overrideText) {
            strength_.Text(winrt::hstring(overrideText));
            return;
        }
        const wchar_t* rating = bits >= 100 ? L"Very strong"
                              : bits >= 70  ? L"Strong"
                              : bits >= 40  ? L"Reasonable"
                                            : L"Weak";
        strength_.Text(winrt::hstring(L"~" + std::to_wstring(bits) + L" bits of entropy  \x2022  " + rating));
    }

    static PasswordOptions LoadOptions() {
        auto& s = Settings::Instance();
        PasswordOptions o;
        o.length = s.GetInt("password.length", o.length);
        if (o.length < 4) o.length = 4;
        if (o.length > 64) o.length = 64;
        o.lowercase = s.GetBool("password.lower", o.lowercase);
        o.uppercase = s.GetBool("password.upper", o.uppercase);
        o.digits = s.GetBool("password.digits", o.digits);
        o.symbols = s.GetBool("password.symbols", o.symbols);
        o.avoidAmbiguous = s.GetBool("password.avoidAmbiguous", o.avoidAmbiguous);
        return o;
    }

    static void SaveOptions(const PasswordOptions& o) {
        auto& s = Settings::Instance();
        s.Set("password.length", o.length);
        s.Set("password.lower", o.lowercase);
        s.Set("password.upper", o.uppercase);
        s.Set("password.digits", o.digits);
        s.Set("password.symbols", o.symbols);
        s.Set("password.avoidAmbiguous", o.avoidAmbiguous);
    }

    static constexpr double kBarWidth = 440.0;  // matches the output field width
    static constexpr int kBarBits = 128;        // entropy at which the bar is full

    winrt::Microsoft::UI::Xaml::Controls::TextBox output_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Slider length_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock lengthLabel_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock strength_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Border strengthTrack_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Border strengthFill_{nullptr};
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
