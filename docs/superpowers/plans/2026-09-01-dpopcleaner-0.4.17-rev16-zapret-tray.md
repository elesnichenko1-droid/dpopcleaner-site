# DPopCleaner 0.4.17 rev.16 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Выпустить DPopCleaner 0.4.17 rev.16 с одной рабочей tray-иконкой с RAM badge, реально проверяемым Zapret lifecycle, единым light/dark оформлением Zapret-кнопок и журналом, скрытым только на вкладке Zapret.

**Architecture:** Frozen `DPopCleaner.Core.exe` 0.2.14 остаётся byte-identical. Все изменения выполняются в `SimpleUpdate`: tray-host сохраняет одну стабильную `(HWND,uID)` identity между core restart; Zapret получает отдельные presentation/journal helpers и installed functional smoke, который управляет теми же UI-действиями и проверяет фактические Windows process/service states. Release rev.16 блокируется, если любой из runtime/installed/UI gates не проходит.

**Tech Stack:** C# .NET Framework 4.8 WinForms/Win32 P/Invoke, PowerShell 7 smoke tests on Windows Server 2022 GitHub Actions, Python contract tests, Inno Setup 6, Flowseal Zapret 1.10.2.

**Spec:** `docs/superpowers/specs/2026-09-01-dpopcleaner-0.4.17-rev16-zapret-tray-design.md`

## Global Constraints

- `DPopCleaner.Core.exe` / frozen core 0.2.14 не изменяется; Git blob остаётся `efd0eff1f4962319282363fa85595c25e0cebe11`.
- Общий дизайн DPopCleaner 0.2.14 сохраняется; другие вкладки не переделываются.
- Flowseal Zapret остаётся строго 1.10.2; bundled strategy count остаётся 22.
- Сохраняются исправления rev.13–rev.15: UAC/code 740, language restart recovery, Settings bridge, tray restart recovery, native Zapret version, ZapretScreenFix, Disk Analyzer, Restore Center и updater.
- На Zapret журнал скрывается только визуально; оригинальные HWND не уничтожаются.
- Никакой публикации rev.16 до GREEN unit/contracts + runtime smokes + installed smokes + production candidate + live manifest/SHA verification.
- Любой functional smoke, создающий/запускающий Zapret service, `winws.exe` или WinDivert, обязан выполнять cleanup в `finally`.

---

## File Structure

**New files**
- `v0417/src/SimpleUpdate/ZapretPresentationHost.cs` — единая light/dark theme + geometry + journal visibility policy для Zapret.
- `v0417/src/SimpleUpdate/ZapretRuntimeState.cs` — маленькая модель наблюдаемого Zapret runtime состояния для диагностики bridge.
- `v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs` — source/behavior contracts для single tray identity, presentation host и restart ownership.
- `tools/dpop0417_rev16_single_tray_smoke.ps1` — installed tray identity/restart/Explorer smoke.
- `tools/dpop0417_rev16_zapret_functional_smoke.ps1` — installed UI-driven install/start/status/stop/change-strategy/remove lifecycle.
- `tools/dpop0417_rev16_zapret_presentation_smoke.ps1` — light/dark layout + journal visibility smoke.
- `tests/test_dpop0417_rev16_release_contract.py` — revision/publisher/release gate contract.

**Modified files**
- `v0417/src/SimpleUpdate/LauncherContext.cs` — сохраняет tray-host через core restart; управляет `ZapretPresentationHost`; вызывает presentation refresh только на Zapret.
- `v0417/src/SimpleUpdate/TrayRamBadgeHost.cs` — стабильная identity не пересоздаётся при core restart; suppression выполняется до publish; exposes diagnostic identity.
- `v0417/src/SimpleUpdate/BridgeTrayGhostSuppressor.cs` — reconciliation только лишних launcher identities; canonical `(HWND,uID)` никогда не удаляется.
- `v0417/src/SimpleUpdate/ZapretEnhancementHost.cs` — больше не навязывает тему; action buttons отдают presentation host-у layout/theme; bridge actions возвращают точную диагностику ошибки.
- `v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs` — удалить hard-coded dark owner-draw path после переноса ответственности в `ZapretPresentationHost` либо оставить как thin version-label helper без самостоятельного theme policy.
- `v0417/src/SimpleUpdate/Program.cs` — `CurrentRevision = 16` только после GREEN feature gates.
- `.github/workflows/DPopCleaner_0.4.17_SIMPLEUPDATE.yml` — новые runtime rev.16 smokes.
- `.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml` — новые installed rev.16 smokes.
- `.github/workflows/publish-dpopcleaner-0.4.17.yml` — обязательные rev.16 release gates и publication identity.
- `version.json`, `update/stable.json`, `release-manifest.js`, `index.html`, `release/RELEASE_NOTES_0.4.17.md` — rev.16 identity/release notes после полного GREEN.

---

### Task 1: Preserve one tray HWND/uID across core restart

