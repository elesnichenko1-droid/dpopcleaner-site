# DPopCleaner 0.4.18 Native Core Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a testable DPopCleaner 0.4.18 native development candidate with application-owned settings, verified update plumbing, non-blocking close, responsive native shell, current companion-module continuity, and a development-only installer/CI path that cannot publish or alter the stable 0.4.17 rev.19 line.

**Architecture:** New code lives only under `v0418/` plus Phase-A-specific tests/tools/workflow/DEV installer files. Historical `feat/dpopcleaner-0.4.18-core-update` files are audited donors, never merged wholesale. Phase A deliberately excludes bundled Zapret and all public-release ownership; current rev.19 remains the stable product and rollback point.

**Tech Stack:** Windows 10/11 x64, C++20, Win32, CMake 3.24+, MSVC/windows-2022, BCrypt SHA-256, WinVerifyTrust/AuthentiCode, WinHTTP, Inno Setup 6, PowerShell, Python 3.12 contract tests, GitHub Actions artifact upload.

**Spec:** `docs/superpowers/specs/2026-09-06-dpopcleaner-0.4.18-clean-resurrection-design.md`

## Global Constraints

- Phase A baseline is `437f209b12ac4990074684e62153aa9c50b29efc` plus the approved design/plan documentation commits.
- Stable production remains `DPopCleaner 0.4.17 rev.19` throughout Phase A.
- Do not delete or modify `.github/workflows/publish-dpopcleaner-0.4.17.yml` except for a separately approved rev.19 fix.
- Do not modify `version.json`, `update/stable.json`, `release-manifest.js`, public site files, `v0.4.17-rev19`, or any rev.19 release asset during Phase A.
- Do not create `.github/workflows/publish-dpopcleaner-0.4.18.yml` during Phase A.
- Do not call `gh release create`, `gh release upload`, `actions/deploy-pages`, or request `pages: write` from the Phase A workflow.
- Phase A workflow permissions are `contents: read` only; `actions/upload-artifact` is allowed.
- Local 0.4.18 identity is version `0.4.18`, version code `418`, revision `1`; the build flavor is `development`; the update channel consumed by the client is `stable`.
- Phase A DEV installer is named `DPopCleaner_Setup_0.4.18_DEV.exe`; the future public `DPopCleaner_Setup_0.4.18.exe` belongs to Phase C.
- Phase A does not stage `ThirdParty/Zapret`, does not compile `ZapretController`, and does not add a native Zapret page. Those are Plan B.
- Current companion binaries remain separate modules: `DPop.Common.dll`, `DiskAnalyzer.exe`, `RestoreCenter.exe`, `ZapretScreenFix.exe`.
- New code must not compile the reconstructed root `MainWindow.cpp` or old historical 0.4.18 workflow/site state.
- All behavioral changes use RED -> GREEN. Every task ends with exact-head verification before the next task starts.
- No merge to `main` until the complete Phase A gate is GREEN and a final diff proves stable metadata/publication files are untouched.

## File Structure Locked By This Plan

```text
.github/workflows/
  DPopCleaner_0.4.18_PHASE_A.yml
release/
  DPopCleaner_0.4.18-dev.iss
tests/
  test_dpop0418_phase_a_contract.py
tools/
  dpop0418_close_smoke.ps1
  dpop0418_layout_smoke.ps1
  dpop0418_stage_phase_a.ps1
  dpop0418_install_phase_a_smoke.ps1
  dpop0418_rev19_upgrade_smoke.ps1
v0418/
  CMakeLists.txt
  stage-phase-a-allowlist.txt
  core/
    AppSettings.h/.cpp
    CompanionPaths.h/.cpp
    Hash.h/.cpp
    LayoutModel.h/.cpp
    MainWindow.h/.cpp
    Signature.h/.cpp
    UpdateClient.h/.cpp
    UpdateManifest.h/.cpp
    UpdatePolicy.h/.cpp
    Version.h
    main.cpp
  updater/
    UpdaterMain.cpp
  resources/
    version.rc.in
  tests/
    AppSettingsTests.cpp
    CompanionPathsTests.cpp
    LayoutModelTests.cpp
    PackageVerificationTests.cpp
    UpdateClientContractTests.cpp
    UpdateManifestTests.cpp
    UpdatePolicyTests.cpp
```

---

### Task 1: Bootstrap the Phase A safety boundary and native skeleton

**Files:**
- Create: `tests/test_dpop0418_phase_a_contract.py`
- Create: `.github/workflows/DPopCleaner_0.4.18_PHASE_A.yml`
- Create: `v0418/CMakeLists.txt`
- Create: `v0418/core/Version.h`
- Create: `v0418/core/MainWindow.h`
- Create: `v0418/core/MainWindow.cpp`
- Create: `v0418/core/main.cpp`
- Create: `v0418/resources/version.rc.in`

**Interfaces:**
- Produces: `dpop0418::RunMainWindow(HINSTANCE,int)`.
- Produces version constants `kVersion`, `kVersionCode`, `kRevision`, `kUpdateChannel`, `kBuildFlavor`.
- Produces an artifact-only workflow used by all later tasks.

- [ ] **Step 1: Create the RED Phase A contract and safe workflow**

Create `tests/test_dpop0418_phase_a_contract.py` with these exact assertions:

```python
import json
import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class PhaseAContract(unittest.TestCase):
    def test_stable_rev19_identity_is_untouched(self):
        version = json.loads((ROOT / "version.json").read_text(encoding="utf-8"))
        stable = json.loads((ROOT / "update/stable.json").read_text(encoding="utf-8"))
        self.assertEqual((version["version"], version["version_code"], version["revision"]),
                         ("0.4.17", 417, 19))
        self.assertEqual((stable["version"], stable["version_code"], stable["revision"]),
                         ("0.4.17", 417, 19))
        self.assertFalse(stable["available"])

    def test_rev19_publisher_remains_owner_of_stable(self):
        text = (ROOT / ".github/workflows/publish-dpopcleaner-0.4.17.yml").read_text(encoding="utf-8")
        self.assertIn("RELEASE_TAG: v0.4.17-rev19", text)
        self.assertNotIn("v0418/**", text)

    def test_phase_a_has_no_public_publisher(self):
        self.assertFalse((ROOT / ".github/workflows/publish-dpopcleaner-0.4.18.yml").exists())
        wf = (ROOT / ".github/workflows/DPopCleaner_0.4.18_PHASE_A.yml").read_text(encoding="utf-8").lower()
        for forbidden in ("pages: write", "contents: write", "deploy-pages", "gh release", "update/stable.json"):
            self.assertNotIn(forbidden, wf)
        self.assertIn("contents: read", wf)
        self.assertIn("actions/upload-artifact", wf)

    def test_native_identity_and_source_boundary_exist(self):
        version_h = (ROOT / "v0418/core/Version.h").read_text(encoding="utf-8")
        self.assertIn('kVersion[] = L"0.4.18"', version_h)
        self.assertIn("kVersionCode = 418", version_h)
        self.assertIn("kRevision = 1", version_h)
        self.assertIn('kBuildFlavor[] = L"development"', version_h)
        cmake = (ROOT / "v0418/CMakeLists.txt").read_text(encoding="utf-8")
        self.assertNotIn("../mainwindow.cpp", cmake.lower())
        self.assertNotIn("zapretcontroller", cmake.lower())

if __name__ == "__main__":
    unittest.main()
```

Create `.github/workflows/DPopCleaner_0.4.18_PHASE_A.yml` initially with `workflow_dispatch`, PR-to-main, and push branches `main` plus `feat/0.4.18-*`. Its paths are only this workflow, `v0418/**`, `tests/test_dpop0418_phase_a_contract.py`, `tools/dpop0418_*phase_a*.ps1`, `tools/dpop0418_close_smoke.ps1`, `tools/dpop0418_layout_smoke.ps1`, and `release/DPopCleaner_0.4.18-dev.iss`. Set:

```yaml
permissions:
  contents: read
```

The first job steps are checkout, Python 3.12, and:

```yaml
- name: Verify Phase A safety contract
  shell: pwsh
  run: python tests/test_dpop0418_phase_a_contract.py -v
```

Do not add any release or Pages step.

- [ ] **Step 2: Run the RED contract**

Run:

```powershell
python tests/test_dpop0418_phase_a_contract.py -v
```

Expected: FAIL because `v0418/core/Version.h` and `v0418/CMakeLists.txt` do not yet exist on the clean resurrection branch.

- [ ] **Step 3: Add the minimal native skeleton**

Create `v0418/core/Version.h`:

```cpp
#pragma once
namespace dpop0418::version {
inline constexpr wchar_t kProductName[] = L"DPopCleaner";
inline constexpr wchar_t kVersion[] = L"0.4.18";
inline constexpr int kVersionCode = 418;
inline constexpr int kRevision = 1;
inline constexpr wchar_t kUpdateChannel[] = L"stable";
inline constexpr wchar_t kBuildFlavor[] = L"development";
}
```

Create `MainWindow.h` with `int RunMainWindow(HINSTANCE instance, int showCommand);`, a minimal Win32 window in `MainWindow.cpp`, and `main.cpp` forwarding `wWinMain` to it. Window title must be `DPopCleaner 0.4.18 DEVELOPMENT`.

Create `v0418/CMakeLists.txt` from the historical donor structure but only build `DPopCleaner` initially; C++20, Unicode, x64/MSVC warnings, generated RC version resource, and no `ZapretController` target/source.

The generated resource must report file/product version `0.4.18.1` and description `DPopCleaner 0.4.18 Development`.

- [ ] **Step 4: Extend workflow to configure/build the skeleton and run GREEN**

Add:

```yaml
- name: Configure native Phase A
  shell: pwsh
  run: cmake -S v0418 -B build0418 -A x64
- name: Build native Phase A
  shell: pwsh
  run: cmake --build build0418 --config Release
```

Run locally or via exact-head CI:

```powershell
python tests/test_dpop0418_phase_a_contract.py -v
cmake -S v0418 -B build0418 -A x64
cmake --build build0418 --config Release
```

Expected: contract PASS and `build0418/bin/Release/DPopCleaner.exe` exists with FileVersion `0.4.18.1`.

- [ ] **Step 5: Commit**

```bash
git add tests/test_dpop0418_phase_a_contract.py .github/workflows/DPopCleaner_0.4.18_PHASE_A.yml v0418
git commit -m "feat: bootstrap 0.4.18 phase A native boundary"
```

