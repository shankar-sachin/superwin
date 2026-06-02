#include <string>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/convert/ConvertLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

winrt::hstring FormatNumber(double v) {
    wchar_t buf[64];
    swprintf_s(buf, L"%.6g", v);
    return buf;
}

class ConvertPage : public IModulePage {
public:
    ConvertPage() { Build(); }
    winrt::Microsoft::UI::Xaml::UIElement Root() override { return root_; }

private:
    void Build() {
        auto body = ui::VStack(16);
        body.Children().Append(ui::Text(L"Units", 16, true));
        body.Children().Append(BuildUnitCard());
        body.Children().Append(ui::Text(L"Number base", 16, true));
        body.Children().Append(BuildBaseCard());
        root_ = ui::Page(L"Unit Converter", body);
    }

    winrt::ComboBox FillCombo(const std::vector<std::string>& items) {
        winrt::ComboBox c;
        for (const auto& s : items) c.Items().Append(winrt::box_value(winrt::hstring(Utf8ToWide(s))));
        if (!items.empty()) c.SelectedIndex(0);
        return c;
    }

    static UnitCategory CategoryFromIndex(int i) {
        switch (i) { case 1: return UnitCategory::Mass; case 2: return UnitCategory::Temperature;
                     case 3: return UnitCategory::DataSize; default: return UnitCategory::Length; }
    }

    winrt::Border BuildUnitCard() {
        category_ = FillCombo(CategoryNames());
        category_.SelectionChanged([this](winrt::IInspectable const&, winrt::IInspectable const&) {
            RepopulateUnits();
            ConvertUnits();
        });

        fromUnit_ = winrt::ComboBox();
        toUnit_ = winrt::ComboBox();
        amount_ = winrt::TextBox();
        amount_.Text(L"1");
        amount_.Width(140);
        auto onChange = [this](winrt::IInspectable const&, auto const&) { ConvertUnits(); };
        amount_.TextChanged(onChange);
        fromUnit_.SelectionChanged(onChange);
        toUnit_.SelectionChanged(onChange);
        unitResult_ = ui::Text(L"", 16, true);

        RepopulateUnits();

        auto row = ui::HStack(10);
        row.VerticalAlignment(winrt::VerticalAlignment::Center);
        row.Children().Append(amount_);
        row.Children().Append(fromUnit_);
        row.Children().Append(ui::Text(L"\x2192", 16));
        row.Children().Append(toUnit_);

        auto col = ui::VStack(12);
        col.Children().Append(category_);
        col.Children().Append(row);
        col.Children().Append(unitResult_);
        ConvertUnits();
        return ui::Card(col);
    }

    void RepopulateUnits() {
        auto units = UnitsFor(CategoryFromIndex(category_.SelectedIndex()));
        fromUnit_.Items().Clear();
        toUnit_.Items().Clear();
        for (const auto& u : units) {
            fromUnit_.Items().Append(winrt::box_value(winrt::hstring(Utf8ToWide(u))));
            toUnit_.Items().Append(winrt::box_value(winrt::hstring(Utf8ToWide(u))));
        }
        if (!units.empty()) {
            fromUnit_.SelectedIndex(0);
            toUnit_.SelectedIndex(units.size() > 1 ? 1 : 0);
        }
    }

    void ConvertUnits() {
        if (fromUnit_.SelectedIndex() < 0 || toUnit_.SelectedIndex() < 0) return;
        double value = 0;
        try { value = std::stod(std::wstring(amount_.Text())); }
        catch (...) { unitResult_.Text(L"Enter a number"); return; }

        const auto cat = CategoryFromIndex(category_.SelectedIndex());
        const std::string from = WideToUtf8(std::wstring(winrt::unbox_value<winrt::hstring>(fromUnit_.SelectedItem())));
        const std::string to = WideToUtf8(std::wstring(winrt::unbox_value<winrt::hstring>(toUnit_.SelectedItem())));
        auto out = ConvertUnit(cat, from, to, value);
        unitResult_.Text(out ? FormatNumber(*out) : winrt::hstring(L"\x2014"));
    }

    winrt::Border BuildBaseCard() {
        baseInput_ = winrt::TextBox();
        baseInput_.Text(L"255");
        baseInput_.Width(220);
        inputBase_ = FillCombo({"Decimal", "Hexadecimal", "Binary", "Octal"});
        auto onChange = [this](winrt::IInspectable const&, auto const&) { ConvertBase(); };
        baseInput_.TextChanged(onChange);
        inputBase_.SelectionChanged(onChange);

        dec_ = MakeReadout(L"Decimal");
        hex_ = MakeReadout(L"Hex");
        bin_ = MakeReadout(L"Binary");
        oct_ = MakeReadout(L"Octal");

        auto inRow = ui::HStack(10);
        inRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        inRow.Children().Append(baseInput_);
        inRow.Children().Append(inputBase_);

        auto col = ui::VStack(10);
        col.Children().Append(inRow);
        col.Children().Append(dec_.row);
        col.Children().Append(hex_.row);
        col.Children().Append(bin_.row);
        col.Children().Append(oct_.row);
        ConvertBase();
        return ui::Card(col);
    }

    struct Readout {
        winrt::Microsoft::UI::Xaml::Controls::StackPanel row{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::TextBlock value{nullptr};
    };

    Readout MakeReadout(winrt::hstring label) {
        Readout r;
        auto l = ui::Text(label, 13, true);
        l.Width(80);
        r.value = ui::Text(L"", 14);
        r.value.FontFamily(winrt::Microsoft::UI::Xaml::Media::FontFamily(L"Consolas"));
        r.row = ui::HStack(10);
        r.row.VerticalAlignment(winrt::VerticalAlignment::Center);
        r.row.Children().Append(l);
        r.row.Children().Append(r.value);
        return r;
    }

    static int BaseFromIndex(int i) {
        switch (i) { case 1: return 16; case 2: return 2; case 3: return 8; default: return 10; }
    }

    void ConvertBase() {
        const int base = BaseFromIndex(inputBase_.SelectedIndex());
        auto parsed = ParseInBase(WideToUtf8(std::wstring(baseInput_.Text())), base);
        if (!parsed) {
            for (auto* r : {&dec_, &hex_, &bin_, &oct_}) r->value.Text(L"\x2014");
            return;
        }
        dec_.value.Text(winrt::hstring(Utf8ToWide(FormatInBase(*parsed, 10))));
        hex_.value.Text(winrt::hstring(Utf8ToWide("0x" + FormatInBase(*parsed, 16))));
        bin_.value.Text(winrt::hstring(Utf8ToWide(FormatInBase(*parsed, 2))));
        oct_.value.Text(winrt::hstring(Utf8ToWide("0" + FormatInBase(*parsed, 8))));
    }

    winrt::Microsoft::UI::Xaml::Controls::ComboBox category_{nullptr}, fromUnit_{nullptr}, toUnit_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBox amount_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBlock unitResult_{nullptr};

    winrt::Microsoft::UI::Xaml::Controls::TextBox baseInput_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ComboBox inputBase_{nullptr};
    Readout dec_, hex_, bin_, oct_;

    winrt::Microsoft::UI::Xaml::UIElement root_{nullptr};
};

}  // namespace

std::unique_ptr<IModulePage> MakeConvertPage() {
    return std::make_unique<ConvertPage>();
}

}  // namespace superwin