**Files:**
- Modify: `v0417/src/SimpleUpdate/LauncherContext.cs`
- Modify: `v0417/src/SimpleUpdate/TrayRamBadgeHost.cs`
- Modify: `v0417/src/SimpleUpdate/BridgeTrayGhostSuppressor.cs`
- Create: `v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs`

**Interfaces:**
- Produces: `TrayRamBadgeHost.MessageWindowHandle : IntPtr`, `TrayRamBadgeHost.IconId : uint`, `TrayRamBadgeHost.ReattachMainWindow(IntPtr mainWindow)`.
- `LauncherContext.ResetBridgeForRestartedCore()` must NOT dispose `_trayRamHost`.
- `TrayRamBadgeHost.Update(int coreProcessId, IntPtr mainWindow, bool enabled)` remains the external update entry point.

- [ ] **Step 1: Write failing tray ownership contracts**

Add MSTests that read production source and require the lifetime invariant:

```csharp
[TestMethod]
public void Core_restart_keeps_the_same_tray_host_identity()
{
    var source = ReadSource("LauncherContext.cs");
    var reset = SliceMethod(source, "private void ResetBridgeForRestartedCore()");
    StringAssert.DoesNotContain(reset, "_trayRamHost.Dispose()");
    StringAssert.Contains(source, "_trayRamHost.ReattachMainWindow");
}

[TestMethod]
public void Tray_host_exposes_one_constant_icon_id()
{
    var source = ReadSource("TrayRamBadgeHost.cs");
    StringAssert.Contains(source, "private const uint TrayIconId = 1;");
    StringAssert.Contains(source, "internal IntPtr MessageWindowHandle");
    StringAssert.Contains(source, "internal uint IconId");
}
```

Implement `ReadSource` by walking upward from `AppDomain.CurrentDomain.BaseDirectory` until `v0417/src/SimpleUpdate` exists, matching the existing `SettingsLanguageContractTests` pattern. `SliceMethod` scans from the method signature to the next same-indent method declaration so the negative assertion is scoped to restart reset only.

- [ ] **Step 2: Run RED**

Run:

```powershell
dotnet test v0417/tests/SimpleUpdate.Tests/SimpleUpdate.Tests.csproj -c Release --nologo --filter Rev16TrayAndZapretContractTests
```

Expected: FAIL because `ResetBridgeForRestartedCore()` currently disposes `_trayRamHost` and the diagnostic properties/Reattach method do not exist.

- [ ] **Step 3: Keep tray host alive during core restart**

Change `ResetBridgeForRestartedCore()` so it disposes Settings/Zapret bridge objects, resets `_mainWindow`, but intentionally preserves `_trayRamHost`:

```csharp
private void ResetBridgeForRestartedCore()
{
    if (_settingsHost != null) _settingsHost.Dispose();
    if (_zapretVisualHost != null) _zapretVisualHost.Dispose();
    if (_zapretHost != null) _zapretHost.Dispose();

    _settingsHost = null;
    _zapretVisualHost = null;
    _zapretHost = null;
    _settingsHostBounds = null;
    _mainWindow = IntPtr.Zero;
    _traySettingKnown = false;
    _trayEnabled = false;
    _iconApplied = false;

    if (_trayRamHost != null)
        _trayRamHost.ReattachMainWindow(IntPtr.Zero);
}
```

In `TrayRamBadgeHost` add:

```csharp
internal IntPtr MessageWindowHandle { get { return _messageWindow.Handle; } }
internal uint IconId { get { return TrayIconId; } }

internal void ReattachMainWindow(IntPtr mainWindow)
{
    if (_disposed) return;
    _mainWindow = mainWindow;
}
```

The next normal `Update()` receives the successor main HWND without recreating `_messageWindow` or `uID`.

- [ ] **Step 4: Reconcile before publish, never after recreating identity**

In `Update()` keep this order:

```csharp
LegacyTrayIconSuppressor.RemoveIconsForProcess(coreProcessId);
BridgeTrayGhostSuppressor.CleanupCurrentProcess(_messageWindow.Handle, TrayIconId);
PublishTrayIcon();
```

Change `BridgeTrayGhostSuppressor.CleanupCurrentProcess` signature to:

```csharp
internal static void CleanupCurrentProcess(IntPtr keepWindow, uint keepIconId)
```

and remove its title lookup. It must compare every current-launcher tray entry against the exact supplied canonical tuple and delete every other current-launcher entry. `keepWindow == IntPtr.Zero` returns without deleting anything.

Remove the second `BridgeTrayGhostSuppressor.CleanupCurrentProcess()` call from `LauncherContext.UpdateTrayRamBadge()` so reconciliation lives in one place and cannot run with a different canonical identity.

- [ ] **Step 5: Run GREEN unit/contracts**

Run:

```powershell
dotnet test v0417/tests/SimpleUpdate.Tests/SimpleUpdate.Tests.csproj -c Release --nologo
```

Expected: all SimpleUpdate tests PASS.

- [ ] **Step 6: Commit**

```bash
git add v0417/src/SimpleUpdate/LauncherContext.cs v0417/src/SimpleUpdate/TrayRamBadgeHost.cs v0417/src/SimpleUpdate/BridgeTrayGhostSuppressor.cs v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs
git commit -m "fix(rev16): preserve single tray identity across core restart"
```

