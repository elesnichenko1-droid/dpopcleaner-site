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
- Phase A workflow permissions are `contents: read` only; `actions/upload-artifact` is allowed and required for candidate inspection.
- Local 0.4.18 identity is version `0.4.18`, version code `418`, revision `1`; build flavor is `development`; the update channel consumed by the client is `stable`.
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

Create `tests/test_dpop0418_phase_a_contract.py`:

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

Create `.github/workflows/DPopCleaner_0.4.18_PHASE_A.yml` with `workflow_dispatch`, PR-to-main, and push branches `main` plus `feat/0.4.18-*`. Paths are only:

```yaml
- '.github/workflows/DPopCleaner_0.4.18_PHASE_A.yml'
- 'v0418/**'
- 'tests/test_dpop0418_phase_a_contract.py'
- 'tools/dpop0418_stage_phase_a.ps1'
- 'tools/dpop0418_install_phase_a_smoke.ps1'
- 'tools/dpop0418_rev19_upgrade_smoke.ps1'
- 'tools/dpop0418_close_smoke.ps1'
- 'tools/dpop0418_layout_smoke.ps1'
- 'release/DPopCleaner_0.4.18-dev.iss'
```

Set:

```yaml
permissions:
  contents: read
```

The first job steps are checkout, Python 3.12, the safety contract, configure/build, and a bootstrap artifact upload:

```yaml
- name: Verify Phase A safety contract
  shell: pwsh
  run: python tests/test_dpop0418_phase_a_contract.py -v

- name: Configure native Phase A
  shell: pwsh
  run: cmake -S v0418 -B build0418 -A x64

- name: Build native Phase A
  shell: pwsh
  run: cmake --build build0418 --config Release

- name: Upload Phase A bootstrap candidate
  uses: actions/upload-artifact@v4
  with:
    name: DPopCleaner-0.4.18-phase-a-bootstrap-${{ github.run_id }}
    path: build0418/bin/Release/DPopCleaner.exe
    if-no-files-found: error
```

Do not add release, Pages, environment, deployment, or write-permission steps.

- [ ] **Step 2: Run the RED contract**

```powershell
python tests/test_dpop0418_phase_a_contract.py -v
```

Expected: FAIL because `v0418/core/Version.h` and `v0418/CMakeLists.txt` do not yet exist.

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

Create `MainWindow.h` with:

```cpp
#pragma once
#include <windows.h>
namespace dpop0418 { int RunMainWindow(HINSTANCE instance, int showCommand); }
```

Create a minimal Win32 main window in `MainWindow.cpp` and `main.cpp` forwarding `wWinMain` to `RunMainWindow`. Window title is `DPopCleaner 0.4.18 DEVELOPMENT`.

Create `v0418/CMakeLists.txt` from the historical donor structure but initially build only `DPopCleaner`; use C++20, Unicode, `/W4 /permissive- /utf-8`, generated RC resource, and no `ZapretController` source/target.

The generated `version.rc.in` reports `0.4.18.1` and description `DPopCleaner 0.4.18 Development`.

- [ ] **Step 4: Run GREEN**

```powershell
python tests/test_dpop0418_phase_a_contract.py -v
cmake -S v0418 -B build0418 -A x64
cmake --build build0418 --config Release
(Get-Item build0418/bin/Release/DPopCleaner.exe).VersionInfo.FileVersion
```

Expected: contract PASS, build PASS, FileVersion `0.4.18.1`.

- [ ] **Step 5: Commit**

```bash
git add tests/test_dpop0418_phase_a_contract.py .github/workflows/DPopCleaner_0.4.18_PHASE_A.yml v0418
git commit -m "feat: bootstrap 0.4.18 phase A native boundary"
```

---

### Task 2: Add application-owned settings with atomic persistence

**Files:**
- Create: `v0418/core/AppSettings.h/.cpp`
- Create: `v0418/tests/AppSettingsTests.cpp`
- Modify: `v0418/CMakeLists.txt`

