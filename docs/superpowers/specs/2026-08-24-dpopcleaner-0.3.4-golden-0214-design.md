# DPopCleaner 0.3.4 BETA R1 — Golden 0.2.14 UX Design

## Goal
DPopCleaner 0.3.4 BETA R1 uses the historical 0.2.14 experience as the UX reference and the verified 0.3.3 code/release pipeline as the technical base.

**0.2.14 = visual/interaction reference. 0.3.3 = maintained core and verified release base. 0.3.4 = the union of both.**

## Reference material and limits
The user supplied `DPopCleaner_Setup_0.2.14_BETA_CLEAN_R1(1).exe` as the golden historical package. Verified SHA-256:

`dac34a1f1697dbc9f7f5a953ade9e1e4f09b39a04d66fb18e404ba511543900e`

The repository clean Inno Setup definition confirms that the package installs one historical standalone `DPopCleaner.exe`. It is therefore a behavioral/visual reference, not recoverable C++ source. Exact old implementation details that are not supported by the binary, historical screenshots, reconstruction notes or existing code must not be invented.

The previous UI recovery spec remains a requirements source for old-series UX: compact horizontal navigation, dedicated pages, dense information, status/session log, and the old functional categories.

## Product identity
- Display version: `0.3.4 BETA R1`
- Version: `0.3.4`
- Version code: `3041`
- Revision: `1`
- Windows resource version: `0.3.4.1`
- Tag: `v0.3.4-beta-r1`
- Installer: `DPopCleaner_Setup_0.3.4_BETA_R1.exe`

## Non-negotiable UX requirements
1. Keep the old-series compact horizontal navigation and gear-based Settings.
2. No page may use overlapping fixed rectangles. Every page uses a shared vertical layout contract: heading, description/status, content, actions, with positive gaps.
3. The application remains usable at 1100×700 and targets 1200×850. Maximized layouts expand content instead of adding decorative empty space.
4. Layout is DPI-aware; acceptance covers 100%, 125% and 150% scale where CI permits it.
5. Text never overlaps cards, list views, graphs, buttons, status bars or the session log.
6. Dedicated screens are not replaced with generic text panels to simplify implementation.
7. Destructive or system-changing actions require explicit user intent and confirmation where appropriate.
8. Long operations remain asynchronous/cancellable and do not freeze the UI.

## Shell and page structure
Primary tabs remain:
1. Обзор
2. Очистка
3. ОЗУ
4. DPopGuard
5. Диск
6. Приложения
7. Windows
8. Дубликаты
9. Инструменты
10. Zapret

Settings stays on the gear button. Bottom area keeps real status plus current-session log. Version text is consistently `0.3.4 BETA R1`.

## Shared layout contract
A page receives a client rectangle and computes child rectangles from metrics instead of hard-coded Y positions.

Required regions:
- `heading`: one-line title, minimum 34 logical px;
- `description`: wrap-capable text measured before content placement;
- `content`: consumes remaining height;
- `actions`: one or two rows depending on count/available width;
- minimum 8 logical px gap between adjacent regions.

A layout test asserts that all visible rectangles are inside the page client area and pairwise non-overlapping for supported viewport/DPI combinations.

## Zapret Center 0.3.4
Zapret is promoted from the reduced 0.3.3 panel to a full management center while preserving the verified 0.3.x safety policy.

### Status
Show real values for:
- bundled Zapret version;
- bundle validity/missing required file;
- service installed/running;
- bundled `winws.exe` running/not running;
- bundle directory;
- selected strategy;
- last action result.

### Strategy discovery
Discover launchable bundled strategy `.bat` files from the verified bundle rather than hard-coding only `general.bat`.

Rules:
- only files inside the verified bundled Zapret root are eligible;
- service-management scripts are excluded from normal strategy choices;
- deterministic sort;
- display friendly name plus relative file name;
- `general.bat` remains the default when present.

### Actions
- Refresh status
- Select strategy
- Launch selected strategy
- Launch default strategy
- Stop bundled winws
- Open service manager (`service.bat`)
- Open bundle directory
- Diagnostics
- Update IPSet/hosts only when backed by a real bundled script or existing safe backend

No silent service installation/removal and no automatic strategy start.

### Process ownership safety
`Stop bundled winws` may target only a `winws.exe` whose executable path resolves inside the application bundled Zapret root. A foreign `winws.exe` is never killed.

### Diagnostics
Diagnostics report required bundle files, detected strategies, service state, bundled winws process path and actionable errors. Diagnostics do not modify system state.

## Feature parity audit
0.3.4 audits the old-series capabilities recorded in the historical binary/reconstruction notes and prior UI recovery spec. Every capability is classified `complete`, `partial`, `missing`, or `not safely recoverable`.

Minimum audited areas:
- Cleaning categories and analyze-before-clean flow
- RAM metrics/cleanup/monitor behavior
- DPopGuard quick/deep/file/folder/quarantine flows
- Disk analysis
- Installed applications, leftovers, WinGet/default-app flows where a real backend exists
- Windows Update/DISM cleanup
- Duplicate finder
- Tools/system repair commands
- Zapret/WinDivert management
- Session logs and quarantine
- Settings/exclusions

The audit never justifies fake features. A button exists only when a real backend exists. Missing historical behavior is implemented only when it is safe and testable.

## Architecture
0.3.4 continues from the verified 0.3.3 reverse-migration base, but 0.3.4-specific behavior is explicit and reviewable.

New structure:
- `tools/dpop034_migrate.py`: uses the 0.3.3 recovered donor, creates `v034`, applies 0.3.4 overlays, verifies identity, builds/tests.
- `v034_overlay/`: maintained 0.3.4 source overlays/new files; no generated binary content.
- `tests/test_dpop034_migrate.py`: migration/identity/overlay safety tests.
- `tests/test_dpop034_layout_contract.py`: supported viewport/DPI layout contract.
- `tests/test_dpop034_zapret_contract.py`: strategy discovery and safe-stop contract.
- `.github/workflows/DPopCleaner_0.3.4_CANDIDATE.yml`: Windows candidate build, CTest, UI smoke, screenshots/evidence; no public release on PR.

0.3.3 remains the rollback path until 0.3.4 candidate verification succeeds.

## Testing and evidence
Before release:
1. Python migration tests pass.
2. Existing recovery policy tests pass.
3. MSVC Release x64 build succeeds.
4. CTest passes.
5. UI smoke starts/closes the real application.
6. Screenshots are captured at 1100×700, 1200×850 and maximized.
7. Layout evidence asserts no heading/description/content/actions overlap.
8. Zapret tests prove foreign winws is never targeted.
9. Candidate artifact contains binaries, report and screenshots.
10. Public release/site/update manifest changes only after candidate verification.

## Release/site requirements
When approved for publication:
- create immutable prerelease `v0.3.4-beta-r1`;
- publish `DPopCleaner_Setup_0.3.4_BETA_R1.exe`;
- update site text/screenshot/download button;
- publish `update/beta.json` with `version=0.3.4`, `version_code=3041`, `revision=1`, exact size and SHA-256;
- live verification downloads the public installer and checks SHA-256 before success.

## Definition of Done
0.3.4 R1 is ready only when:
- old-series shell density and interaction model are restored;
- the 0.3.3 text-overlap defect cannot reproduce at supported sizes;
- Zapret Center exposes strategy selection and the safe action set above;
- no fake/non-backed controls are advertised;
- a feature parity audit accounts for all missing/partial areas;
- Windows build, CTest, UI smoke and candidate workflow are green;
- screenshots are reviewed before public release;
- site and downloadable file identify the same verified 0.3.4 build.
