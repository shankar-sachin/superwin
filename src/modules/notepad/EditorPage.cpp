#include <Windows.h>
#include <commdlg.h>

#include <fstream>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Microsoft.UI.Text.h>
#include <winrt/Microsoft.UI.Dispatching.h>

#include "app/Ui.h"
#include "core/Strings.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
using namespace winrt::Microsoft::UI::Text;
}  // namespace winrt

namespace superwin {
namespace {

const wchar_t* kFonts[] = {
    L"Segoe UI", L"Calibri", L"Cambria", L"Arial", L"Times New Roman",
    L"Georgia", L"Verdana", L"Tahoma", L"Trebuchet MS", L"Courier New",
    L"Consolas", L"Comic Sans MS"};
const double kSizes[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 24, 28, 36, 48, 72};

bool EndsWith(const std::wstring& s, const wchar_t* suffix) {
    std::wstring suf(suffix);
    return s.size() >= suf.size() && _wcsicmp(s.c_str() + (s.size() - suf.size()), suffix) == 0;
}

std::wstring FileDialog(bool save) {
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"Rich Text\0*.rtf\0Text Documents\0*.txt\0All Files\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = L"rtf";
    ofn.Flags = save ? OFN_OVERWRITEPROMPT : (OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST);
    const BOOL ok = save ? ::GetSaveFileNameW(&ofn) : ::GetOpenFileNameW(&ofn);
    return ok ? std::wstring(file) : std::wstring();
}

class EditorPage : public IModulePage {
public:
    EditorPage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }

private:
    winrt::ITextSelection Sel() { return editor_.Document().Selection(); }

    // --- formatting helpers (read-modify-write the selection format) ---
    void Bold()      { auto c = Sel().CharacterFormat(); c.Bold(Toggle(c.Bold())); Sel().CharacterFormat(c); }
    void Italic()    { auto c = Sel().CharacterFormat(); c.Italic(Toggle(c.Italic())); Sel().CharacterFormat(c); }
    void Underline() { auto c = Sel().CharacterFormat();
                       c.Underline(c.Underline() == winrt::UnderlineType::Single ? winrt::UnderlineType::None
                                                                                 : winrt::UnderlineType::Single);
                       Sel().CharacterFormat(c); }
    void Strike()    { auto c = Sel().CharacterFormat(); c.Strikethrough(Toggle(c.Strikethrough())); Sel().CharacterFormat(c); }

    static winrt::FormatEffect Toggle(winrt::FormatEffect e) {
        return e == winrt::FormatEffect::On ? winrt::FormatEffect::Off : winrt::FormatEffect::On;
    }

    void SetFont(winrt::hstring name) { auto c = Sel().CharacterFormat(); c.Name(name); Sel().CharacterFormat(c); }
    void SetSize(double pt) { auto c = Sel().CharacterFormat(); c.Size(static_cast<float>(pt)); Sel().CharacterFormat(c); }
    void SetForeground(winrt::Windows::UI::Color color) {
        auto c = Sel().CharacterFormat(); c.ForegroundColor(color); Sel().CharacterFormat(c);
    }
    void SetHighlight(winrt::Windows::UI::Color color) {
        auto c = Sel().CharacterFormat(); c.BackgroundColor(color); Sel().CharacterFormat(c);
    }
    void Align(winrt::ParagraphAlignment a) { auto p = Sel().ParagraphFormat(); p.Alignment(a); Sel().ParagraphFormat(p); }
    void List(winrt::MarkerType m) {
        auto p = Sel().ParagraphFormat();
        p.ListType(p.ListType() == m ? winrt::MarkerType::None : m);
        Sel().ParagraphFormat(p);
    }

