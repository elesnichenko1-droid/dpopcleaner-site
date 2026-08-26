# DPopCleaner 0.4.18 Core + Auto-update Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship DPopCleaner 0.4.18 with a rebuilt primary Win32 executable that closes immediately, stores an in-app auto-update preference, and performs verified stable-channel updates through DPopUpdater.

**Architecture:** New native code lives only under `v0418/`. The primary executable owns settings and UI lifecycle; network/update verification is isolated behind update units; `DPopUpdater.exe` performs the post-exit installer handoff. Existing 0.4.17 companion modules remain separately built and staged into the 0.4.18 installer.

**Tech Stack:** C++20, Win32, WinHTTP, BCrypt SHA-256, WinVerifyTrust, CMake/CTest, PowerShell smoke tests, Inno Setup, GitHub Actions Windows 2022.

**Spec:** `docs/superpowers/specs/2026-08-26-dpopcleaner-0.4.18-core-autoupdate-design.md`

## Global Constraints

- Product version is exactly `0.4.18`, version code `418`, revision `1`, channel `stable`.
- Public installer is `DPopCleaner_Setup_0.4.18.exe`.
- Windows target is Windows 10/11 x64.
- The preserved 0.2.14 executable remains a reference artifact and is not the 0.4.18 runtime executable.
- Do not compile the old root `MainWindow.cpp` into 0.4.18.
- Optional updates always require explicit user confirmation before download/install.
- Download URL must be HTTPS and package size + SHA-256 must match the manifest.
- UI close must never block on WinHTTP or join a worker thread.
- Existing `Documentation` backups/history must survive an in-place upgrade.

---

### Task 1: Native 0.4.18 scaffold, settings, and version policy

**Files:**
- Create: `v0418/CMakeLists.txt`
- Create: `v0418/core/Version.h`
- Create: `v0418/core/AppSettings.h`
- Create: `v0418/core/AppSettings.cpp`
- Create: `v0418/core/UpdatePolicy.h`
- Create: `v0418/core/UpdatePolicy.cpp`
- Create: `v0418/tests/AppSettingsTests.cpp`
- Create: `v0418/tests/UpdatePolicyTests.cpp`

**Interfaces:**
- `struct AppSettings { bool autoCheckUpdates{true}; };`
- `AppSettings LoadSettings(const std::filesystem::path& path);`
- `bool SaveSettingsAtomic(const std::filesystem::path& path, const AppSettings&, std::wstring& error);`
- `struct VersionIdentity { int versionCode; int revision; };`
- `bool IsRemoteNewer(VersionIdentity local, VersionIdentity remote);`

- [ ] **Step 1: Write failing settings and version-policy tests**

`AppSettingsTests.cpp` must verify missing file => enabled, `auto_check=0` => disabled, malformed value => enabled, and save + reload round-trip. `UpdatePolicyTests.cpp` must verify greater version code, equal version/newer revision, and lower version/higher revision.

```cpp
if (!LoadSettings(missing).autoCheckUpdates) return Fail("missing settings must default enabled");
WriteText(file, "[updates]\nauto_check=0\n");
if (LoadSettings(file).autoCheckUpdates) return Fail("0 must disable auto check");
if (!IsRemoteNewer({418,1}, {418,2})) return Fail("newer revision must update");
if (IsRemoteNewer({418,1}, {417,99})) return Fail("lower version code must not update");
```

- [ ] **Step 2: Run CMake/CTest and verify RED**

```powershell
cmake -S v0418 -B build0418 -A x64
cmake --build build0418 --config Release --target DPop0418Tests
ctest --test-dir build0418 -C Release --output-on-failure
```

Expected: configure/build fails because the new settings/policy units do not exist yet.

- [ ] **Step 3: Implement settings and version policy**

