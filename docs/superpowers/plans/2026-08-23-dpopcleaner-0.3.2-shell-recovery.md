# DPopCleaner 0.3.2 Shell Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the transitional 0.3.1 R4 shell with the recognizable old-series DPopCleaner shell: horizontal tabs, Settings gear, Midnight theme, session log/status footer, responsive layout, and a functional Overview page, while preserving the verified 0.3.x core.

**Architecture:** Keep the current verified R3/R4 backend modules and updater intact. Build 0.3.2 as an overlay source tree: a thin `app/MainWindow.cpp` delegates to a modular native Win32 shell under `src/ui/`; pure shell/layout/model logic is separated from HWND code so it can be unit-tested. This first plan intentionally restores only the shell and Overview; Cleaning/Memory/Guard/Disk/Applications/Windows/Duplicates/Tools/Zapret functional page bodies get their own follow-up plans.

**Tech Stack:** C++20, Win32, Common Controls, GDI/DWM/UxTheme, CMake 3.24+, MSVC x64, PowerShell, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-08-23-dpopcleaner-0.3.2-ui-recovery-design.md`

## Global Constraints

- Display version is exactly `0.3.2 BETA R1`.
- Version is exactly `0.3.2`; version code is `3021`; revision is `1`.
- R4 remains an untouched rollback point.
- The primary navigation is horizontal with exactly ten tabs: Обзор, Очистка, ОЗУ, DPopGuard, Диск, Приложения, Windows, Дубликаты, Инструменты, Zapret.
- Settings opens from a gear button and is not one of the ten navigation tabs.
- No left vertical navigation and no universal giant text slab.
- Default target client layout is approximately 1200×850; minimum supported window is 1100×700.
- Midnight colors follow the approved spec.
- The site, `update/beta.json`, GitHub Release, and Pages are not modified by this plan.
- No Defender/SmartScreen bypasses or exclusions.
- No Zapret service/strategy starts automatically.
- Long-running later features must not block the UI; this plan establishes the message/event boundary they will use.

---

## File Map Locked for This Plan

### New 0.3.2 overlay files

- `v032/CMakeLists.txt` — full candidate CMake entry point for 0.3.2; adds modular UI/test sources without changing stable root build files.
- `v032/Version.h` — 0.3.2 version constants copied into prepared `src/core/Version.h`.
- `v032/version.rc.in` — 0.3.2 Windows version resource template.
- `v032/MainWindow.cpp` — thin adapter implementing the existing `dpop::ui::Run(HINSTANCE,int)` API and delegating to `dpop::ui::shell::Run`.
- `v032/ui/ShellModel.h/.cpp` — page enum, ten-tab descriptors, current-page state; no HWND dependencies.
- `v032/ui/Layout.h/.cpp` — pure integer rectangle calculation for header/tabs/content/footer and Overview card grid.
- `v032/ui/Theme.h/.cpp` — Midnight palette and font metrics.
- `v032/ui/Controls.h/.cpp` — helpers for buttons, labels, group containers, dark ListView/Edit theming.
- `v032/ui/SessionLog.h/.cpp` — in-memory session events and formatted lines.
- `v032/ui/StatusBar.h/.cpp` — footer/status/log control ownership and layout.
- `v032/ui/Shell.h/.cpp` — top-level Win32 window, header, ten tabs, Settings gear, page host, resize, dispatch.
- `v032/ui/pages/OverviewPage.h/.cpp` — real Overview controls and refresh behavior.
- `v032/ui/pages/SettingsStubPage.h/.cpp` — a deliberately non-claiming shell page that shows only the currently fixed BETA defaults (“Русский”, “Midnight”) and a note that functional settings are not enabled in this shell candidate. It exists only so the approved gear behavior is testable; it does not expose fake toggles.
- `v032/tests/ShellModelTests.cpp` — tab order and Settings exclusion tests.
- `v032/tests/LayoutTests.cpp` — minimum/default/maximized layout invariants.
- `v032/tests/SessionLogTests.cpp` — session-log ordering/format tests.
- `v032/tests/OverviewModelTests.cpp` — Overview derived-value tests.
- `v032/tests/Ui032Policy.Tests.ps1` — static source policy checks for old-series shell identity.
- `scripts/Prepare-032Source.ps1` — calls verified R3 source preparation, overlays 0.3.2 files, and emits overlay inventory.
- `.github/workflows/build-dpopcleaner-0.3.2-dev.yml` — manual non-publishing Windows build/test/screenshot artifact workflow.

### Stable files consumed but not changed in this plan

- `scripts/Prepare-R3Source.ps1`
- `scripts/R3ReleasePolicy.psm1`
- `SystemInfo.h/.cpp`
- `Cleaner.h/.cpp`
- `Applications.h/.cpp`
- `DPopGuard.h/.cpp`
- `ZapretManager.h/.cpp`
- updater/update modules
- R4 source/release/site files

---

### Task 1: Add 0.3.2 Source Overlay and Version Boundary

**Files:**
- Create: `v032/Version.h`
- Create: `v032/version.rc.in`
- Create: `v032/MainWindow.cpp`
- Create: `v032/CMakeLists.txt`
- Create: `scripts/Prepare-032Source.ps1`
- Create: `v032/tests/Ui032Policy.Tests.ps1`
- Test: `v032/tests/Ui032Policy.Tests.ps1`

**Interfaces:**
- Consumes: existing `dpop::ui::Run(HINSTANCE instance, int showCommand)` declared by `app/MainWindow.h`.
- Produces: prepared source tree with `DPOP_VERSION="0.3.2"`, `DPOP_VERSION_CODE=3021`, `DPOP_REVISION=1`; `MainWindow.cpp` delegates to `dpop::ui::shell::Run`.

- [ ] **Step 1: Write the failing overlay-policy test**

Create `v032/tests/Ui032Policy.Tests.ps1` with:

```powershell
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$root = Split-Path -Parent $PSScriptRoot
$version = Get-Content -Raw (Join-Path $root 'Version.h')
$adapter = Get-Content -Raw (Join-Path $root 'MainWindow.cpp')
$cmake = Get-Content -Raw (Join-Path $root 'CMakeLists.txt')

