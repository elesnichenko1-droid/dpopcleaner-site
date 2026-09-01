# DPopCleaner 0.4.17 rev.16 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Выпустить DPopCleaner 0.4.17 rev.16 с одной рабочей tray-иконкой с RAM badge, реальным проверяемым Zapret lifecycle, единым light/dark оформлением Zapret-кнопок и журналом, скрытым только на вкладке Zapret.

**Architecture:** Frozen `DPopCleaner.Core.exe` 0.2.14 остаётся byte-identical. Все изменения живут в `SimpleUpdate`: tray-host сохраняет одну стабильную `(HWND,uID)` identity на весь launcher lifetime; Zapret presentation отделяется от команд, а installed functional smoke управляет теми же UI-действиями, что пользователь, и сравнивает UI со фактическими Windows service/process states. Rev.16 не публикуется, пока runtime, installed, theme/layout и lifecycle gates не зелёные на одном exact head.

**Tech Stack:** C# .NET Framework 4.8 WinForms/Win32 P/Invoke, PowerShell 7, Python contracts, Windows Server 2022 GitHub Actions, Inno Setup 6, Flowseal Zapret 1.10.2.

**Spec:** `docs/superpowers/specs/2026-09-01-dpopcleaner-0.4.17-rev16-zapret-tray-design.md`

## Global Constraints

- Frozen core 0.2.14 не изменяется; Git blob обязан оставаться `efd0eff1f4962319282363fa85595c25e0cebe11`.
- Общий UI 0.2.14 и остальные вкладки не переделываются.
- Flowseal Zapret остаётся строго 1.10.2; bundled strategy count остаётся 22.
- Сохраняются rev.13–rev.15: UAC/code 740, Settings language bridge, core self-restart recovery, native Zapret version, ZapretScreenFix, Disk Analyzer, Restore Center, updater.
- Journal на Zapret только скрывается; его native HWND не уничтожаются, не reparent-ятся и не переписываются.
- Functional smoke, который создаёт/запускает `zapret`, `winws.exe` или WinDivert, всегда делает cleanup в `finally`.
- Revision identity меняется с 15 на 16 только после GREEN feature gates.

---

## File Map

**Create**
- `v0417/src/SimpleUpdate/ZapretRuntimeState.cs` — фактическое состояние bundled winws/service.
- `v0417/src/SimpleUpdate/ZapretPresentationHost.cs` — theme/layout + journal policy.
- `v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs` — source/behavior contracts.
- `tools/dpop0417_rev16_single_tray_smoke.ps1` — staged/installed single-tray smoke.
- `tools/dpop0417_rev16_zapret_functional_smoke.ps1` — installed Zapret lifecycle smoke.
- `tools/dpop0417_rev16_zapret_presentation_smoke.ps1` — theme/layout/journal smoke.
- `tests/test_dpop0417_rev16_release_contract.py` — rev.16 publication contract.

**Modify**
- `v0417/src/SimpleUpdate/LauncherContext.cs`
- `v0417/src/SimpleUpdate/TrayRamBadgeHost.cs`
- `v0417/src/SimpleUpdate/BridgeTrayGhostSuppressor.cs`
- `v0417/src/SimpleUpdate/NativeBridge.cs`
- `v0417/src/SimpleUpdate/ZapretEnhancementHost.cs`
- `v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs`
- `v0417/src/SimpleUpdate/Program.cs`
- `.github/workflows/DPopCleaner_0.4.17_SIMPLEUPDATE.yml`
- `.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml`
- `.github/workflows/publish-dpopcleaner-0.4.17.yml`
- `version.json`, `update/stable.json`, `release-manifest.js`, `index.html`, `release/RELEASE_NOTES_0.4.17.md`

---

### Task 1: Keep exactly one tray identity across core restart

**Files:**
- Modify: `v0417/src/SimpleUpdate/LauncherContext.cs`
- Modify: `v0417/src/SimpleUpdate/TrayRamBadgeHost.cs`
- Modify: `v0417/src/SimpleUpdate/BridgeTrayGhostSuppressor.cs`
- Create: `v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs`

**Interfaces:**
- `TrayRamBadgeHost.MessageWindowHandle : IntPtr`
- `TrayRamBadgeHost.IconId : uint`
- `TrayRamBadgeHost.ReattachMainWindow(IntPtr mainWindow)`
- `BridgeTrayGhostSuppressor.CleanupCurrentProcess(IntPtr keepWindow, uint keepIconId)`

