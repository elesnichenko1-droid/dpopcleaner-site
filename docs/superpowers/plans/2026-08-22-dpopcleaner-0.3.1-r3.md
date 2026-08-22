# DPopCleaner 0.3.1 BETA R3 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax so execution can resume without repeating completed work.

**Goal:** Build, verify, and publish DPopCleaner `0.3.1 BETA R3` as a complete Windows x64 installer with the native updater, pinned official Zapret `1.10.1`, correct icon/UAC behavior, single-instance and safe-close guarantees, and an honest website backed by the final release bytes.

**Architecture:** Keep the repository's tracked flat C++ sources as the source of truth and use a tracked PowerShell preparation script to assemble the CMake `src/` and `resources/` layout. Put lifecycle/update decisions and bundled-path validation in small independently testable C++ modules. Use a fail-closed release workflow: the existing public `0.2.14` download stays active until the R3 installer, metadata, Defender checks, and live download checks pass.

**Tech Stack:** C++20, Win32, WinHTTP, Windows Service Control Manager, CMake/MSVC, Inno Setup, PowerShell/Pester-compatible scripts, Node.js tests, GitHub Actions, GitHub Releases, GitHub Pages

**Spec:** `docs/superpowers/specs/2026-08-22-dpopcleaner-0.3.1-r3-design.md`

## Global Constraints

- [ ] Never disable Defender, add exclusions, obfuscate payloads, or change code to evade detection.
- [ ] Never replace the existing `v0.3.1-beta` or `v0.2.14-clean-r1` release assets.
- [ ] Keep the live site on its last verified manifest until the exact R3 asset exists and passes all release gates.
- [ ] Use only the pinned Zapret `1.10.1` archive and license hashes from the approved specification.
- [ ] Do not add a Python or PowerShell runtime downloader to DPopCleaner.
- [ ] Do not install/start/remove the Zapret service automatically.
- [ ] Make each behavior change test-first: observe the new test fail, implement the minimum change, then observe the test pass.
- [ ] Commit after every completed task with `git diff --check`, the task's focused tests, and a clean review of the staged diff.

## Task 1: Establish a reproducible tracked build layout and failing release-policy tests

**Files:**

- Create: `scripts/Prepare-R3Source.ps1`
- Create: `scripts/R3ReleasePolicy.psm1`
- Create: `tests/R3ReleasePolicy.Tests.ps1`
- Modify: `SystemInfo.cpp`
- Modify: `CMakeLists.txt`
- Modify: `Version.h`

**Steps:**

- [ ] Write `tests/R3ReleasePolicy.Tests.ps1` tests for display version `0.3.1 BETA R3`, version code `3013`, revision `3`, expected tag/asset, required tracked source mappings, and rejection of a missing or unexpected source file.
- [ ] Run `pwsh -NoProfile -File tests/R3ReleasePolicy.Tests.ps1` and confirm it fails because the policy module/preparation script does not yet satisfy the contract.
- [ ] Implement `R3ReleasePolicy.psm1` with exported functions that return the immutable release identity, pinned Zapret inputs, exact source map, and validation results. Functions must accept paths/objects as parameters so tests execute behavior rather than search source text.
- [ ] Implement `Prepare-R3Source.ps1` to create a destination tree, copy the tracked C++/header/resource/CMake files according to the policy map, reject missing inputs and unexpected destination collisions, and emit a machine-readable inventory with SHA-256 values.
- [ ] Fix the missing namespace-closing brace in `SystemInfo.cpp` and set all version constants in `Version.h` to the R3 identity.
- [ ] Update `CMakeLists.txt` to include all new modules and test executables added by later tasks while keeping configuration valid before those test files are created through guarded `if(BUILD_TESTING)` blocks.
- [ ] Run the PowerShell policy test and a preparation dry run into `work/r3-source-smoke`; confirm the inventory contains every expected file and no generated C++ source.
- [ ] Run `git diff --check`, review the diff, and commit as `build: establish reproducible R3 source layout`.

## Task 2: Make update and close behavior explicit and testable

**Files:**