---

### Task 2: Add application-owned settings with atomic persistence

**Files:**
- Create: `v0418/core/AppSettings.h`
- Create: `v0418/core/AppSettings.cpp`
- Create: `v0418/tests/AppSettingsTests.cpp`
- Modify: `v0418/CMakeLists.txt`

**Interfaces:**
- Produces: `struct AppSettings { bool autoCheckUpdates{true}; };`
- Produces: `AppSettings LoadSettings(const std::filesystem::path&)`.
- Produces: `bool SaveSettingsAtomic(const std::filesystem::path&, const AppSettings&, std::wstring&)`.

- [ ] **Step 1: Write RED settings tests**

Use the historical `AppSettingsTests.cpp` as a donor but remove all Zapret-strategy assertions. The required test sequence is:

```cpp
const auto defaults = dpop0418::LoadSettings(settingsPath);
if (!defaults.autoCheckUpdates) return Fail("missing settings must default enabled");

WriteText(settingsPath, "[updates]\nauto_check=0\n");
if (dpop0418::LoadSettings(settingsPath).autoCheckUpdates)
    return Fail("auto_check=0 must disable startup checks");

WriteText(settingsPath, "[updates]\nauto_check=maybe\n");
if (!dpop0418::LoadSettings(settingsPath).autoCheckUpdates)
    return Fail("malformed value must fail safe to enabled");

dpop0418::AppSettings disabled{};
disabled.autoCheckUpdates = false;
std::wstring error;
if (!dpop0418::SaveSettingsAtomic(settingsPath, disabled, error)) return Fail("save failed");
if (dpop0418::LoadSettings(settingsPath).autoCheckUpdates) return Fail("saved value did not persist");
if (fs::exists(settingsPath.wstring() + L".tmp")) return Fail("temporary file leaked");
```

Add an `AppSettingsTests` CMake target before implementation.

- [ ] **Step 2: Run RED**

```powershell
cmake -S v0418 -B build0418 -A x64
cmake --build build0418 --config Release --target AppSettingsTests
ctest --test-dir build0418 -C Release -R AppSettingsTests --output-on-failure
```

Expected: compile/link failure because `AppSettings` implementation is missing.

- [ ] **Step 3: Implement the minimum audited donor**

Start from historical `v0418/core/AppSettings.cpp`, but Phase A supports only `[updates] auto_check`. Preserve its Win32 atomic-write sequence exactly:

```cpp
CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
WriteFile(...);
FlushFileBuffers(file);
CloseHandle(file);
MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
```

On every failure delete the `.tmp` file where applicable, return `false`, and set a non-empty user-facing error. Missing file and malformed value return the default `true` setting.

- [ ] **Step 4: Run GREEN and full current Phase A contract**

```powershell
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
python tests/test_dpop0418_phase_a_contract.py -v
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add v0418/core/AppSettings.* v0418/tests/AppSettingsTests.cpp v0418/CMakeLists.txt
git commit -m "feat: add 0.4.18 atomic app settings"
```

---

### Task 3: Add pure update policy and fail-closed stable manifest parsing

**Files:**
- Create: `v0418/core/UpdatePolicy.h/.cpp`
- Create: `v0418/core/UpdateManifest.h/.cpp`
- Create: `v0418/tests/UpdatePolicyTests.cpp`
- Create: `v0418/tests/UpdateManifestTests.cpp`
- Modify: `v0418/CMakeLists.txt`

**Interfaces:**
- Produces: `VersionIdentity { int versionCode; int revision; }`.
- Produces: `bool IsRemoteNewer(VersionIdentity local, VersionIdentity remote)`.
- Produces the `UpdateManifest` struct and parsing/validation functions.

- [ ] **Step 1: Write RED policy tests**

Require exactly:

```cpp
IsRemoteNewer({418,1}, {419,1}) == true;
IsRemoteNewer({418,1}, {418,2}) == true;
IsRemoteNewer({418,1}, {417,99}) == false;
IsRemoteNewer({418,1}, {418,1}) == false;
IsRemoteNewer({418,2}, {418,1}) == false;
```

- [ ] **Step 2: Write RED manifest tests**

Start from the historical donor test and require a valid stable fixture with HTTPS URL, 64-hex SHA and positive size. Add explicit failures for:

```text
missing channel
channel=beta
available=false
http:// download_url
63-character sha256
non-hex 64-character sha256
size=0
revision=0
```

`product` may be absent; when present and not `DPopCleaner`, `IsUsableStableManifest` must reject it.

- [ ] **Step 3: Run RED**

```powershell
cmake --build build0418 --config Release --target UpdatePolicyTests UpdateManifestTests
ctest --test-dir build0418 -C Release -R "UpdatePolicyTests|UpdateManifestTests" --output-on-failure
```

Expected: fail/compile failure before implementation.

- [ ] **Step 4: Implement the pure policy and audited manifest parser**

`UpdatePolicy.cpp` is exactly:

```cpp
bool IsRemoteNewer(VersionIdentity local, VersionIdentity remote) {
    if (remote.versionCode != local.versionCode) return remote.versionCode > local.versionCode;
    return remote.revision > local.revision;
}
```

Use the historical manifest donor as the starting parser, but make `channel` mandatory at parse time. `IsUsableStableManifest` must fail closed on every condition listed by the RED tests.