- [ ] **Step 1: Write RED contracts**

```csharp
[TestMethod]
public void Core_restart_keeps_same_tray_host()
{
    var source = ReadSource("LauncherContext.cs");
    var reset = SliceMethod(source, "private void ResetBridgeForRestartedCore()");
    StringAssert.DoesNotContain(reset, "_trayRamHost.Dispose()");
    StringAssert.Contains(source, "_trayRamHost.ReattachMainWindow");
}

[TestMethod]
public void Tray_identity_is_explicit_and_constant()
{
    var source = ReadSource("TrayRamBadgeHost.cs");
    StringAssert.Contains(source, "private const uint TrayIconId = 1;");
    StringAssert.Contains(source, "internal IntPtr MessageWindowHandle");
    StringAssert.Contains(source, "internal uint IconId");
}
```

`ReadSource` walks upward from `AppDomain.CurrentDomain.BaseDirectory` until `v0417/src/SimpleUpdate` exists. `SliceMethod` extracts only the named method body.

- [ ] **Step 2: Verify RED**

```powershell
dotnet test v0417/tests/SimpleUpdate.Tests/SimpleUpdate.Tests.csproj -c Release --nologo --filter Rev16TrayAndZapretContractTests
```

Expected: FAIL because rev.15 disposes `_trayRamHost` during core restart.

- [ ] **Step 3: Preserve tray-host in restart reset**

Use this shape:

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

Add:

```csharp
internal IntPtr MessageWindowHandle { get { return _messageWindow.Handle; } }
internal uint IconId { get { return TrayIconId; } }

internal void ReattachMainWindow(IntPtr mainWindow)
{
    if (_disposed) return;
    _mainWindow = mainWindow;
}
```

- [ ] **Step 4: Reconcile before publishing the canonical icon**

Inside `TrayRamBadgeHost.Update`, when enabled, use this order:

```csharp
LegacyTrayIconSuppressor.RemoveIconsForProcess(coreProcessId);
BridgeTrayGhostSuppressor.CleanupCurrentProcess(_messageWindow.Handle, TrayIconId);
PublishTrayIcon();
```

Change `BridgeTrayGhostSuppressor` to receive the canonical tuple directly. Delete all other tray entries owned by the current launcher PID; never delete `(keepWindow,keepIconId)`. Remove `LauncherContext`'s second cleanup call so one component owns tray reconciliation.

- [ ] **Step 5: Run GREEN**

```powershell
dotnet test v0417/tests/SimpleUpdate.Tests/SimpleUpdate.Tests.csproj -c Release --nologo
```

Expected: all tests PASS.

- [ ] **Step 6: Commit**

```bash
git add v0417/src/SimpleUpdate/LauncherContext.cs v0417/src/SimpleUpdate/TrayRamBadgeHost.cs v0417/src/SimpleUpdate/BridgeTrayGhostSuppressor.cs v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs
git commit -m "fix(rev16): preserve single tray identity across core restart"
```

---

### Task 2: Prove the tray invariant on staged and installed builds

**Files:**
- Create: `tools/dpop0417_rev16_single_tray_smoke.ps1`
- Modify: `.github/workflows/DPopCleaner_0.4.17_SIMPLEUPDATE.yml`
- Modify: `.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml`

**Interfaces:**

The script has two explicit parameter sets:

```powershell
param(
  [string]$RootPath,
  [string]$InstallerPath,
  [string]$OutputDir = '_release/0.4.17/evidence/rev16-tray'
)
if ([bool]$RootPath -eq [bool]$InstallerPath) { throw 'Pass exactly one of RootPath or InstallerPath.' }
```

Markers: `REV16_SINGLE_TRAY_INITIAL_OK`, `REV16_SINGLE_TRAY_LANGUAGE_RESTART_OK`, `REV16_SINGLE_TRAY_EXPLORER_RESTART_OK`.

- [ ] **Step 1: Write the installed/staged smoke**

Reuse Explorer toolbar introspection from `dpop0417_rev13_uac_tray_smoke.ps1`. Capture canonical bridge `(HWND,uID,ownerPid,hIcon)` and core entries.

