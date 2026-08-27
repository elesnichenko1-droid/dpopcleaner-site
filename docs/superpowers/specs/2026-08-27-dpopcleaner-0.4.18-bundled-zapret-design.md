# DPopCleaner 0.4.18 Bundled Zapret Integration Design

## Status
Approved direction from the user: Zapret must be included in DPopCleaner itself, not merely supported by the separate Zapret Screen Fix module.

## Goal
DPopCleaner 0.4.18 ships with a complete, pinned Flowseal Zapret package and exposes Zapret status, strategy selection, manual start/stop, and service management directly from the main DPopCleaner UI. A fresh DPopCleaner installation must not require a separate Zapret download or an existing external Zapret installation.

## Upstream and supply-chain pin
The bundled third-party payload is Flowseal/zapret-discord-youtube release `1.10.2`.

Pinned release asset:
- file: `zapret-discord-youtube-1.10.2.zip`
- URL: `https://github.com/Flowseal/zapret-discord-youtube/releases/download/1.10.2/zapret-discord-youtube-1.10.2.zip`
- exact size: `1508077` bytes
- SHA-256: `5eaac9fb2e4b1abd693487452a3ff3f4dfe9578a45f9ddddfa4bc1f5a6bb62d5`

The binary archive is not fetched at application runtime. GitHub Actions downloads the pinned archive during the Windows candidate/release build, verifies exact size and SHA-256 before extraction, and fails closed if either value differs. The verified extracted tree is staged under `ThirdParty/Zapret/` and included in the final Inno Setup installer.

DPopCleaner does not silently follow Flowseal `latest` during a 0.4.18 build. Updating the bundled Zapret version requires a reviewed DPopCleaner source change with a new pinned version, size and digest.

## Licensing and attribution
The installer must preserve the upstream `LICENSE.txt` from Flowseal. It contains the MIT notices for bol-van/Flowseal and documents that WinDivert is distributed under the user's choice of LGPLv3 or GPLv2. DPopCleaner must also include `Documentation/THIRD_PARTY_NOTICES.txt` naming Flowseal Zapret, bol-van Zapret and WinDivert, the pinned 1.10.2 version, and the upstream source locations.

No DPopCleaner code may claim ownership of the bundled Zapret/WinDivert binaries.

## Installation layout
After installation:

```text
DPopCleaner/
  DPopCleaner.exe
  DPopUpdater.exe
  Modules/
    DiskAnalyzer.exe
    RestoreCenter.exe
    ZapretScreenFix.exe
    DPop.Common.dll
  ThirdParty/
    Zapret/
      LICENSE.txt
      README.md
      service.bat
      general.bat
      general (ALT...).bat
      bin/
      lists/
      utils/
      ...remaining verified 1.10.2 payload...
```

