# DPopCleaner 0.4.18 — Clean Resurrection Design

## Status

Approved direction from the user on 2026-09-06.

This specification replaces the idea of reviving the historical 0.4.18 branch wholesale. The historical 0.4.18 implementation remains a donor/reference source only. New 0.4.18 development starts from the currently verified 0.4.17 rev.19 `main` line and preserves that stable line intact until a separate, explicitly approved release-activation stage.

## 1. Product goal

DPopCleaner 0.4.18 becomes the next native-core generation of DPopCleaner while preserving the behavior, layout expectations, Zapret compatibility, companion modules and release safety proven in 0.4.17 rev.19.

0.4.18 must deliver a new application-owned native core rather than continuing to depend on the frozen 0.2.14 executable as the primary runtime. The migration must be incremental and testable, not a wholesale restoration of the old 0.3.x or historical 0.4.18 product state.

## 2. Stable baseline and branch policy

The source baseline is the verified rev.19 main line:

- baseline commit: `437f209b12ac4990074684e62153aa9c50b29efc`;
- stable product: `DPopCleaner 0.4.17 rev.19`;
- frozen reference core blob: `efd0eff1f4962319282363fa85595c25e0cebe11`;
- bundled/pinned Flowseal line: `1.10.2`;
- current stable release owner: `.github/workflows/publish-dpopcleaner-0.4.17.yml`.

New implementation work must occur on fresh branches created from current `main`. The historical branch `feat/dpopcleaner-0.4.18-core-update` must never be merged, rebased, or force-updated onto main as a whole.

Historical 0.4.18 commits/files may be copied only as audited donors. Each donor component must be introduced by a new change on top of current main, with current tests and current release-safety contracts preserved.

## 3. Non-negotiable protection of 0.4.17 rev.19

Until the final release-activation stage:

- do not delete or replace `.github/workflows/publish-dpopcleaner-0.4.17.yml`;
- do not retarget `update/stable.json` to 0.4.18;
- do not make repository `version.json` advertise 0.4.18 stable;
- do not create/update public tag `v0.4.18`;
- do not publish a 0.4.18 GitHub Release;
- do not deploy 0.4.18 through GitHub Pages;
- do not overwrite `v0.4.17-rev19` assets;
- do not move `v0.4.17-rev19` away from the verified rev.19 provenance chain.

Development CI for 0.4.18 is build/test/artifact-only. Publication remains disabled until a separate release-activation design and PR are approved.

## 4. Architectural strategy

0.4.18 is resurrected in three independent subprojects. Each subproject must produce testable software without requiring the next one.

### Phase A — Native core foundation

Build the new 0.4.18 native executable and updater without changing stable publication.

Responsibilities:

- `AppSettings` — application-owned per-user settings;
- `UpdatePolicy` — pure version/revision decision logic;
- `UpdateManifest` — stable manifest parsing and validation;
- `UpdateClient` — bounded network/download/verification behavior;
- `DPopUpdater` — independent verified installer handoff;
- main Win32 window/message loop;
- non-blocking shutdown behavior;
- native Settings page for update controls;
- compatibility launch points for existing companion modules.

The first Phase A milestone is a development candidate that can build, run, close correctly, persist update settings, manually check a supplied/test manifest and pass installed smoke. It must not publish stable metadata.

### Phase B — Bundled Zapret integration

After Phase A is stable, integrate the pinned Flowseal Zapret 1.10.2 into the new native core.

Responsibilities:

- bundled payload under `ThirdParty/Zapret/`;
- exact upstream size/SHA verification before extraction;
- `ZapretController` isolated from UI rendering;
- strategy enumeration;
- path-scoped start/stop;
- service install/remove;
- bundled status/version reporting;
- preservation of all 22 supported strategies;
- Discord/screen-share compatibility behavior;
- existing `ZapretScreenFix.exe` remains available as a companion/diagnostic tool.

Phase B must reproduce the functional guarantees already proven by rev.19 before release activation.

### Phase C — Release activation

Only after Phase A and B are independently GREEN:

- introduce a new fail-closed 0.4.18 release workflow;
- create/update `v0.4.18` only from a verified candidate;
- generate real stable manifest SHA/size from the built installer;
- deploy Pages only after all installed gates pass;
- re-download the live installer and verify SHA/size;
- preserve historical rev.19 release/tag/assets unchanged.

Phase C is intentionally excluded from the first implementation plan.

## 5. Source layout

New product-owned code lives under `v0418/`.