- Create: `UpdatePolicy.h`
- Create: `UpdatePolicy.cpp`
- Create: `tests/UpdatePolicyTests.cpp`
- Modify: `UpdateManifest.h`
- Modify: `UpdateManifest.cpp`
- Modify: `UpdateClient.h`
- Modify: `UpdateClient.cpp`
- Modify: `MainWindow.h`
- Modify: `MainWindow.cpp`
- Modify: `UpdaterMain.cpp`
- Modify: `CMakeLists.txt`

**Steps:**

- [ ] Add failing C++ tests proving that unavailable/equal/lower version manifests are not offered, background results cannot enter the install flow, only an explicit interactive result can enter it, and updater arguments never contain `--restart`.
- [ ] Configure the prepared tree with `cmake -S work/r3-source-smoke -B work/r3-build-smoke -DBUILD_TESTING=ON`, build `UpdatePolicyTests`, run it with `ctest`, and confirm the new tests fail before production behavior is changed.
- [ ] Add `available` and `size` parsing/validation to `UpdateManifest`, and require HTTPS, an exact 64-hex SHA-256, and a positive package size before an update can be installable.
- [ ] Implement `UpdatePolicy` as pure decision functions for offer eligibility, prompt/install eligibility, and updater argument construction without restart.
- [ ] Change the startup/background result handler so it only stores/displays availability. It must never prompt, download, close the main window, or start a process.
- [ ] Add a shutdown flag, cancel the startup timer during close, discard late result messages safely, and make `WM_DESTROY` post `WM_QUIT` without launching another executable.
- [ ] Remove `--restart` parsing and all post-install relaunch code from `UpdaterMain.cpp`. Launch the updater only from the explicit interactive path after size/hash/signature checks.
- [ ] Reprepare the tree, rebuild, and run `ctest --test-dir work/r3-build-smoke --output-on-failure`; confirm every update-policy test passes.
- [ ] Run `git diff --check`, review the lifecycle diff against the spec, and commit as `fix: make updates interactive and closing final`.

## Task 3: Enforce one instance, administrator manifest, and DPopCleaner icon

**Files:**

- Create: `SingleInstance.h`
- Create: `SingleInstance.cpp`
- Create: `tests/SingleInstanceTests.cpp`
- Modify: `main.cpp`
- Modify: `MainWindow.cpp`
- Modify: `app.manifest`
- Modify: `app.rc`
- Modify: `updater.rc`
- Modify: `CMakeLists.txt`
- Modify: `DPopCleaner.iss`

**Steps:**

- [ ] Add a failing Windows test that creates the named mutex twice and proves the second guard is not primary without starting a second application loop.
- [ ] Implement an RAII named-mutex guard scoped to the current session and an activation helper that finds the fixed R3 window class, restores the existing window, and brings it to the foreground.
- [ ] Integrate the guard before main-window creation; a secondary invocation activates the primary instance and exits before timers or update workers start.
- [ ] Set the main executable's embedded execution level to `requireAdministrator` through the linker manifest option while keeping the updater `asInvoker`; keep `app.manifest` aligned as a checked-in declaration and avoid duplicate manifest resources.
- [ ] Compile `dpopcleaner.ico` as resource `101` into both executables, load it for `hIcon` and `hIconSm`, and configure Inno Setup/shortcuts to use the same icon.
- [ ] Build and run `SingleInstanceTests`, then inspect both executables with Windows resource tools: DPopCleaner must report `requireAdministrator`, DPopUpdater must report `asInvoker`, and both must expose icon resource `101`.
- [ ] Extract the associated icon to PNG and compare its SHA-256 against the known generic Windows application icon sample; fail if equal.
- [ ] Run `git diff --check`, review, and commit as `fix: enforce UAC single instance and application icon`.

## Task 4: Add safe bundled Zapret Center behavior

**Files:**

- Create: `ZapretPolicy.h`
- Create: `ZapretPolicy.cpp`
- Create: `tests/ZapretPolicyTests.cpp`
- Modify: `ZapretManager.h`
- Modify: `ZapretManager.cpp`
- Modify: `MainWindow.h`
- Modify: `MainWindow.cpp`
- Modify: `CMakeLists.txt`

**Steps:**

