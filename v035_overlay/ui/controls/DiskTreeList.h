#pragma once

#include "modules/DiskAnalyzer.h"

#include <windows.h>
#include <commctrl.h>

#include <cstdint>
#include <string>
#include <vector>

namespace dpop::ui {

struct DiskTreeRow {
    dpop::disk::DiskNodeId nodeId{};
    unsigned depth{};
    bool directory{};
    bool hasChildren{};
    bool expanded{};
    bool protectedPath{};
    bool incomplete{};
    std::wstring name;
    std::wstring sizeText;
    std::wstring allocatedText;
    std::wstring filesText;
    std::wstring dirsText;
    double parentPercent{};
    std::wstring modifiedText;
};

class DiskTreeList {
public:
    bool Create(HWND parent, int controlId, HFONT font);
    void Destroy() noexcept;
    HWND Hwnd() const noexcept { return hwnd_; }

    void SetRows(std::vector<DiskTreeRow> rows);
    const std::vector<DiskTreeRow>& Rows() const noexcept { return rows_; }
    const DiskTreeRow* RowAt(int index) const noexcept;
    int SelectedIndex() const noexcept;
    const DiskTreeRow* SelectedRow() const noexcept;

    // Returns the clicked node id when the click falls on its tree expander.
    dpop::disk::DiskNodeId ExpanderNodeFromClick(const NMITEMACTIVATE& click) const noexcept;

    // Parent forwards NM_CUSTOMDRAW here. Returns true when result was filled.
    bool HandleCustomDraw(const NMLVCUSTOMDRAW& draw, LRESULT& result) const noexcept;

private:
    std::wstring DisplayName(const DiskTreeRow& row) const;
    void InsertColumns();

    HWND hwnd_{};
    std::vector<DiskTreeRow> rows_;
};

} // namespace dpop::ui
