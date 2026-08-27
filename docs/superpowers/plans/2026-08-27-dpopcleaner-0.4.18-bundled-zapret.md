# DPopCleaner 0.4.18 Bundled Zapret Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship Flowseal Zapret 1.10.2 inside DPopCleaner 0.4.18 and manage it safely from the main application.

**Architecture:** Add a focused native `ZapretController` that owns bundled-root validation, strategy enumeration, path-scoped process/service status and elevated operations. A separate PowerShell supply-chain step downloads exactly the pinned upstream ZIP, verifies exact size/SHA-256 before extraction, applies the narrow Discord screen-share TCP-443 patch, and hands the verified tree to staging/installer. `MainWindow.cpp` remains presentation/orchestration only.

**Tech Stack:** C++20/Win32/SCM/Registry/Toolhelp, PowerShell 7, CMake/CTest, .NET Framework 4.8 companion modules, Inno Setup 6, GitHub Actions Windows 2022.

**Spec:** `docs/superpowers/specs/2026-08-27-dpopcleaner-0.4.18-bundled-zapret-design.md`

## Global Constraints

- Bundle Flowseal `zapret-discord-youtube` exactly `1.10.2`.
- ZIP exact size: `1508077` bytes.
- ZIP SHA-256: `5eaac9fb2e4b1abd693487452a3ff3f4dfe9578a45f9ddddfa4bc1f5a6bb62d5`.
- Runtime must not download arbitrary Zapret binaries.
- Bundled root is exactly `<DPopCleaner exe dir>\ThirdParty\Zapret`.
- Never terminate external `winws.exe` by filename alone; ownership is full-path based.
- Preserve upstream `LICENSE.txt` and add `Documentation/THIRD_PARTY_NOTICES.txt`.
- Keep `ZapretScreenFix.exe` as a separate compatibility/rollback tool.
- Existing instant-close and application-update tests must remain green.
- Production publication remains fail-closed until fresh Windows candidate/install smoke passes.

---

### Task 1: Persist selected Zapret strategy and define controller contracts

**Files:**
- Modify: `v0418/core/AppSettings.h`
- Modify: `v0418/core/AppSettings.cpp`
- Modify: `v0418/tests/AppSettingsTests.cpp`
- Create: `v0418/core/ZapretController.h`
- Create: `v0418/tests/ZapretControllerTests.cpp`
- Modify: `v0418/CMakeLists.txt`

**Interfaces:**
- `AppSettings::zapretStrategy` defaults to `L"general.bat"`.
- `BundledZapretRoot(exeDir)` returns `exeDir / L"ThirdParty" / L"Zapret"`.
- `ValidateBundledPayload(root, error)` verifies required pinned runtime files/directories.
- `EnumerateZapretStrategies(root)` returns only top-level non-`service*` `.bat` files.
- `IsBundledWinwsPath(candidate, root)` is a normalized exact-path ownership check.
- `FindStrategyMenuIndex(strategies, selectedName)` returns the 1-based upstream menu index or `0`.

- [ ] Write failing settings/controller native tests.
- [ ] Run Windows Foundation and confirm RED is caused by missing strategy/controller behavior.
- [ ] Implement settings persistence and pure controller helpers.
- [ ] Run CTest and confirm GREEN.
- [ ] Commit.

### Task 2: Add runtime/service controller behavior

**Files:**
- Create: `v0418/core/ZapretController.cpp`
- Modify: `v0418/core/ZapretController.h`
- Modify: `v0418/tests/ZapretControllerTests.cpp`
- Modify: `v0418/CMakeLists.txt`

**Interfaces:**
- `QueryZapretStatus(root)` reports payload, bundled standalone process, service state and registry strategy.
- `StartBundledZapret(strategy, root, error)` validates root/strategy, refuses if service is running, then elevates `cmd.exe /c` without waiting on the UI thread.
- `StopBundledZapret(root, error)` enumerates processes and terminates only the exact bundled `bin\winws.exe` image.
- `InstallBundledZapretService(...)` and `RemoveBundledZapretService(...)` are pinned to 1.10.2 and must first verify service ownership/root.

- [ ] Add failing ownership/service-operation contract tests where behavior can be isolated.
- [ ] Confirm RED.
- [ ] Implement SCM/registry/process/elevation behavior with fail-closed ownership checks.
- [ ] Confirm native tests GREEN.
- [ ] Commit.

### Task 3: Replace Zapret Screen Fix page with integrated Zapret page

**Files:**
- Modify: `v0418/core/MainWindow.cpp`
- Modify: `tests/test_dpop0418_ui_contract.py`

**UI requirements:**
- Navigation label/page becomes `Zapret`.
- Primary buttons: start/stop, strategy, install/remove service, more.
- Secondary buttons: Screen Fix, open bundled folder, refresh, back.
- Page text contains bundled Flowseal version `1.10.2`, selected strategy, payload/runtime/service status and WinDivert antivirus warning.
- Controller operations that can wait run on worker threads and post results to UI.