- [ ] **Step 5: Run GREEN**

```powershell
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
```

Expected: all native tests PASS.

- [ ] **Step 6: Commit**

```bash
git add v0418/core/UpdatePolicy.* v0418/core/UpdateManifest.* v0418/tests/UpdatePolicyTests.cpp v0418/tests/UpdateManifestTests.cpp v0418/CMakeLists.txt
git commit -m "feat: add fail-closed 0.4.18 update manifest policy"
```

---

### Task 4: Add deterministic package verification and Authenticode boundary

**Files:**
- Create: `v0418/core/Hash.h/.cpp`
- Create: `v0418/core/Signature.h/.cpp`
- Create: `v0418/tests/PackageVerificationTests.cpp`
- Modify: `v0418/CMakeLists.txt`

**Interfaces:**
- Produces `bool Sha256File(path, std::wstring& hex, std::wstring& error)`.
- Produces `bool VerifyPackageFile(path, const UpdateManifest&, std::wstring& error)`.
- Produces `bool VerifyAuthenticode(path, std::wstring& error)`.

- [ ] **Step 1: Write RED package tests**

Use a temporary fixture file and require:

```cpp
Sha256File(file, manifest.sha256, error) == true;
VerifyPackageFile(file, manifest, error) == true;
VerifyPackageFile(file, manifest_with_size_plus_one, error) == false;
VerifyPackageFile(file, manifest_with_64_zero_hash, error) == false;
```

Also assert a missing file returns false with non-empty error.

- [ ] **Step 2: Run RED**

```powershell
cmake --build build0418 --config Release --target PackageVerificationTests
ctest --test-dir build0418 -C Release -R PackageVerificationTests --output-on-failure
```

Expected: missing symbols/implementation.

- [ ] **Step 3: Implement audited helpers**

Import the historical BCrypt hash helper and WinVerifyTrust signature helper as isolated donor units. `VerifyPackageFile` checks `is_regular_file`, exact `file_size == manifest.size`, computes SHA-256, and compares case-insensitively. It never launches anything.

Link package verification tests to `bcrypt`; signature users later link `wintrust` and `crypt32`.

- [ ] **Step 4: Run GREEN**

```powershell
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
```

- [ ] **Step 5: Commit**

```bash
git add v0418/core/Hash.* v0418/core/Signature.* v0418/tests/PackageVerificationTests.cpp v0418/CMakeLists.txt
git commit -m "feat: add 0.4.18 package verification"
```

---

### Task 5: Add bounded update client, deterministic evaluation, and safe `.part` promotion

**Files:**
- Create: `v0418/core/UpdateClient.h/.cpp`
- Create: `v0418/tests/UpdateClientContractTests.cpp`
- Modify: `v0418/CMakeLists.txt`

**Interfaces:**
- Consumes `UpdateManifest`, `VersionIdentity`, `Sha256File`, `VerifyPackageFile`, `VerifyAuthenticode`.
- Produces `UpdateCheckResult`.
- Produces `EvaluateStableManifestJson(const std::string&, VersionIdentity)`.
- Produces `CheckStableUpdates(const std::atomic_bool*)`.
- Produces `DownloadVerifiedPackage(...)`.
- Produces `PromoteVerifiedPartFile(partPath, finalPath, manifest, error)`.
- Produces updater argument/staging/launch helpers.

- [ ] **Step 1: Write RED client contract tests without internet**

Require the exact stable manifest URL and pure evaluation:

```cpp
const auto result = dpop0418::EvaluateStableManifestJson(validJson, {418,1});
if (!result.success || !result.updateAvailable) return Fail("newer stable manifest must evaluate as update");
```

Create a local `.part` fixture and test `PromoteVerifiedPartFile`:

```cpp
// correct manifest => final exists, part removed
// wrong size => returns false, part removed, final absent
// wrong hash => returns false, part removed, final absent
```

Keep the historical updater-argument assertions for parent PID, quoted package path, expected SHA, restart path, and explicit `--allow-unsigned`.

- [ ] **Step 2: Run RED**

```powershell
cmake --build build0418 --config Release --target UpdateClientContractTests
ctest --test-dir build0418 -C Release -R UpdateClientContractTests --output-on-failure
```

Expected: missing UpdateClient implementation.

- [ ] **Step 3: Implement the deterministic boundary first**

Add:

```cpp
UpdateCheckResult EvaluateStableManifestJson(const std::string& json, VersionIdentity local) {
    UpdateCheckResult result{};
    if (!ParseUpdateManifestUtf8(json, result.manifest, result.error)) return result;
    if (!IsUsableStableManifest(result.manifest, result.error)) return result;
    result.success = true;
    result.updateAvailable = IsRemoteNewer(local, {result.manifest.versionCode, result.manifest.revision});
    return result;
}
```

`PromoteVerifiedPartFile` must call `VerifyPackageFile`; on any verification or move failure it deletes the `.part` file and returns false. Only a verified part is moved to final with `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`.

- [ ] **Step 4: Add bounded WinHTTP networking**

Use the historical WinHTTP donor with these retained boundaries:

```cpp
WinHttpSetTimeouts(session, 5000, 5000, 10000, 10000);
```