- [ ] Add failing tests for canonical `{exe-dir}\\zapret` resolution, required-file validation, rejection of path traversal/outside paths, and bundled `winws.exe` ownership checks.
- [ ] Implement pure `ZapretPolicy` path and required-tree validation for `general.bat`, `service.bat`, `bin\\winws.exe`, `bin\\WinDivert.dll`, `bin\\WinDivert64.sys`, `bin\\cygwin1.dll`, `utils\\check_updates.enabled`, and `LICENSE.txt`.
- [ ] Change status detection so a standalone process is reported as bundled only when its executable path canonicalizes under the installed Zapret directory. Do not terminate or modify unrelated services/processes.
- [ ] Add explicit-confirmation actions to refresh status, run the exact bundled `general.bat`, open the exact bundled `service.bat`, and open the exact bundled folder. Run batch files visibly through the system command processor with their working directory set to the bundle.
- [ ] Present validation failures in the UI with the exact missing file and no repair/download side effect.
- [ ] Reprepare, rebuild, run all C++ tests, and manually exercise the four buttons against a test bundle copy without installing a service.
- [ ] Run `git diff --check`, review, and commit as `feat: integrate verified bundled Zapret center`.

## Task 5: Build the complete installer and validate the pinned payload

**Files:**

- Create: `release/DPopCleaner_0.3.1_R3.iss`
- Create: `scripts/Stage-R3Payload.ps1`
- Create: `tests/fixtures/zapret-minimal/` test fixture tree
- Modify: `scripts/R3ReleasePolicy.psm1`
- Modify: `tests/R3ReleasePolicy.Tests.ps1`

**Steps:**

- [ ] Add failing policy tests for archive/license SHA mismatch, missing required Zapret files, an unexpected top-level installer component, and an installer source outside the staging root.
- [ ] Implement `Stage-R3Payload.ps1` to verify the exact archive/license hashes, expand the single expected upstream root, validate the required tree, add the pinned `LICENSE.txt`, and stage exactly `DPopCleaner.exe`, `DPopUpdater.exe`, and `zapret/`.
- [ ] Create an Inno definition with application version `0.3.1`, display name `0.3.1 BETA R3`, `PrivilegesRequired=admin`, correct icon/shortcuts, the two exact binaries, and a recursive verified Zapret source. It must not execute any Zapret script/service during install or uninstall.
- [ ] Extend the policy validator to parse the staged payload inventory and fail unless the exact top-level contract is met.
- [ ] Run the negative fixture tests, then stage the downloaded official `1.10.1` archive and license and confirm their pinned hashes and all required files pass.
- [ ] Build a local unsigned installer with Inno Setup and inspect its embedded file list/version/icon.
- [ ] Run `git diff --check`, review licensing and package contents, and commit as `build: package complete verified R3 installer`.

## Task 6: Add fail-closed R3 metadata and website presentation

**Files:**

- Create: `tests/release-manifest.test.mjs`
- Create: `assets/dpopcleaner-0.3.1-r3.png` only after capture from the final binary
- Modify: `release-manifest.js`
- Modify: `version.json`
- Modify: `update/beta.json`
- Modify: `index.html`
- Modify: `styles.css`
- Modify: `script.js`
- Modify: `scripts/Stage-Site.ps1`

**Steps:**

- [ ] Add failing Node tests for wrong tag, asset, version code, revision, URL, size, hash, and `available=false`; the public download resolver must retain the verified prior release in every R3 failure case.
- [ ] Implement a single manifest validator shared by site initialization tests, with the exact R3 identity and fail-closed conditions from the design.
- [ ] Update text for `0.3.1 BETA R3`, Zapret `1.10.1`, native update checking, UAC, and manual service activation, while keeping the previous verified download active until generated R3 metadata is committed.
- [ ] Replace the handcrafted application mockup with an image element reserved for `assets/dpopcleaner-0.3.1-r3.png`; do not add a substitute/rendered illustration.
- [ ] Extend `Stage-Site.ps1` allowlisting to include only the final screenshot and required R3 metadata.
- [ ] Run Node tests and the site-staging test; verify the staged site contains no executable or source files.
- [ ] Run `git diff --check`, review responsive layout locally, and commit as `site: prepare fail-closed R3 presentation`.

## Task 7: Create the gated GitHub build/release workflow

**Files:**