```powershell
Assert-TrayState -ExpectedBridge 1 -ExpectedCore 0
$initial = Get-CanonicalBridgeTrayIdentity
if ($initial.WindowTitle -ne 'DPopCleaner.TrayRamBadgeHost' -or $initial.hIcon -eq [IntPtr]::Zero) { throw 'Canonical RAM tray icon is invalid.' }

Invoke-LanguageRestart -Language 'English'
Assert-TrayState -ExpectedBridge 1 -ExpectedCore 0
$after = Get-CanonicalBridgeTrayIdentity
if ($after.Hwnd -ne $initial.Hwnd -or $after.uID -ne $initial.uID) { throw 'Tray identity changed across core restart.' }
```

For installed mode, install to an isolated temp directory first. For staged mode, launch the staged `SimpleUpdate.exe` with `--no-update-check` and an isolated settings file.

- [ ] **Step 2: Verify RED against rev.15 behavior**

```powershell
./tools/dpop0417_rev16_single_tray_smoke.ps1 -RootPath '_release/0.4.17/stage' -OutputDir '_release/0.4.17/evidence/rev16-tray-stage'
```

Expected before Task 1 fix: FAIL when core restart changes bridge HWND or leaves a duplicate/ghost.

- [ ] **Step 3: Add CI gates**

SimpleUpdate workflow:

```yaml
- name: Run rev.16 staged single tray smoke
  shell: pwsh
  run: ./tools/dpop0417_rev16_single_tray_smoke.ps1 -RootPath '_release/0.4.17/stage' -OutputDir '_release/0.4.17/evidence/rev16-tray-stage'
```

Foundation workflow after installer build:

```yaml
- name: Run rev.16 installed single tray smoke
  shell: pwsh
  run: ./tools/dpop0417_rev16_single_tray_smoke.ps1 -InstallerPath $env:DPOP0417_INSTALLER -OutputDir '_release/0.4.17/evidence/rev16-tray-installed'
```

- [ ] **Step 4: Add Explorer restart assertion**

Restart Explorer only in the installed smoke, wait for `TaskbarCreated`, then require exactly one bridge identity and zero core identities. The canonical `(HWND,uID)` must be unchanged.

- [ ] **Step 5: Run GREEN and commit**

Expected all three markers. Commit:

```bash
git add tools/dpop0417_rev16_single_tray_smoke.ps1 .github/workflows/DPopCleaner_0.4.17_SIMPLEUPDATE.yml .github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml
git commit -m "test(rev16): enforce one tray identity"
```

---

### Task 3: Create a real Zapret lifecycle RED test

**Files:**
- Create: `tools/dpop0417_rev16_zapret_functional_smoke.ps1`
- Modify: `.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml`

**Interfaces:**
- Markers: `REV16_ZAPRET_INSTALL_OK`, `REV16_ZAPRET_START_OK`, `REV16_ZAPRET_STOP_OK`, `REV16_ZAPRET_STRATEGY_CHANGE_OK`, `REV16_ZAPRET_REMOVE_OK`.
- Evidence JSON stores selected strategies, upper status text, bundled `winws.exe` command lines, `zapret`/WinDivert service state, failing stage and cleanup state.

- [ ] **Step 1: Automate the same installed UI path the user uses**

Start installed `SimpleUpdate.exe`, resolve exact installed `DPopCleaner.Core.exe`, open `Zapret`, then locate visible buttons:

```powershell
$installButton = Find-VisibleButton 'Установить сервис'
$startButton   = Find-VisibleButtonPrefix 'Запустить win'
$stopButton    = Find-VisibleButtonPrefix 'Остановить'
$statusButton  = Find-VisibleButton 'Статус'
$removeButton  = Find-VisibleButton 'Удалить сервисы'
```

Use the same child enumeration and ComboBox helpers already proven in `dpop0417_zapret_ui_smoke.ps1`.

- [ ] **Step 2: Observe real Windows state**

```powershell
function Get-BundledWinws {
  @(Get-CimInstance Win32_Process -Filter "Name='winws.exe'" -ErrorAction SilentlyContinue |
    Where-Object { $_.ExecutablePath -and ([IO.Path]::GetFullPath($_.ExecutablePath) -eq [IO.Path]::GetFullPath($script:WinwsPath)) })
}

function Get-ZapretService {
  Get-CimInstance Win32_Service -Filter "Name='zapret'" -ErrorAction SilentlyContinue
}

function Get-WinDivertDrivers {
  @(Get-CimInstance Win32_SystemDriver -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^WinDivert' })
}
```

