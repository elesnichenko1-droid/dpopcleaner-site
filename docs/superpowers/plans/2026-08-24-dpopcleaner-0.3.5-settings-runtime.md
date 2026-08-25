# DPopCleaner 0.3.5 Settings + Runtime Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the flat/fake Settings surface with a typed, atomic, five-section settings center whose visible controls are persisted and consumed by real DPopCleaner runtime behavior.

**Architecture:** `SettingsStore` becomes the canonical typed model/persistence layer and `SettingsController` owns editable/dirty/default/apply/cancel semantics independently of Win32 controls. `SettingsPage` is a compact horizontal-section editor over the controller. Compatibility wrappers keep existing `dpop::full::LoadSettings/SaveSettings` callers working while cleanup, startup actions, memory automation, tray/close behavior, exclusions and thresholds move onto one committed model.

**Tech Stack:** C++20, native Win32/Common Controls, Win32 registry/shell APIs, CMake/CTest, Python 3 contract tests, PowerShell UI smoke, GitHub Actions Windows 2022.

**Spec:** `docs/superpowers/specs/2026-08-24-dpopcleaner-0.3.5-golden-0214-treesize-settings-design.md`

## Global Constraints

- Product identity remains exactly `0.3.5 BETA R1`, version code `3051`, revision `1`.
- Work stays on `feat/dpopcleaner-0.3.5-r1`; do not mutate `main` or published 0.3.4 artifacts.
- Settings has exactly five compact horizontal sections: `Основное`, `Очистка`, `Память`, `Защита`, `Исключения`.
- No left settings sidebar is introduced.
- Static fake language/theme controls are omitted until they change real runtime behavior.
- Canonical path is `%LOCALAPPDATA%\DPopCleaner\settings.json`.
- Writes are atomic: temporary file in the same directory followed by replace/rename with write-through behavior.
- Corrupt settings fall back to safe defaults and produce a log/status event; they do not crash startup.
- Unknown future JSON keys are ignored by 0.3.5.
- `Применить`, `Сохранить`, `Отмена`, and `По умолчанию` have distinct semantics.
- A visible control is incomplete unless the runtime consumes the committed value.
- No startup setting may silently perform destructive cleanup/quarantine/system maintenance.
- Aggressive memory behavior is off by default and only enabled by explicit user choice.

---

## File Structure

**Create**
- `v035_overlay/modules/SettingsStore.h` — typed settings schema, validation and persistence API.
- `v035_overlay/modules/SettingsStore.cpp` — JSON extraction/serialization, schema migration, atomic save, normalization.
- `v035_overlay/ui/settings/SettingsController.h` — working-copy and dirty-state API.
- `v035_overlay/ui/settings/SettingsController.cpp` — apply/save/cancel/default model semantics.
- `v035_overlay/ui/pages/SettingsPage.h` — five-section Win32 page state.
- `v035_overlay/ui/pages/SettingsPage.cpp` — real controls, horizontal selector, persistent bottom actions.
- `v035_overlay/ui/TrayIcon.h` — tray icon command contract.
- `v035_overlay/ui/TrayIcon.cpp` — `Shell_NotifyIconW` lifecycle and restore/exit menu.
- `tests/v035/SettingsStoreTests.cpp` — defaults, validation, round-trip, corrupt fallback, migration, exclusions.
- `tests/v035/SettingsControllerTests.cpp` — dirty/apply/cancel/default semantics.
- `tests/test_dpop035_settings_contract.py` — static page/runtime-consumer contracts.
- `tools/dpop035_settings_smoke.ps1` — real save/reload/close/tray/startup-settings smoke.

**Modify through `v035_overlay/` or generated-source transformation**
- `modules/FullCore.h/.cpp` — compatibility wrappers and cleanup exclusion consumption.
- `ui/Shell.h/.cpp` — startup hooks, background analysis timer, RAM auto-trim timer, tray/close behavior, Settings modal/page lifecycle.
- `ui/pages/MemoryPage.*` only if needed to refresh committed memory automation state; no duplicate persistence logic.
- `ui/pages/GuardPage.*` — expose non-destructive quick startup scan entrypoint.
- `ui/pages/WindowsPage.*` — expose non-destructive update-cache size check entrypoint.
- `ui/pages/UpdatesPage.*` — startup update check entrypoint.
- generated `v035/CMakeLists.txt` — add SettingsStore, SettingsController, TrayIcon and CTest targets.
- `.github/workflows/DPopCleaner_0.3.5_CANDIDATE.yml` — add settings smoke/screenshots; candidate-only.

---

### Task 1: Create the typed SettingsStore and schema migration

