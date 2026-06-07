// File Converter page (native): convert between image formats using the built-in
// Windows Imaging Component, and DOCX <-> PDF using Microsoft Word via COM
// automation (when Word is installed; otherwise those options are hidden).
//
// Format detection / output naming live in ImageConvertLogic (superwin_core,
// unit-tested); this file owns the WIC + Word COM glue and the WinUI wiring.
#include <Windows.h>
#include <commdlg.h>
#include <oleauto.h>
#include <wincodec.h>

#include <cstdarg>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include "app/Ui.h"
#include "core/Strings.h"
#include "modules/fileconv/ImageConvertLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
}  // namespace winrt

namespace superwin {
namespace {

bool WordAvailable() {
    CLSID clsid{};
    return SUCCEEDED(::CLSIDFromProgID(L"Word.Application", &clsid));
}

const GUID* ContainerForExt(const std::string& ext) {
    const ImageFormat f = ImageFormatFromExtension(ext);
    switch (f) {
        case ImageFormat::Png:  return &GUID_ContainerFormatPng;
        case ImageFormat::Jpeg: return &GUID_ContainerFormatJpeg;
        case ImageFormat::Bmp:  return &GUID_ContainerFormatBmp;
        case ImageFormat::Gif:  return &GUID_ContainerFormatGif;
        case ImageFormat::Tiff: return &GUID_ContainerFormatTiff;
        case ImageFormat::Heic: return &GUID_ContainerFormatHeif;
        default: return nullptr;
    }
}

// Transcode an image with WIC. Returns true on success; fills `err` otherwise.
bool ConvertImage(const std::wstring& in, const std::wstring& out, const GUID& container, std::wstring& err) {
    try {
        winrt::com_ptr<IWICImagingFactory> factory;
        winrt::check_hresult(::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                                IID_PPV_ARGS(factory.put())));
        winrt::com_ptr<IWICBitmapDecoder> dec;
        winrt::check_hresult(factory->CreateDecoderFromFilename(
            in.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, dec.put()));
        winrt::com_ptr<IWICBitmapFrameDecode> frame;
        winrt::check_hresult(dec->GetFrame(0, frame.put()));

        winrt::com_ptr<IWICStream> stream;
        winrt::check_hresult(factory->CreateStream(stream.put()));
        winrt::check_hresult(stream->InitializeFromFilename(out.c_str(), GENERIC_WRITE));

        winrt::com_ptr<IWICBitmapEncoder> enc;
        winrt::check_hresult(factory->CreateEncoder(container, nullptr, enc.put()));
        winrt::check_hresult(enc->Initialize(stream.get(), WICBitmapEncoderNoCache));

        winrt::com_ptr<IWICBitmapFrameEncode> fe;
        winrt::com_ptr<IPropertyBag2> props;
        winrt::check_hresult(enc->CreateNewFrame(fe.put(), props.put()));
        winrt::check_hresult(fe->Initialize(props.get()));

        UINT w = 0, h = 0;
        winrt::check_hresult(frame->GetSize(&w, &h));
        winrt::check_hresult(fe->SetSize(w, h));
        WICPixelFormatGUID pf = GUID_WICPixelFormat32bppBGRA;
        winrt::check_hresult(fe->SetPixelFormat(&pf));  // encoder narrows to a format it supports
        winrt::check_hresult(fe->WriteSource(frame.get(), nullptr));  // converts pixels as needed
        winrt::check_hresult(fe->Commit());
        winrt::check_hresult(enc->Commit());
        return true;
    } catch (winrt::hresult_error const& e) {
        err = L"Image conversion failed (the target format's codec may not be installed): " +
              std::wstring(e.message());
        return false;
    }
}

// ---- minimal IDispatch automation helper (MSDN AutoWrap) -------------------
HRESULT AutoWrap(int autoType, VARIANT* pvResult, IDispatch* pDisp, LPOLESTR name, int cArgs...) {
    if (!pDisp) return E_POINTER;
    DISPID dispID = 0;
    HRESULT hr = pDisp->GetIDsOfNames(IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &dispID);
    if (FAILED(hr)) return hr;

    va_list marker;
    va_start(marker, cArgs);
    std::vector<VARIANT> args(static_cast<size_t>(cArgs > 0 ? cArgs : 0));
    for (int i = 0; i < cArgs; ++i) args[i] = va_arg(marker, VARIANT);  // reverse order per DISPPARAMS
    va_end(marker);

    DISPPARAMS dp{};
    dp.cArgs = static_cast<UINT>(cArgs);
    dp.rgvarg = args.empty() ? nullptr : args.data();
    DISPID putId = DISPID_PROPERTYPUT;
    if (autoType == DISPATCH_PROPERTYPUT) {
        dp.cNamedArgs = 1;
        dp.rgdispidNamedArgs = &putId;
    }
    return pDisp->Invoke(dispID, IID_NULL, LOCALE_USER_DEFAULT,
                         static_cast<WORD>(autoType), &dp, pvResult, nullptr, nullptr);
}

VARIANT VBool(bool b) { VARIANT v; VariantInit(&v); v.vt = VT_BOOL; v.boolVal = b ? VARIANT_TRUE : VARIANT_FALSE; return v; }
VARIANT VI4(long n) { VARIANT v; VariantInit(&v); v.vt = VT_I4; v.lVal = n; return v; }
VARIANT VStr(const std::wstring& s) { VARIANT v; VariantInit(&v); v.vt = VT_BSTR; v.bstrVal = ::SysAllocString(s.c_str()); return v; }

// DOCX/PDF conversion via Word. Runs on a worker STA thread (it CoInitializes
// itself), so the UI never freezes while Word starts up.
bool ConvertWithWord(const std::wstring& in, const std::wstring& out, bool toPdf, std::wstring& err) {
    CLSID clsid{};
    if (FAILED(::CLSIDFromProgID(L"Word.Application", &clsid))) { err = L"Microsoft Word is not installed."; return false; }
    IDispatch* word = nullptr;
    if (FAILED(::CoCreateInstance(clsid, nullptr, CLSCTX_LOCAL_SERVER, IID_IDispatch, reinterpret_cast<void**>(&word))) || !word) {
        err = L"Could not start Microsoft Word.";
        return false;
    }

    bool ok = false;
    IDispatch* doc = nullptr;
    {
        VARIANT vis = VBool(false);  AutoWrap(DISPATCH_PROPERTYPUT, nullptr, word, const_cast<LPOLESTR>(L"Visible"), 1, vis);
        VARIANT alerts = VI4(0);     AutoWrap(DISPATCH_PROPERTYPUT, nullptr, word, const_cast<LPOLESTR>(L"DisplayAlerts"), 1, alerts);

        VARIANT docs; VariantInit(&docs);
        if (SUCCEEDED(AutoWrap(DISPATCH_PROPERTYGET, &docs, word, const_cast<LPOLESTR>(L"Documents"), 0)) && docs.pdispVal) {
            VARIANT fn = VStr(in);
            VARIANT docV; VariantInit(&docV);
            if (SUCCEEDED(AutoWrap(DISPATCH_METHOD, &docV, docs.pdispVal, const_cast<LPOLESTR>(L"Open"), 1, fn)) && docV.pdispVal) {
                doc = docV.pdispVal;
                doc->AddRef();
                VARIANT outV = VStr(out);
                VARIANT fmt = VI4(toPdf ? 17 : 12);  // wdFormatPDF / wdFormatXMLDocument
                // SaveAs2(FileName, FileFormat): pass args right-to-left.
                const HRESULT hr = AutoWrap(DISPATCH_METHOD, nullptr, doc, const_cast<LPOLESTR>(L"SaveAs2"), 2, fmt, outV);
                ok = SUCCEEDED(hr);
                if (!ok) err = L"Word could not save the converted file.";
                ::SysFreeString(outV.bstrVal);
                VARIANT noSave = VI4(0);
                AutoWrap(DISPATCH_METHOD, nullptr, doc, const_cast<LPOLESTR>(L"Close"), 1, noSave);
                VariantClear(&docV);
            } else {
                err = L"Word could not open the source file.";
            }
            ::SysFreeString(fn.bstrVal);
            VariantClear(&docs);
        } else {
            err = L"Word automation interface was unavailable.";
        }
        AutoWrap(DISPATCH_METHOD, nullptr, word, const_cast<LPOLESTR>(L"Quit"), 0);
    }
    if (doc) doc->Release();
    word->Release();
    return ok;
}

// ---- common file dialogs ---------------------------------------------------
std::wstring OpenFileDialog() {
    wchar_t buf[MAX_PATH] = L"";
    // Filter with embedded NULs: "label\0pattern\0...\0\0".
    static const wchar_t filter[] =
        L"Supported files\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.webp;*.heic;*.ico;*.docx;*.doc;*.pdf\0"
        L"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.webp;*.heic;*.ico\0"
        L"Documents\0*.docx;*.doc;*.pdf\0"
        L"All files\0*.*\0\0";
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return ::GetOpenFileNameW(&ofn) ? std::wstring(buf) : std::wstring();
}

std::wstring SaveFileDialog(const std::wstring& suggested, const std::wstring& ext) {
    wchar_t buf[MAX_PATH] = L"";
    wcsncpy_s(buf, suggested.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = ext.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return ::GetSaveFileNameW(&ofn) ? std::wstring(buf) : std::wstring();
}

class FileConvertPage : public IModulePage {
public:
    FileConvertPage() { Build(); }
    winrt::UIElement Root() override { return root_; }

private:
    void Build() {
        dq_ = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
        wordOk_ = WordAvailable();

        auto choose = winrt::Button();
        choose.Content(winrt::box_value(winrt::hstring(L"Choose file…")));
        choose.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { OnChoose(); });

        fileLabel_ = ui::Caption(L"No file selected.");
        fileLabel_.VerticalAlignment(winrt::VerticalAlignment::Center);
        auto pickRow = ui::HStack(12);
        pickRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        pickRow.Children().Append(choose);
        pickRow.Children().Append(fileLabel_);

        targetCombo_ = winrt::ComboBox();
        targetCombo_.PlaceholderText(L"Convert to…");
        targetCombo_.MinWidth(200);
        auto targetLabel = ui::Text(L"Convert to", 14, true);
        targetLabel.VerticalAlignment(winrt::VerticalAlignment::Center);
        auto targetRow = ui::HStack(12);
        targetRow.VerticalAlignment(winrt::VerticalAlignment::Center);
        targetRow.Children().Append(targetLabel);
        targetRow.Children().Append(targetCombo_);

        convert_ = winrt::Button();
        convert_.Content(winrt::box_value(winrt::hstring(L"Convert")));
        if (auto st = winrt::Application::Current().Resources()
                          .TryLookup(winrt::box_value(winrt::hstring(L"AccentButtonStyle"))).try_as<winrt::Style>())
            convert_.Style(st);
        convert_.Click([this](winrt::IInspectable const&, winrt::RoutedEventArgs const&) { OnConvert(); });

        status_ = ui::Caption(L"");

        auto card = ui::VStack(14);
        card.Children().Append(ui::Caption(
            L"Convert images (PNG, JPG, BMP, GIF, TIFF, WEBP, HEIC) with built-in Windows imaging."
            L" DOCX \x2194 PDF needs Microsoft Word."));
        card.Children().Append(pickRow);
        card.Children().Append(targetRow);
        card.Children().Append(convert_);
        card.Children().Append(status_);
        if (!wordOk_)
            card.Children().Append(ui::Caption(L"Microsoft Word was not detected — DOCX \x2194 PDF is unavailable."));

        root_ = ui::Page(L"File Converter", ui::Card(card));
    }

    void OnChoose() {
        const std::wstring path = OpenFileDialog();
        if (path.empty()) return;
        input_ = path;
        fileLabel_.Text(winrt::hstring(path));
        status_.Text(L"");
        PopulateTargets();
    }

    void PopulateTargets() {
        targetCombo_.Items().Clear();
        targetExts_.clear();
        const std::string in = WideToUtf8(input_);
        if (IsImageExtension(in)) {
            const ImageFormat src = ImageFormatFromExtension(in);
            for (ImageFormat f : EncodableImageFormats()) {
                if (f == src) continue;
                const std::string ext = ExtensionForImageFormat(f);
                std::string up = ext; for (auto& c : up) c = static_cast<char>(::toupper((unsigned char)c));
                targetCombo_.Items().Append(winrt::box_value(winrt::hstring(Utf8ToWide(up))));
                targetExts_.push_back(ext);
            }
        } else if (IsWordExtension(in) && wordOk_) {
            targetCombo_.Items().Append(winrt::box_value(winrt::hstring(L"PDF")));
            targetExts_.push_back("pdf");
        } else if (IsPdfExtension(in) && wordOk_) {
            targetCombo_.Items().Append(winrt::box_value(winrt::hstring(L"Word document (DOCX)")));
            targetExts_.push_back("docx");
        } else {
            status_.Text(IsWordExtension(in) || IsPdfExtension(in)
                             ? L"This conversion needs Microsoft Word, which was not detected."
                             : L"Unsupported file type.");
        }
        if (!targetExts_.empty()) targetCombo_.SelectedIndex(0);
    }

    void OnConvert() {
        if (input_.empty()) { status_.Text(L"Choose a file first."); return; }
        const int idx = targetCombo_.SelectedIndex();
        if (idx < 0 || idx >= static_cast<int>(targetExts_.size())) { status_.Text(L"Choose a target format."); return; }
        const std::string targetExt = targetExts_[static_cast<size_t>(idx)];

        const std::wstring suggested = Utf8ToWide(ReplaceExtension(WideToUtf8(input_), targetExt));
        const std::wstring out = SaveFileDialog(suggested, Utf8ToWide(targetExt));
        if (out.empty()) return;

        const std::string in = WideToUtf8(input_);
        if (IsImageExtension(in)) {
            const GUID* container = ContainerForExt(targetExt);
            if (!container) { status_.Text(L"Unsupported target format."); return; }
            std::wstring err;
            const bool ok = ConvertImage(input_, out, *container, err);
            status_.Text(ok ? winrt::hstring(L"Saved " + out) : winrt::hstring(err));
            return;
        }

        // DOCX <-> PDF: run Word on a worker STA thread so the UI stays responsive.
        const bool toPdf = (targetExt == "pdf");
        status_.Text(L"Converting with Microsoft Word…");
        std::wstring inCopy = input_, outCopy = out;
        auto dq = dq_;
        auto status = status_;
        std::thread([inCopy, outCopy, toPdf, dq, status]() {
            ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            std::wstring err;
            const bool ok = ConvertWithWord(inCopy, outCopy, toPdf, err);
            ::CoUninitialize();
            if (dq) dq.TryEnqueue([status, ok, err, outCopy]() {
                status.Text(ok ? winrt::hstring(L"Saved " + outCopy) : winrt::hstring(L"Failed: " + err));
            });
        }).detach();
    }

    winrt::UIElement root_{nullptr};
    winrt::TextBlock fileLabel_{nullptr};
    winrt::ComboBox targetCombo_{nullptr};
    winrt::Button convert_{nullptr};
    winrt::TextBlock status_{nullptr};
    winrt::Microsoft::UI::Dispatching::DispatcherQueue dq_{nullptr};
    std::wstring input_;
    std::vector<std::string> targetExts_;
    bool wordOk_ = false;
};

}  // namespace

std::unique_ptr<IModulePage> MakeFileConvertPage() {
    return std::make_unique<FileConvertPage>();
}

}  // namespace superwin
