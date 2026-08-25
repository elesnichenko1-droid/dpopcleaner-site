# DPopCleaner 0.4.17 — original 0.2.14 base design

## Status

Approved direction with one hard constraint from the user: **do not use the reconstructed C++ implementation as the application base or donor**.

## Product identity

- Product: `DPopCleaner`
- Version: `0.4.17`
- No `BETA`, no `R1/R2`, no `Stage`
- Public installer name: `DPopCleaner_Setup_0.4.17.exe`
- ProductVersion: `0.4.17`

## Non-negotiable base

The source of truth is the preserved original binary:

`downloads/DPopCleaner_0.2.14_BETA.exe`

The 0.3.x overlays, recovered shell, `v035_overlay`, and reconstructed C++ application are **not** used as the base and are **not** copied into the 0.4.17 product line.

The original 0.2.14 executable remains the legacy application core and visual reference. We do not replace its main window with a reconstructed clone.

## Architecture

Because the original 0.2.14 source code is unavailable, 0.4.17 is structured as an original-core distribution with separate companion modules instead of pretending we can safely edit unavailable source.

```text
DPopCleaner\
├─ DPopCleaner.exe                 # original 0.2.14 core binary, preserved behavior
├─ Languages\
│  ├─ ru.json
│  └─ en.json
├─ Shell\
│  ├─ commands\
│  ├─ integration\
│  └─ shell.json
├─ Documentation\
│  ├─ History\
│  ├─ Backups\
│  │  ├─ Settings\
│  │  ├─ Registry\
│  │  └─ System\
│  └─ Logs\
├─ Modules\
│  ├─ DiskAnalyzer.exe
│  └─ RestoreCenter.exe
└─ Resources\
```

### Original core rule

`DPopCleaner.exe` is the actual preserved 0.2.14 program. Existing 0.2.14 functions are not reimplemented just to make them look newer.

New functionality is delivered as isolated companion modules. No DLL injection, no undocumented in-process hooks, and no AV-hostile binary instrumentation are part of the first 0.4.17 scope.

If a future requirement demands adding a new button directly inside the legacy 0.2.14 window, that is a separate reverse-engineering task and must be approved separately. It is not silently emulated in 0.4.17.

## Languages

The previous approach of hardcoding multiple languages in one UI implementation is prohibited.

All new 0.4.17 components use external language packs under `Languages`.

Example keys:

- `app.title`
- `disk.title`
- `disk.scan`
- `restore.title`
- `restore.rollback`
- `common.cancel`
- `common.error`

Only one language dictionary is active at a time. Missing keys fall back to Russian. New UI must never create two overlapping translated controls.

The untouched original 0.2.14 legacy window keeps its original built-in text. We do not claim that external language files can safely rewrite hardcoded legacy strings without source code.

## Shell folder

`Shell` is a real product folder and contains Windows integration owned by DPopCleaner:

- command definitions;
- safe shell launch helpers;
- Explorer integration metadata;
- paths to companion modules;
- versioned integration state.

No shell integration is allowed to exist only as hidden installer code with no installed representation.

## Documentation and rollback

`Documentation` is persistent user-visible application state, not a temporary log directory.

Every supported reversible DPopCleaner action creates a history record containing:

- timestamp;
- operation identifier;
- human-readable description;
- affected object/path/registry key;
- previous state;
- new state;
- backup reference;
- whether rollback is available;
- rollback result when attempted.

`Backups` contains the data required for restoration. `History` contains records. `Logs` contains diagnostics.

### Rollback boundary

DPopCleaner only offers `Restore` when it has enough information to restore safely.

Examples suitable for rollback:

- DPopCleaner settings;
- supported registry changes;
- supported startup entries;
- supported system toggles where previous state was captured.

Examples not promised as reversible:

- deleting arbitrary TEMP/cache files;
- emptying the Recycle Bin;
- third-party uninstallers;
- external tool changes not captured by DPopCleaner.

The UI must say `Откат недоступен` rather than presenting a fake Restore action.

## New feature 1 — Disk Analyzer

A separate `DiskAnalyzer.exe` is added to the DPopCleaner distribution.

Required table:

`Имя | Размер | Занято | Файлы | Папки | % родителя | Изменено`

Behavior:

- hierarchical scan;
- asynchronous/cancellable scan;
- reparse points are not followed by default;
- physical allocation is shown only when known;
- unknown allocation is `—`, never substituted with a guessed logical value;
- numeric sorting;
- parent-percentage bars;
- open selected item in Explorer;
- no arbitrary delete button in the analyzer.

Its visual language follows the compact dark/green character of original 0.2.14, but it is a new isolated module rather than a reconstructed replacement for the legacy main window.

## New feature 2 — Restore Center

A separate `RestoreCenter.exe` presents `Documentation` history and backups.

Required columns:

`Дата | Действие | Объект | Состояние | Откат`

The user can inspect a record, view what changed, and restore only records marked reversible.

Rollback itself is transactional where possible:

1. validate backup;
2. capture current state as a new history record;
3. perform restore;
4. verify restored state;
5. write success/failure result.

A failed rollback must leave diagnostic information and must not delete the backup that was used.

## General quality improvements

0.4.17 keeps the original core behavior but improves the distribution around it:

- stable installer/uninstaller;
- correct DPI behavior in new modules;
- long work off the UI thread;
- explicit errors instead of silent failure;
- deterministic folder layout;
- clean logging;
- no overlapping localization controls;
- no destructive operation disguised as analysis;
- Windows 10/11 x64 target.

## Installer

The installer produces a clean `DPopCleaner 0.4.17` installation containing the preserved original core plus the new external modules and folders.

It must create the full folder structure even before the first history item exists, so `Languages`, `Shell`, and `Documentation` are visible and predictable immediately after installation.

Upgrade/uninstall must not silently destroy `Documentation\Backups`. User-created rollback data is preserved unless the user explicitly chooses to remove it.

## Website

The website is redesigned around the real application rather than marketing claims.

Hero section:

- real screenshot of the preserved 0.2.14 DPopCleaner window;
- `DPopCleaner 0.4.17`;
- one primary download button;
- Windows 10/11 x64;
- installer size and SHA-256.

Feature sections show:

- original DPopCleaner core;
- Disk Analyzer;
- Restore Center;
- external Languages architecture;
- Shell and Documentation folder structure.

The download button is enabled for 0.4.17 only after the installer has passed install + launch + module + rollback smoke checks and the live download hash matches release metadata.

## Testing/release gates

0.4.17 is not published until all of the following pass:

1. preserved original 0.2.14 core hash/input contract;
2. installer contains `Languages`, `Shell`, `Documentation`, `Modules`;
3. no reconstructed C++ application sources are included in the 0.4.17 staged payload;
4. Disk Analyzer controlled-fixture scan;
5. unknown allocated size displays `—`;
6. Restore Center reversible action round-trip;
7. non-reversible history item has no fake restore action;
8. language pack switch in new modules does not overlap controls;
9. silent installer test;
10. installed launch smoke;
11. GitHub Release asset hash;
12. live website version and installer SHA-256 verification.

## Explicit exclusions

- No 0.3.5 UI or shell code.
- No `v035_overlay` application code.
- No reconstructed C++ DPopCleaner application as the runtime base.
- No fake claim that the original 0.2.14 hardcoded UI has been fully externalized into language files.
- No direct delete action in Disk Analyzer.
- No rollback promise for data that DPopCleaner cannot actually restore.