Settings parsing is a small line parser restricted to `[updates]` + `auto_check`. Save writes `settings.ini.tmp`, flushes the file handle, then `MoveFileExW(..., MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.

```cpp
bool IsRemoteNewer(VersionIdentity local, VersionIdentity remote) {
    if (remote.versionCode != local.versionCode) return remote.versionCode > local.versionCode;
    return remote.revision > local.revision;
}
```

- [ ] **Step 4: Run CTest and verify GREEN**

Same command as Step 2. Expected: settings and policy tests pass.

- [ ] **Step 5: Commit**

```bash
git add v0418/CMakeLists.txt v0418/core v0418/tests
git commit -m "feat(0.4.18): add settings and update policy"
```

---

### Task 2: Stable manifest parser and package verification

**Files:**
- Create: `v0418/core/UpdateManifest.h`
- Create: `v0418/core/UpdateManifest.cpp`
- Create: `v0418/core/Hash.h`
- Create: `v0418/core/Hash.cpp`
- Create: `v0418/core/Signature.h`
- Create: `v0418/core/Signature.cpp`
- Create: `v0418/tests/UpdateManifestTests.cpp`
- Create: `v0418/tests/PackageVerificationTests.cpp`
- Modify: `v0418/CMakeLists.txt`

**Interfaces:**
- `struct UpdateManifest` with product/channel/version/versionCode/revision/available/mandatory/downloadUrl/sha256/size/signedPackage/notesUrl/installArgs.
- `bool ParseUpdateManifestUtf8(const std::string&, UpdateManifest&, std::wstring& error);`
- `bool IsUsableStableManifest(const UpdateManifest&, std::wstring& error);`
- `bool Sha256File(const std::filesystem::path&, std::wstring& hex, std::wstring& error);`
- `bool VerifyPackageFile(const std::filesystem::path&, const UpdateManifest&, std::wstring& error);`

- [ ] **Step 1: Write failing manifest/package tests**

Tests include a valid stable manifest, bad 63-char hash, `http://` URL, `available=false`, local temp file with wrong byte count, and local temp file with wrong hash.

```cpp
UpdateManifest m{};
std::wstring error;
if (!ParseUpdateManifestUtf8(validJson, m, error)) return Fail("valid manifest must parse");
if (!IsUsableStableManifest(m, error)) return Fail("valid stable manifest must be usable");
m.downloadUrl = L"http://example.invalid/setup.exe";
if (IsUsableStableManifest(m, error)) return Fail("HTTP must fail closed");
```

- [ ] **Step 2: Run CTest and verify RED**

Expected: manifest/package tests fail to compile until units exist.

- [ ] **Step 3: Implement parser, validation, SHA-256, size verification**

Use only exact field extraction required by the manifest contract. Validate 64 hexadecimal SHA characters, HTTPS, size > 0, `versionCode > 0`, `revision >= 1`, channel `stable`, product `DPopCleaner`, and available true.

`VerifyPackageFile` first compares `std::filesystem::file_size`, then SHA-256. It never launches anything.

- [ ] **Step 4: Run CTest and verify GREEN**

Expected: all Task 1 and Task 2 tests pass.

- [ ] **Step 5: Commit**

```bash
git add v0418
git commit -m "feat(0.4.18): validate stable update packages"
```

---

### Task 3: WinHTTP client and independent updater handoff

**Files:**
- Create: `v0418/core/UpdateClient.h`
- Create: `v0418/core/UpdateClient.cpp`
- Create: `v0418/updater/UpdaterMain.cpp`
- Create: `v0418/tests/UpdateClientContractTests.cpp`
- Modify: `v0418/CMakeLists.txt`

**Interfaces:**
- `struct UpdateCheckResult { bool success; bool updateAvailable; UpdateManifest manifest; std::wstring error; };`
- `UpdateCheckResult CheckStableUpdates(const std::atomic_bool* shutdown = nullptr);`
- `bool DownloadVerifiedPackage(const UpdateManifest&, std::filesystem::path& package, std::wstring& error, const std::atomic_bool* shutdown = nullptr);`
- `bool LaunchUpdater(const UpdateManifest&, const std::filesystem::path&, bool allowUnsigned, const std::filesystem::path& updaterExe, const std::filesystem::path& restartExe, std::wstring& error);`

- [ ] **Step 1: Write failing contract tests**