- [ ] **Step 3: Execute exact lifecycle**

1. Select `general.bat`.
2. Click Install; wait for service `zapret` to exist and be `Running`, or record failure.
3. Click Status; verify upper status agrees with Windows state.
4. Click Remove; verify service removed before standalone test.
5. Click Start; wait for bundled `winws.exe`; capture `CommandLine`; upper status must be ON.
6. Click Stop; wait for bundled `winws.exe` to disappear; upper status must be OFF.
7. Select a second `general*.bat`; Start; second `CommandLine` must differ from first; Stop.
8. Click Remove; no `zapret` service and no bundled `winws.exe` may remain.

- [ ] **Step 4: Implement unconditional cleanup**

```powershell
finally {
  try { Click-IfVisible $removeButton } catch { }
  Get-BundledWinws | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
  foreach ($name in @('zapret','WinDivert','WinDivert14')) {
    & sc.exe stop $name | Out-Null
    & sc.exe delete $name | Out-Null
  }
}
```

- [ ] **Step 5: Run and preserve RED evidence**

Run against rev.15 installed candidate. This task is complete only when the smoke produces an actual failing lifecycle stage or proves the native path works. In either case write `rev16-zapret-functional-report.json`; Task 4 uses its `failing_stage` field through the explicit repair table below.

- [ ] **Step 6: Add Foundation gate and commit**

```yaml
- name: Run rev.16 real Zapret lifecycle smoke
  shell: pwsh
  run: ./tools/dpop0417_rev16_zapret_functional_smoke.ps1 -InstallerPath $env:DPOP0417_INSTALLER -OutputDir '_release/0.4.17/evidence/rev16-zapret-functional'
```

```bash
git add tools/dpop0417_rev16_zapret_functional_smoke.ps1 .github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml
git commit -m "test(rev16): exercise real Zapret lifecycle"
```

---

### Task 4: Repair exactly the Zapret lifecycle stage proven broken

**Files:**
- Create: `v0417/src/SimpleUpdate/ZapretRuntimeState.cs`
- Modify: `v0417/src/SimpleUpdate/ZapretEnhancementHost.cs`
- Modify: `v0417/src/SimpleUpdate/LauncherContext.cs`
- Test: `v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs`
- Test: `tools/dpop0417_rev16_zapret_functional_smoke.ps1`

**Interfaces:**
- `ZapretRuntimeState.Read(string applicationRoot) : ZapretRuntimeState`
- Properties: `BundledWinwsRunning`, `ZapretServiceExists`, `ZapretServiceRunning`, `BundledWinwsCommandLine`.
- `ZapretEnhancementHost.RefreshRuntimeStatus()` updates only the existing upper Zapret status control.

- [ ] **Step 1: Write RED runtime-state contract**

```csharp
[TestMethod]
public void Zapret_bridge_reads_factual_runtime_state()
{
    var runtime = ReadSource("ZapretRuntimeState.cs");
    StringAssert.Contains(runtime, "internal static ZapretRuntimeState Read(string applicationRoot)");
    StringAssert.Contains(ReadSource("ZapretEnhancementHost.cs"), "RefreshRuntimeStatus");
}
```

- [ ] **Step 2: Implement `ZapretRuntimeState`**

Compare `Process.GetProcessesByName("winws")` `MainModule.FileName` with exact `<root>\Zapret\bin\winws.exe`. Query `sc.exe query zapret`, `WinDivert`, `WinDivert14` with redirected output and a 5 second timeout. Do not infer running state from UI strings.

- [ ] **Step 3: Refresh upper status after every bridge action**

`RefreshRuntimeStatus()` structurally finds the existing upper status area between version header and Strategy row and writes exactly one factual `ON/OFF` line. No second journal/status control is created.

- [ ] **Step 4: Apply the explicit repair table from Task 3 evidence**

Use exactly one row whose `failing_stage` matches the evidence; working stages remain native.

