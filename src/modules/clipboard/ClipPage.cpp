#include <Windows.h>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/clipboard/ClipStore.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

std::wstring ReadClipboardText() {
    if (!::OpenClipboard(nullptr)) return {};
    std::wstring text;
    if (HANDLE h = ::GetClipboardData(CF_UNICODETEXT)) {
        if (auto* p = static_cast<const wchar_t*>(::GlobalLock(h))) {
            text = p;
            ::GlobalUnlock(h);
        }
    }
    ::CloseClipboard();
    return text;
}

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

winrt::hstring Preview(const std::string& utf8) {
    std::wstring w = Utf8ToWide(utf8);
    for (auto& c : w) if (c == L'\r' || c == L'\n' || c == L'\t') c = L' ';
    if (w.size() > 100) w = w.substr(0, 100) + L"…";
    return winrt::hstring(w);
}

class ClipPage : public IModulePage {
public:
    ClipPage() {
        store_.Load();

        search_ = winrt::TextBox();
        search_.PlaceholderText(L"Search clipboard history");
        search_.TextChanged([this](winrt::IInspectable const&, winrt::IInspectable const&) { RefreshList(); });

        list_ = ui::VStack(8);

        auto body = ui::VStack(14);
        body.Children().Append(search_);
        body.Children().Append(list_);
        root_ = ui::Page(L"Clipboard++", body);

        // Capture every clipboard change (no host window needed).
        token_ = winrt::Windows::ApplicationModel::DataTransfer::Clipboard::ContentChanged(
            [this](winrt::IInspectable const&, winrt::IInspectable const&) {
                std::wstring text = ReadClipboardText();
                if (!text.empty()) {
                    store_.AddText(WideToUtf8(text));
                    RefreshList();
                }
            });
    }

    ~ClipPage() override {
        winrt::Windows::ApplicationModel::DataTransfer::Clipboard::ContentChanged(token_);
    }

    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }
    void OnShown() override { RefreshList(); }

private:
    void RefreshList() {
        list_.Children().Clear();
        const std::string filter = WideToUtf8(std::wstring(search_.Text()));
        auto items = store_.Items(filter);
        if (items.empty()) {
            list_.Children().Append(ui::Caption(L"Nothing here yet — copy some text."));
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
        pin.Click([this, id, pinned = item.pinned](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            store_.Pin(id, !pinned);
            RefreshList();
        });
        auto del = MakeIconButton(0xE74D, L"Delete");
        del.Click([this, id](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            store_.Remove(id);
            RefreshList();
        });

        auto row = ui::HStack(10);
        row.VerticalAlignment(winrt::VerticalAlignment::Center);
        row.Children().Append(preview);
        auto spacer = winrt::Microsoft::UI::Xaml::Controls::StackPanel();
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

    ClipStore store_{50};
    winrt::Microsoft::UI::Xaml::Controls::TextBox search_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::StackPanel list_{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
    winrt::event_token token_{};
};

}  // namespace

std::unique_ptr<IModulePage> MakeClipboardPage() {
    return std::make_unique<ClipPage>();
}

}  // namespace superwin
