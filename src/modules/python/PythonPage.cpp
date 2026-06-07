// Python IDE page: a WebView2-hosted editor (CodeMirror-free, dependency-light
// syntax highlighting in web/) on top, and a native run pipeline that spawns the
// system Python and streams stdout/stderr back into the page's output pane.
//
// Interpreter resolution + command-line quoting live in PythonRunLogic
// (superwin_core, unit-tested); this file owns the WebView2 + CreateProcessW glue.
#include <Windows.h>

#include <atomic>
#include <fstream>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include <winrt/Windows.Foundation.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.Web.WebView2.Core.h>

#include "app/Ui.h"
#include "core/Settings.h"
#include "core/Strings.h"
#include "modules/python/PythonRunLogic.h"

namespace winrt {
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;
namespace wv2 = winrt::Microsoft::Web::WebView2::Core;
}  // namespace winrt

namespace superwin {
namespace {

std::wstring ExeDir() {
    wchar_t b[MAX_PATH]{};
    ::GetModuleFileNameW(nullptr, b, MAX_PATH);
    std::wstring p = b;
    const auto i = p.find_last_of(L"\\/");
    return i == std::wstring::npos ? L"." : p.substr(0, i);
}

// Resolve a usable Python interpreter: an explicit settings override first, then
// the launcher / python on PATH. Returns an empty string if none is found.
std::wstring ResolveInterpreter() {
    const std::wstring override = Utf8ToWide(Settings::Instance().GetString("python.interpreter", ""));
    if (!override.empty()) {
        if (::GetFileAttributesW(override.c_str()) != INVALID_FILE_ATTRIBUTES) return override;
    }
    for (auto const& cand : DefaultInterpreterCandidates()) {
        wchar_t full[MAX_PATH]{};
        if (::SearchPathW(nullptr, cand.c_str(), nullptr, MAX_PATH, full, nullptr) > 0) return full;
    }
    return L"";
}

std::wstring TempScriptPath() {
    wchar_t dir[MAX_PATH]{};
    ::GetTempPathW(MAX_PATH, dir);
    return std::wstring(dir) + L"superwin_ide.py";
}

class PythonPage : public IModulePage {
public:
    PythonPage() { Build(); }
    ~PythonPage() override { if (reader_.joinable()) reader_.detach(); }
    winrt::UIElement Root() override { return root_; }

private:
    void Build() {
        dq_ = winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();

        host_ = winrt::Border();
        host_.CornerRadius(winrt::CornerRadius{10, 10, 10, 10});
        host_.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
        host_.VerticalAlignment(winrt::VerticalAlignment::Stretch);
        if (auto st = ui::ThemeBrush(L"CardStrokeColorDefaultBrush")) {
            host_.BorderBrush(st);
            host_.BorderThickness(winrt::Thickness{1, 1, 1, 1});
        }

        auto grid = winrt::Grid();
        grid.Margin(winrt::Thickness{36, 24, 36, 24});
        auto r0 = winrt::RowDefinition(); r0.Height(winrt::GridLengthHelper::Auto());
        auto r1 = winrt::RowDefinition(); r1.Height(winrt::GridLengthHelper::FromValueAndType(1, winrt::GridUnitType::Star));
        grid.RowDefinitions().Append(r0);
        grid.RowDefinitions().Append(r1);

        auto header = ui::VStack(2);
        header.Margin(winrt::Thickness{0, 0, 0, 14});
        header.Children().Append(ui::Title(L"Python IDE"));
        header.Children().Append(ui::Caption(
            L"Write Python with syntax highlighting and run it with your installed interpreter."));
        winrt::Grid::SetRow(header, 0);
        winrt::Grid::SetRow(host_, 1);
        grid.Children().Append(header);
        grid.Children().Append(host_);
        root_ = grid;

        SetupWebView();
    }

    void SetupWebView() {
        webview_ = winrt::WebView2();
        webview_.HorizontalAlignment(winrt::HorizontalAlignment::Stretch);
        webview_.VerticalAlignment(winrt::VerticalAlignment::Stretch);
        host_.Child(webview_);

        webview_.CoreWebView2Initialized([this](winrt::WebView2 const&, winrt::CoreWebView2InitializedEventArgs const& args) {
            if (args.Exception()) { ShowWebFallback(); return; }
            auto core = webview_.CoreWebView2();
            auto s = core.Settings();
            s.AreDevToolsEnabled(false);
            s.AreDefaultContextMenusEnabled(false);
            s.IsZoomControlEnabled(false);
            s.IsStatusBarEnabled(false);
            core.SetVirtualHostNameToFolderMapping(
                L"superwin.python", ExeDir() + L"\\pyweb", winrt::wv2::CoreWebView2HostResourceAccessKind::Allow);
            core.WebMessageReceived([this](winrt::wv2::CoreWebView2 const&, winrt::wv2::CoreWebView2WebMessageReceivedEventArgs const& e) {
                OnWebMessage(e);
            });
            webview_.NavigationCompleted([this](winrt::WebView2 const&, winrt::wv2::CoreWebView2NavigationCompletedEventArgs const&) {
                PushTheme();
            });
            core.Navigate(L"https://superwin.python/index.html");
        });

        try { webview_.EnsureCoreWebView2Async(); }
        catch (...) { ShowWebFallback(); }
    }