**Files:**
- Create: `v035_overlay/modules/SettingsStore.h`
- Create: `v035_overlay/modules/SettingsStore.cpp`
- Create: `tests/v035/SettingsStoreTests.cpp`
- Modify: `tools/dpop035_core.py` generated CMake injection.

**Interfaces:**
- Produces: `AppSettings`, `SettingsLoadResult`, `LoadAppSettings`, `SaveAppSettings`, `ValidateSettings`, `DefaultSettings`, `NormalizeExclusionPath`, `IsExcludedPath`.
- Schema version for 0.3.5 R1 is `2`.

- [ ] **Step 1: Write failing tests for defaults, validation and path normalization**

```cpp
// tests/v035/SettingsStoreTests.cpp
#include "modules/SettingsStore.h"
#include <cassert>
#include <filesystem>

using namespace dpop::settings;

int main() {
    auto s = DefaultSettings();
    assert(s.schemaVersion == 2);
    assert(s.confirmDestructive);
    assert(s.checkUpdatesAtStartup);
    assert(!s.quickGuardAtStartup);
    assert(!s.memoryAutoTrimEnabled);
    assert(s.memoryAutoTrimPercent == 80);
    assert(s.memoryAutoTrimIntervalMinutes == 15);
    assert(s.largeFileMB == 500);
    assert(s.duplicateMinMB == 10);
    assert(!s.backgroundJunkMonitor);
    assert(!s.alwaysRunAsAdmin);

    std::wstring error;
    s.memoryAutoTrimPercent = 49;
    assert(!ValidateSettings(s, error));
    s.memoryAutoTrimPercent = 80;
    s.largeFileMB = 4097;
    assert(!ValidateSettings(s, error));

    const auto a = NormalizeExclusionPath(L"C:/Users/Test/Cache/");
    const auto b = NormalizeExclusionPath(L"c:\\users\\test\\cache");
    assert(a == b);
    return 0;
}
```

- [ ] **Step 2: Add a failing CTest target**

Generated CMake must contain:

```cmake
add_executable(SettingsStoreTests
    tests/v035/SettingsStoreTests.cpp
    modules/SettingsStore.cpp
)
target_include_directories(SettingsStoreTests PRIVATE .)
target_compile_definitions(SettingsStoreTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(SettingsStoreTests PRIVATE shell32 advapi32)
add_test(NAME SettingsStoreTests COMMAND SettingsStoreTests)
```

Run on Windows:

```powershell
cmake --build build --config Release --target SettingsStoreTests
```

Expected: FAIL because SettingsStore does not exist.

- [ ] **Step 3: Define the exact typed model**

```cpp
// v035_overlay/modules/SettingsStore.h
#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace dpop::settings {
enum class CloseBehavior { Exit = 0, MinimizeToTray = 1, Ask = 2 };
enum class MemoryScope { Safe = 0, Advanced = 1 };

struct AppSettings {
    unsigned schemaVersion{2};
    bool confirmDestructive{true};
    unsigned largeFileMB{500};
    unsigned duplicateMinMB{10};
    bool runAtStartup{false};
    bool alwaysRunAsAdmin{false};
    bool checkUpdatesAtStartup{true};
    bool quickGuardAtStartup{false};
    bool checkUpdateCacheAtStartup{false};
    bool backgroundJunkMonitor{false};
    bool trayEnabled{true};
    CloseBehavior closeBehavior{CloseBehavior::MinimizeToTray};
    bool memoryAutoTrimEnabled{false};
    unsigned memoryAutoTrimPercent{80};
    unsigned memoryAutoTrimIntervalMinutes{15};
    MemoryScope memoryScope{MemoryScope::Safe};
    std::vector<std::wstring> cleanExclusions;
};

struct SettingsLoadResult {
    AppSettings settings;
    bool usedDefaults{};
    bool migrated{};
    std::wstring warning;
};

AppSettings DefaultSettings() noexcept;
std::filesystem::path SettingsPath();
SettingsLoadResult LoadAppSettings();
bool SaveAppSettings(const AppSettings& settings, std::wstring& error);
bool ValidateSettings(const AppSettings& settings, std::wstring& error) noexcept;
std::wstring NormalizeExclusionPath(const std::filesystem::path& path);
bool IsExcludedPath(const std::filesystem::path& path, const AppSettings& settings);
}
```

- [ ] **Step 4: Implement exact validation rules**