`ThirdParty/Zapret/` is application-owned versioned program data. User-created or user-modified Zapret lists must not be blindly overwritten without backup during a DPopCleaner update. Before replacing an existing bundled Zapret directory, the installer preserves user-editable `*-user.txt` list files into `%LOCALAPPDATA%\DPopCleaner\ZapretBackup\<timestamp>\` and restores them into the newly installed tree when names still match.

The existing `ZapretScreenFix.exe` remains available for external Zapret installations and diagnostics. The bundled 1.10.2 payload is treated separately by the main application's Zapret page.

## Main application UI
The existing `ZapretFix` navigation page becomes `Zapret`.

The page displays:
- bundled version: `Flowseal Zapret 1.10.2`;
- bundled payload state: available / damaged / missing;
- runtime state: stopped / standalone running / service running;
- service state: not installed / stopped / running;
- active service strategy when the upstream registry marker is available;
- selected standalone/service strategy;
- note that WinDivert can trigger antivirus/PUA warnings.

Actions:
1. `Запустить` — run the selected bundled `general*.bat` strategy elevated.
2. `Остановить` — stop only the DPopCleaner-bundled Zapret instance/service; do not kill unrelated external `winws.exe` processes by name alone.
3. `Стратегия` — cycle/select among bundled top-level strategy `.bat` files excluding `service*.bat`.
4. `Установить как службу` — install the selected strategy using the pinned upstream 1.10.2 service flow with elevation.
5. `Удалить службу` — remove the bundled Zapret service and its WinDivert service using the pinned upstream service manager flow with elevation.
6. `Фикс демонстрации экрана` — launch the existing `ZapretScreenFix.exe` for explicit patch/rollback tools.
7. `Открыть папку Zapret` — open the bundled directory.

Because the current main window has four reusable action buttons, the page uses a two-state action layout: primary operations on the first screen (`Запустить/Остановить`, `Стратегия`, `Установить/Удалить службу`, `Ещё…`) and secondary tools behind `Ещё…` (`Screen Fix`, folder, refresh status, back). This avoids expanding `MainWindow.cpp` with extra permanent controls solely for this page.

## Strategy model
A new `ZapretController` owns all Zapret-specific behavior. `MainWindow.cpp` only renders controller state and invokes controller operations.

Public model:

```cpp
struct ZapretStatus {
    bool payloadAvailable;
    bool payloadIntegrityOk;
    bool serviceInstalled;
    bool serviceRunning;
    bool bundledWinwsRunning;
    std::wstring serviceStrategy;
    std::wstring error;
};

