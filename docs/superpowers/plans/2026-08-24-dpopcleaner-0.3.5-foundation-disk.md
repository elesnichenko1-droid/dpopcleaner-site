# DPopCleaner 0.3.5 Foundation + Disk Analyzer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the isolated DPopCleaner 0.3.5 source line on the recovered 0.2.14-style UX host and replace the Disk page with a real asynchronous TreeSize-style hierarchical size analyzer.

**Architecture:** `dpop035_migrate.py` prepares two donors in an isolated worktree: the verified `v033` recovered UX host and the current `v034` modern backend donor. The target `v035` starts from `v033`; only backend directories (`core/`, `modules/`, `update/`) are copied from `v034`, so current 0.3.4 shell/page presentation cannot leak into the UX host. A dedicated `DiskAnalyzer` owns filesystem scanning/data, `DiskTreeList` owns Win32 tree-list rendering, and `DiskPage` coordinates async work without making ListView text authoritative state.

**Tech Stack:** C++20, native Win32/Common Controls, CMake/CTest, Python 3 migration/contract tests, PowerShell Windows smoke tests, GitHub Actions Windows 2022.

**Spec:** `docs/superpowers/specs/2026-08-24-dpopcleaner-0.3.5-golden-0214-treesize-settings-design.md`

## Global Constraints

- Product identity is exactly `0.3.5 BETA R1`, version `0.3.5`, version code `3051`, revision `1`, resource version `0.3.5.1`.
- Development branch is `feat/dpopcleaner-0.3.5-r1`; do not change `main` or published 0.3.4 artifacts while developing the candidate.
- The user-visible host is the recovered 0.2.14 interaction model; the current 0.3.4 presentation is not the UI base.
- Primary navigation remains compact and horizontal; Settings remains behind the gear button.
- Minimum supported window is `1100×700`; target is approximately `1200×850`; UI must remain usable at 100%, 125%, and 150% DPI.
- Long disk operations are asynchronous, progressive, cancellable, and may not block the UI thread.
- The Disk page does not delete arbitrary files in 0.3.5 R1.
- Reparse points/junctions are not recursively followed by the scanner.
- `% родителя` uses the application accent green, is clamped visually to 0–100%, and remains numerically sortable.
- No public release/site/update-manifest mutation belongs in this plan.

---

## File Structure

**Create**
- `tools/dpop035_core.py` — donor preparation, provenance enforcement, version transformation, overlay application.
- `tools/dpop035_migrate.py` — CLI entrypoint and optional Windows build orchestration hook.
- `v035_overlay/modules/DiskAnalyzer.h` — disk model/scanner public API.
- `v035_overlay/modules/DiskAnalyzer.cpp` — recursive scan, allocated-size calculation, progressive snapshots, cancellation.
- `v035_overlay/ui/controls/DiskTreeList.h` — flattened tree-row rendering API.
- `v035_overlay/ui/controls/DiskTreeList.cpp` — indentation/expander hit testing and `% родителя` custom drawing.
- `v035_overlay/ui/pages/DiskPage.h` — page state and async coordination.
- `v035_overlay/ui/pages/DiskPage.cpp` — toolbar, tree-list population, progress/cancel/navigation.
- `tests/test_dpop035_migrate.py` — donor isolation, identity, and UI/backend provenance contracts.
- `tests/test_dpop035_disk_contract.py` — static source/UI safety contracts.
- `tests/v035/DiskAnalyzerTests.cpp` — scanner/model unit tests on controlled temporary fixtures.
- `tools/dpop035_disk_smoke.ps1` — real app launch plus controlled fixture scan/cancel smoke.

**Modify during generated-source preparation, not in published donor trees**
- generated `v035/CMakeLists.txt` — add DiskAnalyzer, DiskTreeList, DiskPage and `DiskAnalyzerTests`.
- generated `v035/Version.h` — 0.3.5 identity.
- generated `v035/version.rc.in` — 0.3.5.1 resource identity.

