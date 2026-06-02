#include <Windows.h>
#include <commdlg.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/clipboard/ClipText.h"
#include "modules/hash/HashLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

std::optional<std::filesystem::path> PickFile() {
    wchar_t file[MAX_PATH] = L"";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (::GetOpenFileNameW(&ofn)) return std::filesystem::path(file);
    return std::nullopt;
}

class HashPage : public IModulePage {
public:
    HashPage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }

private:
    struct Row {
        HashAlgo algo;
        winrt::Microsoft::UI::Xaml::Controls::TextBox value{nullptr};
    };

    void Build() {
        input_ = winrt::TextBox();
        input_.PlaceholderText(L"Type or paste text to hash");
        input_.AcceptsReturn(true);
        input_.TextWrapping(winrt::TextWrapping::Wrap);
        input_.Height(110);
        input_.TextChanged([this](winrt::IInspectable const&, winrt::IInspectable const&) { HashText(); });

        auto fileBtn = winrt::Button();
        fileBtn.Content(winrt::box_value(winrt::hstring(L"Hash a file\x2026")));
        fileBtn.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { HashPickedFile(); });

        source_ = ui::Caption(L"Source: text input");

        auto results = ui::VStack(10);
        for (HashAlgo algo : {HashAlgo::Md5, HashAlgo::Sha1, HashAlgo::Sha256}) {
            rows_.push_back(MakeRow(algo, results));
        }

        auto body = ui::VStack(14);
        body.Children().Append(input_);
        auto topRow = ui::HStack(10);
        topRow.Children().Append(fileBtn);
        topRow.Children().Append(source_);
        topRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        body.Children().Append(topRow);
        body.Children().Append(ui::Card(results));

        root_ = ui::Page(L"Hash & Checksum", body);
        HashText();
    }

    Row MakeRow(HashAlgo algo, winrt::Microsoft::UI::Xaml::Controls::StackPanel& parent) {
        Row r{algo};
        auto label = ui::Text(winrt::hstring(Utf8ToWide(HashAlgoName(algo))), 13, true);
        label.Width(80);

        r.value = winrt::TextBox();
        r.value.IsReadOnly(true);
        r.value.FontFamily(winrt::Microsoft::UI::Xaml::Media::FontFamily(L"Consolas"));
        r.value.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);

        auto copy = winrt::Button();
        copy.Content(winrt::box_value(winrt::hstring(L"Copy")));
        auto box = r.value;
        copy.Click([box](winrt::IInspectable const&, winrt::RoutedEventArgs const&) {
            WriteClipboardText(std::wstring(box.Text()));
        });

        auto row = ui::HStack(10);
        row.VerticalAlignment(winrt::VerticalAlignment::Center);
        row.Children().Append(label);
        r.value.Width(520);
        row.Children().Append(r.value);
        row.Children().Append(copy);
        parent.Children().Append(row);
        return r;
    }

    void HashText() {
        const std::string data = WideToUtf8(std::wstring(input_.Text()));
        source_.Text(L"Source: text input");
        for (auto& r : rows_) r.value.Text(winrt::hstring(Utf8ToWide(HashBytes(r.algo, data))));
    }

    void HashPickedFile() {
        auto path = PickFile();
        if (!path) return;
        source_.Text(winrt::hstring(L"Source: " + path->filename().wstring()));
        for (auto& r : rows_) {
            auto digest = HashFile(r.algo, *path);
            r.value.Text(winrt::hstring(digest ? Utf8ToWide(*digest) : L"(unable to read file)"));
        }
    }

    winrt::Microsoft::UI::Xaml::Controls::TextBox input_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock source_{nullptr};
    std::vector<Row> rows_;
    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeHashPage() {
    return std::make_unique<HashPage>();
}

}  // namespace superwin
