// Security & Privacy page: generate cryptographically-secure tokens, gauge a
// password's strength, and quickly wipe clipboard data. Crypto + scoring live in
// SecurityLogic (superwin_core, unit-tested); this file is the WinUI wiring.
#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/clipboard/ClipStore.h"
#include "modules/clipboard/ClipText.h"
#include "modules/security/SecurityLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

class SecurityPage : public IModulePage {
public:
    SecurityPage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }

private:
    void Build() {
        auto body = ui::VStack(16);

        // ---- Secure token generator ----
        {
            bytesBox_ = winrt::NumberBox();
            bytesBox_.Minimum(4);
            bytesBox_.Maximum(256);
            bytesBox_.Value(32);
            bytesBox_.SmallChange(4);
            bytesBox_.SpinButtonPlacementMode(winrt::NumberBoxSpinButtonPlacementMode::Inline);
            bytesBox_.Width(140);

            encoding_ = winrt::ComboBox();
            encoding_.Items().Append(winrt::box_value(winrt::hstring(L"Hex")));
            encoding_.Items().Append(winrt::box_value(winrt::hstring(L"Base64")));
            encoding_.SelectedIndex(0);

            auto gen = winrt::Button();
            gen.Content(winrt::box_value(winrt::hstring(L"Generate")));
            gen.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { GenerateToken(); });

            auto copy = winrt::Button();
            copy.Content(winrt::box_value(winrt::hstring(L"Copy")));
            copy.Click([this, copy](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
                WriteClipboardText(std::wstring(token_.Text()));
                ui::FlashCopied(copy);
            });

            token_ = winrt::TextBox();
            token_.IsReadOnly(true);
            token_.TextWrapping(winrt::TextWrapping::Wrap);
            token_.FontFamily(winrt::Microsoft::UI::Xaml::Media::FontFamily(L"Consolas"));

            auto opts = ui::HStack(10);
            opts.VerticalAlignment(winrt::VerticalAlignment::Center);
            opts.Children().Append(ui::Text(L"Bytes", 13));
            opts.Children().Append(bytesBox_);
            opts.Children().Append(encoding_);
            opts.Children().Append(gen);
            opts.Children().Append(copy);

            auto card = ui::VStack(10);
            card.Children().Append(ui::Text(L"Secure token", 15, true));
            card.Children().Append(ui::Caption(L"Cryptographically-secure random bytes (CNG)."));
            card.Children().Append(opts);
            card.Children().Append(token_);
            body.Children().Append(ui::Card(card));
        }

        // ---- Password strength meter ----
        {
            pwBox_ = winrt::PasswordBox();
            pwBox_.PlaceholderText(L"Type a password to test");
            pwBox_.PasswordChanged([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { ScorePassword(); });

            strengthBar_ = winrt::ProgressBar();
            strengthBar_.Maximum(100);
            strengthBar_.Value(0);
            strengthLabel_ = ui::Text(L"\x2014", 13, true);

            auto card = ui::VStack(10);
            card.Children().Append(ui::Text(L"Password strength", 15, true));
            card.Children().Append(ui::Caption(L"Estimated entropy from length and character variety. Nothing leaves this window."));
            card.Children().Append(pwBox_);
            card.Children().Append(strengthBar_);
            card.Children().Append(strengthLabel_);
            body.Children().Append(ui::Card(card));
        }

        // ---- Privacy actions ----
        {
            auto clearHist = winrt::Button();
            clearHist.Content(winrt::box_value(winrt::hstring(L"Clear clipboard history")));
            clearHist.Click([this, clearHist](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
                SharedClipStore().Clear(/*includePinned=*/true);
                ui::FlashCopied(clearHist);  // reuse the "done" flash
            });

            auto clearSys = winrt::Button();
            clearSys.Content(winrt::box_value(winrt::hstring(L"Empty system clipboard")));
            clearSys.Click([this, clearSys](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
                if (::OpenClipboard(nullptr)) { ::EmptyClipboard(); ::CloseClipboard(); }
                ui::FlashCopied(clearSys);
            });

            auto row = ui::HStack(8);
            row.Children().Append(clearHist);
            row.Children().Append(clearSys);

            auto card = ui::VStack(10);
            card.Children().Append(ui::Text(L"Privacy", 15, true));
            card.Children().Append(ui::Caption(L"Wipe sensitive data you've copied."));
            card.Children().Append(row);
            body.Children().Append(ui::Card(card));
        }

        root_ = ui::Page(L"Security & Privacy", body);
        GenerateToken();
    }

    void GenerateToken() {
        const size_t n = static_cast<size_t>((std::max)(1.0, bytesBox_.Value()));
        const TokenEncoding enc = encoding_.SelectedIndex() == 1 ? TokenEncoding::Base64 : TokenEncoding::Hex;
        token_.Text(winrt::hstring(Utf8ToWide(RandomToken(n, enc))));
    }

    void ScorePassword() {
        const PasswordStrength s = EstimatePasswordStrength(WideToUtf8(std::wstring(pwBox_.Password())));
        strengthBar_.Value((std::min)(100.0, s.bits / 128.0 * 100.0));
        wchar_t buf[96];
        swprintf_s(buf, L"%s  \x2022  %.0f bits", Utf8ToWide(s.label).c_str(), s.bits);
        strengthLabel_.Text(buf);
    }

    winrt::Microsoft::UI::Xaml::Controls::NumberBox bytesBox_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ComboBox encoding_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBox token_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::PasswordBox pwBox_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ProgressBar strengthBar_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock strengthLabel_{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeSecurityPage() {
    return std::make_unique<SecurityPage>();
}

}  // namespace superwin