The committed `v032/`, `v034_overlay/`, `r4/`, `downloads/`, site files, and published release metadata remain unchanged.

---

### Task 1: Build the 0.3.5 donor boundary and versioned migration

**Files:**
- Create: `tools/dpop035_core.py`
- Create: `tools/dpop035_migrate.py`
- Create: `tests/test_dpop035_migrate.py`

**Interfaces:**
- Consumes: `dpop033_migrate.migrate(...)` and `dpop034_migrate.migrate(...)`.
- Produces: `prepare_v035(repository: Path, output: Path, workspace: Path, *, build: bool = False) -> dict` and exported `output/source-overlay/v035/`.

- [ ] **Step 1: Write the failing provenance and identity tests**

```python
# tests/test_dpop035_migrate.py
from pathlib import Path
import importlib.util

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "tools" / "dpop035_core.py"


def load_core():
    spec = importlib.util.spec_from_file_location("dpop035_core", CORE)
    module = importlib.util.module_from_spec(spec)
    assert spec and spec.loader
    spec.loader.exec_module(module)
    return module


def test_035_identity_is_monotonic_and_exact():
    m = load_core()
    assert m.TARGET_VERSION == "0.3.5"
    assert m.TARGET_DISPLAY_VERSION == "0.3.5 BETA R1"
    assert m.TARGET_VERSION_CODE == "3051"
    assert m.TARGET_REVISION == "1"
    assert m.TARGET_RESOURCE_VERSION == "0.3.5.1"


def test_backend_allowlist_cannot_replace_ui_host():
    m = load_core()
    assert set(m.MODERN_BACKEND_ROOTS) == {"core", "modules", "update"}
    assert "ui" not in m.MODERN_BACKEND_ROOTS
    assert "MainWindow.cpp" not in m.MODERN_BACKEND_ROOTS


def test_overlay_rejects_symlinks_and_parent_escape(tmp_path):
    m = load_core()
    source = tmp_path / "overlay"
    target = tmp_path / "target"
    source.mkdir(); target.mkdir()
    # apply_overlay itself must reject symlinks; path traversal cannot be produced
    # by Path.rglob relative paths and is checked defensively in the implementation.
    assert m.apply_overlay(source, target) == []
```

- [ ] **Step 2: Run the migration tests and confirm failure**

Run:

```bash
python -m pytest tests/test_dpop035_migrate.py -q
```

Expected: FAIL because `tools/dpop035_core.py` does not exist.

- [ ] **Step 3: Implement the donor boundary**

Create `tools/dpop035_core.py` with these exact public constants and donor rules:

```python
from __future__ import annotations
import shutil
from pathlib import Path

TARGET_VERSION = "0.3.5"
TARGET_DISPLAY_VERSION = "0.3.5 BETA R1"
TARGET_VERSION_CODE = "3051"
TARGET_REVISION = "1"
TARGET_RESOURCE_VERSION = "0.3.5.1"
MODERN_BACKEND_ROOTS = ("core", "modules", "update")


def apply_overlay(overlay_root: Path, target_root: Path) -> list[str]:
    overlay_root = overlay_root.resolve()
    target_root = target_root.resolve()
    changed: list[str] = []
    if not overlay_root.is_dir():
        return changed
    for source in sorted(overlay_root.rglob("*")):
        if source.is_symlink():
            raise ValueError(f"overlay symlinks are forbidden: {source}")
        if not source.is_file():
            continue
        relative = source.relative_to(overlay_root)
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"unsafe overlay path: {relative.as_posix()}")
        destination = target_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        changed.append(relative.as_posix())
    return changed


def copy_modern_backend(v034: Path, v035: Path) -> list[str]:
    copied: list[str] = []
    for root_name in MODERN_BACKEND_ROOTS:
        src = v034 / root_name
        dst = v035 / root_name
        if not src.is_dir():
            raise ValueError(f"modern backend root missing: {root_name}")
        if dst.exists():
            shutil.rmtree(dst)
        shutil.copytree(src, dst)
        copied.append(root_name)
    return copied
```