A local-file verification test proves the accepted package path is impossible until size + SHA pass. Static contract assertions verify updater source contains an independent `Sha256File` call before its `ShellExecuteExW` installer launch.

- [ ] **Step 2: Run tests and verify RED**

Expected: missing client/updater symbols or contract failure.

- [ ] **Step 3: Implement WinHTTP + updater**

`CheckStableUpdates` requests only:

`https://elesnichenko1-droid.github.io/dpopcleaner-site/update/stable.json`

It applies `IsUsableStableManifest` and `IsRemoteNewer({418,1}, remote)`.

`DownloadVerifiedPackage` streams to `%LOCALAPPDATA%\DPopCleaner\Updates\<name>.part`, checks shutdown between WinHTTP reads, verifies exact size + SHA, then atomically renames the verified file. On any failure it deletes `.part`.

`UpdaterMain.cpp` waits for the parent PID, re-hashes the installer independently, optionally verifies Authenticode, launches the installer elevated, accepts exit `0` or `3010`, then restarts DPopCleaner.

- [ ] **Step 4: Run tests and verify GREEN**

Expected: all native tests pass.

- [ ] **Step 5: Commit**

```bash
git add v0418
git commit -m "feat(0.4.18): add stable updater handoff"
```

---

### Task 4: Fresh primary MainWindow with settings UI and non-blocking close

**Files:**
- Create: `v0418/core/MainWindow.h`
- Create: `v0418/core/MainWindow.cpp`
- Create: `v0418/core/main.cpp`
- Create: `tools/dpop0418_close_smoke.ps1`
- Create: `tests/test_dpop0418_ui_contract.py`
- Modify: `v0418/CMakeLists.txt`

**Interfaces:**
- `int RunMainWindow(HINSTANCE instance, int showCommand);`
- process-wide `std::atomic_bool gShuttingDown` is set before `DestroyWindow`.
- update worker posts only a notification message; result state is kept in process-owned storage rather than heap payload attached to a destroyed HWND.

- [ ] **Step 1: Write failing UI/close contract**

The Python contract requires Settings action labels `Автообновление: ВКЛ/ВЫКЛ`, `Проверить обновления сейчас`, stable manifest URL, `WM_CLOSE` handling that calls `ShowWindow(hwnd, SW_HIDE)` before `DestroyWindow`, and no `join()` in close/destroy handlers.

The PowerShell smoke launches with `DPOP0418_TEST_SLOW_UPDATE_MS=10000`, waits for the update worker to begin, posts `WM_CLOSE`, and fails when the window/process remains visible for >= 500 ms.

- [ ] **Step 2: Run contract and close smoke and verify RED**

Expected: fail because MainWindow does not exist.

- [ ] **Step 3: Implement new 0.4.18 MainWindow**

The UI uses the existing Midnight navy + mint visual language but is newly owned by `v0418`. It contains navigation for Overview, Cleaning/tools entry points, companion tools, Updates, and Settings. Settings page action order is:

1. `Автообновление: ВКЛ/ВЫКЛ`
2. `Проверить обновления сейчас`
3. `Открыть логи`
4. `Сайт проекта`

Startup loads `settings.ini`; when enabled, a short timer starts one background check. Manual checking always works. Optional updates prompt before download. Unsigned packages prompt again after SHA/size verification.

`WM_CLOSE` sets shutdown, kills timer, hides the window, and destroys it immediately. No network wait or worker join occurs on UI thread. Worker checks shutdown before publishing state/posting a result notification.

Test-only `DPOP0418_TEST_SLOW_UPDATE_MS` inserts a cancellable deterministic delay before network work so close smoke does not depend on internet latency.

- [ ] **Step 4: Run native tests + Python contract + close smoke and verify GREEN**

```powershell
ctest --test-dir build0418 -C Release --output-on-failure
python tests/test_dpop0418_ui_contract.py
powershell -ExecutionPolicy Bypass -File tools/dpop0418_close_smoke.ps1 -Exe build0418/bin/Release/DPopCleaner.exe
```

- [ ] **Step 5: Commit**

