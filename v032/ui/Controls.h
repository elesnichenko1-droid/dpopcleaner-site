#pragma once
#include <windows.h>
#include <commctrl.h>

#include <string_view>

namespace dpop::ui {

enum class ButtonVisual {
    Normal,
    Accent,
    Danger
};

struct ButtonPalette {
    COLORREF background;
    COLORREF border;
    COLORREF text;
};

ButtonPalette ButtonColors(ButtonVisual visual, bool pressed = false) noexcept;

HWND CreateTextLabel(
    HWND parent,
    int id,
    std::wstring_view text,
    DWORD style = SS_LEFT | SS_NOPREFIX
) noexcept;

HWND CreatePushButton(
    HWND parent,
    int id,
    std::wstring_view text,
    ButtonVisual visual = ButtonVisual::Normal
) noexcept;

HWND CreateReadOnlyLogEdit(HWND parent, int id) noexcept;
HWND CreateDarkListView(HWND parent, int id) noexcept;

void ApplyControlFont(HWND control, HFONT font) noexcept;
void ApplyDarkEdit(HWND edit) noexcept;
void ApplyDarkListView(HWND list) noexcept;

bool DrawOwnerButton(
    const DRAWITEMSTRUCT& draw,
    std::wstring_view text,
    ButtonVisual visual
) noexcept;

}