Require HTTPS before opening a request, require HTTP 2xx, check `shutdown` between reads, refuse bytes beyond manifest size, flush the `.part`, then delegate final verification/promotion to `PromoteVerifiedPartFile`.

Keep `DPOP0418_TEST_SLOW_UPDATE_MS` as a deterministic test hook that sleeps in <=25 ms slices while observing shutdown.

- [ ] **Step 5: Run GREEN**

```powershell
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
```

- [ ] **Step 6: Commit**

```bash
git add v0418/core/UpdateClient.* v0418/tests/UpdateClientContractTests.cpp v0418/CMakeLists.txt
git commit -m "feat: add bounded 0.4.18 update client"
```

---

### Task 6: Add independent DPopUpdater handoff and second verification

**Files:**
- Create: `v0418/updater/UpdaterMain.cpp`
- Modify: `v0418/tests/UpdateClientContractTests.cpp`
- Modify: `v0418/CMakeLists.txt`

**Interfaces:**
- Consumes updater arguments produced by `BuildUpdaterArguments`.
- Re-verifies expected SHA after parent process exit.
- Runs installer elevated only after verification.
- Restarts installed `DPopCleaner.exe` only after installer exit `0` or `3010`.

- [ ] **Step 1: Strengthen RED updater handoff contract**

Before adding `UpdaterMain.cpp`, add source-contract assertions that require the updater source to contain `Sha256File` before `ShellExecuteExW`, and the client to call `StageUpdaterForHandoff` before setting `execute.lpFile = stagedUpdater.c_str()`.

Expected RED: updater source does not exist.

- [ ] **Step 2: Implement from audited donor**

Use the historical `UpdaterMain.cpp` flow:

1. parse `--parent`, `--package`, `--sha256`, optional `--args`, `--restart`, `--signed`, `--allow-unsigned`;
2. wait up to 30 seconds for the parent;
3. compute SHA-256 and compare case-insensitively;
4. verify Authenticode when required;
5. refuse unsigned package without explicit `--allow-unsigned`;
6. run installer via `ShellExecuteExW` with `runas`;
7. accept exit `0` or `3010` only;
8. restart the supplied executable if present.

Build `DPopUpdater` as a separate WIN32 target with the same `0.4.18.1` version resource.

- [ ] **Step 3: Run GREEN**

```powershell
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
```

Additionally:

```powershell
(Get-Item build0418/bin/Release/DPopUpdater.exe).VersionInfo.FileVersion
```

Expected: `0.4.18.1`.

- [ ] **Step 4: Commit**

```bash
git add v0418/updater/UpdaterMain.cpp v0418/tests/UpdateClientContractTests.cpp v0418/CMakeLists.txt
git commit -m "feat: add verified 0.4.18 updater handoff"
```

---

### Task 7: Add responsive layout model and companion path boundary

**Files:**
- Create: `v0418/core/LayoutModel.h/.cpp`
- Create: `v0418/core/CompanionPaths.h/.cpp`
- Create: `v0418/tests/LayoutModelTests.cpp`
- Create: `v0418/tests/CompanionPathsTests.cpp`
- Modify: `v0418/CMakeLists.txt`

**Interfaces:**
- Produces `MainLayout ComputeMainLayout(int clientWidth, int clientHeight, int dpi)`.
- Produces `CompanionPath(exeDir, CompanionKind)`.
- MainWindow later consumes both; neither depends on HWND.

- [ ] **Step 1: Write RED layout tests**

Define simple integer rectangles and require the model to be valid for these client sizes:

```cpp
{1000, 700}, {1340, 740}, {1650, 780}, {1880, 890}
```

For each layout assert:

- every visible rectangle has positive width/height;
- sidebar ends before content begins;
- page title/hint/content/action rectangles stay inside client bounds;
- action buttons do not overlap each other;
- content bottom is above the first action-row top;
- four actions fit by switching to two rows when the width is insufficient;
- DPI values `96`, `120`, `144` preserve the same non-overlap invariants.

Do not pass theme into `ComputeMainLayout`; geometry is therefore independent of color theme by construction.

- [ ] **Step 2: Write RED companion path tests**

Require exact paths from an executable directory:

```text
Modules/DiskAnalyzer.exe
Modules/RestoreCenter.exe
Modules/ZapretScreenFix.exe
```

No path may escape the executable directory.

- [ ] **Step 3: Run RED**

```powershell
cmake --build build0418 --config Release --target LayoutModelTests CompanionPathsTests
ctest --test-dir build0418 -C Release -R "LayoutModelTests|CompanionPathsTests" --output-on-failure
```

- [ ] **Step 4: Implement pure helpers**

Use a sidebar clamped to a DPI-scaled 190-250 px range; use 16 px logical margins, 44 px logical action height, and choose four columns only when each action can remain at least 150 logical px wide. Otherwise use a 2x2 action grid. Use only the computed layout in MainWindow; do not duplicate position math in page renderers.

`CompanionPath` joins `exeDir / "Modules" / filename` from an enum switch; it never accepts a free-form relative path from UI text.

- [ ] **Step 5: Run GREEN and commit**

```powershell
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
```

```bash
git add v0418/core/LayoutModel.* v0418/core/CompanionPaths.* v0418/tests/LayoutModelTests.cpp v0418/tests/CompanionPathsTests.cpp v0418/CMakeLists.txt
git commit -m "feat: add responsive 0.4.18 layout primitives"
```