**Interfaces:**

```cpp
struct AppSettings { bool autoCheckUpdates{true}; };
AppSettings LoadSettings(const std::filesystem::path& path);
bool SaveSettingsAtomic(const std::filesystem::path& path,
                        const AppSettings& settings,
                        std::wstring& error);
```

- [ ] **Step 1: Write RED settings tests**

Use the historical `AppSettingsTests.cpp` as donor but remove every Zapret-strategy field/assertion. Required sequence:

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

Add `AppSettingsTests` CMake target before implementation.

- [ ] **Step 2: Run RED**

```powershell
cmake -S v0418 -B build0418 -A x64
cmake --build build0418 --config Release --target AppSettingsTests
ctest --test-dir build0418 -C Release -R AppSettingsTests --output-on-failure
```

Expected: compile/link failure because implementation is missing.

- [ ] **Step 3: Implement the audited atomic-write unit**

Start from historical `v0418/core/AppSettings.cpp`, but Phase A supports only `[updates] auto_check`. Preserve the safety sequence:

```cpp
CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
WriteFile(...);
FlushFileBuffers(file);
CloseHandle(file);
MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
```

On failure, delete `.tmp` where applicable, return `false`, and set non-empty `error`. Missing file and malformed values return default `true`.

- [ ] **Step 4: Run GREEN**

```powershell
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
python tests/test_dpop0418_phase_a_contract.py -v
```

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

```cpp
struct VersionIdentity { int versionCode{}; int revision{}; };
bool IsRemoteNewer(VersionIdentity local, VersionIdentity remote);

struct UpdateManifest {
    std::wstring product;
    std::wstring channel;
    std::wstring version;
    int versionCode{};
    int revision{};
    bool available{};
    bool mandatory{};
    std::wstring downloadUrl;
    std::wstring sha256;
    std::uint64_t size{};
    bool signedPackage{};
    std::wstring notesUrl;
    std::wstring installArgs;
};
bool ParseUpdateManifestUtf8(const std::string&, UpdateManifest&, std::wstring& error);
bool IsUsableStableManifest(const UpdateManifest&, std::wstring& error);
```

- [ ] **Step 1: Write RED policy tests**

Require:

```cpp
IsRemoteNewer({418,1}, {419,1}) == true;
IsRemoteNewer({418,1}, {418,2}) == true;
IsRemoteNewer({418,1}, {417,99}) == false;
IsRemoteNewer({418,1}, {418,1}) == false;
IsRemoteNewer({418,2}, {418,1}) == false;
```

- [ ] **Step 2: Write RED manifest tests**

Valid fixture: channel `stable`, HTTPS URL, 64-hex SHA, size >0, revision >=1, available=true. Add explicit rejection cases:

```text
missing channel
channel=beta
available=false
http:// download_url
63-character sha256
64-character non-hex sha256
size=0
revision=0
product present but not DPopCleaner
```

- [ ] **Step 3: Run RED**

```powershell
cmake --build build0418 --config Release --target UpdatePolicyTests UpdateManifestTests
ctest --test-dir build0418 -C Release -R "UpdatePolicyTests|UpdateManifestTests" --output-on-failure
```

- [ ] **Step 4: Implement policy and audited parser**

`UpdatePolicy.cpp`:

```cpp
bool IsRemoteNewer(VersionIdentity local, VersionIdentity remote) {
    if (remote.versionCode != local.versionCode) return remote.versionCode > local.versionCode;
    return remote.revision > local.revision;
}
```

Use historical `UpdateManifest.cpp` as donor, but `channel` is mandatory at parse time. `product` may be absent; if present and different from `DPopCleaner`, usability rejects it. Fail closed for every RED case.

- [ ] **Step 5: Run GREEN and commit**

```powershell
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
```

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