- [ ] Extend UI contract first and confirm RED.
- [ ] Implement rendering/actions.
- [ ] Run UI contract + native CTest + 10-second slow-update close smoke and confirm GREEN.
- [ ] Commit.

### Task 4: Build pinned verified third-party payload

**Files:**
- Create: `tools/dpop0418_prepare_zapret.ps1`
- Create: `tests/test_dpop0418_zapret_bundle_contract.py`
- Create: `v0418/third_party/THIRD_PARTY_NOTICES.txt`
- Modify: `v0418/stage-allowlist.txt`
- Modify: `tools/dpop0418_stage.ps1`
- Modify: `.github/workflows/DPopCleaner_0.4.18_FOUNDATION.yml`

**Supply-chain flow:**
1. Download only the exact 1.10.2 ZIP URL.
2. Verify byte length `1508077`.
3. Verify SHA-256 `5eaac9fb2e4b1abd693487452a3ff3f4dfe9578a45f9ddddfa4bc1f5a6bb62d5`.
4. Only then `Expand-Archive`.
5. Verify `.service/version.txt == 1.10.2`, `LICENSE.txt`, `service.bat`, `general.bat`, `bin/winws.exe`, `bin/WinDivert.dll`, `bin/WinDivert64.sys`, and `lists/`.
6. Patch only a `discord.media` TCP filter missing port 443; verify idempotency.
7. Stage the result as `ThirdParty/Zapret/` and copy third-party notices.

- [ ] Write Python contract first and confirm RED because prepare script/allowlist are absent.
- [ ] Implement prepare/stage/workflow changes.
- [ ] Run contract and Windows staging; confirm GREEN.
- [ ] Commit.

### Task 5: Installer integration and preservation of user Zapret lists

**Files:**
- Modify: `release/DPopCleaner_0.4.18.iss`
- Modify: `tools/dpop0418_install_smoke.ps1`
- Modify: `tests/test_dpop0418_package_contract.py`

**Behavior:**
- Install complete verified tree to `{app}\ThirdParty\Zapret`.
- Before in-place replacement, back up existing `lists\*-user.txt` to `%LOCALAPPDATA%\DPopCleaner\ZapretBackup\<timestamp>`.
- Restore same-named user list files after installing new program-owned tree.
- Installed smoke must assert license/version/service/general/winws/WinDivert files.
- Reinstall smoke writes a sentinel into `list-general-user.txt` and proves it survives.

- [ ] Extend package/install contracts and confirm RED.
- [ ] Implement Inno backup/restore + file inclusion.
- [ ] Build installer and run silent install/reinstall smoke.
- [ ] Confirm GREEN.
- [ ] Commit.

### Task 6: Full 0.4.18 bundled-Zapret candidate gate

**Files:**
- Modify as needed only to resolve evidence-backed failures in Tasks 1-5.

- [ ] Run Foundation end-to-end on Windows 2022.
- [ ] Require native CTest, UI contract, close smoke, companion tests, pinned Zapret verification, exact stage, staged smokes, Inno build, installed/reinstall smoke, uninstall and artifact upload all GREEN.
- [ ] Inspect failures with systematic-debugging before changing implementation.
- [ ] Commit only evidence-backed fixes.

### Task 7: Complete stable 0.4.18 release/site contract and publish only after candidate GREEN

**Files:**
- Create: `.github/workflows/publish-dpopcleaner-0.4.18.yml`
- Create: `release/RELEASE_NOTES_0.4.18.md`
- Modify: `version.json`
- Modify: `update/stable.json`
- Modify: `release-manifest.js`
- Modify: `tests/release-manifest.test.cjs`
- Modify: `index.html`
- Modify: `tests/test_dpop0418_release_contract.py` only if bundled-Zapret release assertions need expansion.

**Publication requirements:**
- Repository `update/stable.json` stays fail-closed until production job injects actual installer size/SHA.
- Production workflow independently repeats pinned Zapret download/size/SHA verification and fresh Windows build/install smoke.
- Publish tag `v0.4.18`, asset `DPopCleaner_Setup_0.4.18.exe`, deploy Pages, then HTTP-download the live installer and verify SHA against live stable manifest.
- Site must describe bundled Flowseal Zapret 1.10.2 and no longer describe Zapret as merely an external Screen Fix.

- [ ] Confirm release contract RED before publisher/site implementation.
- [ ] Implement exact 0.4.18 rev.1 stable contract.
- [ ] Confirm all static/node contracts GREEN.
- [ ] Open PR, review checks, merge only after candidate GREEN.
- [ ] Verify production release/Pages/live SHA before declaring 0.4.18 released.