---

### Task 8: Build the Phase A native shell, Settings/Updates UI, and non-blocking close

**Files:**
- Modify: `v0418/core/MainWindow.cpp`
- Modify: `v0418/CMakeLists.txt`
- Create: `tools/dpop0418_close_smoke.ps1`
- Create: `tools/dpop0418_layout_smoke.ps1`
- Modify: `.github/workflows/DPopCleaner_0.4.18_PHASE_A.yml`

**Interfaces:**
- Consumes AppSettings, UpdateClient, LayoutModel, CompanionPaths.
- Stable Win32 IDs: nav controls `1000+`, action controls `2000+`.
- Uses `WM_APP + 41` for update completion and timer id `4101` for startup update check.

- [ ] **Step 1: Add RED deterministic close smoke**

Port the historical `dpop0418_close_smoke.ps1` with these invariants unchanged:

```powershell
$env:DPOP0418_TEST_SLOW_UPDATE_MS = '10000'
$env:DPOP0418_SETTINGS_PATH = $settings
# allow the 300 ms startup timer to start the worker
Start-Sleep -Milliseconds 900
# PostMessage WM_CLOSE = 0x0010
# process must exit before stopwatch reaches 500 ms
```

Run it against the current skeleton. Expected RED because the skeleton has no slow update worker and Settings/startup-update behavior yet.

- [ ] **Step 2: Add RED runtime layout smoke**

`dpop0418_layout_smoke.ps1` starts the app, finds the main HWND, sends the Settings navigation command, resizes the outer window to `1024x768`, `1366x800`, `1680x840`, `1908x950`, enumerates visible child controls and writes one JSON record per size. Fail if any visible nav/action control lies outside the client rectangle or if visible action rectangles overlap.

Stable Phase A navigation pages are:

```text
Overview
Cleaning
Disk Analyzer
Restore Center
Zapret Screen Fix
Updates
Settings
```

Zapret itself is intentionally absent until Phase B.

- [ ] **Step 3: Implement native shell using the pure layout model**

Replace fixed historical coordinates with `ComputeMainLayout` in `WM_CREATE`, `WM_SIZE`, and DPI changes. Keep owner-drawn action/nav buttons. Main page actions launch only approved module paths through `CompanionPath`.

Settings page actions are exactly:

```text
Автообновление: ВКЛ/ВЫКЛ
Проверить обновления сейчас
Открыть логи
Сайт проекта
```

Toggling `auto_check` must call `SaveSettingsAtomic` immediately and refresh the page only after a successful save.

- [ ] **Step 4: Implement non-blocking update worker lifecycle**

Use process-wide atomics:

```cpp
std::atomic_bool gShuttingDown{false};
std::atomic_bool gUpdateBusy{false};
```

`StartUpdateCheck` launches a detached worker that captures HWND by value only, calls `CheckStableUpdates(&gShuttingDown)`, and posts a heap/value result only through a guarded `PublishPending` that checks shutdown and `IsWindow` before `PostMessageW`.

`WM_CLOSE` is exactly non-blocking in shape:

```cpp
gShuttingDown.store(true, std::memory_order_release);
KillTimer(hwnd, ID_STARTUP_UPDATE_TIMER);
ShowWindow(hwnd, SW_HIDE);
DestroyWindow(hwnd);
return 0;
```

Do not join any worker and do not call WinHTTP from `WM_CLOSE` or `WM_DESTROY`.

Automatic offline failures are silent; manual-check failures render on the Updates page. Optional update download begins only after explicit user confirmation.

- [ ] **Step 5: Run GREEN UI/close checks**

```powershell
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
./tools/dpop0418_close_smoke.ps1 -Exe build0418/bin/Release/DPopCleaner.exe
./tools/dpop0418_layout_smoke.ps1 -Exe build0418/bin/Release/DPopCleaner.exe -OutputDir _release/0.4.18-dev/evidence/layout
```

Expected: close <500 ms and all four size records PASS.

- [ ] **Step 6: Add both smokes to Phase A workflow and commit**

```bash
git add v0418/core/MainWindow.cpp tools/dpop0418_close_smoke.ps1 tools/dpop0418_layout_smoke.ps1 .github/workflows/DPopCleaner_0.4.18_PHASE_A.yml
git commit -m "feat: add responsive non-blocking 0.4.18 native shell"
```

---

### Task 9: Stage a Phase A payload and build a development-only installer

**Files:**
- Create: `v0418/stage-phase-a-allowlist.txt`
- Create: `tools/dpop0418_stage_phase_a.ps1`
- Create: `release/DPopCleaner_0.4.18-dev.iss`
- Create: `tools/dpop0418_install_phase_a_smoke.ps1`
- Modify: `.github/workflows/DPopCleaner_0.4.18_PHASE_A.yml`

**Interfaces:**
- Produces `_release/0.4.18-dev/stage`.
- Produces `_release/0.4.18-dev/installer/DPopCleaner_Setup_0.4.18_DEV.exe`.
- Does not contain `ThirdParty/Zapret`.

- [ ] **Step 1: Add RED exact stage allowlist**

The file content is exactly:

```text
DPopCleaner.exe
DPopUpdater.exe
Languages/
Shell/
Documentation/
Modules/DPop.Common.dll
Modules/DiskAnalyzer.exe
Modules/RestoreCenter.exe
Modules/ZapretScreenFix.exe
Resources/
```

`dpop0418_stage_phase_a.ps1` must reject any allowlist difference and assert `ThirdParty/Zapret` is absent from the staged tree.

- [ ] **Step 2: Implement Phase A staging**

Copy native binaries from `build0418/bin/Release`, current companion binaries from `v0417/src/**/bin/Release/net48`, and current approved resource directories from `v0417/payload`. Verify both native FileVersions equal `0.4.18.1`.

Do not copy historical 0.4.18 third-party notices or Zapret payload.

- [ ] **Step 3: Create DEV Inno installer**

Use the existing DPopCleaner AppId so upgrade mechanics are representative, but make identity visibly development-only:

```ini
AppVersion=0.4.18
AppVerName=DPopCleaner 0.4.18 Development
VersionInfoVersion=0.4.18.1
OutputBaseFilename=DPopCleaner_Setup_0.4.18_DEV
```

Install only the Phase A stage files. There is no `ThirdParty` directory, no `[InstallDelete]` for Zapret, and no Zapret backup code in this DEV installer.

- [ ] **Step 4: Write RED/then GREEN installed smoke**

The installed smoke must verify:

- `DPopCleaner.exe` and `DPopUpdater.exe`, FileVersion `0.4.18.1`;
- all four current companion module files;
- current Languages/Shell/Documentation resources;
- `ThirdParty/Zapret` does not exist in a clean Phase A install;
- Documentation remains writable/preserved across in-place DEV reinstall;
- installed close smoke passes;
- Disk Analyzer and Restore Center existing smoke scripts pass;
- silent uninstall removes the native exe.

Build and run:

```powershell
./tools/dpop0418_stage_phase_a.ps1 -RequireCompanions
& "$env:ProgramFiles(x86)\Inno Setup 6\ISCC.exe" release/DPopCleaner_0.4.18-dev.iss
./tools/dpop0418_install_phase_a_smoke.ps1 -InstallerPath _release/0.4.18-dev/installer/DPopCleaner_Setup_0.4.18_DEV.exe
```

- [ ] **Step 5: Add staging/installer/install smoke to CI and commit**

```bash
git add v0418/stage-phase-a-allowlist.txt tools/dpop0418_stage_phase_a.ps1 release/DPopCleaner_0.4.18-dev.iss tools/dpop0418_install_phase_a_smoke.ps1 .github/workflows/DPopCleaner_0.4.18_PHASE_A.yml
git commit -m "feat: add 0.4.18 phase A development installer gate"
```

---

### Task 10: Prove rev.19 -> Phase A upgrade safety without activating 0.4.18 stable

**Files:**
- Create: `tools/dpop0418_rev19_upgrade_smoke.ps1`
- Modify: `.github/workflows/DPopCleaner_0.4.18_PHASE_A.yml`
- Modify: `tests/test_dpop0418_phase_a_contract.py`

**Interfaces:**
- Consumes the final verified rev.19 release artifact.
- Installs rev.19 to an isolated temp directory, creates user-data sentinels, overlays the Phase A DEV installer, and verifies preservation.

- [ ] **Step 1: Pin the rev.19 rollback source in the contract**

The current verified production reference is:

```text
tag: v0.4.17-rev19
source/release target: 437f209b12ac4990074684e62153aa9c50b29efc
installer size: 3523734
installer SHA-256: 4818b2faa6d7deb48d9777cf9704240336201f79757d3296d66396770c6b6e61
```

The smoke must refuse to proceed if the downloaded rev.19 installer differs from that exact size/hash.

- [ ] **Step 2: Implement isolated upgrade smoke**

Workflow:

1. download `DPopCleaner_Setup_0.4.17.exe` from `v0.4.17-rev19`;
2. verify exact size/hash above;
3. silently install it into a temporary directory;
4. create a unique sentinel under installed `Documentation` and one under `%LOCALAPPDATA%\DPopCleaner`;
5. install `DPopCleaner_Setup_0.4.18_DEV.exe` to the same directory;
6. assert both sentinels remain;
7. assert installed core/updater are `0.4.18.1`;
8. run installed close smoke;
9. uninstall and clean temporary state.

Do not check or alter live stable manifest during this smoke.

- [ ] **Step 3: Run the upgrade smoke in exact-head CI**

Add it after the normal installed smoke. Expected: PASS. The workflow still has only `contents: read` and uploads artifacts only.

- [ ] **Step 4: Commit**

```bash
git add tools/dpop0418_rev19_upgrade_smoke.ps1 tests/test_dpop0418_phase_a_contract.py .github/workflows/DPopCleaner_0.4.18_PHASE_A.yml
git commit -m "test: prove rev19 to 0.4.18 phase A upgrade safety"
```

---

### Task 11: Finalize the complete Phase A candidate gate and review evidence

**Files:**
- Modify: `.github/workflows/DPopCleaner_0.4.18_PHASE_A.yml`
- Modify only if an assertion gap is found: `tests/test_dpop0418_phase_a_contract.py`

**Interfaces:**
- Produces one inspectable artifact `DPopCleaner-0.4.18-phase-a-${{ github.run_id }}`.
- No publication side effect.