| `failing_stage` | Repair |
|---|---|
| `start` | Hide native Start button and create same-bounds bridge proxy; launch selected bundled `general*.bat` with `UseShellExecute=true` and Zapret root as working directory. |
| `stop` | Hide native Stop button and create same-bounds bridge proxy; terminate only `winws.exe` whose `MainModule.FileName` equals bundled `Zapret\bin\winws.exe`. |
| `status` | Hide native Status button and create same-bounds bridge proxy; call `ZapretRuntimeState.Read` and refresh existing upper status. |
| `install` | Keep upstream Flowseal 1.10.2 as authority; proxy the button to launch bundled `service.bat` visibly in Zapret root, then poll factual service state and refresh status. Do not duplicate Flowseal's service-install parser. |
| `remove` | Proxy Remove to bundled `service.bat` visible manager if native path is wrong; after manager exits, poll `zapret`/WinDivert/bundled winws and refresh status. |
| `none` | Do not replace lifecycle buttons; retain only factual status refresh and the installed regression gate. |

The proxy uses the original native button bounds/font and is owned by `ZapretEnhancementHost`. It is hidden/restored together with other Zapret bridge controls.

- [ ] **Step 5: Improve error diagnostics**

For bridge commands, errors must include executable/arguments and exit code. `RepairConnection` keeps user-list load, TCP timestamps and DNS flush, but `RunHidden` throws messages shaped as:

```text
Команда: ipconfig.exe /flushdns
Код выхода: 1
```

- [ ] **Step 6: Run GREEN + legacy regressions**

```powershell
dotnet test v0417/tests/SimpleUpdate.Tests/SimpleUpdate.Tests.csproj -c Release --nologo
./tools/dpop0417_rev16_zapret_functional_smoke.ps1 -InstallerPath '_release/0.4.17/installer/DPopCleaner_Setup_0.4.17.exe' -OutputDir '_release/0.4.17/evidence/rev16-zapret-functional'
python tests/test_dpop0417_zapret_bundle_contract.py -v
python tests/test_dpop0417_zapret_screen_fix_contract.py -v
```

Expected: five lifecycle markers, cleanup clean, version 1.10.2/22 strategies unchanged.

- [ ] **Step 7: Commit**

```bash
git add v0417/src/SimpleUpdate/ZapretRuntimeState.cs v0417/src/SimpleUpdate/ZapretEnhancementHost.cs v0417/src/SimpleUpdate/LauncherContext.cs v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs
git commit -m "fix(rev16): repair proven Zapret lifecycle path"
```

---

### Task 5: Create one Zapret presentation owner for theme and layout

**Files:**
- Create: `v0417/src/SimpleUpdate/ZapretPresentationHost.cs`
- Modify: `v0417/src/SimpleUpdate/NativeBridge.cs`
- Modify: `v0417/src/SimpleUpdate/ZapretEnhancementHost.cs`
- Modify: `v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs`
- Modify: `v0417/src/SimpleUpdate/LauncherContext.cs`
- Test: `v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs`
- Create: `tools/dpop0417_rev16_zapret_presentation_smoke.ps1`

**Interfaces:**
- `ZapretPresentationHost(IntPtr parent, ZapretEnhancementHost enhancement)`
- `Show()`, `Hide()`, `Refresh()`, `Dispose()`.
- `ZapretEnhancementHost.ActionToolbarHandle`, `UpdateToolbarHandle`.
- `NativeBridge.FindSettingsComboBoxes(IntPtr parent)` returns language combo then theme combo using geometry/item content, regardless current visibility.

- [ ] **Step 1: Write RED contract against forced dark**

```csharp
[TestMethod]
public void Zapret_theme_has_one_owner()
{
    var enhancement = ReadSource("ZapretEnhancementHost.cs");
    var polish = ReadSource("ZapretVisualPolishHost.cs");
    Assert.IsFalse(enhancement.Contains("SetWindowTheme(button, \"DarkMode_Explorer\""));
    Assert.IsFalse(polish.Contains("EnsureDarkBridgeButtons"));
    StringAssert.Contains(ReadSource("ZapretPresentationHost.cs"), "internal sealed class ZapretPresentationHost");
}
```

- [ ] **Step 2: Add structural Settings combo lookup**

`NativeBridge.FindSettingsComboBoxes` enumerates ComboBoxes even when Settings is hidden. Identify language combo by list containing `English`; theme combo is the nearest ComboBox below it in Settings geometry. Return exactly two handles or empty array. This avoids localized Static labels.

- [ ] **Step 3: Detect theme from the actual theme ComboBox**

Read selected theme item from the second combo. Treat `Midnight`/dark-named selection as Dark and the light-named selection as Light. The presentation smoke first dumps actual item names and the C# test pins the exact bundled values discovered from frozen core; do not guess from OS theme.

- [ ] **Step 4: Remove independent theme ownership from old hosts**

