#include "core/TrayIcon.h"

#include <shellapi.h>

namespace superwin {

TrayIcon::TrayIcon(HWND owner, HICON icon, std::wstring tooltip) : owner_(owner) {
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = owner_;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid_.uCallbackMessage = kCallbackMessage;
    nid_.hIcon = icon;
    // Tooltip is fixed-size; copy with truncation guard.
    wcsncpy_s(nid_.szTip, tooltip.c_str(), _TRUNCATE);
    ::Shell_NotifyIconW(NIM_ADD, &nid_);

    nid_.uVersion = NOTIFYICON_VERSION_4;
    ::Shell_NotifyIconW(NIM_SETVERSION, &nid_);
}

TrayIcon::~TrayIcon() {
    ::Shell_NotifyIconW(NIM_DELETE, &nid_);
}

void TrayIcon::AddMenuItem(std::wstring text, Action action) {
    items_.push_back(Item{std::move(text), std::move(action), false, nextCommandId_++});
}

void TrayIcon::AddSeparator() {
    items_.push_back(Item{L"", nullptr, true, 0});
}

void TrayIcon::ShowBalloon(const std::wstring& title, const std::wstring& text) {
    NOTIFYICONDATAW b = nid_;
    b.uFlags = NIF_INFO;
    b.dwInfoFlags = NIIF_INFO;
    wcsncpy_s(b.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(b.szInfo, text.c_str(), _TRUNCATE);
    ::Shell_NotifyIconW(NIM_MODIFY, &b);
}

bool TrayIcon::HandleCallback(WPARAM /*wParam*/, LPARAM lParam) {
    switch (LOWORD(lParam)) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
            if (primary_) primary_();
            return true;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
            ShowContextMenu();
            return true;
        default:
            return false;
    }
}

bool TrayIcon::HandleCommand(WORD commandId) {
    for (auto& item : items_) {
        if (!item.separator && item.id == commandId) {
            if (item.action) item.action();
            return true;
        }
    }
    return false;
}

void TrayIcon::ShowContextMenu() {
    HMENU menu = ::CreatePopupMenu();
    if (!menu) return;
    for (const auto& item : items_) {
        if (item.separator) {
            ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        } else {
            ::AppendMenuW(menu, MF_STRING, item.id, item.text.c_str());
        }
    }

    POINT pt{};
    ::GetCursorPos(&pt);
    // Required so the menu dismisses correctly when focus is lost.
    ::SetForegroundWindow(owner_);
    ::TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN,
                     pt.x, pt.y, 0, owner_, nullptr);
    ::PostMessageW(owner_, WM_NULL, 0, 0);
    ::DestroyMenu(menu);
}

}  // namespace superwin