$requiredVersionTokens = @(
  '0.3.2 BETA R1',
  '0.3.2',
  '3021',
  '1'
)
foreach ($token in $requiredVersionTokens) {
  if (-not $version.Contains($token)) { throw "Missing 0.3.2 version token: $token" }
}
if (-not $adapter.Contains('dpop::ui::shell::Run')) {
  throw 'MainWindow adapter must delegate to dpop::ui::shell::Run.'
}
foreach ($source in @(
  'src/ui/Shell.cpp',
  'src/ui/ShellModel.cpp',
  'src/ui/Layout.cpp',
  'src/ui/Theme.cpp',
  'src/ui/Controls.cpp',
  'src/ui/SessionLog.cpp',
  'src/ui/StatusBar.cpp',
  'src/ui/pages/OverviewPage.cpp',
  'src/ui/pages/SettingsStubPage.cpp'
)) {
  if (-not $cmake.Contains($source)) { throw "0.3.2 CMake missing source: $source" }
}
'Ui032Policy PASS'
```

- [ ] **Step 2: Run the policy test and verify it fails**

Run:

```powershell
pwsh -NoProfile -File v032/tests/Ui032Policy.Tests.ps1
```

Expected: FAIL because the overlay files do not exist yet.

- [ ] **Step 3: Add exact version constants and adapter**

Create `v032/Version.h`:

```cpp
#pragma once
namespace dpop::version {
inline constexpr wchar_t kVersion[] = L"0.3.2";
inline constexpr wchar_t kDisplayVersion[] = L"0.3.2 BETA R1";
inline constexpr int kVersionCode = 3021;
inline constexpr int kRevision = 1;
}
```

Create `v032/MainWindow.cpp`:

```cpp
#include "app/MainWindow.h"
#include "ui/Shell.h"

