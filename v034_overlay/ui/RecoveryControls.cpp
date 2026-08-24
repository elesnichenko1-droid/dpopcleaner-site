#include "ui/RecoveryControls.h"

#include "ui/Theme.h"

#include <algorithm>
#include <commdlg.h>
#include <array>
#include <shellapi.h>
#include <shlobj.h>
#include <string>
#include <uxtheme.h>

namespace dpop::ui {

RecoveryFonts::~RecoveryFonts() {
    Destroy();
}

bool RecoveryFonts::Create() noexcept {
    Destroy();
    title = CreateUiFont(18, FW_BOLD);
    section = CreateUiFont(11, FW_SEMIBOLD);
    body = CreateUiFont(10, FW_NORMAL);
    smallFont = CreateUiFont(9, FW_NORMAL);
    if (!title || !section || !body || !smallFont) {
        Destroy();
        return false;
    }
    return true;
}

void RecoveryFonts::Destroy() noexcept {
    for (HFONT* font : {&title, &section, &body, &smallFont}) {
        if (*font) DeleteObject(*font);
        *font = nullptr;
    }
}

namespace {
HWND CreateChild(
    DWORD exStyle,
    const wchar_t* className,
    std::wstring_view text,
    DWORD style,
    HWND parent,
    int id
) noexcept {
    const std::wstring owned{text};
    return CreateWindowExW(
        exStyle,
        className,
        owned.c_str(),
        WS_CHILD | WS_VISIBLE | style,
        0, 0, 0, 0,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr
    );
}
}

HWND CreateDarkEdit(HWND parent, int id, std::wstring_view text, bool readOnly) noexcept {
    DWORD style = ES_LEFT | ES_AUTOHSCROLL | WS_TABSTOP;
    if (readOnly) style |= ES_READONLY;
    HWND edit = CreateChild(WS_EX_CLIENTEDGE, L"EDIT", text, style, parent, id);
    ApplyDarkEdit(edit);
    return edit;
}

HWND CreateCheckBox(HWND parent, int id, std::wstring_view text, bool checked) noexcept {
    HWND box = CreateChild(0, L"BUTTON", text, BS_AUTOCHECKBOX | WS_TABSTOP, parent, id);
    if (box) {
        SetWindowTheme(box, L"DarkMode_Explorer", nullptr);
        SendMessageW(box, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    return box;
}

HWND CreateDropDown(HWND parent, int id) noexcept {
    HWND combo = CreateChild(
        0,
        WC_COMBOBOXW,
        L"",
        CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
        parent,
        id
    );
    if (combo) SetWindowTheme(combo, L"DarkMode_Explorer", nullptr);
    return combo;
}

HWND CreateProgress(HWND parent, int id) noexcept {
    HWND progress = CreateChild(
        0,
        PROGRESS_CLASSW,
        L"",
        PBS_SMOOTH,
        parent,
        id
    );
    if (progress) {
        SendMessageW(progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
        const auto& p = MidnightPalette();
        SendMessageW(progress, PBM_SETBARCOLOR, 0, p.accent);
        SendMessageW(progress, PBM_SETBKCOLOR, 0, p.control);
    }
    return progress;
}

void SetChecked(HWND checkbox, bool checked) noexcept {
    if (checkbox) SendMessageW(checkbox, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

bool IsChecked(HWND checkbox) noexcept {
    return checkbox && SendMessageW(checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void SetControlText(HWND control, std::wstring_view text) noexcept {
    if (!control) return;
    const std::wstring owned{text};
    SetWindowTextW(control, owned.c_str());
}

std::wstring GetControlText(HWND control) {
    if (!control) return {};
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) return {};
    std::wstring text(static_cast<std::size_t>(length + 1), L'\0');
    GetWindowTextW(control, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

void ResetList(HWND list, bool checkboxes) noexcept {
    if (!list) return;
    ListView_DeleteAllItems(list);
    HWND header = ListView_GetHeader(list);
    int count = header ? Header_GetItemCount(header) : 0;
    while (count-- > 0) ListView_DeleteColumn(list, 0);

    DWORD ex = LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_GRIDLINES;
    if (checkboxes) ex |= LVS_EX_CHECKBOXES;
    ListView_SetExtendedListViewStyle(list, ex);
}

void AddListColumn(HWND list, int column, std::wstring_view title, int width) noexcept {
    if (!list) return;
    std::wstring owned{title};
    LVCOLUMNW col{};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.pszText = owned.data();
    col.cx = width;
    col.iSubItem = column;
    ListView_InsertColumn(list, column, &col);
}

int AddListRow(HWND list, const std::vector<std::wstring>& columns) noexcept {
    if (!list || columns.empty()) return -1;
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = ListView_GetItemCount(list);
    item.pszText = const_cast<wchar_t*>(columns[0].c_str());
    const int index = ListView_InsertItem(list, &item);
    for (int c = 1; c < static_cast<int>(columns.size()); ++c) {
        ListView_SetItemText(
            list,
            index,
            c,
            const_cast<wchar_t*>(columns[static_cast<std::size_t>(c)].c_str())
        );
    }
    return index;
}

int SelectedListIndex(HWND list) noexcept {
    return list ? ListView_GetNextItem(list, -1, LVNI_SELECTED) : -1;
}

std::vector<int> SelectedListIndices(HWND list) {
    std::vector<int> out;
    if (!list) return out;
    int index = -1;
    while ((index = ListView_GetNextItem(list, index, LVNI_SELECTED)) != -1) out.push_back(index);
    return out;
}

std::vector<int> CheckedListIndices(HWND list) {
    std::vector<int> out;
    if (!list) return out;
    const int count = ListView_GetItemCount(list);
    for (int i = 0; i < count; ++i) {
        if (ListView_GetCheckState(list, i)) out.push_back(i);
    }
    return out;
}

void DrawPanel(HDC dc, const RECT& rect, bool accent) noexcept {
    const auto& p = MidnightPalette();
    HBRUSH brush = CreateSolidBrush(p.control);
    HPEN pen = CreatePen(PS_SOLID, accent ? 2 : 1, accent ? p.accent : p.border);
    if (!brush || !pen) {
        if (brush) DeleteObject(brush);
        if (pen) DeleteObject(pen);
        return;
    }
    HGDIOBJ oldBrush = SelectObject(dc, brush);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, 10, 10);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(brush);
    DeleteObject(pen);
}

void DrawPanelTitle(HDC dc, const RECT& rect, std::wstring_view text, HFONT font) noexcept {
    const auto& p = MidnightPalette();
    RECT title{rect.left + 14, rect.top + 8, rect.right - 14, rect.top + 32};
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, p.text);
    std::wstring owned{text};
    DrawTextW(dc, owned.data(), static_cast<int>(owned.size()), &title, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(dc, old);
}

void DrawPageHeading(
    HDC dc,
    int x,
    int y,
    std::wstring_view title,
    std::wstring_view subtitle,
    HFONT titleFont,
    HFONT bodyFont
) noexcept {
    const auto& p = MidnightPalette();
    int right = x + 320;
    RECT client{};
    if (HWND hwnd = WindowFromDC(dc); hwnd && GetClientRect(hwnd, &client)) {
        right = std::max(x + 120, static_cast<int>(client.right) - 18);
    } else {
        RECT clip{};
        if (GetClipBox(dc, &clip) != ERROR) right = std::max(x + 120, static_cast<int>(clip.right) - 18);
    }
    RECT a{x, y, right, y + 34};
    RECT b{x, y + 32, right, y + 82};
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, p.text);
    HGDIOBJ old = SelectObject(dc, titleFont);
    std::wstring ownedTitle{title};
    DrawTextW(dc, ownedTitle.data(), static_cast<int>(ownedTitle.size()), &a, DT_LEFT | DT_TOP | DT_NOPREFIX);
    SelectObject(dc, bodyFont);
    SetTextColor(dc, p.muted);
    std::wstring ownedSub{subtitle};
    DrawTextW(dc, ownedSub.data(), static_cast<int>(ownedSub.size()), &b, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(dc, old);
}

void DrawMeter(HDC dc, const RECT& rect, unsigned percent) noexcept {
    const auto& p = MidnightPalette();
    const unsigned clamped = std::min(percent, 100u);
    HBRUSH bg = CreateSolidBrush(p.background);
    HBRUSH fill = CreateSolidBrush(p.accent);
    if (!bg || !fill) {
        if (bg) DeleteObject(bg);
        if (fill) DeleteObject(fill);
        return;
    }
    FillRect(dc, &rect, bg);
    RECT used = rect;
    used.right = used.left + static_cast<int>((rect.right - rect.left) * clamped / 100u);
    FillRect(dc, &used, fill);
    DeleteObject(bg);
    DeleteObject(fill);
}

std::filesystem::path ChooseFolder(HWND owner, std::wstring_view title) {
    std::wstring owned{title};
    BROWSEINFOW bi{};
    bi.hwndOwner = owner;
    bi.lpszTitle = owned.c_str();
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return {};
    wchar_t path[32768]{};
    std::filesystem::path result;
    if (SHGetPathFromIDListW(pidl, path)) result = path;
    CoTaskMemFree(pidl);
    return result;
}

std::filesystem::path ChooseFile(HWND owner, std::wstring_view title) {
    wchar_t file[32768]{};
    std::wstring owned{title};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrTitle = owned.c_str();
    ofn.lpstrFile = file;
    ofn.nMaxFile = static_cast<DWORD>(std::size(file));
    ofn.lpstrFilter = L"Все файлы\0*.*\0\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    return GetOpenFileNameW(&ofn) ? std::filesystem::path(file) : std::filesystem::path{};
}

void OpenPathInExplorer(HWND owner, const std::filesystem::path& path, bool selectFile) noexcept {
    if (path.empty()) return;
    std::wstring params;
    if (selectFile) params = L"/select,\"" + path.wstring() + L"\"";
    else params = L"\"" + path.wstring() + L"\"";
    ShellExecuteW(owner, L"open", L"explorer.exe", params.c_str(), nullptr, SW_SHOWNORMAL);
}

bool ConfirmAction(HWND owner, std::wstring_view text, bool strong) noexcept {
    const std::wstring owned{text};
    return MessageBoxW(
        owner,
        owned.c_str(),
        L"DPopCleaner",
        MB_OKCANCEL | MB_ICONWARNING | (strong ? MB_DEFBUTTON2 : 0)
    ) == IDOK;
}

}