    void ShowWebFallback() {
        auto col = ui::VStack(8);
        col.Margin(winrt::Thickness{16, 16, 16, 16});
        col.Children().Append(ui::Text(L"WebView2 runtime not found", 15, true));
        col.Children().Append(ui::Caption(
            L"The Python editor needs the Microsoft Edge WebView2 runtime (preinstalled on "
            L"Windows 11). Install it, then reopen SuperWin."));
        host_.Child(col);
    }

    void PushTheme() {
        if (!webview_ || !webview_.CoreWebView2()) return;
        const bool dark = host_ && host_.ActualTheme() == winrt::ElementTheme::Dark;
        webview_.ExecuteScriptAsync(dark ? L"swSetTheme('dark')" : L"swSetTheme('light')");
    }

    void OnWebMessage(winrt::wv2::CoreWebView2WebMessageReceivedEventArgs const& e) {
        std::string utf8;
        try { utf8 = WideToUtf8(std::wstring(e.TryGetWebMessageAsString())); }
        catch (...) { return; }
        auto j = nlohmann::json::parse(utf8, nullptr, false);
        if (j.is_discarded() || !j.is_object()) return;
        if (j.value("type", std::string()) != "run") return;
        if (running_.load()) return;
        RunCode(j.value("code", std::string()));
    }

    // Marshal a JS call onto the UI thread (where ExecuteScriptAsync must run).
    void RunScript(std::wstring script) {
        if (!dq_) return;
        dq_.TryEnqueue([this, script]() {
            if (webview_ && webview_.CoreWebView2()) webview_.ExecuteScriptAsync(script);
        });
    }
    void AppendOut(const std::string& chunk) {
        const std::string js = nlohmann::json(chunk).dump(
            -1, ' ', false, nlohmann::json::error_handler_t::replace);
        RunScript(L"swAppendOutput(" + Utf8ToWide(js) + L")");
    }

    void RunCode(const std::string& codeUtf8) {
        // Write the buffer to a temp .py (UTF-8; Python 3 reads UTF-8 by default).
        const std::wstring scriptPath = TempScriptPath();
        {
            std::ofstream f(scriptPath, std::ios::binary | std::ios::trunc);
            if (!f) { AppendOut("Could not write the temp script file.\n"); return; }
            f.write(codeUtf8.data(), static_cast<std::streamsize>(codeUtf8.size()));
        }

        const std::wstring interp = ResolveInterpreter();
        if (interp.empty()) {
            AppendOut("Python interpreter not found.\n"
                      "Install Python from https://www.python.org/downloads/ (tick \"Add to PATH\"),\n"
                      "or set the full path in Settings (python.interpreter).\n");
            RunScript(L"swRunState(false)");
            return;
        }

        if (reader_.joinable()) reader_.join();
        running_.store(true);
        RunScript(L"swRunState(true)");
        RunScript(L"swSetStatus('')");

        std::wstring cmd = BuildPythonCommandLine(interp, scriptPath);
        SpawnAndStream(std::move(cmd), scriptPath.substr(0, scriptPath.find_last_of(L"\\/")));
    }

    void SpawnAndStream(std::wstring cmd, std::wstring workDir) {
        SECURITY_ATTRIBUTES sa{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        HANDLE rd = nullptr, wr = nullptr;
        if (!::CreatePipe(&rd, &wr, &sa, 0)) {
            AppendOut("Failed to create output pipe.\n");
            running_.store(false); RunScript(L"swRunState(false)");
            return;
        }
        ::SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);  // keep the read end private

        HANDLE nul = ::CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, 0, nullptr);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = wr;
        si.hStdError = wr;
        si.hStdInput = nul;

        PROCESS_INFORMATION pi{};
        std::wstring mutableCmd = cmd;  // CreateProcessW may modify the buffer
        const BOOL ok = ::CreateProcessW(
            nullptr, mutableCmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
            nullptr, workDir.empty() ? nullptr : workDir.c_str(), &si, &pi);

        ::CloseHandle(wr);  // parent keeps only the read end
        if (nul && nul != INVALID_HANDLE_VALUE) ::CloseHandle(nul);

        if (!ok) {
            ::CloseHandle(rd);
            AppendOut("Failed to launch the Python interpreter.\n");
            running_.store(false); RunScript(L"swRunState(false)");
            return;
        }
        ::CloseHandle(pi.hThread);

        reader_ = std::thread([this, rd, proc = pi.hProcess]() {
            char buf[4096];
            DWORD n = 0;
            while (::ReadFile(rd, buf, sizeof(buf), &n, nullptr) && n > 0) {
                AppendOut(std::string(buf, buf + n));
            }
            ::WaitForSingleObject(proc, INFINITE);
            DWORD code = 0;
            ::GetExitCodeProcess(proc, &code);
            ::CloseHandle(rd);
            ::CloseHandle(proc);
            running_.store(false);
            RunScript(L"swRunState(false)");
            RunScript(L"swSetStatus('Exited with code " + std::to_wstring(static_cast<int>(code)) + L"')");
        });
    }

    winrt::UIElement root_{nullptr};
    winrt::Border host_{nullptr};
    winrt::WebView2 webview_{nullptr};
    winrt::Microsoft::UI::Dispatching::DispatcherQueue dq_{nullptr};
    std::thread reader_;
    std::atomic<bool> running_{false};
};

}  // namespace

std::unique_ptr<IModulePage> MakePythonPage() {
    return std::make_unique<PythonPage>();
}

}  // namespace superwin