Delete unconditional `SetWindowTheme(button, "DarkMode_Explorer", null)` in `ZapretEnhancementHost`. Remove owner-draw theme code from `ZapretVisualPolishHost`; keep that class only for installed Zapret version-dependent button captions (`Игровой фильтр 1.10.2`, `Менеджер 1.10.2`). `ZapretPresentationHost` becomes the only component that calls `SetWindowTheme` for bridge Zapret buttons.

- [ ] **Step 5: Replace fixed 709px action row with measured layout**

Measure captions using `TextRenderer.MeasureText`. Available row is from `Дополнительно` right edge + gap to parent/right native `Тесты` boundary. Use minimum 120px per bridge action, preferred measured text + 24px, gap 8px, fallback gap 4px. Proportionally shrink preferred widths but never below minimum. Apply child positions with `BeginDeferWindowPos/DeferWindowPos/EndDeferWindowPos`, then redraw once.

- [ ] **Step 6: Create light/dark layout smoke**

The script opens Zapret, captures visible button ids/text/bounds, switches Light → Dark → Light through native Theme ComboBox and asserts:

```powershell
if ($button.Right -gt $pageRight -or $button.Left -lt $pageLeft) { throw 'Button outside Zapret panel.' }
if ($button.Width -lt 120) { throw 'Bridge button clipped.' }
if (Test-AnyOverlap $bridgeButtons) { throw 'Bridge buttons overlap.' }
```

Emit `REV16_ZAPRET_LIGHT_LAYOUT_OK`, `REV16_ZAPRET_DARK_LAYOUT_OK`, `REV16_ZAPRET_THEME_ROUNDTRIP_OK`. Save screenshots for both themes as CI artifacts.

- [ ] **Step 7: Run GREEN and commit**

```powershell
dotnet test v0417/tests/SimpleUpdate.Tests/SimpleUpdate.Tests.csproj -c Release --nologo
./tools/dpop0417_rev16_zapret_presentation_smoke.ps1 -RootPath '_release/0.4.17/stage' -OutputDir '_release/0.4.17/evidence/rev16-zapret-presentation'
```

```bash
git add v0417/src/SimpleUpdate/ZapretPresentationHost.cs v0417/src/SimpleUpdate/NativeBridge.cs v0417/src/SimpleUpdate/ZapretEnhancementHost.cs v0417/src/SimpleUpdate/ZapretVisualPolishHost.cs v0417/src/SimpleUpdate/LauncherContext.cs v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs tools/dpop0417_rev16_zapret_presentation_smoke.ps1
git commit -m "fix(rev16): unify Zapret theme and layout"
```

---

### Task 6: Hide Journal only on Zapret and restore original controls elsewhere

**Files:**
- Modify: `v0417/src/SimpleUpdate/ZapretPresentationHost.cs`
- Test: `v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs`
- Test: `tools/dpop0417_rev16_zapret_presentation_smoke.ps1`

**Interfaces:**
- `CaptureJournalControls()`
- `HideJournalControls()`
- `RestoreJournalControls()`

- [ ] **Step 1: Write RED reversible-policy contract**

```csharp
[TestMethod]
public void Journal_policy_is_structural_and_reversible()
{
    var source = ReadSource("ZapretPresentationHost.cs");
    StringAssert.Contains(source, "CaptureJournalControls");
    StringAssert.Contains(source, "RestoreJournalControls");
    Assert.IsFalse(source.Contains("FindChildByText(_parent, \"Журнал\""));
}
```

- [ ] **Step 2: Capture native journal structurally**

Below the row containing `NativeBridge.ZapretApplyButtonId`, select the largest visible multiline `Edit`/list-like child in the lower half of the client area as journal body. Select the nearest visible `Static` directly above it with overlapping horizontal range as heading. Capture handle, bounds and original visibility:

```csharp
private sealed class NativeControlSnapshot
{
    internal IntPtr Handle;
    internal NativeBridge.ClientBounds Bounds;
    internal bool Visible;
}
```

- [ ] **Step 3: Hide/restore without recreating HWND**

`Show()` hides captured heading/body. `Hide()` restores captured bounds and original visibility. `Dispose()` restores before clearing references. A successor core creates a fresh presentation host and fresh snapshots.

- [ ] **Step 4: Extend smoke**