namespace dpop::ui {
int Run(HINSTANCE instance, int showCommand) {
    return shell::Run(instance, showCommand);
}
}
```

Create `v032/version.rc.in` from the stable resource template, changing the numeric version to `0,3,2,1`, `FileVersion` to `0.3.2.1`, and `ProductVersion` to `0.3.2 BETA R1` while leaving the app/updater descriptions driven by the existing CMake variables.

- [ ] **Step 4: Add deterministic source preparation**

Create `scripts/Prepare-032Source.ps1` with this contract:

```powershell
[CmdletBinding()]
param(
  [Parameter(Mandatory)][string]$RepositoryRoot,
  [Parameter(Mandatory)][string]$Destination
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

& (Join-Path $RepositoryRoot 'scripts/Prepare-R3Source.ps1') `
  -RepositoryRoot $RepositoryRoot `
  -Destination $Destination

Copy-Item (Join-Path $RepositoryRoot 'v032/CMakeLists.txt') `
  (Join-Path $Destination 'CMakeLists.txt') -Force
Copy-Item (Join-Path $RepositoryRoot 'v032/MainWindow.cpp') `
  (Join-Path $Destination 'src/app/MainWindow.cpp') -Force
Copy-Item (Join-Path $RepositoryRoot 'v032/Version.h') `
  (Join-Path $Destination 'src/core/Version.h') -Force
Copy-Item (Join-Path $RepositoryRoot 'v032/version.rc.in') `
  (Join-Path $Destination 'resources/version.rc.in') -Force

Copy-Item (Join-Path $RepositoryRoot 'v032/ui') `
  (Join-Path $Destination 'src/ui') -Recurse -Force
Copy-Item (Join-Path $RepositoryRoot 'v032/tests') `
  (Join-Path $Destination 'tests/v032') -Recurse -Force

Get-ChildItem (Join-Path $RepositoryRoot 'v032') -File -Recurse |
  Sort-Object FullName |
  ForEach-Object {
    [pscustomobject]@{
      path = $_.FullName.Substring((Join-Path $RepositoryRoot 'v032').Length + 1).Replace('\','/')
      sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
  } | ConvertTo-Json -Depth 4 |
  Set-Content (Join-Path $Destination 'v032-overlay-inventory.json') -Encoding utf8
```

- [ ] **Step 5: Add 0.3.2 CMake entry point**

Copy the stable `CMakeLists.txt` to `v032/CMakeLists.txt`, set:

```cmake
project(DPopCleaner VERSION 0.3.2 LANGUAGES CXX RC)
```

and replace the single `src/app/MainWindow.cpp` UI implementation list with the adapter plus:

```cmake
    src/ui/Shell.cpp
    src/ui/ShellModel.cpp
    src/ui/Layout.cpp
    src/ui/Theme.cpp
    src/ui/Controls.cpp
    src/ui/SessionLog.cpp
    src/ui/StatusBar.cpp
    src/ui/pages/OverviewPage.cpp
    src/ui/pages/SettingsStubPage.cpp
```

Under `BUILD_TESTING`, add test executables for ShellModel, Layout, SessionLog, and OverviewModel as defined in later tasks.

- [ ] **Step 6: Re-run the overlay policy**

Run:

```powershell
pwsh -NoProfile -File v032/tests/Ui032Policy.Tests.ps1
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add v032/Version.h v032/version.rc.in v032/MainWindow.cpp v032/CMakeLists.txt scripts/Prepare-032Source.ps1 v032/tests/Ui032Policy.Tests.ps1
git commit -m "build: establish DPopCleaner 0.3.2 overlay"
```

---

### Task 2: Define the Ten-Tab Shell Model

**Files:**
- Create: `v032/ui/ShellModel.h`
- Create: `v032/ui/ShellModel.cpp`
- Create: `v032/tests/ShellModelTests.cpp`
- Modify: `v032/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `enum class Page { Overview, Cleaning, Memory, Guard, Disk, Applications, WindowsUpdate, Duplicates, Tools, Zapret, Settings };`
  - `std::span<const TabDescriptor> PrimaryTabs();`
  - `bool IsPrimaryTab(Page page);`
  - `Page PageForCommand(int commandId);`
- Settings is deliberately excluded from `PrimaryTabs()`.

- [ ] **Step 1: Write failing tests**

Create `v032/tests/ShellModelTests.cpp`:

```cpp
#include "ui/ShellModel.h"
#include <cassert>
#include <string_view>

int main() {
    using namespace dpop::ui;
    const auto tabs = PrimaryTabs();
    assert(tabs.size() == 10);

    const std::wstring_view expected[] = {
        L"Обзор", L"Очистка", L"ОЗУ", L"DPopGuard", L"Диск",
        L"Приложения", L"Windows", L"Дубликаты", L"Инструменты", L"Zapret"
    };
    for (std::size_t i = 0; i < tabs.size(); ++i) {
        assert(tabs[i].label == expected[i]);
        assert(IsPrimaryTab(tabs[i].page));
        assert(PageForCommand(tabs[i].commandId) == tabs[i].page);
    }

    assert(!IsPrimaryTab(Page::Settings));
    return 0;
}
```

- [ ] **Step 2: Configure and run only this test; verify failure**

Run:

```powershell
./scripts/Prepare-032Source.ps1 -RepositoryRoot $PWD -Destination "$PWD/build-032-src"
cmake -S build-032-src -B build-032 -A x64 -DBUILD_TESTING=ON
cmake --build build-032 --config Release --target ShellModelTests
ctest --test-dir build-032 -C Release -R ShellModelTests --output-on-failure
```

Expected: configure/build FAIL because `ShellModel` does not exist.

- [ ] **Step 3: Implement the model**

Create `v032/ui/ShellModel.h`:

```cpp
#pragma once
#include <span>
#include <string_view>

namespace dpop::ui {

enum class Page {
    Overview,
    Cleaning,
    Memory,
    Guard,
    Disk,
    Applications,
    WindowsUpdate,
    Duplicates,
    Tools,
    Zapret,
    Settings
};

struct TabDescriptor {
    Page page;
    int commandId;
    std::wstring_view label;
};

std::span<const TabDescriptor> PrimaryTabs();
bool IsPrimaryTab(Page page);
Page PageForCommand(int commandId);

}
```

Implement `v032/ui/ShellModel.cpp` with command IDs 1000..1009 in the approved order and throw no exceptions: unknown IDs return `Page::Overview`.

- [ ] **Step 4: Run the model tests**

Run:

```powershell
cmake --build build-032 --config Release --target ShellModelTests
ctest --test-dir build-032 -C Release -R ShellModelTests --output-on-failure
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add v032/ui/ShellModel.h v032/ui/ShellModel.cpp v032/tests/ShellModelTests.cpp v032/CMakeLists.txt
git commit -m "feat: define DPopCleaner 0.3.2 navigation model"
```

---

### Task 3: Implement Pure Responsive Layout

**Files:**
- Create: `v032/ui/Layout.h`
- Create: `v032/ui/Layout.cpp`
- Create: `v032/tests/LayoutTests.cpp`
- Modify: `v032/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `struct Box { int x; int y; int width; int height; };`
  - `struct ShellLayout { Box header; Box tabs; Box content; Box footer; Box status; Box log; Box support; Box version; std::array<Box,6> overviewCards; std::array<Box,5> overviewActions; };`
  - `ShellLayout ComputeShellLayout(int clientWidth, int clientHeight);`

- [ ] **Step 1: Write failing layout invariants**

Create `v032/tests/LayoutTests.cpp`:

```cpp
#include "ui/Layout.h"
#include <cassert>

static bool inside(const dpop::ui::Box& child, const dpop::ui::Box& parent) {
    return child.x >= parent.x &&
           child.y >= parent.y &&
           child.x + child.width <= parent.x + parent.width &&
           child.y + child.height <= parent.y + parent.height;
}

int main() {
    using namespace dpop::ui;

    for (const auto [w, h] : { std::pair{1100,700}, {1200,850}, {1920,1080} }) {
        const auto l = ComputeShellLayout(w, h);
        assert(l.header.width == w);
        assert(l.tabs.width == w);
        assert(l.content.width > 900);
        assert(l.content.height > 350);
        assert(l.footer.y + l.footer.height == h);
        assert(inside(l.log, l.footer));
        for (const auto& card : l.overviewCards) {
            assert(inside(card, l.content));
            assert(card.width >= 250);
            assert(card.height >= 90);
        }
    }

    const auto normal = ComputeShellLayout(1200, 850);
    assert(normal.overviewCards[0].x < normal.overviewCards[1].x);
    assert(normal.overviewCards[1].x < normal.overviewCards[2].x);
    assert(normal.overviewCards[3].y > normal.overviewCards[0].y);
    return 0;
}
```

- [ ] **Step 2: Run and verify failure**

Run:

```powershell
cmake --build build-032 --config Release --target LayoutTests
ctest --test-dir build-032 -C Release -R LayoutTests --output-on-failure
```

Expected: FAIL until `Layout` exists.

- [ ] **Step 3: Implement layout math**

Use these constants in `Layout.cpp`:

```cpp
constexpr int kMargin = 24;
constexpr int kHeaderHeight = 100;
constexpr int kTabsHeight = 54;
constexpr int kFooterHeight = 150;
constexpr int kGap = 14;
constexpr int kMinCardWidth = 250;
```

`ComputeShellLayout()` clamps the effective size to at least 1100×700 for geometry calculation, creates a full-width header, a tab band immediately below it, a content rectangle between tabs/footer, and a footer anchored to the bottom. Overview cards form a 3×2 grid. Overview actions form a single row when width >= 1180 and wrap to two rows below that threshold.

- [ ] **Step 4: Run layout tests**

Run the same `ctest` command.

Expected: PASS for 1100×700, 1200×850, 1920×1080.

- [ ] **Step 5: Commit**

```bash
git add v032/ui/Layout.h v032/ui/Layout.cpp v032/tests/LayoutTests.cpp v032/CMakeLists.txt
git commit -m "feat: add responsive 0.3.2 shell layout"
```

---

### Task 4: Add Midnight Theme and Reusable Win32 Controls

**Files:**
- Create: `v032/ui/Theme.h`
- Create: `v032/ui/Theme.cpp`
- Create: `v032/ui/Controls.h`
- Create: `v032/ui/Controls.cpp`
- Modify: `v032/tests/Ui032Policy.Tests.ps1`

**Interfaces:**
- Produces:
  - `const Palette& MidnightPalette();`
  - `HFONT CreateUiFont(int points, int weight);`
  - `HWND CreateTextLabel(...)`
  - `HWND CreatePushButton(...)`
  - `void ApplyDarkListView(HWND list);`
  - `void ApplyDarkEdit(HWND edit);`
- Controls do not own application behavior; they only create/style HWNDs.

- [ ] **Step 1: Extend the policy test to fail on R4 visual leftovers**

Append to `Ui032Policy.Tests.ps1`:

```powershell
$theme = Get-Content -Raw (Join-Path $root 'ui/Theme.cpp')
$shell = Get-Content -Raw (Join-Path $root 'ui/Shell.cpp') -ErrorAction SilentlyContinue

foreach ($hex in @('0x0B1017','0x1B1F25','0x141D28','0x39D0A0','0xF6F7F9')) {
  if (-not $theme.ToUpperInvariant().Contains($hex)) {
    throw "Midnight palette token missing: $hex"
  }
}
if ($shell -and ($shell -match 'Sunset|PaintSunset|SIDEBAR|ID_NAV_SETTINGS')) {
  throw 'R4 sunset/sidebar concepts must not exist in the 0.3.2 shell.'
}
```

- [ ] **Step 2: Run and verify failure**

Run:

```powershell
pwsh -NoProfile -File v032/tests/Ui032Policy.Tests.ps1
```

Expected: FAIL because theme/control files do not exist.

- [ ] **Step 3: Implement exact palette**

`Theme.h` defines:

```cpp
struct Palette {
    COLORREF background;
    COLORREF title;
    COLORREF control;
    COLORREF hover;
    COLORREF border;
    COLORREF text;
    COLORREF muted;
    COLORREF accent;
    COLORREF accentHover;
    COLORREF warning;
    COLORREF error;
};
```

`Theme.cpp` stores the approved palette as explicit `0xRRGGBB` constants (for policy readability) and converts them to Win32 `COLORREF` with a small `ToColorRef(0xRRGGBB)` helper.

- [ ] **Step 4: Implement reusable control creators**

`Controls.cpp` uses `CreateWindowExW` for STATIC/BUTTON/EDIT/LISTVIEW, assigns Segoe UI fonts, enables `SetWindowTheme(..., L"DarkMode_Explorer", ...)` for list/edit where appropriate, and uses owner-draw only for buttons that need the mint active state.

- [ ] **Step 5: Re-run policy and full compile**

Run:

```powershell
pwsh -NoProfile -File v032/tests/Ui032Policy.Tests.ps1
cmake --build build-032 --config Release
```

Expected: PASS/compile success.

- [ ] **Step 6: Commit**

```bash
git add v032/ui/Theme.h v032/ui/Theme.cpp v032/ui/Controls.h v032/ui/Controls.cpp v032/tests/Ui032Policy.Tests.ps1
git commit -m "feat: restore Midnight visual system"
```

---

### Task 5: Add Session Log and Footer Status Area

**Files:**
- Create: `v032/ui/SessionLog.h`
- Create: `v032/ui/SessionLog.cpp`
- Create: `v032/ui/StatusBar.h`
- Create: `v032/ui/StatusBar.cpp`
- Create: `v032/tests/SessionLogTests.cpp`
- Modify: `v032/CMakeLists.txt`

**Interfaces:**
- Produces:
  - `enum class EventLevel { Info, Warning, Error };`
  - `struct SessionEvent { std::chrono::system_clock::time_point time; std::wstring category; EventLevel level; std::wstring message; };`
  - `class SessionLog { void Append(...); std::span<const SessionEvent> Events() const; std::wstring RenderText() const; };`
  - `class StatusBar { Create(HWND parent); Layout(const ShellLayout&); SetStatus(std::wstring_view); AppendLog(...); };`

- [ ] **Step 1: Write failing log test**

Create `v032/tests/SessionLogTests.cpp`:

```cpp
#include "ui/SessionLog.h"
#include <cassert>

int main() {
    dpop::ui::SessionLog log;
    log.Append(L"Shell", dpop::ui::EventLevel::Info, L"DPopCleaner 0.3.2 запущен.");
    log.Append(L"Overview", dpop::ui::EventLevel::Warning, L"Тест предупреждения.");

    assert(log.Events().size() == 2);
    const auto text = log.RenderText();
    assert(text.find(L"[Shell]") != std::wstring::npos);
    assert(text.find(L"DPopCleaner 0.3.2 запущен.") != std::wstring::npos);
    assert(text.find(L"[WARNING]") != std::wstring::npos);
    return 0;
}
```

- [ ] **Step 2: Run and verify failure**

Run target `SessionLogTests`; expected FAIL before implementation.

- [ ] **Step 3: Implement SessionLog**

`RenderText()` renders one event per line as:

```text
HH:MM:SS [CATEGORY] [LEVEL] message
```

with `INFO`, `WARNING`, or `ERROR`. Preserve insertion order.

- [ ] **Step 4: Implement StatusBar**

Create:
- status STATIC control above log;
- read-only multiline EDIT log control;
- `Поддержка` button bottom-left;
- `v0.3.2 BETA` label bottom-right.

`StatusBar::Layout()` uses the exact footer boxes produced by `ComputeShellLayout()`.

- [ ] **Step 5: Run tests/build**

Run:

```powershell
ctest --test-dir build-032 -C Release -R SessionLogTests --output-on-failure
cmake --build build-032 --config Release
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add v032/ui/SessionLog.h v032/ui/SessionLog.cpp v032/ui/StatusBar.h v032/ui/StatusBar.cpp v032/tests/SessionLogTests.cpp v032/CMakeLists.txt
git commit -m "feat: add DPopCleaner session log footer"
```

---

### Task 6: Build the Old-Series Shell, Horizontal Tabs, and Settings Gear

**Files:**
- Create: `v032/ui/Shell.h`
- Create: `v032/ui/Shell.cpp`
- Create: `v032/ui/pages/SettingsStubPage.h`
- Create: `v032/ui/pages/SettingsStubPage.cpp`
- Modify: `v032/tests/Ui032Policy.Tests.ps1`

**Interfaces:**
- Produces: `int dpop::ui::shell::Run(HINSTANCE instance, int showCommand);`
- Shell owns top-level HWND, header labels, ten tab buttons, Settings gear, page host, `StatusBar`, active `Page`.
- Page switch API inside Shell: `void ShowPage(Page page);`

- [ ] **Step 1: Add static policy assertions for exact navigation**

Append:

```powershell
$shell = Get-Content -Raw (Join-Path $root 'ui/Shell.cpp')
foreach ($label in @(
  'Обзор','Очистка','ОЗУ','DPopGuard','Диск',
  'Приложения','Windows','Дубликаты','Инструменты','Zapret'
)) {
  if (-not $shell.Contains($label)) { throw "Shell missing tab label: $label" }
}
if (-not $shell.Contains('⚙')) { throw 'Settings gear is missing.' }
if ($shell -match 'sidebar|SIDEBAR|vertical navigation') {
  throw 'Vertical navigation is forbidden in 0.3.2.'
}
```

- [ ] **Step 2: Run and verify failure**

Expected: FAIL before `Shell.cpp`.

- [ ] **Step 3: Implement top-level window**

`Shell.cpp` creates the window with:
- title `DPopCleaner 0.3.2 BETA R1`;
- `WS_OVERLAPPEDWINDOW`;
- minimum tracking size 1100×700 via `WM_GETMINMAXINFO`;
- DWM dark mode;
- no custom network/update side effects during startup.

Header text:
- `DPopCleaner`;
- `BETA`;
- `Очистка • память • защита • диски • Windows`.

Create ten tab buttons from `PrimaryTabs()`, not ten duplicated hard-coded command handlers.

Create a gear button with command ID `1100`; on click call `ShowPage(Page::Settings)`.

- [ ] **Step 4: Implement Settings stub without fake features**

`SettingsStubPage` renders:
- heading `Настройки`;
- `Язык: Русский`;
- `Тема: Midnight`;
- `Бесплатная BETA`;
- explanatory line `Функциональные настройки будут подключены отдельным этапом 0.3.2; этот candidate не сохраняет параметры.`

It must contain no checkboxes or buttons that pretend to persist settings.

- [ ] **Step 5: Implement page switch state**

For `Overview`, call `OverviewPage::Show()`.
For the remaining nine primary pages in this shell-only phase, render a consistent non-interactive page title and the exact text `Раздел будет подключён в следующем функциональном этапе 0.3.2.` using the page host. These interim messages exist only in the development artifact; the final candidate workflow is not introduced until all page plans are complete.

- [ ] **Step 6: Implement responsive resize**

On `WM_SIZE`, call `ComputeShellLayout(clientWidth, clientHeight)`, move header/tabs/page host/footer, and ask the active page to layout inside `content`.

On `WM_GETMINMAXINFO`, enforce 1100×700 outer minimum adjusted for non-client metrics.

- [ ] **Step 7: Re-run policy and build**

Run:

```powershell
pwsh -NoProfile -File v032/tests/Ui032Policy.Tests.ps1
cmake --build build-032 --config Release
```

Expected: PASS.

- [ ] **Step 8: Commit**

```bash
git add v032/ui/Shell.h v032/ui/Shell.cpp v032/ui/pages/SettingsStubPage.h v032/ui/pages/SettingsStubPage.cpp v032/tests/Ui032Policy.Tests.ps1
git commit -m "feat: restore old-series DPopCleaner shell"
```

---

### Task 7: Restore the Functional Overview Dashboard

**Files:**
- Create: `v032/ui/pages/OverviewPage.h`
- Create: `v032/ui/pages/OverviewPage.cpp`
- Create: `v032/tests/OverviewModelTests.cpp`
- Modify: `v032/CMakeLists.txt`

**Interfaces:**
- Consumes:
  - `dpop::system_info::Snapshot`
  - `dpop::apps::EnumerateInstalledApps()`
  - `dpop::cleaner::EstimateRecycleBinBytes()`
  - `dpop::zapret::QueryStatus()`
- Produces:
  - `struct OverviewModel { ... };`
  - `OverviewModel BuildOverviewModel(const system_info::Snapshot&, std::size_t appCount, std::uint64_t recycleBytes, bool zapretServiceInstalled, bool zapretWinwsRunning);`
  - `OverviewPage::Create(HWND parent, SessionLog&, std::function<void(Page)> navigate)`
  - `OverviewPage::Refresh()`
  - `OverviewPage::Layout(const Box&)`

- [ ] **Step 1: Write failing model test**

Create `v032/tests/OverviewModelTests.cpp`:

```cpp
#include "ui/pages/OverviewPage.h"
#include <cassert>

int main() {
    dpop::system_info::Snapshot s{};
    s.cpuCount = 12;
    s.ramTotal = 16ull * 1024 * 1024 * 1024;
    s.ramAvailable = 10ull * 1024 * 1024 * 1024;
    s.systemDriveTotal = 500ull * 1024 * 1024 * 1024;
    s.systemDriveFree = 100ull * 1024 * 1024 * 1024;
    s.processCount = 280;

    const auto m = dpop::ui::BuildOverviewModel(s, 76, 0, false, false);
    assert(m.ramUsedBytes == 6ull * 1024 * 1024 * 1024);
    assert(m.ramUsedPercent == 38);
    assert(m.driveUsedPercent == 80);
    assert(m.appCount == 76);
    assert(m.recycleEmpty);
    assert(m.zapretText == L"Сервис не установлен • winws: OFF");
    return 0;
}
```

- [ ] **Step 2: Run and verify failure**

Expected: FAIL because Overview model does not exist.

- [ ] **Step 3: Implement pure Overview model**

Use integer rounding:

```cpp
percent = total == 0 ? 0 : static_cast<unsigned>(
    (used * 100 + total / 2) / total);
```

`zapretText` must derive only from supplied real state, never from hard-coded “ready”.

- [ ] **Step 4: Implement Overview controls**

Create six old-series cards:
1. Диск C:
2. Оперативная память
3. Установленные приложения
4. DPopGuard
5. Zapret
6. Заполненность корзины

Create actions and pass navigation through the `std::function<void(Page)> navigate` callback supplied by Shell:
- Обновить
- Быстрая очистка
- Быстрый DPopGuard
- Открыть диск
- Открыть приложения

For this shell plan:
- `Обновить` performs `Refresh()`.
- `Быстрая очистка` switches to Cleaning (no cleanup runs).
- `Быстрый DPopGuard` switches to Guard (no scan runs).
- `Открыть диск` switches to Disk.
- `Открыть приложения` switches to Applications.

This preserves safe navigation and avoids pretending those later page actions are already implemented.

- [ ] **Step 5: Fetch real data on Refresh**

`Refresh()`:
- calls `system_info::Collect()`;
- counts `EnumerateInstalledApps()`;
- calls `EstimateRecycleBinBytes()`;
- calls `QueryStatus()`;
- updates card text;
- appends `Overview / Info / Обзор обновлён.` to session log.

- [ ] **Step 6: Run tests/build**

Run:

```powershell
ctest --test-dir build-032 -C Release -R OverviewModelTests --output-on-failure
cmake --build build-032 --config Release
```

Expected: PASS.

- [ ] **Step 7: Commit**

```bash
git add v032/ui/pages/OverviewPage.h v032/ui/pages/OverviewPage.cpp v032/tests/OverviewModelTests.cpp v032/CMakeLists.txt
git commit -m "feat: restore functional overview dashboard"
```

---

### Task 8: Add Development Build, Resize Smoke, and Screenshot Artifact

**Files:**
- Create: `.github/workflows/build-dpopcleaner-0.3.2-dev.yml`
- Create: `scripts/Capture-032Shell.ps1`
- Modify: `v032/tests/Ui032Policy.Tests.ps1`

**Interfaces:**
- Produces a GitHub Actions artifact only.
- Does not create a Release.
- Does not modify `update/beta.json`, site files, tags, or Pages.

- [ ] **Step 1: Add failing workflow policy**

Append to `Ui032Policy.Tests.ps1`:

```powershell
$workflow = Get-Content -Raw (Join-Path $root '..\.github\workflows\build-dpopcleaner-0.3.2-dev.yml') -ErrorAction SilentlyContinue
if (-not $workflow) { throw '0.3.2 dev workflow is missing.' }

foreach ($forbidden in @(
  'gh release create',
  'update/beta.json',
  'deploy-pages',
  'git push'
)) {
  if ($workflow.Contains($forbidden)) {
    throw "Dev workflow must not publish: $forbidden"
  }
}
```

- [ ] **Step 2: Run and verify failure**

Expected: FAIL until workflow exists.

- [ ] **Step 3: Create shell capture script**

`scripts/Capture-032Shell.ps1` takes:

```powershell
param(
  [Parameter(Mandatory)][string]$ExecutablePath,
  [Parameter(Mandatory)][string]$OutputDirectory
)
```

It:
1. starts the executable;
2. waits for `MainWindowHandle`;
3. captures default 1200×850 screenshot;
4. resizes to 1100×700 and captures;
5. maximizes and captures;
6. sends `WM_CLOSE`;
7. verifies process exits within 10 seconds;
8. writes `capture-report.json` with `default`, `minimum`, `maximized`, and `graceful_close`.

Reuse the proven Win32 screenshot/PInvoke approach from `scripts/Capture-AppScreenshot.ps1`; do not use OCR.

- [ ] **Step 4: Create non-publishing dev workflow**

`.github/workflows/build-dpopcleaner-0.3.2-dev.yml`:
- `workflow_dispatch` only;
- `windows-2022`;
- checkout;
- run stable R3 policy tests;
- run `v032/tests/Ui032Policy.Tests.ps1`;
- prepare source with `Prepare-032Source.ps1`;
- CMake x64;
- build Release;
- run all CTest tests;
- verify `DPopCleaner.exe` FileVersion `0.3.2.1` and ProductVersion `0.3.2 BETA R1`;
- run `Capture-032Shell.ps1`;
- upload:
  - `DPopCleaner.exe`
  - `DPopUpdater.exe`
  - three screenshots
  - `capture-report.json`
  - `v032-overlay-inventory.json`.

No installer, Zapret download, Defender scan, release, or site deployment in this first visual shell workflow.

- [ ] **Step 5: Re-run policy**

Run:

```powershell
pwsh -NoProfile -File v032/tests/Ui032Policy.Tests.ps1
```

Expected: PASS.

- [ ] **Step 6: Run the workflow**

Run manually:
`Actions → Build DPopCleaner 0.3.2 development candidate → Run workflow`.

Expected:
- all C++ tests PASS;
- capture report says `graceful_close: true`;
- artifact contains three screenshots and executables.

- [ ] **Step 7: Visual review gate**

Compare the three screenshots against the approved old-series references:
- horizontal navigation present;
- no left sidebar;
- Settings gear upper-right;
- 6-card Overview in 3×2 grid;
- five quick actions;
- bottom status/log/support/version;
- no sunset graphic;
- no huge blank technical text slab;
- controls remain usable at 1100×700 and maximized.

Do not proceed to functional page plans until this gate is accepted.

- [ ] **Step 8: Commit**

```bash
git add .github/workflows/build-dpopcleaner-0.3.2-dev.yml scripts/Capture-032Shell.ps1 v032/tests/Ui032Policy.Tests.ps1
git commit -m "ci: add visual gate for DPopCleaner 0.3.2 shell"
```

---

## Follow-Up Plans After This One

These are separate plans because each is independently reviewable and testable:

1. **0.3.2 Existing Core Pages Plan** — Cleaning, Applications, DPopGuard, Zapret, Tools.
2. **0.3.2 Memory Plan** — DPopMemory metrics, graph, safe memory operations, auto rules.
3. **0.3.2 Disk and Duplicates Plan** — DiskAnalyzer + SHA-256 duplicate finder.
4. **0.3.2 Windows and Settings Plan** — Windows Update cleanup, settings persistence, exclusions, localization, startup/tray controls.
5. **0.3.2 Integration and Release Plan** — async worker coordination, final UI smoke across all pages, installer, Defender, candidate workflow, manual approval, publish workflow, site.

The site remains on verified 0.3.1 R4 until the final plan's manual visual gate is approved.