```cpp
bool Sha256File(const std::filesystem::path&, std::wstring& hex, std::wstring& error);
bool VerifyPackageFile(const std::filesystem::path&, const UpdateManifest&, std::wstring& error);
bool VerifyAuthenticode(const std::filesystem::path&, std::wstring& error);
```

- [ ] **Step 1: Write RED verification tests**

Temporary fixture must pass its computed SHA/size and fail for size+1, 64 zero hash, and missing file. Every failure returns non-empty error.

- [ ] **Step 2: Run RED**

```powershell
cmake --build build0418 --config Release --target PackageVerificationTests
ctest --test-dir build0418 -C Release -R PackageVerificationTests --output-on-failure
```

- [ ] **Step 3: Implement audited helpers**

Import the historical BCrypt SHA helper and WinVerifyTrust helper as isolated donor units. `VerifyPackageFile` checks regular file, exact `file_size`, SHA-256, and case-insensitive digest equality. It never launches anything.

Link hash tests to `bcrypt`; signature users later link `wintrust` and `crypt32`.

- [ ] **Step 4: Run GREEN and commit**

```powershell
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
```

```bash
git add v0418/core/Hash.* v0418/core/Signature.* v0418/tests/PackageVerificationTests.cpp v0418/CMakeLists.txt
git commit -m "feat: add 0.4.18 package verification"
```

---

### Task 5: Add bounded update client and deterministic `.part` promotion

**Files:**
- Create: `v0418/core/UpdateClient.h/.cpp`
- Create: `v0418/tests/UpdateClientContractTests.cpp`
- Modify: `v0418/CMakeLists.txt`

**Interfaces:**

```cpp
inline constexpr wchar_t kStableManifestUrl[] =
    L"https://elesnichenko1-droid.github.io/dpopcleaner-site/update/stable.json";

struct UpdateCheckResult {
    bool success{};
    bool updateAvailable{};
    UpdateManifest manifest{};
    std::wstring error;
};

UpdateCheckResult EvaluateStableManifestJson(const std::string&, VersionIdentity local);
UpdateCheckResult CheckStableUpdates(const std::atomic_bool* shutdown = nullptr);
bool DownloadVerifiedPackage(const UpdateManifest&, std::filesystem::path& package,
                             std::wstring& error, const std::atomic_bool* shutdown = nullptr);
bool PromoteVerifiedPartFile(const std::filesystem::path& part,
                             const std::filesystem::path& final,
                             const UpdateManifest&, std::wstring& error);
std::wstring BuildUpdaterArguments(const UpdateManifest&, const std::filesystem::path& package,
                                   bool allowUnsigned, const std::filesystem::path& restartExe,
                                   unsigned long parentPid);
bool StageUpdaterForHandoff(const std::filesystem::path& installedUpdater,
                            std::filesystem::path& stagedUpdater,
                            std::wstring& error);
bool LaunchUpdater(const UpdateManifest&, const std::filesystem::path& package,
                   bool allowUnsigned, const std::filesystem::path& updaterExe,
                   const std::filesystem::path& restartExe, std::wstring& error);
std::filesystem::path AppDataDirectory();
std::filesystem::path UpdatesDirectory();
```

- [ ] **Step 1: Write RED tests without live internet**

Require the exact stable URL, pure evaluation, quoted updater arguments, staged updater copy, and local `.part` behavior:

```cpp
const auto result = dpop0418::EvaluateStableManifestJson(validJson, {418,1});
if (!result.success || !result.updateAvailable) return Fail("newer stable manifest must evaluate as update");
```

For `PromoteVerifiedPartFile`:

```text
correct manifest => true; final exists; part absent
wrong size => false; part absent; final absent
wrong hash => false; part absent; final absent
```

- [ ] **Step 2: Run RED**

```powershell
cmake --build build0418 --config Release --target UpdateClientContractTests
ctest --test-dir build0418 -C Release -R UpdateClientContractTests --output-on-failure
```

- [ ] **Step 3: Implement deterministic evaluation and promotion**