`ValidateSettings` rejects with a user-readable message when:
- `largeFileMB` is outside `50..4096`;
- `duplicateMinMB` is outside `1..1024`;
- `memoryAutoTrimPercent` is outside `50..98`;
- `memoryAutoTrimIntervalMinutes` is outside `1..1440`;
- `closeBehavior == MinimizeToTray` while `trayEnabled == false`.

No other field requires validation in R1.

- [ ] **Step 5: Implement load/migration without external JSON dependency**

Use the existing 0.3.x known-key extraction style but centralize it in `SettingsStore.cpp`: helpers `ExtractBool`, `ExtractUInt`, `ExtractInt`, `ExtractStringArray`, `JsonEscape`. Unknown keys are ignored.

Migration rules:
- missing `schema_version` means schema 1;
- `minimize_to_tray=true` from old files maps to `trayEnabled=true` and `closeBehavior=MinimizeToTray`;
- old `memory_auto_trim_percent` maps directly;
- old `clean_exclusions` maps to normalized unique entries;
- old `monitor_installations` is intentionally ignored because 0.3.5 has no fake installation watcher control;
- missing new fields take the safe defaults above;
- after migration, returned `settings.schemaVersion` is `2` and `migrated=true`.

If the file is unreadable or structurally malformed (missing outer `{`/`}` or invalid extracted numeric values), return `DefaultSettings()`, `usedDefaults=true`, and a warning string. Do not overwrite the corrupt file during load.

- [ ] **Step 6: Implement same-directory atomic save**

Serialize deterministic UTF-8 JSON to `settings.json.tmp` in the same directory, flush/close the file, then replace:

```cpp
if (!ReplaceFileW(finalPath.c_str(), tempPath.c_str(), nullptr,
                  REPLACEFILE_WRITE_THROUGH, nullptr, nullptr)) {
    const DWORD replaceError = GetLastError();
    if (!MoveFileExW(tempPath.c_str(), finalPath.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = L"Не удалось атомарно сохранить настройки. Код Windows: " +
                std::to_wstring(GetLastError() ? GetLastError() : replaceError);
        DeleteFileW(tempPath.c_str());
        return false;
    }
}
```

Create `%LOCALAPPDATA%\DPopCleaner` before writing. Never write secrets.

- [ ] **Step 7: Extend tests with round-trip, corruption and schema migration**

Add a test-only overload or environment-root injection so tests write under a temporary directory rather than real LocalAppData. Test:
- save/load equality for every field;
- corrupt `{not-json` returns defaults with warning;
- schema-1 JSON migrates old fields;
- duplicate exclusions differing only in case/slashes collapse to one normalized entry.

- [ ] **Step 8: Run tests and commit**

```powershell
cmake --build build --config Release --target SettingsStoreTests
ctest --test-dir build -C Release -R SettingsStoreTests --output-on-failure
```

Expected: PASS.

```bash
git add v035_overlay/modules/SettingsStore.h v035_overlay/modules/SettingsStore.cpp tests/v035/SettingsStoreTests.cpp tools/dpop035_core.py
git commit -m "feat: add typed atomic settings store"
```

---

### Task 2: Add a testable SettingsController for Apply/Save/Cancel/Defaults

**Files:**
- Create: `v035_overlay/ui/settings/SettingsController.h`
- Create: `v035_overlay/ui/settings/SettingsController.cpp`
- Create: `tests/v035/SettingsControllerTests.cpp`
- Modify: generated CMake through `tools/dpop035_core.py`.

**Interfaces:**
- Consumes: `AppSettings`, `LoadAppSettings`, `SaveAppSettings`.
- Produces: editor working copy and exact command semantics for the page.

- [ ] **Step 1: Write failing controller tests**

```cpp
// tests/v035/SettingsControllerTests.cpp
#include "ui/settings/SettingsController.h"
#include <cassert>

using namespace dpop::settings;
using namespace dpop::ui;

int main() {
    AppSettings persisted = DefaultSettings();
    SettingsController c(persisted);
    assert(!c.Dirty());

    c.Edit().largeFileMB = 700;
    c.MarkDirty();
    assert(c.Dirty());

    c.CancelEdits();
    assert(!c.Dirty());
    assert(c.Edit().largeFileMB == 500);

    c.LoadDefaults();
    assert(c.Dirty());
    assert(c.Edit().largeFileMB == 500);

    c.Edit().largeFileMB = 800;
    c.MarkDirty();
    c.CommitInMemory();
    assert(!c.Dirty());
    assert(c.Persisted().largeFileMB == 800);
    return 0;
}
```

- [ ] **Step 2: Implement the controller**