---

### Task 2: Prove single tray identity on a real installed package

**Files:**
- Create: `tools/dpop0417_rev16_single_tray_smoke.ps1`
- Modify: `.github/workflows/DPopCleaner_0.4.17_SIMPLEUPDATE.yml`
- Modify: `.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml`

**Interfaces:**
- Smoke output markers: `REV16_SINGLE_TRAY_INITIAL_OK`, `REV16_SINGLE_TRAY_LANGUAGE_RESTART_OK`, `REV16_SINGLE_TRAY_EXPLORER_RESTART_OK`.
- Evidence JSON: `_release/0.4.17/evidence/rev16-tray/rev16-tray-report.json`.

- [ ] **Step 1: Write the failing smoke**

Reuse the Explorer toolbar introspection structs from `dpop0417_rev13_uac_tray_smoke.ps1`, but capture `(hwnd,uID,ownerPid)` before and after each event. Required assertions:

```powershell
Assert-TrayState -ExpectedBridge 1 -ExpectedCore 0
$initial = Get-CanonicalBridgeTrayIdentity

Invoke-LanguageRestart -Language 'English'
Assert-TrayState -ExpectedBridge 1 -ExpectedCore 0
$afterLanguage = Get-CanonicalBridgeTrayIdentity
if ($afterLanguage.Hwnd -ne $initial.Hwnd -or $afterLanguage.uID -ne $initial.uID) {
    throw "Tray identity changed across core restart: initial=$($initial.Hwnd)/$($initial.uID) after=$($afterLanguage.Hwnd)/$($afterLanguage.uID)"
}

Stop-Process -Name explorer -Force
Start-Process explorer.exe
Wait-ForTrayIdentity
Assert-TrayState -ExpectedBridge 1 -ExpectedCore 0
```

The smoke must also verify the canonical HWND window title equals `DPopCleaner.TrayRamBadgeHost` and `hIcon != 0` in Explorer tray data.

- [ ] **Step 2: Run against rev.15 to verify RED**

Build current installer and run:

```powershell
./tools/dpop0417_rev16_single_tray_smoke.ps1 -InstallerPath '_release/0.4.17/installer/DPopCleaner_Setup_0.4.17.exe' -OutputDir '_release/0.4.17/evidence/rev16-tray'
```

Expected on unfixed rev.15 behavior: FAIL when the core restart recreates the bridge tray HWND or leaves a duplicate/ghost identity.

- [ ] **Step 3: Add runtime and installed CI gates**

In `DPopCleaner_0.4.17_SIMPLEUPDATE.yml`, run the smoke against the authentic build payload if the script supports `-RootPath`; otherwise add a non-installed companion check that verifies stable HWND during artificial successor PID replacement.

In `DPopCleaner_0.4.17_FOUNDATION.yml`, after `Run installed package smoke`, add:

```yaml
- name: Run rev.16 single tray identity smoke
  shell: pwsh
  run: ./tools/dpop0417_rev16_single_tray_smoke.ps1 -InstallerPath $env:DPOP0417_INSTALLER -OutputDir '_release/0.4.17/evidence/rev16-tray'
```

- [ ] **Step 4: Run GREEN**

Expected markers:

```text
REV16_SINGLE_TRAY_INITIAL_OK
REV16_SINGLE_TRAY_LANGUAGE_RESTART_OK
REV16_SINGLE_TRAY_EXPLORER_RESTART_OK
```

- [ ] **Step 5: Commit**

```bash
git add tools/dpop0417_rev16_single_tray_smoke.ps1 .github/workflows/DPopCleaner_0.4.17_SIMPLEUPDATE.yml .github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml
git commit -m "test(rev16): enforce one tray identity"
```

---

### Task 3: Add a real Zapret installed lifecycle test before changing Zapret behavior

**Files:**
- Create: `tools/dpop0417_rev16_zapret_functional_smoke.ps1`
- Modify: `.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml`

**Interfaces:**
- Smoke markers: `REV16_ZAPRET_INSTALL_OK`, `REV16_ZAPRET_START_OK`, `REV16_ZAPRET_STOP_OK`, `REV16_ZAPRET_STRATEGY_CHANGE_OK`, `REV16_ZAPRET_REMOVE_OK`.
- Evidence: `rev16-zapret-functional-report.json` containing selected strategy, second strategy, UI status text, `winws.exe` command line, service states and cleanup result.

- [ ] **Step 1: Build UI automation around the installed DPopCleaner window**

Copy the Win32 child enumeration/ComboBox helpers from `dpop0417_zapret_ui_smoke.ps1`. Start the installed `SimpleUpdate.exe`, find `DPopCleaner.Core.exe` by exact installed path, click the native `Zapret` navigation button, then locate lifecycle buttons by visible Russian captions for this test run:

```powershell
$installButton = Find-VisibleButton 'Установить сервис'
$startButton   = Find-VisibleButtonPrefix 'Запустить win'
$stopButton    = Find-VisibleButtonPrefix 'Остановить'
$statusButton  = Find-VisibleButton 'Статус'
$removeButton  = Find-VisibleButton 'Удалить сервисы'
```

Do not invoke `service.bat` directly for the primary test; the point is to verify the same UI path the user uses.

- [ ] **Step 2: Define observable Windows state helpers**

Implement:

```powershell
function Get-ZapretServiceState {
    $zapret = Get-CimInstance Win32_Service -Filter "Name='zapret'" -ErrorAction SilentlyContinue
    $divert = @(Get-CimInstance Win32_SystemDriver -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^WinDivert' })
    [pscustomobject]@{ Zapret=$zapret; WinDivert=$divert }
}

function Get-BundledWinws {
    @(Get-CimInstance Win32_Process -Filter "Name='winws.exe'" -ErrorAction SilentlyContinue |
      Where-Object { $_.ExecutablePath -and ([IO.Path]::GetFullPath($_.ExecutablePath) -eq [IO.Path]::GetFullPath($script:WinwsPath)) })
}
```

Read `CommandLine` from `Win32_Process` so strategy changes are observable.

- [ ] **Step 3: Implement lifecycle assertions**

Run in this exact order:

1. Select `general.bat` in the ComboBox.
2. Click install service; wait until service `zapret` exists and is `Running` or until UI explicitly reports failure.
3. Click Status; assert upper status block contains service/run state consistent with Windows service state.
4. Remove service so standalone strategy can be tested cleanly.
5. Click Start; wait for bundled `winws.exe`; capture command line; assert upper status block reports `ON`.
6. Click Stop; wait until bundled `winws.exe` disappears; assert upper status block reports `OFF`.
7. Select a second real `general*.bat` entry; Start again; assert `winws.exe` command line differs from the first strategy command line.
8. Stop; click Remove Services; assert no `zapret` service and no bundled `winws.exe` remain.

Always execute cleanup in `finally`:

```powershell
try { Click-IfVisible $removeButton } catch { }
Get-BundledWinws | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
sc.exe stop zapret | Out-Null
sc.exe delete zapret | Out-Null
foreach ($name in @('WinDivert','WinDivert14')) { sc.exe stop $name | Out-Null; sc.exe delete $name | Out-Null }
```

- [ ] **Step 4: Run RED on current rev.15**

Run installed smoke locally/CI candidate. Expected: at least one lifecycle assertion reproduces the user's Zapret failure. Preserve the evidence JSON and exact failing step; do not modify production behavior until this RED evidence exists.

- [ ] **Step 5: Add Foundation gate**

```yaml
- name: Run rev.16 real Zapret lifecycle smoke
  shell: pwsh
  run: ./tools/dpop0417_rev16_zapret_functional_smoke.ps1 -InstallerPath $env:DPOP0417_INSTALLER -OutputDir '_release/0.4.17/evidence/rev16-zapret-functional'
```

- [ ] **Step 6: Commit the RED harness**

```bash
git add tools/dpop0417_rev16_zapret_functional_smoke.ps1 .github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml
git commit -m "test(rev16): reproduce real Zapret lifecycle failure"
```

---

### Task 4: Repair the Zapret lifecycle path exposed by Task 3

**Files:**
- Create: `v0417/src/SimpleUpdate/ZapretRuntimeState.cs`
- Modify: `v0417/src/SimpleUpdate/ZapretEnhancementHost.cs`
- Modify: `v0417/src/SimpleUpdate/LauncherContext.cs`
- Test: `v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs`
- Test: `tools/dpop0417_rev16_zapret_functional_smoke.ps1`

**Interfaces:**
- `ZapretRuntimeState.Read(string applicationRoot) : ZapretRuntimeState`
- Properties: `bool BundledWinwsRunning`, `bool ZapretServiceExists`, `bool ZapretServiceRunning`, `string BundledWinwsCommandLine`.
- `ZapretEnhancementHost.RefreshRuntimeStatus()` writes the upper existing status control only; it does not create a second log/status panel.

- [ ] **Step 1: Write RED unit contract for runtime state and status refresh**

```csharp
[TestMethod]
public void Zapret_bridge_has_runtime_state_reader_and_status_refresh()
{
    var enhancement = ReadSource("ZapretEnhancementHost.cs");
    var runtime = ReadSource("ZapretRuntimeState.cs");
    StringAssert.Contains(runtime, "internal sealed class ZapretRuntimeState");
    StringAssert.Contains(runtime, "internal static ZapretRuntimeState Read(string applicationRoot)");
    StringAssert.Contains(enhancement, "RefreshRuntimeStatus");
}
```

Run and expect FAIL because the runtime state file/method do not exist.

- [ ] **Step 2: Implement `ZapretRuntimeState` using exact bundled path**

Use `System.Diagnostics.Process.GetProcessesByName("winws")` and compare `MainModule.FileName` to `<applicationRoot>\Zapret\bin\winws.exe`. Query services without adding a new dependency by launching `sc.exe query zapret` and `sc.exe query WinDivert` through a small private helper with `UseShellExecute=false`, redirected stdout/stderr and a 5 second timeout. Map only observable states; do not infer success from UI text.