- Create: `.github/workflows/build-dpopcleaner-0.3.1-r3.yml`
- Modify: `.github/workflows/static.yml`
- Modify: `tests/R3ReleasePolicy.Tests.ps1`

**Steps:**

- [ ] Add failing workflow-policy tests for job/gate order, exact pinned URLs/hashes, explicit test execution, resource validation, optional signing only through named secrets, exact tag/asset, release evidence, manifest generation from final bytes, and Pages deployment only after release/live verification.
- [ ] Implement the Windows workflow using tracked scripts and sources. YAML may orchestrate commands but must not generate or overwrite C++/resource source text.
- [ ] Gate release upload on policy tests, MSVC build/CTest, manifest/icon/file-version inspection, payload validation, Inno build, optional Authenticode signing, installer hash/size generation, Defender custom scans of installer and expanded payload, and a test that closing the final executable creates no DPopCleaner/DPopUpdater/installer process.
- [ ] Upload logs, prepared source inventory, payload inventory, resource reports, Defender results, and manifest as build evidence.
- [ ] Create prerelease `v0.3.1-beta-r3` idempotently, upload only `DPopCleaner_Setup_0.3.1_BETA_R3.exe` and evidence, then verify the asset via a fresh HTTPS download before generating public metadata.
- [ ] Make Pages consume only the verified generated metadata/site artifact; preserve the currently live release if any upstream gate fails.
- [ ] Run workflow-policy tests, parse the YAML, run all local tests, and commit as `ci: gate and publish DPopCleaner 0.3.1 R3`.

## Task 8: Execute release, capture the real screenshot, and switch the live site

**Files:**

- Create: `scripts/Capture-AppScreenshot.ps1`
- Create: `assets/dpopcleaner-0.3.1-r3.png` from the final R3 executable window
- Modify: `release-manifest.js`
- Modify: `version.json`
- Modify: `update/beta.json`

**Steps:**

- [ ] Run the complete local verification suite and perform a structured review of every branch diff against the approved specification.
- [ ] Fetch `origin/main`, verify it still descends from recorded base `b37287d006f8fbf9deec2816acc639cdd90c106f`, then push the reviewed feature branch and fast-forward main only if there is no unexpected remote change.
- [ ] Monitor the R3 Actions workflow. For every failure, download its evidence, reproduce locally, add a regression test first, fix, recommit, and rerun until all release gates pass.
- [ ] Download the exact release asset, independently verify SHA-256/size, and run local Defender custom scans on the installer and extracted/installed payload. Do not expose R3 on the site if either scan reports a threat.
- [ ] Install/test with UAC, verify the icon and full Zapret tree, confirm native update files, confirm the four Zapret actions target the bundled folder, and verify normal close creates no DPopCleaner/DPopUpdater/installer process.
- [ ] Launch the exact final `DPopCleaner.exe`, capture its window with `Capture-AppScreenshot.ps1`, verify the screenshot visually, and record the executable SHA-256 alongside capture metadata.
- [ ] Commit the real screenshot plus final generated R3 manifest values from the release bytes; rerun site tests and deploy Pages.
- [ ] Verify live HTTP 200 responses for the site, screenshot, `version.json`, `update/beta.json`, and installer URL; download again and match live SHA-256/size.
- [ ] Run `git diff --check`, confirm the worktree is clean and `origin/main` contains the release/site commits, and record final release URL, installer hash/size, Actions run, Pages run, Defender result, and close-test result for handoff.

## Final Acceptance Checklist

- [ ] Site download label and metadata say `0.3.1 BETA R3` and resolve to tag `v0.3.1-beta-r3`.
- [ ] Installer requests elevation, installs both application binaries plus complete verified Zapret `1.10.1`, and starts no service automatically.
- [ ] DPopCleaner requests elevation, uses the DPopCleaner icon, enforces a single instance, and closes without relaunch.
- [ ] Background update checks cannot enter installation; explicit user action is required and the updater never restarts the app.
- [ ] Live installer bytes match the published SHA-256 and size.
- [ ] Defender reports no detections for the final downloaded installer and expanded payload.
- [ ] Website screenshot is captured from the exact final binary and the mockup/caption are gone.
- [ ] All automated tests, GitHub Actions release gates, Pages deployment, and live verification pass.
