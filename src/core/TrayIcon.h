// System-tray presence for SuperWin (WinUI 3 has no tray API, so this is raw
// Shell_NotifyIcon). The icon shares the app's hidden message window; tray
// callbacks and context-menu commands arrive there as WM_APP+1 / WM_COMMAND.
#pragma once

#include <Windows.h>
#include <shellapi.h>  // NOTIFYICONDATAW

#include <functional>
#include <string>
#include <vector>

namespace superwin {

class TrayIcon {
public:
    using Action = std::function<void()>;

    static constexpr UINT kCallbackMessage = WM_APP + 1;

    TrayIcon(HWND owner, HICON icon, std::wstring tooltip);
    ~TrayIcon();

    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;

    // Append a context-menu command. `action` runs when the item is chosen.
    void AddMenuItem(std::wstring text, Action action);
    void AddSeparator();

    // Action invoked on left-click / double-click (typically "open dashboard").
    void SetPrimaryAction(Action action) { primary_ = std::move(action); }

    // Transient balloon notification.
    void ShowBalloon(const std::wstring& title, const std::wstring& text);

    // Route messages from the owner window proc. Returns true if consumed.
    bool HandleCallback(WPARAM wParam, LPARAM lParam);
    bool HandleCommand(WORD commandId);

private:
    void ShowContextMenu();

    struct Item {
        std::wstring text;
        Action       action;
        bool         separator = false;
        UINT         id = 0;
    };

    HWND               owner_;
    NOTIFYICONDATAW    nid_{};
    std::vector<Item>  items_;
    Action             primary_;
    UINT               nextCommandId_ = 0xC000;
};

}  // namespace superwin