```cpp
UpdateCheckResult EvaluateStableManifestJson(const std::string& json, VersionIdentity local) {
    UpdateCheckResult result{};
    if (!ParseUpdateManifestUtf8(json, result.manifest, result.error)) return result;
    if (!IsUsableStableManifest(result.manifest, result.error)) return result;
    result.success = true;
    result.updateAvailable = IsRemoteNewer(local,
        {result.manifest.versionCode, result.manifest.revision});
    return result;
}
```

`PromoteVerifiedPartFile` calls `VerifyPackageFile`; on verification or move failure it deletes the `.part` file. Only a verified part is moved with `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH`.

- [ ] **Step 4: Add bounded WinHTTP**

Use historical donor flow with:

```cpp
WinHttpSetTimeouts(session, 5000, 5000, 10000, 10000);
```

Require HTTPS before request, HTTP 2xx, `shutdown` checks between reads, refusal to read beyond manifest size, flush before verification, and final promotion through `PromoteVerifiedPartFile`.

Keep deterministic slow-worker hooks:

```text
DPOP0418_TEST_SLOW_UPDATE_MS
DPOP0418_TEST_SLOW_UPDATE_MARKER
```

When a positive fake delay begins, create/truncate the marker file named by `DPOP0418_TEST_SLOW_UPDATE_MARKER` before entering the sliced sleep loop. Sleep in <=25 ms slices and observe shutdown between slices. The marker proves the close smoke is exercising an actual in-flight update worker rather than merely closing an idle window.

- [ ] **Step 5: Run GREEN and commit**

```powershell
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
```

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
- Consumes arguments from `BuildUpdaterArguments`.
- Re-verifies expected SHA after confirmed parent exit.
- Always verifies Authenticode when manifest/handoff says signed.
- Runs installer elevated only after verification.
- Restarts installed core only after installer exit `0` or `3010`.

- [ ] **Step 1: Strengthen RED updater handoff contract**

Before adding updater source, assert source order `Sha256File` before `ShellExecuteExW`, and client order `StageUpdaterForHandoff` before `execute.lpFile = stagedUpdater.c_str()`.

Also require source text that distinguishes `WAIT_OBJECT_0` from timeout; the updater must not install after a 30-second parent wait timeout.

- [ ] **Step 2: Implement audited donor with two fail-closed corrections**

Flow:

1. parse `--parent`, `--package`, `--sha256`, optional `--args`, `--restart`, `--signed`, `--allow-unsigned`;
2. open parent with `SYNCHRONIZE`;
3. if parent handle exists, require `WaitForSingleObject(parent, 30000) == WAIT_OBJECT_0`; `WAIT_TIMEOUT` or `WAIT_FAILED` returns nonzero without launching installer;
4. if `OpenProcess` fails, proceed only when `GetLastError() == ERROR_INVALID_PARAMETER` (parent already gone); otherwise fail closed;
5. compute SHA-256 and compare case-insensitively;
6. if `signedPackage == true`, **always** call `VerifyAuthenticode`, regardless of `allowUnsigned`;
7. if `signedPackage == false`, require explicit `allowUnsigned == true`;
8. run installer via elevated `ShellExecuteExW`;
9. accept exit `0` or `3010` only;
10. restart supplied executable if present.

These two rules intentionally tighten the historical donor: timeout cannot fall through to installation, and `--allow-unsigned` can never bypass a manifest that claims the package is signed.

- [ ] **Step 3: Build/run GREEN**

```powershell
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
(Get-Item build0418/bin/Release/DPopUpdater.exe).VersionInfo.FileVersion
```

Expected FileVersion: `0.4.18.1`.

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