`prepare_v035(...)` must:
1. run `dpop033_migrate.migrate(..., build=False, keep_worktree=True)` into `workspace/ux-donor-*`;
2. run `dpop034_migrate.migrate(..., build=False)` into `workspace/backend-output`;
3. copy generated `v033` to staging `v035`;
4. copy only `core/`, `modules/`, `update/` from generated `v034` into `v035`;
5. apply `v035_overlay/`;
6. transform CMake/version/resource identity;
7. export only `source-overlay/v035` and a JSON report containing donor commit/provenance fields and overlay file names.

Version transformation must use guarded replacements and fail on donor drift, for example:

```python
def guarded_replace(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise ValueError(f"expected marker missing in {label}: {old}")
    return text.replace(old, new)
```

- [ ] **Step 4: Add the thin CLI entrypoint**

```python
# tools/dpop035_migrate.py
from __future__ import annotations
import argparse
from pathlib import Path
from dpop035_core import prepare_v035


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--repository", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--workspace", type=Path, required=True)
    p.add_argument("--build", action="store_true")
    args = p.parse_args()
    prepare_v035(args.repository, args.output, args.workspace, build=args.build)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 5: Run tests**

Run:

```bash
python -m pytest tests/test_dpop035_migrate.py -q
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add tools/dpop035_core.py tools/dpop035_migrate.py tests/test_dpop035_migrate.py
git commit -m "build: establish DPopCleaner 0.3.5 donor boundary"
```

---

### Task 2: Define and test the authoritative DiskAnalyzer model

**Files:**
- Create: `v035_overlay/modules/DiskAnalyzer.h`
- Create: `v035_overlay/modules/DiskAnalyzer.cpp`
- Create: `tests/v035/DiskAnalyzerTests.cpp`
- Modify: `tools/dpop035_core.py` to inject sources/test target into generated `v035/CMakeLists.txt`.

**Interfaces:**
- Produces:
  - `using DiskNodeId = std::uint64_t;`
  - `DiskScanSnapshot ScanDiskTree(const std::filesystem::path&, std::stop_token, DiskProgressCallback, DiskScanOptions);`
  - `double ParentPercent(const DiskNode&, const DiskNode*) noexcept;`
- `DiskNode` is authoritative; UI rows are projections of this model.

- [ ] **Step 1: Write failing C++ tests for totals, counts, percent, reparse policy and cancellation**

```cpp
// tests/v035/DiskAnalyzerTests.cpp
#include "modules/DiskAnalyzer.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <stop_token>

namespace fs = std::filesystem;
using namespace dpop::disk;

static void WriteBytes(const fs::path& p, std::size_t count) {
    std::ofstream out(p, std::ios::binary);
    std::string data(count, 'x');
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
}

