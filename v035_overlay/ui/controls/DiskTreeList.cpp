#include "ui/controls/DiskTreeList.h"

#include "ui/Theme.h"

#include <algorithm>
#include <cmath>
#include <cwchar>

namespace dpop::ui {
namespace {
constexpr int kNameColumn = 0;
constexpr int kSizeColumn = 1;
constexpr int kAllocatedColumn = 2;
constexpr int kFilesColumn = 3;
constexpr int kFoldersColumn = 4;
constexpr int kPercentColumn = 5;
constexpr int kModifiedColumn = 6;
constexpr int kIndentPixels = 18;
constexpr int kExpanderPixels = 16;

void AddColumn(HWND list, int index, const wchar_t* text, int width, int fmt = LVCFMT_LEFT) {
    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;
    column.pszText = const_cast<LPWSTR>(text);
    column.cx = width;
    column.iSubItem = index;
    column.fmt = fmt;
    ListView_InsertColumn(list, index, &column);
}

std::wstring PercentText(double value) {
    wchar_t buffer[32]{};
    swprintf_s(buffer, L"%.1f%%", std::clamp(value, 0.0, 100.0));
    return buffer;
}
} // namespace

bool DiskTreeList::Create(HWND parent, int controlId, HFONT font) {
    Destroy();
    hwnd_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
        0, 0, 100, 100,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
        GetModuleHandleW(nullptr),
        nullptr);
    if (!hwnd_) return false;

