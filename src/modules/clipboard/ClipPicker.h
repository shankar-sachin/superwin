// Win+Shift+V quick picker: a small, borderless, always-on-top window that
// surfaces the clipboard history right at the cursor. Search as you type,
// pinned items first, full keyboard navigation, and (optionally) auto-paste
// straight back into the app you were using -- a friendlier take on Win+V.
//
// Created once and shown/hidden on demand by AppHost.
#pragma once

#include <Windows.h>

#include <cstdint>
#include <vector>

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Windowing.h>

namespace superwin {

class ClipPicker {
public:
    ClipPicker();

    // Pop the picker at the cursor. `previousForeground` is the window that had
    // focus before we appeared, so a chosen clip can be pasted back into it.
    void Show(HWND previousForeground);

private:
    void Build();
    void Populate();
    void Hide();
    void ChooseSelected();
    void ChooseId(uint64_t id);
    void PasteIntoPrevious();

    winrt::Microsoft::UI::Xaml::Window               window_{nullptr};
    winrt::Microsoft::UI::Windowing::AppWindow       appWindow_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::TextBox    search_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ListView   list_{nullptr};
    winrt::Microsoft::UI::Xaml::DispatcherTimer      pasteTimer_{nullptr};

    HWND previous_ = nullptr;
    bool autoPaste_ = true;
    bool built_ = false;
};

}  // namespace superwin
