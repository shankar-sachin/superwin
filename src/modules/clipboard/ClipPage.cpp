#include <Windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/clipboard/ClipStore.h"
#include "modules/clipboard/ClipText.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

winrt::hstring Preview(const std::string& utf8) {
    std::wstring w = Utf8ToWide(utf8);
    for (auto& c : w) if (c == L'\r' || c == L'\n' || c == L'\t') c = L' ';
    if (w.size() > 100) w = w.substr(0, 100) + L"\x2026";
    return winrt::hstring(w);
}

class ClipPage : public IModulePage {
public:
    ClipPage() {
        search_ = winrt::TextBox();
        search_.PlaceholderText(L"Search clipboard history");
        search_.TextChanged([this](winrt::IInspectable const&, winrt::IInspectable const&) { RefreshList(); });

        list_ = ui::VStack(8);

        auto body = ui::VStack(14);
        body.Children().Append(search_);
        body.Children().Append(list_);
        root_ = ui::Page(L"Clipboard", body);

        // Refresh whenever the shared store changes (the AppHost watcher captures
        // clips process-wide, even while this page is closed).
        token_ = SharedClipStore().Subscribe([this] { RefreshList(); });
    }

    ~ClipPage() override {
        SharedClipStore().Unsubscribe(token_);
    }

    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }
    void OnShown() override { RefreshList(); }

private:
    void RefreshList() {
        list_.Children().Clear();
        const std::string filter = WideToUtf8(std::wstring(search_.Text()));
        auto items = SharedClipStore().Items(filter);
        if (items.empty()) {
            list_.Children().Append(ui::Caption(L"Nothing here yet \x2014 copy some text, or press Win+Shift+V anywhere."));
            return;
        }
        for (const auto& item : items) list_.Children().Append(BuildCard(item));
    }

    winrt::Border BuildCard(const ClipItem& item) {
        const uint64_t id = item.id;
        const std::string text = item.text;

        auto preview = ui::Text(Preview(item.text), 13);
        preview.MaxLines(2);
        preview.TextTrimming(winrt::TextTrimming::CharacterEllipsis);
        preview.Width(420);

        auto copy = MakeIconButton(0xE8C8, L"Copy");
        copy.Click([text](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            WriteClipboardText(Utf8ToWide(text));
        });
        auto pin = MakeIconButton(item.pinned ? 0xE77A : 0xE718, item.pinned ? L"Unpin" : L"Pin");
        pin.Click([id, pinned = item.pinned](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            SharedClipStore().Pin(id, !pinned);
        });
        auto del = MakeIconButton(0xE74D, L"Delete");
        del.Click([id](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            SharedClipStore().Remove(id);
        });

        auto row = ui::HStack(10);
        row.VerticalAlignment(winrt::VerticalAlignment::Center);
        row.Children().Append(preview);
        row.Children().Append(copy);
        row.Children().Append(pin);
        row.Children().Append(del);
        return ui::Card(row, 12);
    }

    winrt::Button MakeIconButton(wchar_t glyph, winrt::hstring tooltip) {
        winrt::FontIcon icon;
        icon.Glyph(winrt::hstring(&glyph, 1));
        icon.FontSize(15);
        winrt::Button b;
        b.Content(icon);
        winrt::Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(b, winrt::box_value(tooltip));
        return b;
    }

    winrt::Microsoft::UI::Xaml::Controls::TextBox search_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::StackPanel list_{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
    ClipStore::ChangeToken token_ = 0;
};

}  // namespace

std::unique_ptr<IModulePage> MakeClipboardPage() {
    return std::make_unique<ClipPage>();
}

}  // namespace superwin