```text
v0418/
├─ CMakeLists.txt
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
├─ zapret/
│  ├─ ZapretController.h/.cpp
│  └─ ...version-specific helpers only when required
├─ tests/
│  ├─ AppSettingsTests.cpp
│  ├─ UpdatePolicyTests.cpp
│  ├─ UpdateManifestTests.cpp
│  ├─ UpdateClientContractTests.cpp
│  └─ later ZapretController tests
└─ resources/
   └─ version.rc.in
```

Existing 0.4.17 companion modules are not copied into this directory. They remain separate build artifacts consumed by staging/installer tooling.

No new 0.4.18 code may accidentally compile the old reconstructed root `MainWindow.cpp` as the native 0.4.18 UI.

## 6. Donor-code policy

The historical 0.4.18 branch already contains useful implementations of:

- `AppSettings`;
- hashing/signature helpers;
- update manifest/policy/client;
- updater handoff;
- `ZapretController`;
- installer/staging tests.

These files are examples, not authoritative current code.

For every donor unit:

1. copy only the unit needed for the current task;
2. compare its assumptions with current rev.19 layout and release behavior;
3. remove obsolete workflow/site/version assumptions;
4. write or restore a RED test on the new branch before accepting implementation behavior;
5. make the minimum implementation pass;
6. run current rev.19 protection contracts where applicable.

No historical `.github/workflows/publish-dpopcleaner-0.4.18.yml`, old Pages hotfix workflow or obsolete `stable.json` content is to be copied during Phase A/B.

## 7. Native UI continuity

0.4.18 is a new native executable, but its product identity must feel like the stabilized DPopCleaner line rather than a return to the abandoned 0.3.x presentation.

The native UI must preserve these rev.19 user expectations:

- compact, clean controls without native/ghost border mismatches;
- Light and Midnight use identical geometry;
- no clipped buttons at 1024×768, 1366×800, 1680×840 or 1908×950;
- wide screens actually use the available space;
- Settings remains an in-application page;
- Journal behavior remains consistent with the accepted product behavior;
- Zapret page in Phase B must retain clear status + strategy + service actions;
- one canonical tray identity when tray functionality is migrated.

Phase A does not need to clone every rev.19 page immediately, but it must define compatible navigation/layout primitives so later pages do not require another wholesale UI rewrite.

## 8. Phase A functional requirements

### 8.1 Non-blocking close

Closing DPopCleaner must not wait for a network/update worker.

Acceptance:

- main window disappears within 500 ms in a deterministic slow-update test;
- no blocking `join()` or network wait on the UI thread;
- no update-result callback may touch a destroyed window;
- no new update check starts once shutdown begins.

### 8.2 Settings

Settings file:

`%LOCALAPPDATA%\DPopCleaner\settings.ini`

Initial key:

```ini
[updates]
auto_check=1
```

Rules:

- missing/malformed value defaults to enabled;
- `0` disables startup automatic checking;
- manual check remains available;
- writes use temp-file + replace behavior;
- save failure is reported without crashing.

### 8.3 Update manifest and policy

0.4.18 local identity:

- version `0.4.18`;
- version code `418`;
- development revision starts at `1`.

Remote is newer only when:

- remote version code > 418; or
- remote version code == 418 and remote revision > local revision.

A usable manifest must have a stable channel, HTTPS download URL, valid 64-character SHA-256, nonzero size and `available=true` for install purposes.

### 8.4 Download verification and updater

Before installer handoff:

- require 2xx response;
- require exact byte count;
- require SHA-256 match;
- require Authenticode if manifest says signed;
- delete rejected `.part` files;
- do not launch installer on verification failure.

`DPopUpdater.exe` independently re-verifies the package after the parent exits and before installer execution.

## 9. Companion continuity

Phase A development staging keeps the current tested companions:

- `Modules/DiskAnalyzer.exe`;
- `Modules/RestoreCenter.exe`;
- `Modules/ZapretScreenFix.exe`;
- required shared DLL/resources.

0.4.18 must not regress or remove user backups/history produced by the existing product line.

Companion integration is validated by launch/smoke contracts; Phase A does not rewrite companion internals.

## 10. Phase B Zapret invariants

The following current product guarantees are carried forward:

- Flowseal release exactly `1.10.2` unless a separately reviewed version bump is approved;
- pinned upstream ZIP size `1508077` bytes;
- pinned SHA-256 `5eaac9fb2e4b1abd693487452a3ff3f4dfe9578a45f9ddddfa4bc1f5a6bb62d5`;
- 22 strategies preserved;
- no global kill of unrelated `winws.exe` processes;
- standalone ownership matched by full executable path beneath bundled root;
- service management only targets the bundled/pinned integration;
- start/install operations are non-blocking from the UI thread;
- the existing screen-share compatibility semantics are retained;
- damaged/missing WinDivert/Zapret files produce a visible error instead of silent redownload.