```cpp
struct RectI { int x{}, y{}, width{}, height{}; };
struct MainLayout {
    RectI sidebar, pageTitle, pageHint, content;
    std::array<RectI,4> actions;
    int actionRows{};
};
MainLayout ComputeMainLayout(int clientWidth, int clientHeight, int dpi);

enum class CompanionKind { DiskAnalyzer, RestoreCenter, ZapretScreenFix };
std::filesystem::path CompanionPath(const std::filesystem::path& exeDir, CompanionKind kind);
```

- [ ] **Step 1: Write RED layout tests**

Test client sizes `{1000,700}`, `{1340,740}`, `{1650,780}`, `{1880,890}` at DPI `96`, `120`, `144`. Assert positive rectangles, inside-bounds, sidebar/content separation, no action overlap, content above actions, and 2x2 action fallback when four >=150-logical-px columns do not fit.

`ComputeMainLayout` has no theme parameter so future Light/Midnight palette changes cannot alter geometry.

- [ ] **Step 2: Write RED companion tests**

Require exact paths:

```text
Modules/DiskAnalyzer.exe
Modules/RestoreCenter.exe
Modules/ZapretScreenFix.exe
```

No UI string or arbitrary relative path is accepted.

- [ ] **Step 3: Implement pure helpers**

Use a DPI-scaled sidebar clamped to 190-250 logical px, 16 logical px margins, 44 logical px action height, and 4-column layout only when every action stays >=150 logical px; otherwise use 2x2.

`CompanionPath` is enum-to-fixed-filename mapping joined beneath `<exeDir>/Modules`.

- [ ] **Step 4: Run GREEN and commit**

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
- Nav control IDs start at `1000`; action IDs start at `2000`.
- Uses `WM_APP + 41` for update completion and timer `4101` for startup update check.

- [ ] **Step 1: Add a genuinely RED close smoke**

Port the historical close smoke but require proof that the intentionally slow worker really started:

```powershell
$env:DPOP0418_TEST_SLOW_UPDATE_MS = '10000'
$env:DPOP0418_TEST_SLOW_UPDATE_MARKER = $marker
$env:DPOP0418_SETTINGS_PATH = $settings
```

Start the app and wait up to 3 seconds for `$marker` to exist. If it never appears, fail with `slow update worker did not start`; do not post `WM_CLOSE`. Only after the marker appears, start a stopwatch, post `WM_CLOSE`, and require process exit within 500 ms.

Expected RED against the skeleton: no worker marker is created.

- [ ] **Step 2: Add a genuinely RED layout smoke**

`dpop0418_layout_smoke.ps1` starts the app, requires seven nav child controls and four Settings action controls, sends Settings nav command, resizes outer window to `1024x768`, `1366x800`, `1680x840`, `1908x950`, enumerates visible controls, and records JSON evidence. Fail if a required control is absent, outside client bounds, or action rectangles overlap.

Phase A navigation is exactly:

```text
Overview
Cleaning
Disk Analyzer
Restore Center
Zapret Screen Fix
Updates
Settings
```

Native Zapret is absent until Phase B.

Expected RED against the skeleton: required nav/action IDs are absent.

- [ ] **Step 3: Implement the shell from pure layout primitives**

Use `ComputeMainLayout` in `WM_CREATE`, `WM_SIZE`, and DPI changes; no page renderer has independent coordinate math. Keep owner-drawn navigation/action controls.

Companion actions call only `CompanionPath`. Missing module yields visible warning instead of crash.

Settings actions are exactly:

```text
Автообновление: ВКЛ/ВЫКЛ
Проверить обновления сейчас
Открыть логи
Сайт проекта
```

Toggle saves through `SaveSettingsAtomic` immediately; refresh the state only after save success.

- [ ] **Step 4: Implement the non-blocking worker lifecycle**

Use:

```cpp
std::atomic_bool gShuttingDown{false};
std::atomic_bool gUpdateBusy{false};
```

`StartUpdateCheck` launches a detached worker capturing HWND by value, calls `CheckStableUpdates(&gShuttingDown)`, and reports only through a guarded `PublishPending` that rechecks shutdown and `IsWindow` before `PostMessageW`.