On Zapret: journal heading/body hidden, upper status still visible. Navigate to RAM/Settings: the same captured native HWNDs are restored to original bounds and visibility. Return to Zapret: hidden again, no coordinate drift.

Emit `REV16_ZAPRET_JOURNAL_HIDDEN_OK`, `REV16_OTHER_PAGE_JOURNAL_RESTORED_OK`, `REV16_ZAPRET_JOURNAL_ROUNDTRIP_OK`.

- [ ] **Step 5: Run GREEN and commit**

```bash
git add v0417/src/SimpleUpdate/ZapretPresentationHost.cs v0417/tests/SimpleUpdate.Tests/Rev16TrayAndZapretContractTests.cs tools/dpop0417_rev16_zapret_presentation_smoke.ps1
git commit -m "feat(rev16): hide journal only on Zapret"
```

---

### Task 7: Integrate presentation lifecycle in `LauncherContext` and repeat restart twice

**Files:**
- Modify: `v0417/src/SimpleUpdate/LauncherContext.cs`
- Modify: `tools/dpop0417_rev15_restart_recovery_smoke.ps1`
- Modify: `.github/workflows/DPopCleaner_0.4.17_SIMPLEUPDATE.yml`

**Interfaces:**
- New field: `_zapretPresentationHost`.
- Marker: `REV16_RESTART_TRAY_ZAPRET_REATTACH_OK`.

- [ ] **Step 1: Wire host lifecycle**

When Zapret is visible: ensure `ZapretEnhancementHost`, then ensure `ZapretPresentationHost`, call `Show/Refresh`. When Zapret is not visible: call presentation `Hide()` before hiding enhancement controls. On core restart and launcher exit: dispose presentation host; tray host remains alive across core restart.

- [ ] **Step 2: Extend restart smoke**

Execute Russian → English → Russian, requiring a new core PID each time but same launcher PID and same tray `(HWND,uID)`. After each successor attach, open Zapret and assert one set of bridge IDs, buttons inside panel, journal hidden; leave Zapret and assert journal restored.

- [ ] **Step 3: Run full SimpleUpdate suite**

```powershell
dotnet test v0417/tests/SimpleUpdate.Tests/SimpleUpdate.Tests.csproj -c Release --nologo
./tools/dpop0417_rev15_restart_recovery_smoke.ps1 -RootPath '_release/0.4.17/stage'
```

Expected existing rev.15 markers plus `REV16_RESTART_TRAY_ZAPRET_REATTACH_OK`.

- [ ] **Step 4: Commit**

```bash
git add v0417/src/SimpleUpdate/LauncherContext.cs tools/dpop0417_rev15_restart_recovery_smoke.ps1 .github/workflows/DPopCleaner_0.4.17_SIMPLEUPDATE.yml
git commit -m "test(rev16): cover repeated restart and Zapret reattach"
```

---

### Task 8: Promote all release-facing identity to rev.16 after feature GREEN

**Files:**
- Create: `tests/test_dpop0417_rev16_release_contract.py`
- Modify: `v0417/src/SimpleUpdate/Program.cs`
- Modify: `v0417/src/SimpleUpdate/LauncherContext.cs`
- Modify: `version.json`, `update/stable.json`, `release-manifest.js`, `index.html`
- Modify: `release/RELEASE_NOTES_0.4.17.md`
- Modify: `.github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml`
- Modify: `.github/workflows/publish-dpopcleaner-0.4.17.yml`

**Interfaces:**
- `Program.CurrentRevision == 16`
- tag `v0.4.17-rev16`
- candidate `dpopcleaner-0.4.17-rev16-release-candidate`

- [ ] **Step 1: Write RED release contract**

```python
self.assertEqual(version['revision'], 16)
self.assertEqual(stable['revision'], 16)
self.assertIn('v0.4.17-rev16', workflow)
self.assertIn('dpop0417_rev16_single_tray_smoke.ps1', workflow)
self.assertIn('dpop0417_rev16_zapret_functional_smoke.ps1', workflow)
self.assertIn('dpop0417_rev16_zapret_presentation_smoke.ps1', workflow)
self.assertIn('revision=16', workflow)
```

Run and expect FAIL while identity remains 15.

- [ ] **Step 2: Bump identity atomically**

Set `CurrentRevision = 16`, User-Agent `DPopCleaner-SimpleUpdate/0.4.17-rev16`, manifests/site/release notes revision 16, publisher tag/title/concurrency/artifact names rev16.