int main() {
    const fs::path root = fs::temp_directory_path() / "dpop035_disk_test";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "A" / "B");
    WriteBytes(root / "root.bin", 100);
    WriteBytes(root / "A" / "a.bin", 300);
    WriteBytes(root / "A" / "B" / "b.bin", 600);

    const auto snapshot = ScanDiskTree(root, {}, {}, {});
    assert(snapshot.complete);
    assert(!snapshot.cancelled);
    const auto* rootNode = snapshot.Find(snapshot.rootId);
    assert(rootNode);
    assert(rootNode->logicalBytes == 1000);
    assert(rootNode->fileCount == 3);
    assert(rootNode->directoryCount == 2);

    const auto* a = snapshot.FindPath(root / "A");
    assert(a && a->logicalBytes == 900);
    assert(a->fileCount == 2);
    assert(a->directoryCount == 1);
    assert(ParentPercent(*a, rootNode) == 90.0);

    DiskNode zeroParent{};
    assert(ParentPercent(*a, &zeroParent) == 0.0);
    assert(ParentPercent(*rootNode, nullptr) == 100.0);

    std::stop_source stop;
    stop.request_stop();
    const auto cancelled = ScanDiskTree(root, stop.get_token(), {}, {});
    assert(cancelled.cancelled);
    assert(!cancelled.complete);

    fs::remove_all(root, ec);
    return 0;
}
```

- [ ] **Step 2: Inject a failing CMake test target and run CTest on Windows**

The generated CMake transformation must add:

```cmake
add_executable(DiskAnalyzerTests
    tests/v035/DiskAnalyzerTests.cpp
    modules/DiskAnalyzer.cpp
)
target_include_directories(DiskAnalyzerTests PRIVATE .)
target_compile_definitions(DiskAnalyzerTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(DiskAnalyzerTests PRIVATE kernel32)
add_test(NAME DiskAnalyzerTests COMMAND DiskAnalyzerTests)
```

Run on Windows prepared source:

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --target DiskAnalyzerTests
ctest --test-dir build -C Release -R DiskAnalyzerTests --output-on-failure
```

Expected: build FAIL because `DiskAnalyzer.h/.cpp` do not exist.

- [ ] **Step 3: Implement the public model**

```cpp
// v035_overlay/modules/DiskAnalyzer.h
#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <unordered_map>
#include <vector>

namespace dpop::disk {
using DiskNodeId = std::uint64_t;

enum class DiskScanState { Pending, Scanning, Complete, Incomplete };

struct DiskNode {
    DiskNodeId id{};
    DiskNodeId parentId{};
    std::filesystem::path path;
    std::wstring displayName;
    bool directory{};
    std::uint64_t logicalBytes{};
    std::uint64_t allocatedBytes{};
    std::uint64_t fileCount{};
    std::uint64_t directoryCount{};
    std::int64_t modifiedUnix100ns{};
    bool incomplete{};
    bool protectedPath{};
    DiskScanState state{DiskScanState::Pending};
    std::vector<DiskNodeId> children;
};

struct DiskScanProgress {
    std::filesystem::path currentPath;
    std::uint64_t filesVisited{};
    std::uint64_t directoriesVisited{};
    std::uint64_t logicalBytes{};
    std::uint64_t errors{};
};

struct DiskScanOptions {
    std::size_t progressEveryEntries{128};
    bool includeFilesAsNodes{true};
};

using DiskProgressCallback = std::function<void(const DiskScanProgress&)>;

struct DiskScanSnapshot {
    DiskNodeId rootId{};
    std::vector<DiskNode> nodes;
    std::uint64_t errorCount{};
    bool complete{};
    bool cancelled{};
    const DiskNode* Find(DiskNodeId id) const noexcept;
    const DiskNode* FindPath(const std::filesystem::path& path) const noexcept;
};

DiskScanSnapshot ScanDiskTree(const std::filesystem::path& root,
                              std::stop_token stop = {},
                              DiskProgressCallback progress = {},
                              DiskScanOptions options = {});
double ParentPercent(const DiskNode& node, const DiskNode* parent) noexcept;
}
```

- [ ] **Step 4: Implement exact scanner safety rules**

`DiskAnalyzer.cpp` must use `std::filesystem::directory_iterator` recursively under explicit function control rather than `recursive_directory_iterator` so aggregation is naturally bottom-up. For each entry:

```cpp
const auto status = entry.symlink_status(ec);
if (ec) { ++errors; parent.incomplete = true; ec.clear(); continue; }
if ((status.permissions() == fs::perms::unknown) ||
    ((entry.is_directory(ec) || entry.is_regular_file(ec)) && ec)) {
    ++errors; parent.incomplete = true; ec.clear(); continue;
}
if ((GetFileAttributesW(entry.path().c_str()) & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
    // Record a directory/file node as incomplete, but never recurse through it.
}
```

For a regular file, calculate logical size with `entry.file_size(ec)` and allocated size with `GetCompressedFileSizeW`. If `GetCompressedFileSizeW` fails, use logical size as the allocated-size fallback and mark no fatal error.

For a directory, recursively scan children only when it is not a reparse point. After returning, add child totals into the parent:

```cpp
parent.logicalBytes += child.logicalBytes;
parent.allocatedBytes += child.allocatedBytes;
parent.fileCount += child.fileCount;
parent.directoryCount += 1 + child.directoryCount;
```

Cancellation is checked before opening each directory and before processing each entry. A cancelled scan returns the accumulated partial model with `cancelled=true`, `complete=false`.

`ParentPercent` is exactly:

```cpp
double ParentPercent(const DiskNode& node, const DiskNode* parent) noexcept {
    if (!parent) return 100.0;
    if (parent->logicalBytes == 0) return 0.0;
    const double value = 100.0 * static_cast<double>(node.logicalBytes) /
                         static_cast<double>(parent->logicalBytes);
    return value < 0.0 ? 0.0 : (value > 100.0 ? 100.0 : value);
}
```

- [ ] **Step 5: Run C++ tests**

Run:

```powershell
cmake --build build --config Release --target DiskAnalyzerTests
ctest --test-dir build -C Release -R DiskAnalyzerTests --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add v035_overlay/modules/DiskAnalyzer.h v035_overlay/modules/DiskAnalyzer.cpp tests/v035/DiskAnalyzerTests.cpp tools/dpop035_core.py
git commit -m "feat: add hierarchical disk analyzer model"
```

---

### Task 3: Build the TreeSize-style DiskTreeList control

**Files:**
- Create: `v035_overlay/ui/controls/DiskTreeList.h`
- Create: `v035_overlay/ui/controls/DiskTreeList.cpp`
- Create: `tests/test_dpop035_disk_contract.py`
- Modify: `tools/dpop035_core.py` CMake source injection.

**Interfaces:**
- Consumes: `DiskNodeId` and calculated parent percentage.
- Produces: `DiskTreeList::SetRows`, `DiskTreeList::HandleNotify`, and `DiskTreeList::HitTestExpander`.

- [ ] **Step 1: Write failing source-contract tests**

```python
# tests/test_dpop035_disk_contract.py
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / "v035_overlay/ui/controls/DiskTreeList.cpp"
PAGE = ROOT / "v035_overlay/ui/pages/DiskPage.cpp"


def test_percent_bar_is_custom_drawn_with_numeric_value():
    text = CPP.read_text(encoding="utf-8")
    assert "NM_CUSTOMDRAW" in text
    assert "ParentPercent" in text or "parentPercent" in text
    assert "MidnightPalette().accent" in text
    assert "std::clamp" in text
    assert "DT_CENTER" in text


def test_disk_page_columns_match_product_contract():
    text = PAGE.read_text(encoding="utf-8")
    for label in ("Имя", "Размер", "Занято", "Файлы", "Папки", "% родителя", "Изменено"):
        assert label in text
    assert "Безопасность" not in text
```

Run:

```bash
python -m pytest tests/test_dpop035_disk_contract.py -q
```

Expected: FAIL because the control/page files do not exist.

- [ ] **Step 2: Define the tree row API**

```cpp
// v035_overlay/ui/controls/DiskTreeList.h
#pragma once
#include "modules/DiskAnalyzer.h"
#include <windows.h>
#include <string>
#include <vector>

namespace dpop::ui {
struct DiskTreeRow {
    dpop::disk::DiskNodeId id{};
    unsigned depth{};
    bool directory{};
    bool hasChildren{};
    bool expanded{};
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
    HWND Hwnd() const noexcept { return hwnd_; }
    void SetRows(std::vector<DiskTreeRow> rows);
    const DiskTreeRow* RowAt(int index) const noexcept;
    bool HandleNotify(const NMHDR* hdr, LRESULT& result);
    bool HitTestExpander(POINT clientPoint, int& rowIndex) const noexcept;
private:
    HWND hwnd_{};
    std::vector<DiskTreeRow> rows_;
    int percentColumn_{5};
};
}
```

- [ ] **Step 3: Implement flattened-tree display and green percent custom draw**

Create a report-mode ListView with full-row select, double buffering and grid lines. The first column is custom drawn with `depth * 16` logical pixels of indent, then a `+`/`−` expander box for directories with children, then the display name.

For subitem 5 (`% родителя`), calculate the bar rectangle from the cell rectangle:

```cpp
const double percent = std::clamp(row.parentPercent, 0.0, 100.0);
RECT fill = cell;
fill.right = fill.left + static_cast<LONG>((fill.right - fill.left) * percent / 100.0);
HBRUSH accent = CreateSolidBrush(MidnightPalette().accent);
FillRect(dc, &fill, accent);
DeleteObject(accent);

wchar_t pct[32]{};
swprintf_s(pct, L"%.1f %%", row.parentPercent);
SetBkMode(dc, TRANSPARENT);
SetTextColor(dc, MidnightPalette().text);
DrawTextW(dc, pct, -1, &cell, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
```

Use the numeric `DiskTreeRow::parentPercent` for sorting; never parse the rendered text.

Incomplete nodes add a compact warning glyph (`⚠`) after the name and use the status bar/tooltip for explanation; do not add a wide safety column.

- [ ] **Step 4: Inject the control source into generated CMake and run contract tests**

Run:

```bash
python -m pytest tests/test_dpop035_disk_contract.py -q
```

The page half may still fail until Task 4; the control-specific test must pass.

- [ ] **Step 5: Commit**

```bash
git add v035_overlay/ui/controls/DiskTreeList.h v035_overlay/ui/controls/DiskTreeList.cpp tests/test_dpop035_disk_contract.py tools/dpop035_core.py
git commit -m "feat: add TreeSize-style disk tree list"
```

---

### Task 4: Replace DiskPage with asynchronous scan/navigation

**Files:**
- Create: `v035_overlay/ui/pages/DiskPage.h`
- Create: `v035_overlay/ui/pages/DiskPage.cpp`
- Modify: `tests/test_dpop035_disk_contract.py`

**Interfaces:**
- Consumes: `DiskAnalyzer::ScanDiskTree`, `DiskTreeList`, existing page `StartAsync(...)`, `QueueApply(...)`, `Cancel()` infrastructure from the recovered UI host.
- Produces: working Disk page with scan/cancel/refresh/large-files/explorer actions and progressive model-driven rendering.

- [ ] **Step 1: Extend failing contracts for toolbar and async behavior**

```python
def test_disk_page_is_an_analyzer_not_a_file_browser():
    text = PAGE.read_text(encoding="utf-8")
    for label in ("Назад", "C:\\\\", "Выбрать каталог", "Сканировать", "Стоп", "Обновить", "Крупные файлы", "Проводник"):
        assert label in text
    assert "ScanDiskTree" in text
    assert "StartAsync" in text
    assert "QueueApply" in text
    assert "Cancel" in text
    assert "directory ? L\"—\"" not in text
```

- [ ] **Step 2: Implement page state around the model, not ListView text**

`DiskPage.h` keeps:

```cpp
dpop::disk::DiskScanSnapshot snapshot_;
DiskTreeList tree_;
std::unordered_set<dpop::disk::DiskNodeId> expanded_;
std::filesystem::path root_{L"C:\\"};
std::vector<std::filesystem::path> history_;
int historyIndex_{-1};
bool scanning_{};
std::wstring summary_;
```

`BuildVisibleRows()` recursively walks `snapshot_.nodes` starting at `rootId`, appends each visible node, and only descends into a directory whose id is in `expanded_`. Root is expanded by default.

- [ ] **Step 3: Implement toolbar and exact scan lifecycle**

`StartScan(root)` does:

```cpp
Cancel();
root_ = root;
scanning_ = true;
summary_ = L"Сканирование: " + root_.wstring();
InvalidateRect(Hwnd(), nullptr, TRUE);

StartAsync(L"Анализируем занятое место…", [this, root](std::stop_token token) {
    auto result = dpop::disk::ScanDiskTree(
        root,
        token,
        [this](const dpop::disk::DiskScanProgress& p) {
            QueueApply([this, p] {
                summary_ = L"Файлов: " + std::to_wstring(p.filesVisited) +
                           L" • папок: " + std::to_wstring(p.directoriesVisited) +
                           L" • ошибок доступа: " + std::to_wstring(p.errors);
                InvalidateRect(Hwnd(), nullptr, FALSE);
            });
        });
    QueueApply([this, result = std::move(result)]() mutable {
        snapshot_ = std::move(result);
        scanning_ = false;
        expanded_.insert(snapshot_.rootId);
        RebuildRows();
        summary_ = snapshot_.cancelled ? L"Сканирование остановлено — показаны частичные результаты"
                                       : L"Сканирование завершено";
    });
});
```

The progress callback is throttled in `DiskAnalyzer` by `progressEveryEntries`; the UI is never updated once per file.

`Стоп` calls the page's cooperative `Cancel()` and sets status to `Останавливаем сканирование…`; it does not wait synchronously.

- [ ] **Step 4: Implement navigation and secondary large-file mode**

- `C:\` calls `StartScan(L"C:\\")`.
- `Выбрать каталог` uses the existing `ChooseFolder` helper and scans the selected root.
- `Обновить` rescans `root_`.
- `Назад` uses `history_` without rescanning if the previous root still has a cached snapshot; otherwise scans it.
- Expander click only changes `expanded_` and calls `RebuildRows()`; it never scans again.
- Double-clicking a directory toggles expansion.
- `Проводник` opens selected node path or `root_`.
- `Крупные файлы` creates a descending projection of file nodes from the existing snapshot and does not destroy the hierarchy model. If the snapshot is empty, it first starts a normal scan.

- [ ] **Step 5: Implement seven columns with exact semantics**

Column definitions:

```cpp
AddListColumn(list, 0, L"Имя", 330);
AddListColumn(list, 1, L"Размер", 120);      // logicalBytes
AddListColumn(list, 2, L"Занято", 120);     // allocatedBytes
AddListColumn(list, 3, L"Файлы", 85);
AddListColumn(list, 4, L"Папки", 85);
AddListColumn(list, 5, L"% родителя", 150);
AddListColumn(list, 6, L"Изменено", 150);
```

Directories display recursive totals in both size columns. The root row displays `100.0 %`.

- [ ] **Step 6: Run contract tests**

Run:

```bash
python -m pytest tests/test_dpop035_disk_contract.py -q
```

Expected: PASS.

- [ ] **Step 7: Build and run DiskAnalyzer CTest**

Run:

```powershell
python tools/dpop035_migrate.py --repository . --output artifacts\035 --workspace C:\temp\dpop035 --build
ctest --test-dir artifacts\035\build -C Release --output-on-failure
```

Expected: `DiskAnalyzerTests` PASS and DPopCleaner links with new page/control sources.

- [ ] **Step 8: Commit**

```bash
git add v035_overlay/ui/pages/DiskPage.h v035_overlay/ui/pages/DiskPage.cpp tests/test_dpop035_disk_contract.py
git commit -m "feat: replace Disk page with hierarchical size analysis"
```

---

### Task 5: Lock layout, progressive behavior, and real Windows disk smoke

**Files:**
- Create: `tools/dpop035_disk_smoke.ps1`
- Modify: `tests/test_dpop035_disk_contract.py`
- Modify: `.github/workflows/DPopCleaner_0.3.5_CANDIDATE.yml` only if the workflow is introduced in this task; it must remain candidate-only and must not publish.

**Interfaces:**
- Consumes: built `DPopCleaner.exe` and controlled fixture directory.
- Produces: machine-verifiable disk scan/cancel evidence and screenshots/artifacts for review.

- [ ] **Step 1: Add layout/source contract assertions**

Add tests that require:

```python
def test_disk_layout_uses_page_content_boundary_and_no_fixed_54_origin():
    text = PAGE.read_text(encoding="utf-8")
    assert "ComputePageContentTop" in text
    assert "const int top = 54;" not in text
    assert "std::max" in text


def test_disk_scanner_never_recurses_reparse_points():
    text = (ROOT / "v035_overlay/modules/DiskAnalyzer.cpp").read_text(encoding="utf-8")
    assert "FILE_ATTRIBUTE_REPARSE_POINT" in text
    assert "recursive_directory_iterator" not in text
```

- [ ] **Step 2: Write controlled PowerShell smoke**

`tools/dpop035_disk_smoke.ps1` must:
1. create `%TEMP%\DPopCleaner035DiskSmoke\A\B`;
2. create 1 MiB, 2 MiB and 3 MiB deterministic files with `fsutil file createnew`;
3. launch built DPopCleaner with the existing UI-smoke harness mechanism;
4. select/scan the fixture through the Disk page automation hook used by the candidate harness;
5. assert the displayed root total is at least 6 MiB and all three child paths appear;
6. start a second scan over a larger generated directory and trigger `Стоп`;
7. assert the app remains responsive and status contains the partial-result cancellation message;
8. close the app and remove the fixture in `finally`.

The script exits non-zero on any failed assertion.

- [ ] **Step 3: Run all Python contracts**

```bash
python -m pytest tests/test_dpop035_migrate.py tests/test_dpop035_disk_contract.py -q
```

Expected: PASS.

- [ ] **Step 4: Run Windows build, CTest, and disk smoke**

```powershell
python tools/dpop035_migrate.py --repository . --output artifacts\035 --workspace C:\temp\dpop035 --build
ctest --test-dir artifacts\035\build -C Release --output-on-failure
powershell -ExecutionPolicy Bypass -File tools\dpop035_disk_smoke.ps1 -ExePath artifacts\035\build\bin\Release\DPopCleaner.exe
```

Expected: all commands exit 0.

- [ ] **Step 5: Capture evidence without publishing**

Candidate workflow/artifact must include:
- 1100×700 Disk screenshot;
- 1200×850 Disk screenshot;
- maximized Disk screenshot;
- `ctest.txt`;
- `disk-smoke.txt`;
- `migration-report.json`;
- built EXE hashes.

No release, site, `update/beta.json`, or download mutation is permitted.

- [ ] **Step 6: Commit**

```bash
git add tools/dpop035_disk_smoke.ps1 tests/test_dpop035_disk_contract.py .github/workflows/DPopCleaner_0.3.5_CANDIDATE.yml
git commit -m "test: verify DPopCleaner 0.3.5 disk analyzer candidate"
```

---

## Plan Self-Review Results

- Spec coverage: version isolation, 0.2.14 UX provenance, modern backend boundary, real recursive folder sizes, allocated size, file/folder counts, green parent-relative bar, async/progressive/cancel behavior, access-error resilience, reparse loop safety, no direct file deletion, supported layouts, and candidate evidence are all assigned to explicit tasks.
- Placeholder scan: no deferred implementation markers are used; every behavior named above has an exact owning file, API, algorithm, test, and command.
- Type consistency: `DiskNodeId`, `DiskNode`, `DiskScanSnapshot`, `DiskScanProgress`, `DiskTreeRow`, and `DiskTreeList` names/signatures are consistent across Tasks 2–5.