```cpp
// v035_overlay/ui/settings/SettingsController.h
#pragma once
#include "modules/SettingsStore.h"

namespace dpop::ui {
class SettingsController {
public:
    explicit SettingsController(dpop::settings::AppSettings persisted);
    const dpop::settings::AppSettings& Persisted() const noexcept;
    dpop::settings::AppSettings& Edit() noexcept;
    const dpop::settings::AppSettings& Edit() const noexcept;
    bool Dirty() const noexcept;
    void MarkDirty() noexcept;
    void CancelEdits() noexcept;
    void LoadDefaults();
    void CommitInMemory();
private:
    dpop::settings::AppSettings persisted_;
    dpop::settings::AppSettings edit_;
    bool dirty_{};
};
}
```

Semantics:
- constructor copies persisted to both models;
- `MarkDirty()` sets dirty true;
- `CancelEdits()` copies persisted → edit and clears dirty;
- `LoadDefaults()` loads safe defaults into edit and sets dirty iff defaults differ from persisted;
- `CommitInMemory()` copies edit → persisted and clears dirty; persistence is deliberately performed by SettingsPage after `ValidateSettings` and `SaveAppSettings` succeed.

- [ ] **Step 3: Run CTest and commit**

```powershell
cmake --build build --config Release --target SettingsControllerTests
ctest --test-dir build -C Release -R SettingsControllerTests --output-on-failure
```

Expected: PASS.

```bash
git add v035_overlay/ui/settings/SettingsController.h v035_overlay/ui/settings/SettingsController.cpp tests/v035/SettingsControllerTests.cpp tools/dpop035_core.py
git commit -m "feat: add settings editor state controller"
```

---

### Task 3: Replace SettingsPage with five real horizontal sections

**Files:**
- Create/replace: `v035_overlay/ui/pages/SettingsPage.h`
- Create/replace: `v035_overlay/ui/pages/SettingsPage.cpp`
- Create: `tests/test_dpop035_settings_contract.py`

**Interfaces:**
- Consumes: `SettingsController`, SettingsStore, existing dark-control helpers.
- Produces: `ApplyChanges(bool closeAfter)`, `CancelChanges()`, `LoadDefaults()`, `HasUnsavedChanges()`.

- [ ] **Step 1: Write failing UI-contract tests**

```python
# tests/test_dpop035_settings_contract.py
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PAGE = ROOT / "v035_overlay/ui/pages/SettingsPage.cpp"


def test_settings_has_five_horizontal_sections_and_four_actions():
    text = PAGE.read_text(encoding="utf-8")
    for label in ("Основное", "Очистка", "Память", "Защита", "Исключения"):
        assert label in text
    for label in ("Применить", "Сохранить", "Отмена", "По умолчанию"):
        assert label in text
    assert "Язык: Русский" not in text
    assert "Тема: Midnight" not in text


def test_settings_page_uses_controller_and_store():
    text = PAGE.read_text(encoding="utf-8")
    assert "SettingsController" in text
    assert "ValidateSettings" in text
    assert "SaveAppSettings" in text
    assert "LoadDefaults" in text
    assert "CancelEdits" in text
```

Run:

```bash
python -m pytest tests/test_dpop035_settings_contract.py -q
```

Expected: FAIL before the page is replaced.

- [ ] **Step 2: Implement compact horizontal section selector**

Use five push/toggle buttons in one row; selected section uses accent visual. Only controls belonging to the active section are visible. Do not create a left navigation child window.

Section enum:

```cpp
enum class SettingsSection { General, Cleanup, Memory, Protection, Exclusions };
```

Persistent bottom buttons are always visible and laid out from the bottom edge so 1100×700 cannot hide them.

- [ ] **Step 3: Implement exact visible controls**

**Основное**
- `Запускать DPopCleaner вместе с Windows` → `runAtStartup`.
- `Показывать значок в трее` → `trayEnabled`.
- close behavior combo with exactly `Выходить`, `Сворачивать в трей`, `Спрашивать` → `CloseBehavior`.
- `Проверять обновления DPopCleaner при запуске` → `checkUpdatesAtStartup`.
- `Всегда запускать от администратора` → `alwaysRunAsAdmin`.

**Очистка**
- `Подтверждать удаление перед очисткой` → `confirmDestructive`.
- `Фоновый анализ мусора каждые 30 минут` → `backgroundJunkMonitor`; analysis only, never automatic cleanup.
- numeric `Порог крупных файлов, МБ` → `largeFileMB`.
- numeric `Мин. размер дубликата, МБ` → `duplicateMinMB`.