`WM_CLOSE`:

```cpp
gShuttingDown.store(true, std::memory_order_release);
KillTimer(hwnd, ID_STARTUP_UPDATE_TIMER);
ShowWindow(hwnd, SW_HIDE);
DestroyWindow(hwnd);
return 0;
```

No `join`, network wait, or WinHTTP call from `WM_CLOSE`/`WM_DESTROY`.

Automatic offline failure is silent; manual failure is rendered on Updates page. Optional update download starts only after explicit confirmation.

- [ ] **Step 5: Run GREEN**

```powershell
cmake --build build0418 --config Release
ctest --test-dir build0418 -C Release --output-on-failure
./tools/dpop0418_close_smoke.ps1 -Exe build0418/bin/Release/DPopCleaner.exe
./tools/dpop0418_layout_smoke.ps1 -Exe build0418/bin/Release/DPopCleaner.exe -OutputDir _release/0.4.18-dev/evidence/layout
```

Expected: worker marker observed, process exits <500 ms while 10-second fake delay is active, and all four layout sizes PASS.

- [ ] **Step 6: Add smokes to workflow and commit**

```bash
git add v0418/core/MainWindow.cpp tools/dpop0418_close_smoke.ps1 tools/dpop0418_layout_smoke.ps1 .github/workflows/DPopCleaner_0.4.18_PHASE_A.yml
git commit -m "feat: add responsive non-blocking 0.4.18 native shell"
```

---

### Task 9: Stage Phase A and build a development-only installer

**Files:**
- Create: `v0418/stage-phase-a-allowlist.txt`
- Create: `tools/dpop0418_stage_phase_a.ps1`
- Create: `release/DPopCleaner_0.4.18-dev.iss`
- Create: `tools/dpop0418_install_phase_a_smoke.ps1`
- Modify: `.github/workflows/DPopCleaner_0.4.18_PHASE_A.yml`

**Interfaces:**
- Produces `_release/0.4.18-dev/stage`.
- Produces `_release/0.4.18-dev/installer/DPopCleaner_Setup_0.4.18_DEV.exe`.
- Clean Phase A stage contains no `ThirdParty/Zapret`.

- [ ] **Step 1: Create exact allowlist**

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

The stage script rejects any allowlist difference and explicitly fails if `ThirdParty/Zapret` appears.

- [ ] **Step 2: Implement Phase A staging**

Copy native binaries from `build0418/bin/Release`, current companion binaries from `v0417/src/**/bin/Release/net48`, and current approved `Languages`, `Shell`, `Documentation`, `Resources` from `v0417/payload`. Require both native FileVersions `0.4.18.1`.

Do not copy historical 0.4.18 third-party notices or Zapret payload.

- [ ] **Step 3: Create DEV Inno installer**

Use existing DPopCleaner AppId to exercise representative in-place upgrade semantics but clearly label the artifact:

```ini
AppVersion=0.4.18
AppVerName=DPopCleaner 0.4.18 Development
VersionInfoVersion=0.4.18.1
OutputBaseFilename=DPopCleaner_Setup_0.4.18_DEV
```

Install only Phase A allowlist files. No `ThirdParty` directories, no Zapret `[InstallDelete]`, no Zapret backup code.

- [ ] **Step 4: Write installed smoke and get RED before installer/stage implementation is complete**

Require:

- installed core/updater FileVersion `0.4.18.1`;
- four companion module files;
- current Languages/Shell/Documentation;
- no `ThirdParty/Zapret` in a clean Phase A install;
- Documentation sentinel survives in-place DEV reinstall;
- installed close smoke passes and observes slow-worker marker;
- existing Disk Analyzer and Restore Center smokes pass;
- silent uninstall removes core.

- [ ] **Step 5: Build/run GREEN**