## 11. Development CI design

0.4.18 Phase A/B CI is intentionally separate from stable publishing.

Allowed automatic actions:

- configure/build native x64 Release;
- CTest/unit tests;
- deterministic close smoke;
- stage development payload;
- build development installer/candidate artifact;
- silent install/reinstall smoke;
- UI/companion/Zapret runtime smoke as each subsystem arrives;
- artifact upload for inspection.

Forbidden before Phase C approval:

- `gh release create/upload` for 0.4.18;
- `deploy-pages`;
- writes to stable release metadata;
- moving public stable tags;
- overwriting rev.19 assets;
- committing generated stable metadata back to main.

Development workflows should use `contents: read` unless artifact/check behavior requires another permission. They must not request Pages write permissions.

## 12. Testing strategy

All implementation proceeds RED → GREEN.

### Phase A minimum gates

1. settings default enabled;
2. persisted disabled value reloads;
3. malformed setting safely defaults;
4. atomic save contract;
5. version-code comparison cases;
6. revision comparison cases;
7. manifest HTTPS/SHA/size validation;
8. wrong-size download rejected/deleted;
9. wrong-SHA download rejected/deleted;
10. updater re-verifies before installer launch;
11. auto-check skipped when disabled;
12. manual check still available when disabled;
13. deterministic slow-worker close under 500 ms;
14. no result dispatch after shutdown begins;
15. native build x64 Release;
16. installed smoke verifies `DPopCleaner.exe` and `DPopUpdater.exe`;
17. Settings page exposes auto-update + manual-check actions;
18. companion module launch paths resolve correctly;
19. no 0.4.18 publication workflow/action is enabled;
20. rev.19 stable workflow/tag/release metadata remains untouched by the development branch.

### Phase B additional gates

1. pinned Flowseal ZIP exact size + SHA;
2. staged/installed required Zapret files;
3. 22 strategies;
4. path-scoped `winws.exe` ownership;
5. strategy persistence;
6. service install/remove lifecycle;
7. Light/Midnight layout parity;
8. 1024/1366/1680/1908 responsive checks;
9. canonical tray identity when tray is migrated;
10. screen-share compatibility;
11. existing rev.19 Zapret behavior has an explicit equivalent gate before release activation.

## 13. Error handling

The native core uses fail-closed behavior for update and bundled third-party operations.

- network unavailable during automatic check: no crash, no blocking close, no forced dialog;
- manual check failure: visible actionable message;
- malformed manifest: reject update;
- failed package verification: delete partial/rejected file and keep current install untouched;
- updater cannot verify: do not start installer;
- elevation denied: report cancellation and preserve state;
- missing companion: page/action reports missing component rather than crashing;
- missing/damaged Zapret payload in Phase B: disable unsafe operations and report exact missing requirement;
- any development CI failure: no stable publication exists to continue.

## 14. Migration and rollback model

0.4.17 rev.19 remains the production rollback point throughout Phase A/B.

Development 0.4.18 installer artifacts may be produced for CI inspection, but they are not advertised by stable manifest or Pages.

Before Phase C, an explicit upgrade/reinstall smoke must prove that installing the development candidate over a rev.19 installation preserves user data expected to survive an upgrade. Release activation will additionally require a rollback/recovery policy review.

## 15. Explicit exclusions from Phase A

- no public 0.4.18 release;
- no stable manifest switch;
- no Pages deployment;
- no deletion of rev.19 workflow;
- no wholesale merge/rebase of historical 0.4.18 branch;
- no redesign of Disk Analyzer/Restore Center/ZapretScreenFix;
- no automatic Zapret strategy probing;
- no independent Zapret self-update;
- no silent installation of optional DPopCleaner updates;
- no claim that the 0.4.18 executable is byte-identical to 0.2.14.

## 16. Implementation decomposition

This architecture is intentionally implemented with separate plans:

1. **Plan A — Native core foundation**: `AppSettings`, update policy/manifest/client, updater, non-blocking close, native Settings UI, build/install development CI.
2. **Plan B — Bundled Zapret parity**: audited `ZapretController`, pinned payload, service/strategy/runtime/UI parity gates.
3. **Plan C — Release activation**: fail-closed 0.4.18 publisher, stable manifest, GitHub Release, Pages and live provenance verification.

Only Plan A is eligible to start after this design is reviewed and approved.

## Acceptance summary

The clean resurrection approach is accepted when 0.4.18 development starts from the verified rev.19 main line, imports historical code only as audited donor units, produces a separately testable native core without touching stable publication, preserves current companion and UI expectations, and postpones all public release ownership until a later explicitly approved release-activation stage.