- [ ] **Step 3: Make bridge actions refresh factual status**

After each bridge-owned action in `WindowProc`, call:

```csharp
RefreshRuntimeStatus();
```

`RefreshRuntimeStatus()` finds the existing upper Zapret status Static by structure: visible Static below the version/status header and above the Strategy row. Write one concise line derived from `ZapretRuntimeState`, e.g. `winws.exe (DPopCleaner Zapret): ON` or `OFF`; preserve the frozen status/header controls and do not create another text area.

For `RepairConnection`, keep `load_user_lists`, TCP timestamps and DNS flush, but include exact command/exit-code in thrown errors by changing `RunHidden` to return a small result or to throw:

```text
Команда: ipconfig.exe /flushdns
Код выхода: <n>
```

- [ ] **Step 4: If Task 3 proves a frozen lifecycle button is broken, proxy only that lifecycle action**

The allowed repair boundary is explicit: hide the broken native lifecycle button and create a same-bounds bridge proxy in `ZapretEnhancementHost`; route it through the bundled Flowseal 1.10.2 files from `_applicationRoot\Zapret`. Do not modify `DPopCleaner.Core.exe` and do not replace working native lifecycle buttons.

For standalone Start/Stop, the bridge implementation must use the selected `general*.bat` path for Start and terminate only `winws.exe` whose executable path equals the bundled `Zapret\bin\winws.exe` for Stop. For install/remove, continue using the upstream 1.10.2 `service.bat` UI path unless RED evidence specifically proves that frozen wiring calls the wrong path; then the proxy launches that same bundled `service.bat` visibly so the upstream `Install Service` / `Remove Services` menu remains authoritative.

- [ ] **Step 5: Run functional GREEN**

Run:

```powershell
./tools/dpop0417_rev16_zapret_functional_smoke.ps1 -InstallerPath '_release/0.4.17/installer/DPopCleaner_Setup_0.4.17.exe' -OutputDir '_release/0.4.17/evidence/rev16-zapret-functional'
```

Expected all five markers and cleanup with no remaining bundled `winws.exe`/`zapret` service.

- [ ] **Step 6: Run existing Zapret regression suite**

```powershell
python tests/test_dpop0417_zapret_bundle_contract.py -v
python tests/test_dpop0417_zapret_screen_fix_contract.py -v
./tools/dpop0417_zapret_ui_smoke.ps1 -RootPath '_release/0.4.17/stage' -OutputDir '_release/0.4.17/evidence/zapret-ui'
```

Expected PASS, version 1.10.2, 22 strategies, frozen core hash unchanged.

- [ ] **Step 7: Commit**

```bash
git add v0417/src/SimpleUpdate/ZapretRuntimeState.cs v0417/src/SimpleUpdate/ZapretEnhancementHost.cs v0417/src/SimpleUpdate/LauncherContext.cs v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs
git commit -m "fix(rev16): make Zapret lifecycle observable and reliable"
```

---

### Task 5: Unify Zapret light/dark button styling and fit all bridge actions inside the panel

**Files:**
- Create: `v0417/src/SimpleUpdate/ZapretPresentationHost.cs`
- Modify: `v0417/src/SimpleUpdate/ZapretEnhancementHost.cs`
- Modify: `v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs`
- Modify: `v0417/src/SimpleUpdate/LauncherContext.cs`
- Test: `v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs`
- Create: `tools/dpop0417_rev16_zapret_presentation_smoke.ps1`

**Interfaces:**
- `ZapretPresentationHost(IntPtr parent, ZapretEnhancementHost enhancement)`
- `void Show()` / `void Hide()` / `void Refresh()` / `void Dispose()`.
- `ZapretEnhancementHost.ActionToolbarHandle : IntPtr`, `UpdateToolbarHandle : IntPtr` expose only handles required for layout.

- [ ] **Step 1: Write RED contracts against forced dark styling**

```csharp
[TestMethod]
public void Zapret_buttons_use_one_presentation_host_not_forced_dark_theme()
{
    var enhancement = ReadSource("ZapretEnhancementHost.cs");
    var polish = ReadSource("ZapretVisualPolishHost.cs");
    Assert.IsFalse(enhancement.Contains("SetWindowTheme(button, \"DarkMode_Explorer\""));
    Assert.IsFalse(polish.Contains("EnsureDarkBridgeButtons"));
    StringAssert.Contains(ReadSource("ZapretPresentationHost.cs"), "internal sealed class ZapretPresentationHost");
}
```

Expected RED on rev.15 because both hard-coded dark paths exist.

- [ ] **Step 2: Move theme decision into `ZapretPresentationHost`**

Determine current theme from the frozen Theme ComboBox rather than guessing from OS global theme. Reuse `NativeBridge.GetChildren(_parent)`; select the visible Settings theme ComboBox only when present, otherwise cache the last observed theme and default to the current native Zapret button appearance. Represent state as:

```csharp
private enum ZapretTheme { Light, Dark }
```

Apply one theme consistently to all bridge-owned Zapret buttons. For light use `SetWindowTheme(button, "Explorer", null)`; for dark use `SetWindowTheme(button, "DarkMode_Explorer", null)`. Remove `BS_OWNERDRAW` forcing from `ZapretVisualPolishHost`; it may remain only for installed Zapret version label refresh if needed.

- [ ] **Step 3: Replace fixed 709 px toolbar math with measured layout**

Expose the action toolbar from `ZapretEnhancementHost` and let presentation calculate width from the actual Zapret page right edge (`Тесты` native button right edge / parent client width) and left edge after the `Дополнительно` heading.

Measure each caption with `TextRenderer.MeasureText(text, font)` and allocate:

```csharp
var widths = captions.Select(c => Math.Max(140, TextRenderer.MeasureText(c, font).Width + 24)).ToArray();
var available = right - left;
var gaps = ButtonGap * (widths.Length - 1);
ScaleWidthsToFit(widths, available - gaps, minimumWidth: 120);
```

`ScaleWidthsToFit` proportionally shrinks only if needed, never below 120 px; if even four minimum widths do not fit, reduce gap from 8 to 4 before failing. Apply all child bounds with `DeferWindowPos` and then one redraw to avoid mixed intermediate states.

- [ ] **Step 4: Apply layout/theme atomically on every Zapret tick when state changed**

Cache `(theme,parentClientWidth,languageVersion)` in `ZapretPresentationHost`; `Refresh()` returns immediately when unchanged. On change, update all bridge buttons and toolbar bounds in one pass. `LauncherContext.UpdateZapretEnhancements()` calls presentation `Show/Refresh` after enhancement host exists.

- [ ] **Step 5: Create presentation smoke**

The PowerShell smoke opens Zapret, captures every visible Button `text,id,bounds`, switches Light → Dark → Light through the native Theme ComboBox, and asserts:

```powershell
if ($button.Left -lt $pageLeft -or $button.Right -gt $pageRight) { throw 'Zapret button outside panel' }
if ($button.Width -lt 120) { throw 'Zapret button clipped below minimum width' }
if (Test-AnyOverlap $buttons) { throw 'Zapret buttons overlap' }
```

For bridge IDs `1720..1725`, inspect `GetWindowTheme`/theme class where possible; otherwise verify style flags and capture screenshot evidence for both themes. Emit `REV16_ZAPRET_LIGHT_LAYOUT_OK`, `REV16_ZAPRET_DARK_LAYOUT_OK`, `REV16_ZAPRET_THEME_ROUNDTRIP_OK`.

- [ ] **Step 6: Run GREEN**

```powershell
dotnet test v0417/tests/SimpleUpdate.Tests/SimpleUpdate.Tests.csproj -c Release --nologo
./tools/dpop0417_rev16_zapret_presentation_smoke.ps1 -RootPath '_release/0.4.17/stage' -OutputDir '_release/0.4.17/evidence/rev16-zapret-presentation'
```

Expected PASS and no mixed dark/light bridge buttons.

- [ ] **Step 7: Commit**

```bash
git add v0417/src/SimpleUpdate/ZapretPresentationHost.cs v0417/src/SimpleUpdate/ZapretEnhancementHost.cs v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs v0417/src/SimpleUpdate/LauncherContext.cs v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs tools/dpop0417_rev16_zapret_presentation_smoke.ps1
git commit -m "fix(rev16): unify Zapret theme and layout"
```

---

### Task 6: Hide Journal only while Zapret is active and restore it byte-for-byte at HWND level

**Files:**
- Modify: `v0417/src/SimpleUpdate/ZapretPresentationHost.cs`
- Modify: `v0417/src/SimpleUpdate/LauncherContext.cs`
- Test: `v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs`
- Test: `tools/dpop0417_rev16_zapret_presentation_smoke.ps1`

**Interfaces:**
- `ZapretPresentationHost.Show()` hides journal controls.
- `ZapretPresentationHost.Hide()` restores exactly captured native visibility and bounds.
- No localized `"Журнал"` lookup is allowed for the primary journal discovery path.

- [ ] **Step 1: Write RED source contract for structural journal discovery**

```csharp
[TestMethod]
public void Zapret_journal_policy_is_structural_and_reversible()
{
    var source = ReadSource("ZapretPresentationHost.cs");
    StringAssert.Contains(source, "CaptureJournalControls");
    StringAssert.Contains(source, "RestoreJournalControls");
    Assert.IsFalse(source.Contains("FindChildByText(_parent, \"Журнал\""));
}
```

- [ ] **Step 2: Capture journal controls structurally**

On first visible Zapret frame, enumerate visible children below the row containing `NativeBridge.ZapretApplyButtonId`. Choose the largest visible multiline `Edit`/list-like control in the lower half of the client area as the journal body. Choose the nearest visible `Static` whose bottom is above the body top and horizontal ranges overlap as the journal heading. Capture for each:

```csharp
private sealed class NativeControlSnapshot
{
    internal IntPtr Handle;
    internal NativeBridge.ClientBounds Bounds;
    internal bool Visible;
}
```

Do not destroy, reparent or rewrite text.

- [ ] **Step 3: Hide and restore**

`Show()` calls `SW_HIDE` on captured heading/body after capture. `Hide()` restores captured bounds with `PositionChildWindow` and original visibility state. `Dispose()` calls restore before releasing references. On core restart a new presentation host captures the successor HWNDs from scratch.

- [ ] **Step 4: Extend presentation smoke**

On Zapret page assert the large lower journal body and its heading are hidden while the upper status block remains visible. Navigate to RAM or Settings and assert the same captured HWNDs return to their original bounds/visibility. Navigate back to Zapret and assert they hide again without cumulative coordinate drift.

Emit:

```text
REV16_ZAPRET_JOURNAL_HIDDEN_OK
REV16_OTHER_PAGE_JOURNAL_RESTORED_OK
REV16_ZAPRET_JOURNAL_ROUNDTRIP_OK
```

- [ ] **Step 5: Run GREEN**

Run the presentation smoke across Light and Dark themes and once after language/core restart. Expected all journal markers PASS and no changes to other page controls.

- [ ] **Step 6: Commit**

```bash
git add v0417/src/SimpleUpdate/ZapretPresentationHost.cs v0417/src/SimpleUpdate/LauncherContext.cs v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs tools/dpop0417_rev16_zapret_presentation_smoke.ps1
git commit -m "feat(rev16): hide Zapret journal only on Zapret page"
```

---

### Task 7: Add the integrated restart/theme/Zapret regression gate

**Files:**
- Modify: `tools/dpop0417_rev15_restart_recovery_smoke.ps1`
- Modify: `.github/workflows/DPopCleaner_0.4.17_SIMPLEUPDATE.yml`
- Modify: `.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml`

**Interfaces:**
- New marker: `REV16_RESTART_TRAY_ZAPRET_REATTACH_OK`.

- [ ] **Step 1: Extend the existing restart smoke rather than creating another launcher model**

After the successor core PID is attached, navigate to Zapret and assert:

```powershell
Assert-SingleTrayIdentity
Assert-ZapretBridgeButtonsVisible -Ids @(1720,1721,1722,1723)
Assert-ZapretButtonsInsidePanel
Assert-ZapretJournalHidden
```

Then leave Zapret and assert journal restoration.

- [ ] **Step 2: Execute two consecutive language restarts**

Run Russian → English → Russian. After each new core PID, require the same launcher PID and the same tray `(HWND,uID)`. Require newly attached Zapret native HWNDs but only one set of bridge button IDs.

- [ ] **Step 3: Run full SimpleUpdate workflow locally/CI**

```powershell
dotnet test v0417/tests/SimpleUpdate.Tests/SimpleUpdate.Tests.csproj -c Release --nologo
./tools/dpop0417_rev15_restart_recovery_smoke.ps1 -RootPath '_release/0.4.17/stage'
```

Expected: existing rev.15 markers plus `REV16_RESTART_TRAY_ZAPRET_REATTACH_OK`.

- [ ] **Step 4: Commit**

```bash
git add tools/dpop0417_rev15_restart_recovery_smoke.ps1 .github/workflows/DPopCleaner_0.4.17_SIMPLEUPDATE.yml .github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml
git commit -m "test(rev16): cover repeated restart tray and Zapret reattach"
```

---

### Task 8: Promote release identity to rev.16 only after all feature gates are GREEN

**Files:**
- Create: `tests/test_dpop0417_rev16_release_contract.py`
- Modify: `v0417/src/SimpleUpdate/Program.cs`
- Modify: `v0417/src/SimpleUpdate/LauncherContext.cs`
- Modify: `version.json`
- Modify: `update/stable.json`
- Modify: `release-manifest.js`
- Modify: `index.html`
- Modify: `release/RELEASE_NOTES_0.4.17.md`
- Modify: `.github/workflows/publish-dpopcleaner-0.4.17.yml`
- Modify: `.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml`

**Interfaces:**
- `Program.CurrentRevision == 16`.
- Release tag `v0.4.17-rev16`.
- Candidate artifact `dpopcleaner-0.4.17-rev16-release-candidate`.

- [ ] **Step 1: Write RED release contract**

The Python test must assert all release-facing surfaces agree on 16 and the publisher contains all three rev.16 installed gates:

```python
self.assertEqual(version['revision'], 16)
self.assertEqual(stable['revision'], 16)
self.assertIn('v0.4.17-rev16', workflow)
self.assertIn('dpop0417_rev16_single_tray_smoke.ps1', workflow)
self.assertIn('dpop0417_rev16_zapret_functional_smoke.ps1', workflow)
self.assertIn('dpop0417_rev16_zapret_presentation_smoke.ps1', workflow)
self.assertIn('revision=16', workflow)
```

Run and expect FAIL while production identity is still 15.

- [ ] **Step 2: Bump runtime/manifests/site/publisher atomically**

Set:

```csharp
internal const int CurrentRevision = 16;
```

Change User-Agent to `DPopCleaner-SimpleUpdate/0.4.17-rev16`, `version.json` and `update/stable.json` revision to 16, website/release manifest to rev.16, workflow `RELEASE_TAG` to `v0.4.17-rev16`, concurrency group and artifact names to rev16.

- [ ] **Step 3: Add production installed gates before publication payload**

After the normal installed package smoke and retained rev.15 regression smoke, add:

```yaml
- name: Run rev.16 single tray identity smoke
  shell: pwsh
  run: ./tools/dpop0417_rev16_single_tray_smoke.ps1 -InstallerPath $env:DPOP0417_INSTALLER -OutputDir '_release/0.4.17/evidence/rev16-tray'

- name: Run rev.16 real Zapret lifecycle smoke
  shell: pwsh
  run: ./tools/dpop0417_rev16_zapret_functional_smoke.ps1 -InstallerPath $env:DPOP0417_INSTALLER -OutputDir '_release/0.4.17/evidence/rev16-zapret-functional'

- name: Run rev.16 Zapret presentation smoke
  shell: pwsh
  run: ./tools/dpop0417_rev16_zapret_presentation_smoke.ps1 -InstallerPath $env:DPOP0417_INSTALLER -OutputDir '_release/0.4.17/evidence/rev16-zapret-presentation'
```

The Zapret functional smoke must finish cleanup before publication can proceed.

- [ ] **Step 4: Update release notes with exact user-facing fixes**

Release notes must state: one tray icon with RAM badge; bridge tray HWND persists across language restart; Zapret lifecycle is now functionally installed-tested; buttons share one theme/layout policy; Journal hidden only on Zapret; frozen core remains byte-identical; Flowseal remains 1.10.2/22 strategies.

- [ ] **Step 5: Run complete candidate verification**

Run:

```powershell
python tests/test_dpop0417_rev16_release_contract.py -v
Get-ChildItem tests -Filter 'test_dpop0417_*.py' | Sort-Object Name | ForEach-Object { python $_.FullName -v; if ($LASTEXITCODE -ne 0) { throw $_.Name } }
dotnet test v0417/tests/SimpleUpdate.Tests/SimpleUpdate.Tests.csproj -c Release --nologo
```

Then run the full PR workflows. Required exact-head GREEN:

- `DPopCleaner 0.4.17 SimpleUpdate`
- `DPopCleaner 0.4.17 Foundation`
- `DPopCleaner 0.4.17 rev.7 UI diagnostic`
- `Build, release and deploy DPopCleaner 0.4.17 rev.16` build-package job; publish skipped on PR.

- [ ] **Step 6: Review diff and frozen core evidence**

Require staged hash output exactly:

```text
efd0eff1f4962319282363fa85595c25e0cebe11
```

Verify no `v0417/original/DPopCleaner.exe` or frozen binary content change is present in the diff.

- [ ] **Step 7: Commit**

```bash
git add tests/test_dpop0417_rev16_release_contract.py v0417/src/SimpleUpdate/Program.cs v0417/src/SimpleUpdate/LauncherContext.cs version.json update/stable.json release-manifest.js index.html release/RELEASE_NOTES_0.4.17.md .github/workflows/publish-dpopcleaner-0.4.17.yml .github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml
git commit -m "release: prepare DPopCleaner 0.4.17 rev.16"
```

---

### Task 9: Merge and verify the published rev.16 asset

**Files:**
- No production code changes expected.

**Interfaces:**
- Main SHA must equal the exact verified PR head or a GitHub merge commit containing that exact head.
- Published tag: `v0.4.17-rev16`.

- [ ] **Step 1: Fresh verification immediately before integration**

Fetch PR info and workflow runs for exact head. Do not rely on older green runs. Require all four workflows `success` and PR mergeable.

- [ ] **Step 2: Merge using `expected_head_sha`**

Use normal PR merge when possible. If connector draft/GraphQL bug recurs, do not force-update `main`; first prove `behind_by=0` and merge-base equals current `main`, then only a non-force fast-forward of the exact GREEN head is acceptable.

- [ ] **Step 3: Watch production push run**

Require production build-package to pass all installed rev.16 gates, then publish job to pass GitHub Release, Pages and `Verify live manifest, installer SHA256 and current screenshot`.

- [ ] **Step 4: Verify release asset from GitHub Release**

Fetch `v0.4.17-rev16`; record exact asset size and `sha256:` digest from the Release API. Confirm `draft=false`, `prerelease=false`, and `target_commitish` equals the integrated production SHA.

- [ ] **Step 5: Verify live stable manifest**

Confirm live `update/stable.json` has `version=0.4.17`, `revision=16`, `channel=stable`, and the same SHA-256/size as the published installer.

- [ ] **Step 6: Close implementation PR and leave the old experimental 0.4.18 branches untouched**

Do not merge or revive `feat/dpopcleaner-0.4.18-core-update`; rev.16 remains on the proven 0.2.14 frozen-core + SimpleUpdate architecture.