```powershell
./tools/dpop0418_stage_phase_a.ps1 -RequireCompanions
& "$env:ProgramFiles(x86)\Inno Setup 6\ISCC.exe" release/DPopCleaner_0.4.18-dev.iss
./tools/dpop0418_install_phase_a_smoke.ps1 -InstallerPath _release/0.4.18-dev/installer/DPopCleaner_Setup_0.4.18_DEV.exe
```

- [ ] **Step 6: Add stage/installer/install smoke to CI and commit**

```bash
git add v0418/stage-phase-a-allowlist.txt tools/dpop0418_stage_phase_a.ps1 release/DPopCleaner_0.4.18-dev.iss tools/dpop0418_install_phase_a_smoke.ps1 .github/workflows/DPopCleaner_0.4.18_PHASE_A.yml
git commit -m "feat: add 0.4.18 phase A development installer gate"
```

---

### Task 10: Prove rev.19 -> Phase A upgrade safety without activating stable

**Files:**
- Create: `tools/dpop0418_rev19_upgrade_smoke.ps1`
- Modify: `.github/workflows/DPopCleaner_0.4.18_PHASE_A.yml`
- Modify: `tests/test_dpop0418_phase_a_contract.py`

**Interfaces:**
- Consumes exact final rev.19 production installer.
- Performs isolated temp-directory upgrade only.

- [ ] **Step 1: Pin the rollback source**

Exact verified rev.19 reference:

```text
tag: v0.4.17-rev19
release target: 437f209b12ac4990074684e62153aa9c50b29efc
installer size: 3523734
installer SHA-256: 4818b2faa6d7deb48d9777cf9704240336201f79757d3296d66396770c6b6e61
```

The smoke refuses to run if downloaded bytes differ.

- [ ] **Step 2: Implement isolated upgrade smoke**

1. download `v0.4.17-rev19/DPopCleaner_Setup_0.4.17.exe`;
2. verify exact size/SHA above;
3. silently install to temp directory;
4. create unique sentinel under installed `Documentation` and one under `%LOCALAPPDATA%\DPopCleaner`;
5. install DEV 0.4.18 to same directory;
6. assert sentinels survive;
7. assert core/updater `0.4.18.1`;
8. run installed close smoke;
9. uninstall and clean temp/local test state.

The smoke never modifies or publishes stable metadata.

- [ ] **Step 3: Run in exact-head CI**

Add after normal installed smoke. Workflow remains `contents: read` and artifact-only.

- [ ] **Step 4: Commit**

```bash
git add tools/dpop0418_rev19_upgrade_smoke.ps1 tests/test_dpop0418_phase_a_contract.py .github/workflows/DPopCleaner_0.4.18_PHASE_A.yml
git commit -m "test: prove rev19 to 0.4.18 phase A upgrade safety"
```

---

### Task 11: Finalize the complete Phase A candidate gate and review evidence

**Files:**
- Modify: `.github/workflows/DPopCleaner_0.4.18_PHASE_A.yml`
- Modify only if a real assertion gap is found: `tests/test_dpop0418_phase_a_contract.py`

**Interfaces:**
- Produces `DPopCleaner-0.4.18-phase-a-${{ github.run_id }}`.
- No publication side effect.

- [ ] **Step 1: Make workflow order exact**

```text
1. checkout exact revision
2. Python Phase A safety contract
3. configure x64 CMake
4. build native tests + DPopCleaner + DPopUpdater
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
16. record DEV installer SHA-256 and exact size
17. upload stage/installer/evidence artifact
```

No `publish` job, `github-pages` environment, Pages write permission, or release command.

- [ ] **Step 2: Run fresh complete verification**

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

- [ ] **Step 3: Final diff safety review**

Allowed changed paths:

```text
.github/workflows/DPopCleaner_0.4.18_PHASE_A.yml
docs/superpowers/specs/2026-09-06-dpopcleaner-0.4.18-clean-resurrection-design.md
docs/superpowers/plans/2026-09-06-dpopcleaner-0.4.18-native-core-foundation.md
release/DPopCleaner_0.4.18-dev.iss
tests/test_dpop0418_phase_a_contract.py
tools/dpop0418_close_smoke.ps1
tools/dpop0418_layout_smoke.ps1
tools/dpop0418_stage_phase_a.ps1
tools/dpop0418_install_phase_a_smoke.ps1
tools/dpop0418_rev19_upgrade_smoke.ps1
v0418/**
```

Reject the PR if diff includes `version.json`, `update/stable.json`, `release-manifest.js`, site files, `.github/workflows/publish-dpopcleaner-0.4.17.yml`, or a public 0.4.18 publisher.

- [ ] **Step 4: Open/update development-only PR**

Title:

```text
DPopCleaner 0.4.18 — Phase A native core foundation
```

Body states: no publication capability; stable remains rev.19; Zapret is Plan B; DEV installer is inspection/test artifact only.

- [ ] **Step 5: Merge only after exact-head GREEN and explicit final approval**

Use exact-head merge guard. After merge, verify the artifact-only Phase A workflow may run but rev.19 publisher does not trigger from Phase A-only paths, and verify rev.19 tag/release/live stable publication remains unchanged. Do not start Phase B or C in the same merge.

---

## Plan Self-Review

### Spec coverage

- Native core source boundary: Tasks 1-8.
- App-owned settings + atomic save: Task 2.
- Update policy/manifest: Task 3.
- Size/SHA/AuthentiCode verification: Tasks 4-6.
- Independent updater: Task 6.
- Responsive native shell and companion continuity: Tasks 7-9.
- Non-blocking close under deterministic active slow update: Tasks 5 and 8.
- Development-only staging/installer: Task 9.
- rev.19 user-data migration safety: Task 10.
- No stable publication/Pages/release ownership: Tasks 1 and 11 plus Global Constraints.
- Bundled Zapret: deliberately excluded and assigned to Plan B.
- Public release activation: deliberately excluded and assigned to Plan C.

### Placeholder scan

No `TBD`, `TODO`, "implement later", or unspecified error-handling steps are present. Phase B/Phase C exclusions are deliberate scope boundaries, not unfinished Phase A requirements.

### Type/interface consistency

- `VersionIdentity`: Task 3 -> Task 5.
- `UpdateManifest`: Task 3 -> Tasks 4-6.
- `Sha256File` / `VerifyPackageFile` / `VerifyAuthenticode`: Task 4 -> Tasks 5-6.
- `UpdateClient`: Task 5 -> MainWindow Task 8.
- `LayoutModel` / `CompanionPaths`: Task 7 -> MainWindow Task 8.
- `DPopUpdater.exe`: Task 6 -> stage/install Task 9.
- Slow-update marker hook: Task 5 -> close smoke Task 8/9/10.

### Self-review corrections already incorporated

- Bootstrap workflow uploads an artifact, matching the safety contract.
- `tools/dpop0418_rev19_upgrade_smoke.ps1` is explicitly included in workflow path filters.
- Close smoke cannot false-pass on an idle skeleton: it requires a marker created by an active 10-second fake update worker before posting `WM_CLOSE`.
- Layout smoke cannot false-pass on an empty shell: it requires seven nav controls and four Settings actions before geometry checks.
- Updater does not continue after parent-wait timeout/failure.
- `signed=true` always forces Authenticode verification; `--allow-unsigned` never bypasses a signed-package claim.

## Completion Criterion

Plan A is complete only when an installed `0.4.18.1 DEVELOPMENT` candidate built from current rev.19 ancestry passes native unit tests, deterministic <500 ms close while a proven 10-second fake update worker is active, four-size responsive layout smoke, companion-module staging/launch smoke, DEV installer/reinstall smoke, isolated rev.19 -> DEV upgrade preservation, and the Phase A publication-safety contract — with no public 0.4.18 Release, stable manifest switch, Pages deployment, or rev.19 asset/tag mutation.