struct ZapretStrategy {
    std::wstring displayName;
    std::filesystem::path batchPath;
};
```

Controller responsibilities:
- resolve bundled root strictly as `<DPopCleaner exe dir>\ThirdParty\Zapret`;
- verify required files (`LICENSE.txt`, `service.bat`, `bin\winws.exe`, WinDivert driver/DLL, lists directory) before allowing start/install;
- enumerate only top-level `.bat` strategies whose names do not begin with `service`;
- persist the selected strategy in the existing per-user DPopCleaner settings file;
- query Windows service `zapret` through SCM;
- read `HKLM\SYSTEM\CurrentControlSet\Services\zapret\zapret-discord-youtube` when present to show the installed strategy;
- identify standalone `winws.exe` by full executable path beneath the bundled root, not by filename alone;
- elevate operations only when needed using `ShellExecuteExW(..., L"runas", ...)`;
- never block the UI thread waiting for a long-running strategy process.

## Manual start and stop
Manual start launches the selected upstream strategy `.bat` from its own bundled working directory using an elevated `cmd.exe /c` process. Flowseal documents manual `general*.bat` files as the normal way to test strategies.

Before standalone start, DPopCleaner refreshes status. If the `zapret` service is already running, manual start is refused with a clear message so two bypass instances are not intentionally stacked.

Standalone stop enumerates running `winws.exe` processes, resolves each full executable image path, and terminates only processes whose path equals `<bundled root>\bin\winws.exe` after canonical/path-normalized comparison. It must not issue a global `taskkill /IM winws.exe` from the DPopCleaner controller.

## Service install/remove
Flowseal 1.10.2 `service.bat` is interactive and contains the official logic for translating a selected strategy into a `zapret` Windows service. DPopCleaner must not reimplement that argument parser in 0.4.18.

For 0.4.18 the controller uses a small pinned wrapper that drives the unmodified upstream `service.bat admin` with deterministic standard input for the selected strategy index. The wrapper is version-specific, generated by DPopCleaner tooling, and only enabled when the detected bundled version is exactly `1.10.2`.

The service action is elevated. DPopCleaner waits for the short wrapper process on a worker thread, then refreshes SCM/registry state. A timeout produces an error and leaves the upstream console visible rather than force-killing system service operations.

Removal likewise drives the upstream 1.10.2 `Remove Services` menu flow. DPopCleaner does not directly delete arbitrary services and does not invoke service management against an external Zapret directory.

If the upstream service menu layout changes in a future pinned version, its wrapper contract must be updated and retested as part of that DPopCleaner version bump.

## Screen-sharing fix for bundled payload
The existing Discord screen-sharing issue is preserved as a first-class compatibility check. During staging, after the verified upstream archive is extracted, DPopCleaner tooling applies the existing narrowly scoped Zapret strategy patcher semantics to bundled strategy files: only a `discord.media` TCP filter missing port 443 may receive `443`; unrelated filters are untouched. The original upstream archive remains verified before this controlled local transformation.

A staging contract verifies that every bundled top-level strategy containing `--hostlist-domains=discord.media` either already has TCP 443 in the relevant filter or is patched to include it. This means the bundled payload works with our screen-sharing fix out of the box, while `ZapretScreenFix.exe` remains useful for external installations and rollback tooling.

## Update interaction
DPopCleaner application auto-update remains the owner of bundled Zapret updates. Zapret is not independently auto-updated behind DPopCleaner's back.

When DPopCleaner updates itself:
1. the new installer contains a newly verified/pinned bundled Zapret tree;
2. user-editable Zapret user lists are backed up;
3. an active bundled standalone process/service is detected;
4. the updater requests a controlled stop before program-file replacement;
5. the installer replaces program-owned Zapret files;
6. user list overrides are restored;
7. DPopCleaner does not automatically restart Zapret after update unless a later explicitly designed setting enables that behavior.

This keeps update rollback and integrity reasoning inside one product update channel.

## Failure handling
- Missing/damaged bundled files: page remains accessible, but start/service buttons are disabled and the status explains which required file is missing.
- Upstream ZIP size/SHA mismatch: candidate/release build fails; no installer is published.
- Elevation denied: operation returns `cancelled` without changing DPopCleaner state.
- Strategy disappears between enumeration and launch: operation fails before elevation.
- Service already running when standalone start requested: refuse standalone start.
- External `winws.exe` detected outside bundled root: show it as external/conflicting information but never terminate it automatically.
- Antivirus quarantines WinDivert: integrity/status reports the missing binary/driver and points the user to the bundled upstream notice rather than silently redownloading it.

## Security boundaries
- No runtime download of arbitrary Zapret binaries.
- No execution of strategy paths outside the bundled root.
- No strategy name may contain path traversal after canonical validation.
- Stop logic is path-scoped to bundled `bin\winws.exe`.
- Service automation is pinned to known Flowseal 1.10.2 behavior.
- Third-party payload is verified before modification/staging.
- DPopCleaner release publication remains fail-closed behind Windows tests and installed-package smoke.

## Tests and release gates
TDD coverage must include:
- strategy enumeration excludes `service*.bat` and paths outside bundled root;
- selected strategy persists in `settings.ini`;
- bundled path validation refuses missing `winws.exe`/`service.bat`/license;
- path-scoped process matching distinguishes bundled and external `winws.exe`;
- service wrapper input maps a selected strategy name to the correct upstream menu index for pinned 1.10.2;
- screen-sharing staging patch is idempotent and touches only `discord.media` filters;
- stage allowlist requires `ThirdParty/Zapret/`;
- build workflow downloads only the pinned 1.10.2 ZIP and verifies exact size + SHA-256 before extraction;
- staged package contains `ThirdParty/Zapret/LICENSE.txt`, `service.bat`, `bin/winws.exe`, WinDivert files and at least `general.bat`;
- installed-package smoke verifies the same files after Inno Setup install;
- UI contract contains Zapret status, strategy and start/stop/service actions;
- existing 0.4.18 non-blocking close and updater tests remain green.

The production 0.4.18 release workflow must rebuild the verified third-party payload from the pinned archive. It must never publish a candidate produced with an unverified or different Zapret archive.

## Out of scope for 0.4.18
- automatic probing to choose the best strategy;
- DPopCleaner-managed editing of arbitrary Zapret command-line arguments;
- independent background Zapret self-update;
- automatic antivirus exclusions;
- modifying system Secure DNS settings;
- automatically restarting Zapret after a DPopCleaner update;
- replacing Flowseal's service argument parser with a custom parser.

These can be designed separately after the bundled integration is stable.