```bash
git add v0418 tools/dpop0418_close_smoke.ps1 tests/test_dpop0418_ui_contract.py
git commit -m "feat(0.4.18): add in-app autoupdate and instant close"
```

---

### Task 5: Stage existing companion modules and build the 0.4.18 installer

**Files:**
- Create: `v0418/stage-allowlist.txt`
- Create: `tools/dpop0418_stage.ps1`
- Create: `tools/dpop0418_install_smoke.ps1`
- Create: `release/DPopCleaner_0.4.18.iss`
- Create: `release/RELEASE_NOTES_0.4.18.md`
- Create: `tests/test_dpop0418_installer_contract.py`

**Interfaces:**
- staged root: `artifacts/dpop0418/stage/`
- required executables: `DPopCleaner.exe`, `DPopUpdater.exe`, `Modules/DiskAnalyzer.exe`, `Modules/RestoreCenter.exe`, `Modules/ZapretScreenFix.exe`.

- [ ] **Step 1: Write failing installer/stage contract**

Require version 0.4.18, updater executable, all three modules, preservation of `Documentation` on uninstall/upgrade, and installer name `DPopCleaner_Setup_0.4.18.exe`.

- [ ] **Step 2: Run contract and verify RED**

Expected: missing stage/installer files.

- [ ] **Step 3: Implement staging and installer**

Stage the freshly built `v0418` native executables and rebuild the three existing v0417 companion projects. Copy existing Languages/Shell/Documentation resources. Inno Setup uses the same AppId as 0.4.17 for in-place upgrade and must not delete user backup/history directories.

- [ ] **Step 4: Build installer + silent install smoke**

Smoke checks installed `DPopCleaner.exe`, `DPopUpdater.exe`, three modules, and Settings UI contract. It also verifies the process can launch and close.

- [ ] **Step 5: Commit**

```bash
git add v0418/stage-allowlist.txt tools release tests/test_dpop0418_installer_contract.py
git commit -m "build(0.4.18): add installer and upgrade staging"
```

---

### Task 6: Dedicated 0.4.18 CI/release workflow and site manifest

**Files:**
- Create: `.github/workflows/publish-dpopcleaner-0.4.18.yml`
- Create: `tests/test_dpop0418_release_contract.py`
- Modify at release time: `version.json`
- Modify at release time: `update/stable.json`
- Modify at release time: `index.html`
- Modify at release time: `release-manifest.js`

**Interfaces:**
- tag: `v0.4.18`
- asset: `DPopCleaner_Setup_0.4.18.exe`
- manifest identity: `version=0.4.18`, `version_code=418`, `revision=1`.

- [ ] **Step 1: Write failing release contract**

Require dedicated Windows workflow, native tests, close smoke, installer smoke, SHA/size calculation, release upload, stable manifest publication, Pages deploy, and live installer re-download/hash verification.

- [ ] **Step 2: Run contract and verify RED**

Expected: missing workflow / 0.4.18 site identity.

- [ ] **Step 3: Implement workflow**

PR runs build/test/package without publishing. Push to `main` after merge creates/updates `v0.4.18`, uploads installer, writes the exact built SHA-256 and size into staged `update/stable.json`, deploys Pages, then re-downloads the live installer and compares SHA-256.

- [ ] **Step 4: Run full PR workflow and inspect every gate**

Expected: unit tests, UI close smoke, companion tests, staging, Inno build, install smoke all green. Publishing steps must be skipped on PR.

- [ ] **Step 5: Review diff and merge only after green verification**

After merge, monitor the `main` production run through release + Pages + live SHA verification before claiming 0.4.18 is released.

---

## Plan self-review

- Spec coverage: settings, version comparison, manifest validation, size/SHA, updater re-verification, non-blocking close, settings UI, companion continuity, installer upgrade, release workflow are all assigned to tasks.
- No old root `MainWindow.cpp` is compiled into `v0418`.
- Every network/install path fails closed; manual check remains independent from the auto-check preference.
- Close smoke is deterministic through a test-only slow-update environment variable rather than public internet timing.