**Память**
- `Автоочистка памяти` → `memoryAutoTrimEnabled`.
- numeric `Порог использования RAM, %` → `memoryAutoTrimPercent`.
- numeric `Минимальный интервал, минут` → `memoryAutoTrimIntervalMinutes`.
- combo `Безопасный` / `Расширенный` → `MemoryScope`; safe default.

**Защита**
- `Quick DPopGuard-скан при запуске` → `quickGuardAtStartup`.
- `Проверять размер кэша Windows Update при запуске` → `checkUpdateCacheAtStartup`.

**Исключения**
- one ListView column `Файл / папка, исключённые из очистки`.
- buttons `Добавить файл`, `Добавить папку`, `Удалить`.
- add operations normalize only for duplicate detection; display the user-readable path.

Do not show `monitorInstallations` because 0.3.5 does not ship a real installation watcher in this plan.

- [ ] **Step 4: Implement exact bottom-action semantics**

`ApplyChanges(false)`:
1. pull visible controls into `controller_.Edit()`;
2. validate with `ValidateSettings`;
3. if run-at-startup/admin registry state changes, call existing safe setters and abort on failure before saving JSON;
4. call `SaveAppSettings`;
5. call `controller_.CommitInMemory()`;
6. notify Shell that runtime settings changed;
7. remain open.

`ApplyChanges(true)` performs the same sequence and closes Settings only after success.

`Отмена` calls `CancelEdits()`, repopulates controls, and closes without saving.

`По умолчанию` calls `LoadDefaults()` and repopulates controls; it does not write settings until Apply/Save.

When Settings is being closed by the window/gear action and `Dirty()==true`, show exactly a three-way prompt: `Сохранить`, `Не сохранять`, `Отмена`. Cancel keeps Settings open.

- [ ] **Step 5: Run contracts and commit**

```bash
python -m pytest tests/test_dpop035_settings_contract.py -q
```

Expected: page-only tests PASS.

```bash
git add v035_overlay/ui/pages/SettingsPage.h v035_overlay/ui/pages/SettingsPage.cpp tests/test_dpop035_settings_contract.py
git commit -m "feat: rebuild Settings as five functional sections"
```

---

### Task 4: Connect SettingsStore to cleanup and compatibility APIs

**Files:**
- Modify via overlay/transform: generated `modules/FullCore.h`
- Modify via overlay/transform: generated `modules/FullCore.cpp`
- Modify: `tests/v035/SettingsStoreTests.cpp`
- Modify: `tests/test_dpop035_settings_contract.py`

**Interfaces:**
- Consumes: `dpop::settings::AppSettings`.
- Produces compatibility wrappers for existing pages and real cleanup exclusion filtering.

- [ ] **Step 1: Add failing contracts proving exclusions affect analysis and cleaning**

```python
def test_cleanup_consumes_settings_exclusions():
    source = (ROOT / "tools/dpop035_core.py").read_text(encoding="utf-8")
    assert "IsExcludedPath" in source
    assert "AnalyzeCleaning" in source
    assert "CleanSelected" in source
```

And add a C++ fixture test that creates an excluded temp subtree, runs the internal tree estimate/clean helper through a test-visible wrapper, and asserts excluded files are neither counted nor deleted.

- [ ] **Step 2: Keep old callers source-compatible through aliases/wrappers**

Generated `FullCore.h` must expose:

```cpp
using Settings = dpop::settings::AppSettings;
inline Settings LoadSettings() { return dpop::settings::LoadAppSettings().settings; }
inline bool SaveSettings(const Settings& s, std::wstring& error) {
    return dpop::settings::SaveAppSettings(s, error);
}
inline bool IsPathExcluded(const std::filesystem::path& path, const Settings& s) {
    return dpop::settings::IsExcludedPath(path, s);
}
```

Keep existing `SetRunAtStartup` and `SetAlwaysRunAsAdmin` system integration functions in FullCore or move them behind explicit wrappers without changing call sites in this wave.

- [ ] **Step 3: Apply exclusions in both analysis and execution**

At every cleanup traversal root and entry:

```cpp
const Settings settings = LoadSettings();
if (IsPathExcluded(root, settings)) return;
...
if (IsPathExcluded(it->path(), settings)) {
    if (it->is_directory(ec)) it.disable_recursion_pending();
    ec.clear();
    continue;
}
```

The same rule is used by `AnalyzeCleaning` and `CleanSelected`; an exclusion that hides bytes from analysis must also prevent deletion.

- [ ] **Step 4: Run CTest and Python contract**

```powershell
ctest --test-dir build -C Release -R "SettingsStoreTests|Cleanup" --output-on-failure
```

