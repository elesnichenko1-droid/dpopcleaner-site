# DPopCleaner 0.4.18 — core close and auto-update design

## Status

Approved by the user on 2026-08-26.

This specification intentionally changes the architectural rule used by 0.4.17: for 0.4.18 the user explicitly approved making changes **inside DPopCleaner itself** instead of keeping every new feature as an external companion module.

## Product identity

- Product: `DPopCleaner`
- Version: `0.4.18`
- Version code: `418`
- Initial revision: `1`
- Channel: `stable`
- Public installer: `DPopCleaner_Setup_0.4.18.exe`
- Windows target: Windows 10/11 x64

## Relationship to 0.4.17 and the original 0.2.14 binary

DPopCleaner 0.4.17 deliberately kept `downloads/DPopCleaner_0.2.14_BETA.exe` byte-identical as the legacy runtime core because its original source was unavailable.

0.4.18 is the first version in this line where that immutable-runtime rule no longer applies. The preserved 0.2.14 binary remains in the repository as a reference artifact and visual/behavioral baseline, but it is not the runtime `DPopCleaner.exe` for 0.4.18.

This change is narrow and explicit: the reason for rebuilding the primary executable is to support application-owned settings, correct process shutdown behavior, and a real updater flow inside the application. It does **not** authorize silently restoring the old 0.3.x product line wholesale.

The existing reconstructed root C++ sources may be used only as audited donors for already-written low-level Windows functionality such as update-manifest parsing, SHA-256 verification, updater process orchestration, paths, logging, and selected safe system helpers. The 0.4.18 runtime must be versioned and tested as a new primary core, and must not be presented as the byte-identical 0.2.14 executable.

## Scope

0.4.18 adds three directly related capabilities:

1. immediate application close without waiting for background update/network work;
2. persistent `Автоматически проверять обновления` setting inside the application's Settings page;
3. a complete stable-channel update flow using the existing GitHub Pages manifest and verified installer.

Existing 0.4.17 companion modules remain part of the product distribution:

- `DiskAnalyzer.exe`;
- `RestoreCenter.exe`;
- `ZapretScreenFix.exe`.

This release does not redesign those modules.

## 1. Immediate close behavior

### User-visible requirement

When the user presses the window close button, DPopCleaner must visually disappear immediately and the process must terminate without waiting for a network timeout or an update check to finish.

### Shutdown state

The main application owns a process-wide shutdown state. Once shutdown begins:

- the startup update timer is killed;
- no new update check may start;
- the main window is hidden before lengthy cleanup;
- background workers must not post UI work to a destroyed window;
- pending update-result payloads are discarded safely;
- the normal message loop exits without joining a network worker on the UI thread.

The implementation must not call a blocking `join()` or wait on WinHTTP from `WM_CLOSE`, `WM_DESTROY`, or the UI thread.

### Background update worker rule

Update checks may run on a worker thread. The worker receives a cancellation/shutdown signal and checks it before posting results. A result may be posted only when the main window is still valid and shutdown has not begun.

The application is allowed to let Windows terminate a detached worker with the process after the main message loop exits, provided no object lifetime or UI callback can outlive the process. No detached worker may capture references to stack-owned UI objects.

### Acceptance target

A Windows UI smoke test must demonstrate that sending `WM_CLOSE` causes the main window to disappear within 500 ms under a simulated slow update check. The test must not depend on internet speed.

## 2. Settings storage

### Setting

The Settings page includes:

`Автоматически проверять обновления` — `Вкл` / `Выкл`

and a separate action:

`Проверить обновления сейчас`

### Default

For a fresh 0.4.18 installation, automatic update checking defaults to **enabled**.

Turning it off disables only automatic startup checks. The manual `Проверить обновления сейчас` action remains available.

### Persistence

Application-owned settings are stored per user under:

`%LOCALAPPDATA%\DPopCleaner\settings.ini`

The minimum supported file is:

```ini
[updates]
auto_check=1
```

Rules:

- missing file -> defaults are used;
- missing `auto_check` -> enabled;
- `0` means disabled;
- `1` means enabled;
- malformed values fall back to enabled;
- writes are atomic: write a temporary file in the same directory, flush/close, then replace the target;
- failure to save is shown as an error and does not crash the application.

The settings store is a separate unit from UI code so it can be tested without creating a Win32 window.

## 3. Settings UI

The existing `Настройки` navigation entry remains inside the main application.

The page must show the current auto-update state in the action text itself, for example:

- `Автообновление: ВКЛ`;
- `Автообновление: ВЫКЛ`.

Actions on the page:

1. toggle automatic update checking;
2. `Проверить обновления сейчас`;
3. `Открыть логи`;
4. `Сайт проекта`.

After toggling, the setting is saved immediately and the page refreshes to show the persisted state.

No separate Settings companion executable is introduced.

## 4. Stable update manifest

0.4.18 consumes:

`https://elesnichenko1-droid.github.io/dpopcleaner-site/update/stable.json`

The manifest contract includes:

- `product` = `DPopCleaner`;
- `channel` = `stable`;
- `version`;
- `version_code`;
- `revision`;
- `available`;
- `mandatory`;
- `download_url`;
- `sha256`;
- `size`;
- `signed`;
- `notes_url`;
- `install_args`.

A manifest is usable only when:

- `product` is `DPopCleaner` when present;
- `channel` is `stable`;
- `available` is true;
- `version_code > 0`;
- `revision >= 1`;
- `download_url` uses HTTPS;
- `sha256` is exactly 64 hexadecimal characters;
- `size > 0`.

Malformed or unavailable manifests fail closed and never start an installer.

## 5. Version comparison

Local identity for 0.4.18 is:

- `version_code = 418`;
- `revision = 1`.

Remote identity is newer when either:

1. remote `version_code` is greater than local `version_code`; or
2. version codes are equal and remote `revision` is greater than local `revision`.

A lower version code is never treated as an update even if its revision number is larger.

Version comparison is implemented as a pure testable function rather than embedded in network/UI code.

## 6. Automatic check behavior

On startup:

1. load settings;
2. create the main window normally;
3. if `auto_check=1`, schedule one background update check after a short UI-start delay;
4. if disabled, do not create the startup update worker.

The startup check is non-interactive when no update exists or when the network is unavailable. It must not show an error dialog just because the machine is offline.

If a newer usable stable manifest is found, the application shows a clear update prompt containing the remote version and asks whether to download/install it.

The application must never begin downloading an optional update solely because an automatic check was enabled. Download/install begins only after explicit user confirmation.

## 7. Manual check behavior

`Проверить обновления сейчас` performs the same stable-manifest check but is interactive:

- checking state is shown on the Updates/Settings UI;
- network/manifest errors are shown to the user;
- current-version state is shown explicitly;
- a newer version follows the same verified download/install flow as an automatic check.

Manual checking works even when automatic checking is disabled.

## 8. Download verification

The installer is downloaded to `%LOCALAPPDATA%\DPopCleaner\Updates` using a `.part` temporary file.

Before the file is accepted:

1. HTTP response must be 2xx;
2. download URL must be HTTPS;
3. downloaded byte count must equal manifest `size` exactly;
4. SHA-256 of the completed `.part` file must match manifest `sha256` case-insensitively;
5. if `signed=true`, Authenticode verification must succeed.

If any check fails:

- the `.part` file is deleted;
- no updater/installer is launched;
- the existing DPopCleaner installation is untouched;
- an interactive operation reports the failure.

For `signed=false`, 0.4.18 may install only after an explicit warning/confirmation. The SHA-256 and size checks are still mandatory.

## 9. Updater handoff

`DPopUpdater.exe` remains a separate helper process because the running `DPopCleaner.exe` cannot safely replace itself while executing.

The handoff passes:

- current DPopCleaner process ID;
- verified package path;
- expected SHA-256;
- install arguments;
- restart path;
- unsigned-package permission only after explicit confirmation.

`DPopUpdater.exe` must independently verify SHA-256 again after the parent process exits and before launching the installer. Signed packages are independently Authenticode-verified unless the explicitly confirmed unsigned path is active.

After a successful installer exit code (`0` or `3010`), the updater restarts the installed DPopCleaner executable.

## 10. Update handoff and close interaction

When the user accepts an update and updater launch succeeds:

- mark shutdown as updater-driven;
- hide/close DPopCleaner immediately;
- do not display a second close prompt;
- let `DPopUpdater.exe` wait for the parent process to terminate;
- installer replacement happens only after the parent is gone.

This path uses the same non-blocking close contract as a normal user close.

## 11. Source layout

New 0.4.18-owned code is organized under a versioned native core rather than mixing additional behavior into the legacy 0.4.17 companion tree.

Target layout:

```text
v0418/
├─ core/
│  ├─ AppSettings.h/.cpp
│  ├─ UpdatePolicy.h/.cpp
│  ├─ UpdateManifest.h/.cpp
│  ├─ UpdateClient.h/.cpp
│  ├─ MainWindow.h/.cpp
│  ├─ Version.h
│  └─ main.cpp
├─ updater/
│  └─ UpdaterMain.cpp
├─ tests/
│  ├─ AppSettingsTests.cpp
│  ├─ UpdatePolicyTests.cpp
│  └─ UpdateManifestTests.cpp
└─ CMakeLists.txt
```

Low-level donor code from the repository root must be copied only when it is actually needed and then becomes 0.4.18-owned code under this directory. The build must not accidentally compile the old root `MainWindow.cpp` as the 0.4.18 UI.

## 12. Companion-module continuity

The 0.4.18 installer continues to ship the tested 0.4.17 companion modules unless a later feature explicitly replaces them:

```text
Modules\DiskAnalyzer.exe
Modules\RestoreCenter.exe
Modules\ZapretScreenFix.exe
```

The update from 0.4.17 to 0.4.18 must preserve user rollback/history data under the existing Documentation/data locations.

## 13. Installer and upgrade behavior

A new installer definition publishes `DPopCleaner_Setup_0.4.18.exe`.

It must install:

- rebuilt 0.4.18 `DPopCleaner.exe`;
- `DPopUpdater.exe`;
- existing companion modules;
- Languages/Shell/Documentation resources still required by companions.

Upgrade from 0.4.17 is in-place. User-created backups/history are preserved.

The installed application version resources, window title, About/Settings text, `version.json`, site metadata, and stable manifest must all agree on `0.4.18` / version code `418`.

## 14. Testing strategy

0.4.18 follows TDD for the new behavior.

Required automated tests/gates:

1. settings default is auto-check enabled;
2. persisted `auto_check=0` reloads as disabled;
3. malformed setting falls back safely;
4. settings write/replace is atomic at the contract level;
5. version comparison: newer version code -> update;
6. version comparison: equal version code + newer revision -> update;
7. lower version code + higher revision -> no update;
8. manifest rejects missing/invalid SHA-256;
9. manifest rejects non-HTTPS download URLs;
10. manifest rejects `available=false` for install purposes;
11. downloaded file with wrong byte size is deleted/rejected;
12. downloaded file with wrong SHA-256 is deleted/rejected;
13. updater re-verifies SHA-256 before installer launch;
14. startup auto-check is skipped when disabled;
15. manual check remains available when disabled;
16. UI close smoke: main window disappears within 500 ms while update check is intentionally slow;
17. no update-result UI handling after shutdown begins;
18. installed-package smoke verifies both `DPopCleaner.exe` and `DPopUpdater.exe`;
19. installed Settings page exposes the auto-update toggle and manual check action;
20. installed update-manifest URL is the stable 0.4.18 channel endpoint.

## 15. Release workflow

0.4.18 gets its own Windows build/release workflow rather than mutating the already-published 0.4.17 workflow in place.

The workflow must:

1. run native unit tests;
2. build x64 Release `DPopCleaner.exe` and `DPopUpdater.exe`;
3. stage companion modules/resources;
4. run close/UI smoke;
5. build the Inno Setup installer;
6. perform a silent install smoke;
7. calculate installer SHA-256 and exact size from the built artifact;
8. create/update tag `v0.4.18` and release asset `DPopCleaner_Setup_0.4.18.exe`;
9. publish `update/stable.json` with `version_code=418`, `revision=1`, real size and SHA-256;
10. deploy the website;
11. re-download the live installer and verify its SHA-256 against the live manifest.

Publication fails closed on any mismatch.

## 16. Explicit exclusions

- No silent background installation of optional updates without user confirmation.
- No HTTP update URLs.
- No installer launch before size and SHA-256 validation.
- No blocking wait for update/network workers on the UI close path.
- No separate Settings companion executable.
- No claim that the 0.4.18 primary executable is byte-identical to original 0.2.14.
- No wholesale return of the old 0.3.x reconstructed application without re-versioning, isolation, tests, and the 0.4.18 source boundary above.
- No removal of existing 0.4.17 user backups/history during upgrade.

## Acceptance summary

0.4.18 is accepted when the installed main application closes immediately, remembers the user's auto-update preference, can manually or automatically discover a newer stable build, refuses corrupted/unverified packages, launches the separate updater only after verification, and upgrades/restarts successfully without damaging the existing installation or companion data.
