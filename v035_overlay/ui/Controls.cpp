#include "ui/Controls.h"

#include "ui/Theme.h"

#include <string>
#include <uxtheme.h>

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")

namespace dpop::ui {
namespace {

constexpr wchar_t kButtonVisualProperty[] = L"DPopCleaner.ButtonVisual";

HWND CreateChild(
    DWORD exStyle,
    const wchar_t* className,
    std::wstring_view text,
    DWORD style,
    HWND parent,
    int id
) noexcept {
    const std::wstring ownedText{text};

    return CreateWindowExW(
        exStyle,
        className,
        ownedText.c_str(),
        WS_CHILD | WS_VISIBLE | style,
        0,
        0,
        0,
        0,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr
    );
}

}

ButtonPalette ButtonColors(ButtonVisual visual, bool pressed) noexcept {
    const auto& p = MidnightPalette();

    switch (visual) {
    case ButtonVisual::Accent:
        return {
            pressed ? p.accentHover : p.accent,
            pressed ? p.accentHover : p.accent,
            p.background
        };

    case ButtonVisual::Danger:
        return {
            pressed ? p.hover : p.control,
            p.error,
            p.text
        };

    case ButtonVisual::Normal:
    default:
        return {
            pressed ? p.hover : p.control,
            p.border,
            p.text
        };
    }
}

HWND CreateTextLabel(
    HWND parent,
    int id,
    std::wstring_view text,
    DWORD style
) noexcept {
    return CreateChild(
        0,
        L"STATIC",
        text,
        style,
        parent,
        id
    );
}

HWND CreatePushButton(
    HWND parent,
    int id,
    std::wstring_view text,
    ButtonVisual visual
) noexcept {
    HWND button = CreateChild(
        0,
        L"BUTTON",
        text,
        BS_OWNERDRAW | BS_NOTIFY | WS_TABSTOP,
        parent,
        id
    );
    if (!button) return nullptr;

    const auto encoded = static_cast<INT_PTR>(visual) + 1;
    if (!SetPropW(button, kButtonVisualProperty, reinterpret_cast<HANDLE>(encoded))) {
        DestroyWindow(button);
        return nullptr;
    }
    return button;
}

ButtonVisual ButtonVisualFor(HWND button) noexcept {
    if (!button) return ButtonVisual::Normal;
    const HANDLE raw = GetPropW(button, kButtonVisualProperty);
    if (!raw) return ButtonVisual::Normal;
    const auto decoded = reinterpret_cast<INT_PTR>(raw) - 1;
    if (decoded < static_cast<INT_PTR>(ButtonVisual::Normal) ||
        decoded > static_cast<INT_PTR>(ButtonVisual::Danger)) {
        return ButtonVisual::Normal;
    }
    return static_cast<ButtonVisual>(decoded);
}

HWND CreateReadOnlyLogEdit(HWND parent, int id) noexcept {
    HWND edit = CreateChild(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL |
            ES_READONLY | WS_VSCROLL,
        parent,
        id
    );

    ApplyDarkEdit(edit);
    return edit;
}

HWND CreateDarkListView(HWND parent, int id) noexcept {
    HWND list = CreateChild(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWW,
        L"",
        LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
        parent,
        id
    );

    ApplyDarkListView(list);
    return list;
}

void ApplyControlFont(HWND control, HFONT font) noexcept {
    if (control && font) {
        SendMessageW(
            control,
            WM_SETFONT,
            reinterpret_cast<WPARAM>(font),
            TRUE
        );
    }
}

void ApplyDarkEdit(HWND edit) noexcept {
    if (!edit) {
        return;
    }

    SetWindowTheme(edit, L"DarkMode_Explorer", nullptr);
}

void ApplyDarkListView(HWND list) noexcept {
    if (!list) {
        return;
    }

    const auto& p = MidnightPalette();

    SetWindowTheme(list, L"DarkMode_Explorer", nullptr);
    ListView_SetBkColor(list, p.control);
    ListView_SetTextBkColor(list, p.control);
    ListView_SetTextColor(list, p.text);
}

bool DrawOwnerButton(
    const DRAWITEMSTRUCT& draw,
    std::wstring_view text,
    ButtonVisual visual
) noexcept {
    if (draw.CtlType != ODT_BUTTON || !draw.hDC) {
        return false;
    }

    const bool pressed = (draw.itemState & ODS_SELECTED) != 0;
    const bool disabled = (draw.itemState & ODS_DISABLED) != 0;
    const auto colors = ButtonColors(visual, pressed);
    const auto& p = MidnightPalette();

    HBRUSH brush = CreateSolidBrush(colors.background);
    HPEN pen = CreatePen(PS_SOLID, 1, colors.border);

    if (!brush || !pen) {
        if (brush) DeleteObject(brush);
        if (pen) DeleteObject(pen);
        return false;
    }

    HGDIOBJ oldBrush = SelectObject(draw.hDC, brush);
    HGDIOBJ oldPen = SelectObject(draw.hDC, pen);

    RECT rc = draw.rcItem;
    RoundRect(
        draw.hDC,
        rc.left,
        rc.top,
        rc.right,
        rc.bottom,
        10,
        10
    );

    SelectObject(draw.hDC, oldBrush);
    SelectObject(draw.hDC, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);

    SetBkMode(draw.hDC, TRANSPARENT);
    SetTextColor(draw.hDC, disabled ? p.muted : colors.text);

    std::wstring ownedText{text};
    DrawTextW(
        draw.hDC,
        ownedText.data(),
        static_cast<int>(ownedText.size()),
        &rc,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX
    );

    if ((draw.itemState & ODS_FOCUS) != 0) {
        RECT focus = rc;
        InflateRect(&focus, -4, -4);
        DrawFocusRect(draw.hDC, &focus);
    }

    return true;
}

}