```bash
python -m pytest tests/test_dpop035_settings_contract.py -q
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add tools/dpop035_core.py tests/v035/SettingsStoreTests.cpp tests/test_dpop035_settings_contract.py
git commit -m "feat: enforce cleanup exclusions from committed settings"
```

---

### Task 5: Implement real runtime consumers: startup hooks, RAM automation and background junk analysis

**Files:**
- Modify/overlay: `v035_overlay/ui/Shell.h`
- Modify/overlay: `v035_overlay/ui/Shell.cpp`
- Modify/overlay as needed: `ui/pages/GuardPage.h/.cpp`, `ui/pages/WindowsPage.h/.cpp`, `ui/pages/UpdatesPage.h/.cpp`
- Modify: `tests/test_dpop035_settings_contract.py`

**Interfaces:**
- Consumes committed `AppSettings`.
- Produces non-destructive startup actions, background junk analysis timer, auto RAM-trim timer.

- [ ] **Step 1: Add failing runtime-source contracts**

```python
def test_shell_consumes_every_non_ui_runtime_setting():
    shell = (ROOT / "v035_overlay/ui/Shell.cpp").read_text(encoding="utf-8")
    for marker in (
        "checkUpdatesAtStartup",
        "quickGuardAtStartup",
        "checkUpdateCacheAtStartup",
        "backgroundJunkMonitor",
        "memoryAutoTrimEnabled",
        "memoryAutoTrimPercent",
        "memoryAutoTrimIntervalMinutes",
        "memoryScope",
    ):
        assert marker in shell
    startup = shell[shell.index("RunConfiguredStartupActions"):shell.index("void Shell::ApplyRuntimeSettings")]
    assert "CleanSelected" not in startup
    assert "Quarantine" not in startup
    assert "ResetBase" not in startup
```

- [ ] **Step 2: Implement exact startup actions**

After the shell is visible, queue `RunConfiguredStartupActions()` once:

```cpp
const auto settings = dpop::settings::LoadAppSettings().settings;
if (settings.checkUpdatesAtStartup) updatesPage_.CheckAtStartup();
if (settings.quickGuardAtStartup) guardPage_.RunQuickScanAtStartup();
if (settings.checkUpdateCacheAtStartup) windowsPage_.CheckUpdateCacheAtStartup();
```

These entrypoints are non-destructive: Guard reports findings only; Windows Update startup check only estimates cache size; updater only checks availability.

- [ ] **Step 3: Implement background junk analysis timer**

Use a 30-minute timer only when `backgroundJunkMonitor` is true. Timer handler dispatches `AnalyzeCleaning` asynchronously, sums bytes, records `Последний фоновый анализ: X можно очистить` in log/status/Overview state. It never calls `CleanSelected`.

On settings apply, `ApplyRuntimeSettings()` creates/kills this timer immediately.

- [ ] **Step 4: Implement explicit RAM auto-trim**

Use a one-minute observation timer when `memoryAutoTrimEnabled` is true. Keep `lastAutoTrim_` as `std::chrono::steady_clock::time_point`.

On each tick:
1. query `QueryMemoryStats()`;
2. return if `usedPercent < memoryAutoTrimPercent`;
3. return if elapsed since `lastAutoTrim_` is less than configured interval;
4. run `TrimWorkingSets(settings.memoryScope == MemoryScope::Advanced, token)` asynchronously;
5. set `lastAutoTrim_` after completion and log attempted/trimmed/failed counts.

No undocumented standby-list purge is added by this plan.

- [ ] **Step 5: Reload runtime state immediately after Apply/Save**

`Shell::ApplyRuntimeSettings(const AppSettings&)` updates timers and tray behavior without app restart. Registry-backed startup/elevation changes may naturally take effect on the next process launch, but the UI committed state updates immediately after the setter succeeds.

- [ ] **Step 6: Run contracts, build and commit**

```bash
python -m pytest tests/test_dpop035_settings_contract.py -q
```

```powershell
cmake --build build --config Release
test-path build\bin\Release\DPopCleaner.exe
ctest --test-dir build -C Release --output-on-failure
```

Expected: PASS.

```bash
git add v035_overlay/ui/Shell.h v035_overlay/ui/Shell.cpp v035_overlay/ui/pages tests/test_dpop035_settings_contract.py
git commit -m "feat: consume committed settings at runtime"
```

---

### Task 6: Implement real tray and close behavior instead of a decorative checkbox

**Files:**
- Create: `v035_overlay/ui/TrayIcon.h`
- Create: `v035_overlay/ui/TrayIcon.cpp`
- Modify: `v035_overlay/ui/Shell.h`
- Modify: `v035_overlay/ui/Shell.cpp`
- Modify: `tests/test_dpop035_settings_contract.py`