    winrt::Button IconBtn(wchar_t glyph, winrt::hstring tip, std::function<void()> onClick) {
        winrt::FontIcon icon; icon.Glyph(winrt::hstring(&glyph, 1)); icon.FontSize(15);
        winrt::Button b; b.Content(icon);
        winrt::Controls::ToolTipService::SetToolTip(b, winrt::box_value(tip));
        b.Click([onClick](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { onClick(); });
        return b;
    }

    winrt::Button ColorBtn(wchar_t glyph, winrt::hstring tip, bool highlight) {
        winrt::FontIcon icon; icon.Glyph(winrt::hstring(&glyph, 1)); icon.FontSize(15);
        winrt::Button b; b.Content(icon);
        winrt::Controls::ToolTipService::SetToolTip(b, winrt::box_value(tip));

        winrt::ColorPicker picker;
        picker.IsAlphaEnabled(false);
        picker.ColorChanged([this, highlight](winrt::ColorPicker const& p,
                              winrt::ColorChangedEventArgs const&) {
            if (highlight) SetHighlight(p.Color()); else SetForeground(p.Color());
        });
        winrt::Flyout flyout; flyout.Content(picker);
        b.Flyout(flyout);
        return b;
    }

    void Build() {
        editor_ = winrt::RichEditBox();
        editor_.PlaceholderText(L"Start writing…");
        editor_.MinHeight(440);
        editor_.TextChanged([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { UpdateCount(); });

        // Font family + size.
        winrt::ComboBox fontBox; fontBox.MinWidth(150);
        for (auto f : kFonts) fontBox.Items().Append(winrt::box_value(winrt::hstring(f)));
        fontBox.SelectedIndex(0);
        fontBox.SelectionChanged([this, fontBox](winrt::IInspectable const&, winrt::SelectionChangedEventArgs const&) {
            if (auto v = fontBox.SelectedItem()) SetFont(winrt::unbox_value<winrt::hstring>(v));
        });

        winrt::ComboBox sizeBox; sizeBox.MinWidth(72);
        for (auto s : kSizes) sizeBox.Items().Append(winrt::box_value(winrt::hstring(std::to_wstring(static_cast<int>(s)))));
        sizeBox.SelectedIndex(5);  // 14
        sizeBox.SelectionChanged([this, sizeBox](winrt::IInspectable const&, winrt::SelectionChangedEventArgs const&) {
            if (auto v = sizeBox.SelectedItem())
                SetSize(_wtof(winrt::unbox_value<winrt::hstring>(v).c_str()));
        });

        auto bar = ui::HStack(6);
        bar.Children().Append(fontBox);
        bar.Children().Append(sizeBox);
        bar.Children().Append(MakeSep());
        bar.Children().Append(IconBtn(0xE8DD, L"Bold (Ctrl+B)", [this] { Bold(); }));
        bar.Children().Append(IconBtn(0xE8DB, L"Italic (Ctrl+I)", [this] { Italic(); }));
        bar.Children().Append(IconBtn(0xE8DC, L"Underline (Ctrl+U)", [this] { Underline(); }));
        bar.Children().Append(IconBtn(0xEDE0, L"Strikethrough", [this] { Strike(); }));
        bar.Children().Append(ColorBtn(0xE790, L"Text color", false));
        bar.Children().Append(ColorBtn(0xE7E6, L"Highlight", true));
        bar.Children().Append(MakeSep());
        bar.Children().Append(IconBtn(0xE8E4, L"Align left", [this] { Align(winrt::ParagraphAlignment::Left); }));
        bar.Children().Append(IconBtn(0xE8E3, L"Align center", [this] { Align(winrt::ParagraphAlignment::Center); }));
        bar.Children().Append(IconBtn(0xE8E2, L"Align right", [this] { Align(winrt::ParagraphAlignment::Right); }));
        bar.Children().Append(IconBtn(0xE8FD, L"Bulleted list", [this] { List(winrt::MarkerType::Bullet); }));
        bar.Children().Append(IconBtn(0xE8FE, L"Numbered list", [this] { List(winrt::MarkerType::Arabic); }));
        bar.Children().Append(MakeSep());
        bar.Children().Append(IconBtn(0xE7A7, L"Undo", [this] { editor_.Document().Undo(); }));
        bar.Children().Append(IconBtn(0xE7A6, L"Redo", [this] { editor_.Document().Redo(); }));

        auto bar2 = ui::HStack(6);
        winrt::Button openBtn; openBtn.Content(winrt::box_value(winrt::hstring(L"Open")));
        openBtn.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { OpenAsync(); });
        winrt::Button saveBtn; saveBtn.Content(winrt::box_value(winrt::hstring(L"Save")));
        saveBtn.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { SaveAsync(); });
        bar2.Children().Append(openBtn);
        bar2.Children().Append(saveBtn);

        status_ = ui::Caption(L"0 words   \x2022   0 characters");

        auto body = ui::VStack(12);
        body.Children().Append(ui::Card(bar, 10));
        body.Children().Append(bar2);
        body.Children().Append(editor_);
        body.Children().Append(status_);
        root_ = ui::Page(L"Notepad Super", body);
    }

    winrt::Controls::AppBarSeparator MakeSep() { return winrt::Controls::AppBarSeparator(); }

    winrt::hstring PlainText() {
        winrt::hstring text;
        editor_.Document().GetText(winrt::TextGetOptions::None, text);
        return text;
    }

    void UpdateCount() {
        std::wstring t(PlainText());
        size_t words = 0; bool in = false;
        for (wchar_t c : t) {
            if (c == L' ' || c == L'\r' || c == L'\n' || c == L'\t') in = false;
            else if (!in) { in = true; ++words; }
        }
        status_.Text(winrt::hstring(std::to_wstring(words) + L" words   \x2022   " +
                                    std::to_wstring(t.size()) + L" characters"));
    }

    // RTF (or plain) round-trip preserving formatting. In-memory stream ops
    // complete synchronously (no UI-thread dependency), so we stay on the UI
    // thread the whole time and avoid coroutine/threading complexity.
    void SaveAsync() {
        std::wstring path = FileDialog(true);
        if (path.empty()) return;
        const bool rtf = !EndsWith(path, L".txt");

        winrt::InMemoryRandomAccessStream stream;
        editor_.Document().SaveToStream(rtf ? winrt::TextGetOptions::FormatRtf
                                            : winrt::TextGetOptions::None, stream);
        stream.Seek(0);
        winrt::DataReader reader(stream);
        const uint32_t size = static_cast<uint32_t>(stream.Size());
        reader.LoadAsync(size).get();
        std::vector<uint8_t> bytes(size);
        reader.ReadBytes(bytes);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (out) out.write(reinterpret_cast<const char*>(bytes.data()), size);
    }

    void OpenAsync() {
        std::wstring path = FileDialog(false);
        if (path.empty()) return;
        const bool rtf = EndsWith(path, L".rtf");

        std::ifstream in(path, std::ios::binary);
        if (!in) return;
        std::vector<char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        winrt::InMemoryRandomAccessStream stream;
        winrt::DataWriter writer(stream);
        writer.WriteBytes(winrt::array_view<uint8_t const>(
            reinterpret_cast<uint8_t const*>(data.data()),
            reinterpret_cast<uint8_t const*>(data.data() + data.size())));
        writer.StoreAsync().get();
        writer.DetachStream();
        stream.Seek(0);
        editor_.Document().LoadFromStream(rtf ? winrt::TextSetOptions::FormatRtf
                                              : winrt::TextSetOptions::None, stream);
        UpdateCount();
    }

    winrt::Microsoft::UI::Xaml::Controls::RichEditBox editor_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock status_{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeNotepadPage() {
    return std::make_unique<EditorPage>();
}

}  // namespace superwin