    ListView_SetExtendedListViewStyle(
        hwnd_,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP | LVS_EX_GRIDLINES);
    if (font) SendMessageW(hwnd_, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    const auto& palette = MidnightPalette();
    ListView_SetBkColor(hwnd_, palette.control);
    ListView_SetTextBkColor(hwnd_, palette.control);
    ListView_SetTextColor(hwnd_, palette.text);
    SetWindowTheme(hwnd_, L"DarkMode_Explorer", nullptr);
    if (HWND header = ListView_GetHeader(hwnd_)) SetWindowTheme(header, L"DarkMode_Explorer", nullptr);
    InsertColumns();
    return true;
}

void DiskTreeList::Destroy() noexcept {
    rows_.clear();
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void DiskTreeList::InsertColumns() {
    while (ListView_DeleteColumn(hwnd_, 0)) {}
    AddColumn(hwnd_, kNameColumn, L"Имя", 330);
    AddColumn(hwnd_, kSizeColumn, L"Размер", 116, LVCFMT_RIGHT);
    AddColumn(hwnd_, kAllocatedColumn, L"Занято", 116, LVCFMT_RIGHT);
    AddColumn(hwnd_, kFilesColumn, L"Файлы", 76, LVCFMT_RIGHT);
    AddColumn(hwnd_, kFoldersColumn, L"Папки", 76, LVCFMT_RIGHT);
    AddColumn(hwnd_, kPercentColumn, L"% родителя", 150, LVCFMT_RIGHT);
    AddColumn(hwnd_, kModifiedColumn, L"Изменено", 155);
}

std::wstring DiskTreeList::DisplayName(const DiskTreeRow& row) const {
    std::wstring text(row.depth * 3, L' ');
    if (row.directory && row.hasChildren) text += row.expanded ? L"▼ " : L"▶ ";
    else if (row.directory) text += L"  ";
    if (row.protectedPath) text += L"⚠ ";
    text += row.name;
    if (row.incomplete) text += L"  [частично]";
    return text;
}

void DiskTreeList::SetRows(std::vector<DiskTreeRow> rows) {
    rows_ = std::move(rows);
    ListView_DeleteAllItems(hwnd_);
    for (std::size_t i = 0; i < rows_.size(); ++i) {
        const auto& row = rows_[i];
        std::wstring name = DisplayName(row);
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = static_cast<int>(i);
        item.pszText = name.data();
        item.lParam = static_cast<LPARAM>(row.nodeId);
        const int inserted = ListView_InsertItem(hwnd_, &item);
        if (inserted < 0) continue;

        auto set = [&](int column, const std::wstring& value) {
            ListView_SetItemText(hwnd_, inserted, column, const_cast<LPWSTR>(value.c_str()));
        };
        set(kSizeColumn, row.sizeText);
        set(kAllocatedColumn, row.allocatedText);
        set(kFilesColumn, row.filesText);
        set(kFoldersColumn, row.dirsText);
        set(kPercentColumn, PercentText(row.parentPercent));
        set(kModifiedColumn, row.modifiedText);
    }
}

const DiskTreeRow* DiskTreeList::RowAt(int index) const noexcept {
    if (index < 0 || index >= static_cast<int>(rows_.size())) return nullptr;
    return &rows_[static_cast<std::size_t>(index)];
}

int DiskTreeList::SelectedIndex() const noexcept {
    if (!hwnd_) return -1;
    return ListView_GetNextItem(hwnd_, -1, LVNI_SELECTED);
}

const DiskTreeRow* DiskTreeList::SelectedRow() const noexcept {
    return RowAt(SelectedIndex());
}

dpop::disk::DiskNodeId DiskTreeList::ExpanderNodeFromClick(const NMITEMACTIVATE& click) const noexcept {
    const auto* row = RowAt(click.iItem);
    if (!row || click.iSubItem != kNameColumn || !row->directory || !row->hasChildren) return 0;
    RECT cell{};
    if (!ListView_GetSubItemRect(hwnd_, click.iItem, kNameColumn, LVIR_BOUNDS, &cell)) return 0;
    const int left = cell.left + 4 + static_cast<int>(row->depth) * kIndentPixels;
    const int right = left + kExpanderPixels + 8;
    return (click.ptAction.x >= left && click.ptAction.x <= right) ? row->nodeId : 0;
}

bool DiskTreeList::HandleCustomDraw(const NMLVCUSTOMDRAW& draw, LRESULT& result) const noexcept {
    if (!hwnd_ || draw.nmcd.hdr.hwndFrom != hwnd_) return false;
    if (draw.nmcd.dwDrawStage == CDDS_PREPAINT) {
        result = CDRF_NOTIFYITEMDRAW;
        return true;
    }
    if (draw.nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
        result = CDRF_NOTIFYSUBITEMDRAW;
        return true;
    }
    if (draw.nmcd.dwDrawStage != (CDDS_ITEMPREPAINT | CDDS_SUBITEM) || draw.iSubItem != kPercentColumn) {
        return false;
    }

    const int index = static_cast<int>(draw.nmcd.dwItemSpec);
    const auto* row = RowAt(index);
    if (!row) return false;

    RECT cell{};
    if (!ListView_GetSubItemRect(hwnd_, index, kPercentColumn, LVIR_BOUNDS, &cell)) return false;
    const auto& palette = MidnightPalette();
    HDC dc = draw.nmcd.hdc;

    HBRUSH background = CreateSolidBrush(palette.control);
    FillRect(dc, &cell, background);
    DeleteObject(background);

    RECT inner = cell;
    InflateRect(&inner, -3, -4);
    const double percent = std::clamp(row->parentPercent, 0.0, 100.0);
    RECT fill = inner;
    fill.right = fill.left + static_cast<LONG>(std::llround((inner.right - inner.left) * percent / 100.0));
    if (fill.right > fill.left) {
        HBRUSH accent = CreateSolidBrush(palette.accent);
        FillRect(dc, &fill, accent);
        DeleteObject(accent);
    }

    std::wstring text = PercentText(percent);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, palette.text);
    RECT textRect = cell;
    textRect.right -= 7;
    DrawTextW(dc, text.data(), static_cast<int>(text.size()), &textRect,
              DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    result = CDRF_SKIPDEFAULT;
    return true;
}

} // namespace dpop::ui