**Interfaces:**
- Produces: `TrayIcon::Install`, `TrayIcon::Remove`, `TrayIcon::ShowMenu`, restore and exit command ids.
- Consumes: `trayEnabled`, `CloseBehavior`.

- [ ] **Step 1: Add failing tray contracts**

```python
def test_tray_is_real_and_close_behavior_is_consumed():
    tray = (ROOT / "v035_overlay/ui/TrayIcon.cpp").read_text(encoding="utf-8")
    shell = (ROOT / "v035_overlay/ui/Shell.cpp").read_text(encoding="utf-8")
    assert "Shell_NotifyIconW" in tray
    assert "NIM_ADD" in tray and "NIM_DELETE" in tray
    assert "CloseBehavior::Exit" in shell
    assert "CloseBehavior::MinimizeToTray" in shell
    assert "CloseBehavior::Ask" in shell
```

- [ ] **Step 2: Implement tray lifecycle**

`TrayIcon::Install` sets `NOTIFYICONDATAW::hWnd`, id, callback message, icon and tooltip `DPopCleaner 0.3.5 BETA R1`, calls `Shell_NotifyIconW(NIM_ADD, ...)`, then sets version `NOTIFYICON_VERSION_4`.

Context menu has exactly:
- `Открыть DPopCleaner` → show/restore/foreground main window;
- separator;
- `Выход` → post dedicated explicit-exit command that bypasses minimize-to-tray close policy.

`Remove()` is idempotent and called on real application shutdown.

- [ ] **Step 3: Implement close policy**

On `WM_CLOSE`:
- `Exit`: normal close.
- `MinimizeToTray`: if tray enabled, `ShowWindow(hwnd, SW_HIDE)` and keep process alive; if tray is unavailable, exit rather than trapping an invisible process.
- `Ask`: show `Закрыть DPopCleaner?` with three choices implemented as a TaskDialog when available: `Выйти`, `Свернуть в трей`, `Отмена`; hide-to-tray choice is disabled/falls back to exit when tray is disabled.

Explicit tray `Выход` always exits after requesting/cancelling active workers through the shell's existing shutdown path.

- [ ] **Step 4: Apply tray changes immediately**

`ApplyRuntimeSettings()` installs/removes the tray icon when `trayEnabled` changes. If settings change from `MinimizeToTray` to tray disabled, validation prevents persisting the invalid combination.

- [ ] **Step 5: Build/contracts and commit**

```bash
python -m pytest tests/test_dpop035_settings_contract.py -q
```

