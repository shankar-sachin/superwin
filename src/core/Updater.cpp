// In-app self-updater.
//
// SuperWin installs per-user (a user-writable folder), so updates do NOT need an
// installer or admin rights. We fetch the appcast, and if a newer version is
// published we hand a small PowerShell helper the portable .zip URL; it waits for
// this process to exit, downloads + extracts the zip over the app folder, and
// relaunches SuperWin. From the user's point of view the app just restarts on the
// new version.
#include "core/Updater.h"

#include <Windows.h>
#include <shellapi.h>
#include <winhttp.h>

#include <array>
#include <chrono>
#include <fstream>
#include <string>
#include <thread>

#include "Version.h"
#include "core/Settings.h"
#include "core/Strings.h"

#pragma comment(lib, "winhttp.lib")

namespace superwin {
namespace {

// Appcast feed; the FIRST <item> is treated as the latest release. Its
// <enclosure url="..."> must point at a portable .zip of the app folder.
constexpr char kDefaultAppcastUrl[] =
    "https://raw.githubusercontent.com/shankar-sachin/superwin/master/installer/appcast.xml";

bool g_initialized = false;

std::string HttpGet(const std::wstring& url) {
    URL_COMPONENTS uc{}; uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {}, path[2048] = {};
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    if (!::WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return {};

    std::string body;
    HINTERNET session = ::WinHttpOpen(L"SuperWin-Updater/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                      WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) return {};
    if (HINTERNET conn = ::WinHttpConnect(session, host, uc.nPort, 0)) {
        const DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        if (HINTERNET req = ::WinHttpOpenRequest(conn, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                                 WINHTTP_DEFAULT_ACCEPT_TYPES, flags)) {
            if (::WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                ::WinHttpReceiveResponse(req, nullptr)) {
                for (;;) {
                    DWORD avail = 0;
                    if (!::WinHttpQueryDataAvailable(req, &avail) || avail == 0) break;
                    std::string chunk(avail, '\0');
                    DWORD read = 0;
                    if (!::WinHttpReadData(req, chunk.data(), avail, &read)) break;
                    body.append(chunk.data(), read);
                }
            }
            ::WinHttpCloseHandle(req);
        }
        ::WinHttpCloseHandle(conn);
    }
    ::WinHttpCloseHandle(session);
    return body;
}

// Read the value of attribute `key` (e.g. `sparkle:version="`) -- first match.
std::string Attr(const std::string& xml, const std::string& key) {
    auto p = xml.find(key);
    if (p == std::string::npos) return {};
    p += key.size();
    auto q = xml.find('"', p);
    return q == std::string::npos ? std::string() : xml.substr(p, q - p);
}

// Plain-text release notes from the first <item>'s CDATA <description>, so the
// update prompt can show "what's new" per release.
std::wstring FirstItemNotes(const std::string& xml) {
    const auto item = xml.find("<item>");
    if (item == std::string::npos) return {};
    const auto cd = xml.find("<![CDATA[", item);
    if (cd == std::string::npos) return {};
    const auto start = cd + 9;
    const auto end = xml.find("]]>", start);
    if (end == std::string::npos) return {};
    std::string html = xml.substr(start, end - start);

    // Turn list items into bullets, then strip the remaining tags.
    std::string text;
    text.reserve(html.size());
    for (size_t i = 0; i < html.size();) {
        if (html[i] == '<') {
            const auto close = html.find('>', i);
            const std::string tag = html.substr(i, close == std::string::npos ? 0 : close - i + 1);
            if (tag.rfind("<li", 0) == 0) text += "\n  \xE2\x80\xA2 ";  // • bullet
            else if (tag.rfind("</ul", 0) == 0 || tag.rfind("</h", 0) == 0) text += "\n";
            i = (close == std::string::npos) ? html.size() : close + 1;
        } else {
            text += html[i++];
        }
    }
    // Decode the handful of entities we actually emit.
    auto replace = [](std::string& s, const std::string& a, const std::string& b) {
        for (size_t p = s.find(a); p != std::string::npos; p = s.find(a, p + b.size())) s.replace(p, a.size(), b);
    };
    replace(text, "&amp;", "&"); replace(text, "&lt;", "<"); replace(text, "&gt;", ">");
    // Collapse runs of blank lines / leading whitespace for a tidy message box.
    std::string out; bool lastNl = false;
    for (char c : text) {
        if (c == '\r') continue;
        if (c == '\n') { if (!lastNl && !out.empty()) out += '\n'; lastNl = true; }
        else { out += c; lastNl = false; }
    }
    while (!out.empty() && (out.front() == '\n' || out.front() == ' ')) out.erase(out.begin());
    if (out.size() > 1200) out = out.substr(0, 1200) + "\xE2\x80\xA6";
    return Utf8ToWide(out);
}

std::array<int, 4> ParseVersion(const std::string& v) {
    std::array<int, 4> out{0, 0, 0, 0};
    int idx = 0; size_t i = 0;
    while (i < v.size() && idx < 4) {
        int n = 0; bool any = false;
        while (i < v.size() && v[i] >= '0' && v[i] <= '9') { n = n * 10 + (v[i] - '0'); ++i; any = true; }
        if (any) out[idx++] = n;
        while (i < v.size() && (v[i] < '0' || v[i] > '9')) ++i;
    }
    return out;
}

bool IsNewer(const std::string& candidate, const std::string& current) {
    auto a = ParseVersion(candidate), b = ParseVersion(current);
    for (int i = 0; i < 4; ++i) { if (a[i] != b[i]) return a[i] > b[i]; }
    return false;
}

std::wstring SelfExe() { wchar_t b[MAX_PATH]; ::GetModuleFileNameW(nullptr, b, MAX_PATH); return b; }
std::wstring DirOf(const std::wstring& p) { auto i = p.find_last_of(L"\\/"); return i == std::wstring::npos ? L"." : p.substr(0, i); }

// Check the feed. Returns true and fills the latest version + zip URL + notes.
bool Check(std::string& version, std::wstring& zipUrl, std::wstring& notes) {
    const std::string url = Settings::Instance().GetString("update.appcastUrl", kDefaultAppcastUrl);
    const std::string xml = HttpGet(Utf8ToWide(url));
    if (xml.empty()) return false;
    version = Attr(xml, "sparkle:version=\"");
    const std::string enc = Attr(xml, "url=\"");  // first enclosure (latest item)
    if (version.empty() || enc.empty()) return false;
    zipUrl = Utf8ToWide(enc);
    notes = FirstItemNotes(xml);
    return true;
}

// Write the helper script and launch it, then quit so it can replace our files.
void ApplyUpdate(const std::wstring& zipUrl) {
    const std::wstring exe = SelfExe();
    const std::wstring dir = DirOf(exe);

    wchar_t tmp[MAX_PATH]; ::GetTempPathW(MAX_PATH, tmp);
    const std::wstring script = std::wstring(tmp) + L"superwin_update.ps1";

    // ASCII-only script; the exe/dir/url come in as parameters (so any Unicode in
    // the install path is passed safely on the command line, not embedded here).
    // The helper ALWAYS relaunches the app (finally block) even if the download or
    // copy fails, so an update error can never leave the user with a closed app.
    // Failures are logged to %TEMP%\superwin_update.log for diagnosis.
    std::ofstream f(script, std::ios::binary | std::ios::trunc);
    f <<
        "param($url,$dir,$exe,$ppid)\n"
        "$log = Join-Path $env:TEMP 'superwin_update.log'\n"
        "function L($m){ try { Add-Content -Path $log -Value ((Get-Date).ToString('s')+' '+$m) } catch {} }\n"
        "try { Wait-Process -Id $ppid -Timeout 60 } catch {}\n"
        "Start-Sleep -Milliseconds 500\n"
        "$tmp = $null\n"
        "try {\n"
        "  $tmp = Join-Path $env:TEMP ('superwin_upd_' + [guid]::NewGuid())\n"
        "  New-Item -ItemType Directory -Force $tmp | Out-Null\n"
        "  $zip = Join-Path $tmp 'pkg.zip'\n"
        "  L \"downloading $url\"\n"
        "  Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing\n"
        "  $ex = Join-Path $tmp 'x'\n"
        "  Expand-Archive -Path $zip -DestinationPath $ex -Force\n"
        "  $top = @(Get-ChildItem -Force $ex)\n"
        "  $src = $ex\n"
        "  if ($top.Count -eq 1 -and $top[0].PSIsContainer) { $src = $top[0].FullName }\n"
        "  L \"copying $src -> $dir\"\n"
        "  Copy-Item -Path (Join-Path $src '*') -Destination $dir -Recurse -Force\n"
        "  L 'update applied'\n"
        "} catch {\n"
        "  L (\"ERROR: \" + $_.Exception.Message)\n"
        "} finally {\n"
        "  try { Start-Process -FilePath $exe } catch { L (\"relaunch failed: \" + $_.Exception.Message) }\n"
        "  try { if ($tmp) { Remove-Item -Recurse -Force $tmp } } catch {}\n"
        "}\n";
    f.close();

    std::wstring args = L"-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File \"" + script +
                        L"\" -url \"" + zipUrl + L"\" -dir \"" + dir + L"\" -exe \"" + exe +
                        L"\" -ppid " + std::to_wstring(::GetCurrentProcessId());
    ::ShellExecuteW(nullptr, L"open", L"powershell.exe", args.c_str(), nullptr, SW_HIDE);
    ::ExitProcess(0);  // let the helper swap our files and relaunch us
}

void Prompt(const std::string& version, const std::wstring& zipUrl, const std::wstring& notes) {
    std::wstring msg = L"SuperWin " + Utf8ToWide(version) + L" is available.";
    if (!notes.empty()) msg += L"\n\nWhat's new:\n" + notes;
    msg += L"\n\nUpdate now? SuperWin will briefly close and reopen on the new version.";
    if (::MessageBoxW(nullptr, msg.c_str(), L"SuperWin update",
                      MB_YESNO | MB_ICONINFORMATION) == IDYES) {
        ApplyUpdate(zipUrl);
    }
}

}  // namespace

void Updater::Initialize() {
    if (g_initialized) return;
    g_initialized = true;
    if (!Settings::Instance().GetBool("update.autoCheck", true)) return;

    // Quiet background check shortly after launch.
    std::thread([] {
        std::this_thread::sleep_for(std::chrono::seconds(6));
        std::string ver; std::wstring zip, notes;
        if (Check(ver, zip, notes) && IsNewer(ver, SUPERWIN_VERSION_STRING)) Prompt(ver, zip, notes);
    }).detach();
}

void Updater::CheckNow() {
    std::string ver; std::wstring zip, notes;
    if (!Check(ver, zip, notes)) {
        ::MessageBoxW(nullptr, L"Couldn't reach the update server. Are you connected to the internet?",
                      L"SuperWin", MB_OK | MB_ICONWARNING);
        return;
    }
    if (IsNewer(ver, SUPERWIN_VERSION_STRING)) {
        Prompt(ver, zip, notes);
    } else {
        ::MessageBoxW(nullptr, L"You're on the latest version of SuperWin.",
                      L"SuperWin", MB_OK | MB_ICONINFORMATION);
    }
}

void Updater::Shutdown() {}

}  // namespace superwin