- [ ] **Step 3: Put all installed rev.16 gates before publication payload**

Publisher order after installer build:

1. Existing installed package smoke.
2. Existing rev.15 language-restart smoke.
3. `dpop0417_rev16_single_tray_smoke.ps1`.
4. `dpop0417_rev16_zapret_functional_smoke.ps1`.
5. `dpop0417_rev16_zapret_presentation_smoke.ps1`.
6. Existing Zapret version/update regressions.
7. Build publication payload.

- [ ] **Step 4: Update release notes**

State exactly: one tray icon with RAM badge; tray HWND persists across core restart; real Zapret start/stop/install/remove verification; unified light/dark bridge buttons; Journal hidden only on Zapret; frozen core hash unchanged; Flowseal 1.10.2/22 strategies retained.

- [ ] **Step 5: Run complete candidate verification**

```powershell
python tests/test_dpop0417_rev16_release_contract.py -v
Get-ChildItem tests -Filter 'test_dpop0417_*.py' -File | Sort-Object Name | ForEach-Object { python $_.FullName -v; if ($LASTEXITCODE -ne 0) { throw $_.Name } }
dotnet test v0417/tests/SimpleUpdate.Tests/SimpleUpdate.Tests.csproj -c Release --nologo
```

Then require exact-head success for SimpleUpdate, Foundation, UI diagnostic and rev.16 production-candidate build-package; PR publish remains skipped.

- [ ] **Step 6: Verify frozen core evidence**

Staged `git hash-object DPopCleaner.exe` must equal exactly `efd0eff1f4962319282363fa85595c25e0cebe11`. No frozen binary file appears modified in diff.

- [ ] **Step 7: Commit**

```bash
git add tests/test_dpop0417_rev16_release_contract.py v0417/src/SimpleUpdate/Program.cs v0417/src/SimpleUpdate/LauncherContext.cs version.json update/stable.json release-manifest.js index.html release/RELEASE_NOTES_0.4.17.md .github/workflows/DPopCleaner_0.4.17_FOUNDATION.yml .github/workflows/publish-dpopcleaner-0.4.17.yml
git commit -m "release: prepare DPopCleaner 0.4.17 rev.16"
```

---

### Task 9: Integrate and verify the published asset

**Files:** no expected production edits.

- [ ] **Step 1: Fresh pre-merge verification**

Fetch PR exact head and its workflow runs immediately before integration. Require all four workflows success and mergeability. Do not rely on earlier green SHA.

- [ ] **Step 2: Integrate exact GREEN head safely**

Use normal PR merge with `expected_head_sha`. If the known connector ready-for-review GraphQL bug recurs, never force `main`: first require `behind_by=0` and merge-base=current `main`, then only non-force fast-forward exact GREEN head is acceptable.

- [ ] **Step 3: Watch production push**

Require build-package success including all installed rev.16 gates; then publish success including GitHub Release, Pages deploy and live installer SHA verification.

- [ ] **Step 4: Verify `v0.4.17-rev16`**

Fetch Release API and record exact installer size/digest. Require `draft=false`, `prerelease=false`, target SHA equals integrated SHA.

- [ ] **Step 5: Verify live stable manifest**

Live `update/stable.json` must report `version=0.4.17`, `revision=16`, `channel=stable`, and same SHA-256/size as Release asset.

- [ ] **Step 6: Close implementation PR**

Leave old experimental `feat/dpopcleaner-0.4.18-core-update` and related 0.4.18 branches untouched.

---

## Self-Review Result

- **Spec coverage:** single tray ownership → Tasks 1–2/7; real Zapret lifecycle → Tasks 3–4/8; unified theme/layout → Task 5/7; journal-only-on-Zapret → Task 6/7; restart integration → Task 7; release gate → Tasks 8–9.
- **Placeholder scan:** no `TODO`, `TBD`, “implement later”, or unspecified error-handling steps. Task 4 uses a closed decision table keyed by the actual RED evidence rather than an open-ended repair instruction.
- **Type consistency:** tray diagnostic properties and cleanup signature are defined in Task 1 and consumed in Task 2; presentation host interface is defined in Task 5 and consumed in Tasks 6–7; runtime-state interface is defined in Task 4 and consumed by functional status refresh.
- **Scope:** no C++/0.4.18 migration, no frozen-core edit, no site redesign, no Flowseal version upgrade.