```powershell
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Expected: PASS.

```bash
git add v035_overlay/ui/TrayIcon.h v035_overlay/ui/TrayIcon.cpp v035_overlay/ui/Shell.h v035_overlay/ui/Shell.cpp tests/test_dpop035_settings_contract.py
git commit -m "feat: add real tray and close behavior"
```

---

### Task 7: Verify Settings save/reload, unsaved state and candidate screenshots

**Files:**
- Create: `tools/dpop035_settings_smoke.ps1`
- Modify: `.github/workflows/DPopCleaner_0.3.5_CANDIDATE.yml`
- Modify: `tests/test_dpop035_settings_contract.py`

**Interfaces:**
- Consumes: built DPopCleaner, isolated test LocalAppData, UI automation hooks used by existing shell smoke tooling.
- Produces: settings smoke evidence and screenshots; no release.

- [ ] **Step 1: Add final static contracts**

Require:
- no `Язык: Русский` / static theme marker;
- all five section labels;
- persistent four-button action row;
- `SettingsStore`, controller and tray source included by generated CMake;
- `settings.json.tmp`, `ReplaceFileW` and `MoveFileExW` present in SettingsStore;
- `monitorInstallations` absent from the 0.3.5 SettingsPage.

- [ ] **Step 2: Write PowerShell settings smoke**

`tools/dpop035_settings_smoke.ps1` must run DPopCleaner with an isolated LocalAppData test root or supported test override and perform this deterministic sequence:
1. open Settings from gear;
2. switch through all five sections and assert each unique section control is visible;
3. set large-file threshold to `777`, duplicate threshold to `33`, enable Quick Guard startup, add a temporary exclusion path;
4. click `Применить`; assert Settings remains open;
5. verify `settings.json` exists and contains `777`, `33`, quick-guard true and the exclusion;
6. change threshold to `888`, click `Отмена`; reopen Settings and assert value is still `777`;
7. click `По умолчанию`; assert UI shows `500` but disk file still contains `777`; click `Отмена`;
8. reopen, set `888`, click `Сохранить`; assert Settings closes and JSON contains `888`;
9. restart application; assert `888` reloads;
10. test minimize-to-tray then restore from tray;
11. close cleanly and remove the temporary settings root.

Exit non-zero on any failed assertion.

- [ ] **Step 3: Run all unit/contract tests**

```bash
python -m pytest tests/test_dpop035_migrate.py tests/test_dpop035_disk_contract.py tests/test_dpop035_settings_contract.py -q
```

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Expected: PASS.

- [ ] **Step 4: Run settings smoke**

```powershell
powershell -ExecutionPolicy Bypass -File tools\dpop035_settings_smoke.ps1 -ExePath build\bin\Release\DPopCleaner.exe
```

Expected: exit 0.

- [ ] **Step 5: Extend candidate evidence**

Candidate artifacts must include screenshots at 1100×700 and 1200×850 for:
- Settings / Основное;
- Settings / Очистка;
- Settings / Память;
- Settings / Защита;
- Settings / Исключения.

Also include `settings-smoke.txt`, CTest output, migration report, EXE hashes. Workflow must not create a GitHub release, update the website, or mutate `update/beta.json`.

- [ ] **Step 6: Commit**

```bash
git add tools/dpop035_settings_smoke.ps1 tests/test_dpop035_settings_contract.py .github/workflows/DPopCleaner_0.3.5_CANDIDATE.yml
git commit -m "test: verify DPopCleaner 0.3.5 settings candidate"
```

---

### Task 8: Full 0.3.5 candidate gate before any publication

**Files:**
- Modify only candidate workflow/evidence scripts if failures expose gaps.
- Do not modify release/site manifests in this task.

**Interfaces:**
- Consumes: completed Foundation + Disk plan and Tasks 1–7 above.
- Produces: reviewable 0.3.5 BETA R1 candidate evidence.

- [ ] **Step 1: Prepare from a clean checkout**

```powershell
python tools\dpop035_migrate.py --repository . --output artifacts\035-final --workspace C:\temp\dpop035-final --build
```

Expected: report says target `0.3.5 BETA R1`, UX donor `v033`, backend donor `v034`, and lists only allowed modern backend roots plus `v035_overlay` files.

- [ ] **Step 2: Run every test suite**

```bash
python -m pytest tests/test_dpop033_migrate.py tests/test_dpop034_migrate.py tests/test_dpop035_migrate.py tests/test_dpop035_disk_contract.py tests/test_dpop035_settings_contract.py -q
```

```powershell
ctest --test-dir artifacts\035-final\build -C Release --output-on-failure
```

Expected: all PASS.

- [ ] **Step 3: Run real app smoke tests**

```powershell
powershell -ExecutionPolicy Bypass -File tools\dpop035_disk_smoke.ps1 -ExePath artifacts\035-final\build\bin\Release\DPopCleaner.exe
powershell -ExecutionPolicy Bypass -File tools\dpop035_settings_smoke.ps1 -ExePath artifacts\035-final\build\bin\Release\DPopCleaner.exe
```

Expected: both exit 0 and app closes without orphan worker/tray process.

- [ ] **Step 4: Verify product identity**

PowerShell evidence checks file/product version `0.3.5.1`, visible shell label `0.3.5 BETA R1`, and confirms no target-visible `0.3.4 BETA R2` identity remains except historical docs/log fixtures outside exported `v035`.

- [ ] **Step 5: Stop for manual screenshot review**

Do not publish. The next separate release task is allowed only after the Disk and all five Settings screenshots plus test evidence have been manually approved.

- [ ] **Step 6: Commit any candidate-only verification changes**

```bash
git add .github/workflows/DPopCleaner_0.3.5_CANDIDATE.yml tools tests
git commit -m "ci: gate DPopCleaner 0.3.5 candidate for visual review"
```

---

## Plan Self-Review Results

- Spec coverage: typed storage, schema versioning, atomic save, corrupt fallback, safe defaults, five horizontal sections, no fake language/theme, exclusions, thresholds, Apply/Save/Cancel/Defaults, dirty-close prompt, real startup hooks, background analysis, RAM automation, real tray/close behavior, cleanup consumption, and candidate screenshots are each assigned to explicit tasks.
- Placeholder scan: no deferred implementation markers are used; unsupported installation watching is explicitly omitted rather than represented by a fake control.
- Type consistency: `AppSettings`, `SettingsLoadResult`, `CloseBehavior`, `MemoryScope`, `SettingsController`, and SettingsStore function names are consistent through Tasks 1–8.