- [ ] **Step 1: Make the Phase A workflow run the full gate in this order**

```text
1. checkout exact revision
2. Python Phase A safety contract
3. configure x64 CMake
4. build all native tests + DPopCleaner + DPopUpdater
5. CTest --output-on-failure
6. build current rev.19 companion modules
7. native close smoke
8. native four-size layout smoke
9. stage exact Phase A payload
10. staged close smoke
11. staged Disk Analyzer smoke
12. staged Restore Center smoke
13. build DEV Inno installer
14. installed Phase A smoke
15. rev.19 -> Phase A isolated upgrade smoke
16. record DEV installer SHA-256 and size
17. upload stage/installer/evidence artifact
```

No job named `publish`; no environment `github-pages`; no write permission.

- [ ] **Step 2: Run fresh complete exact-head verification**

Required fresh evidence before claiming Phase A complete:

```powershell
python tests/test_dpop0418_phase_a_contract.py -v
cmake -S v0418 -B build0418 -A x64
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
./tools/dpop0418_close_smoke.ps1 -Exe build0418/bin/Release/DPopCleaner.exe
./tools/dpop0418_layout_smoke.ps1 -Exe build0418/bin/Release/DPopCleaner.exe -OutputDir _release/0.4.18-dev/evidence/layout
./tools/dpop0418_stage_phase_a.ps1 -RequireCompanions
./tools/dpop0418_install_phase_a_smoke.ps1 -InstallerPath _release/0.4.18-dev/installer/DPopCleaner_Setup_0.4.18_DEV.exe
./tools/dpop0418_rev19_upgrade_smoke.ps1 -DevInstaller _release/0.4.18-dev/installer/DPopCleaner_Setup_0.4.18_DEV.exe
```

CI conclusion must be `success` on the exact PR head.

- [ ] **Step 3: Perform the final diff safety review**

Compare implementation head to the design baseline. Allowed changed paths are only:

```text
.github/workflows/DPopCleaner_0.4.18_PHASE_A.yml
docs/superpowers/specs/2026-09-06-dpopcleaner-0.4.18-clean-resurrection-design.md
docs/superpowers/plans/2026-09-06-dpopcleaner-0.4.18-native-core-foundation.md
release/DPopCleaner_0.4.18-dev.iss
tests/test_dpop0418_phase_a_contract.py
tools/dpop0418_*.ps1
v0418/**
```

Explicitly fail review if the diff contains `version.json`, `update/stable.json`, `release-manifest.js`, site files, `.github/workflows/publish-dpopcleaner-0.4.17.yml`, or a public 0.4.18 publisher.

- [ ] **Step 4: Open/update the Phase A PR as development-only**

PR title:

```text
DPopCleaner 0.4.18 — Phase A native core foundation
```

PR body must state that it cannot publish, current stable remains rev.19, Zapret integration is deferred to Plan B, and the DEV installer is an inspection/test artifact only.

- [ ] **Step 5: Merge only after exact-head GREEN and final approval**

Use an exact-head merge guard. After merge, verify that the only automatic 0.4.18 workflow is the artifact-only Phase A workflow and that the rev.19 tag/release/live stable publication remains unchanged. Do not activate Phase B or Phase C in the same merge.

---

## Plan Self-Review

### Spec coverage

- Native core source boundary: Tasks 1-8.
- App-owned settings + atomic save: Task 2.
- Update policy/manifest: Task 3.
- Size/SHA/AuthentiCode verification: Tasks 4-6.
- Independent updater: Task 6.
- Responsive native shell and companion continuity: Tasks 7-9.
- Non-blocking close under deterministic slow update: Task 8.
- Development-only staging/installer: Task 9.
- rev.19 user-data migration safety: Task 10.
- No stable publication/Pages/release ownership: Tasks 1 and 11 plus Global Constraints.
- Bundled Zapret: intentionally excluded and assigned to Plan B per approved spec.
- Public release activation: intentionally excluded and assigned to Plan C per approved spec.

### Placeholder scan

No TBD/TODO/"implement later" placeholders are permitted by this plan. Phase B/Phase C exclusions are deliberate scope boundaries, not incomplete Phase A work.

### Type/interface consistency

- `VersionIdentity` is produced in Task 3 and consumed by Task 5.
- `UpdateManifest` is produced in Task 3 and consumed by Tasks 4-6.
- `Sha256File`, `VerifyPackageFile`, `VerifyAuthenticode` are produced in Task 4 and consumed by Tasks 5-6.
- `UpdateClient` produces the worker-facing API consumed by MainWindow in Task 8.
- `LayoutModel` and `CompanionPaths` are produced in Task 7 and consumed by MainWindow in Task 8.
- `DPopUpdater.exe` is built in Task 6 and staged/installed in Task 9.

## Completion Criterion

Plan A is complete only when an installed `0.4.18.1 DEVELOPMENT` candidate built from current rev.19 ancestry passes native unit tests, deterministic <500 ms close under a 10-second fake update delay, four-size responsive layout smoke, companion-module staging/launch smoke, development installer/reinstall smoke, isolated rev.19 -> DEV upgrade preservation, and the Phase A publication-safety contract — with no public 0.4.18 Release, stable manifest switch, Pages deployment, or rev.19 asset/tag mutation.