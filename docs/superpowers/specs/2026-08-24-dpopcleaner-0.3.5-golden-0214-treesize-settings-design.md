# DPopCleaner 0.3.5 BETA R1 — Golden 0.2.14 + TreeSize Disk + Settings Recovery

## Goal

DPopCleaner 0.3.5 BETA R1 returns to the historical 0.2.14 interaction model as the user-facing baseline while retaining the verified modern backend and release infrastructure from the current 0.3.x line.

The release is intentionally not another cosmetic patch over the current 0.3.4 screens. The product direction is:

**0.2.14 UX = host/reference**  
**0.3.x backend = donor**  
**0.3.5 = maintained union with a real disk-size analyzer and real settings**

The two highest-priority recoveries are:

1. rebuild the Disk page as a TreeSize-style hierarchical disk analyzer;
2. replace the current Settings page with a complete, persisted, backend-connected settings center.

## Why 0.3.5

The repository already contains published 0.3.4 builds, so reusing `0.3.3` or `0.3.4` for the corrected architecture would create downgrade/version-order problems for updater and release logic. The next corrected build is therefore `0.3.5 BETA R1`.

Product identity:

- Display version: `0.3.5 BETA R1`
- Version: `0.3.5`
- Version code: `3051`
- Revision: `1`
- Windows resource version: `0.3.5.1`
- Tag: `v0.3.5-beta-r1`
- Installer: `DPopCleaner_Setup_0.3.5_BETA_R1.exe`

## Source of truth

### UX reference

The historical 0.2.14 series remains the visual and interaction reference. The exact old executable is not patched or reverse-engineered into source. The maintainable reconstruction and previous recovery work are the implementation base.

### Technical donor

The modern donor is the verified 0.3.x code line already present in the repository, including safe async execution, diagnostics, logging, updater/release plumbing and functional modules that have real backend behavior.

### Existing design continuity

This design supersedes the direction of merely improving the current 0.3.4 presentation. It preserves the useful rules already established by the earlier 0.3.2/0.3.3/0.3.4 recovery designs:

- compact native Win32 UI;
- horizontal top navigation;
- Settings behind the gear button;
- no generic text panel as a replacement for a dedicated feature page;
- asynchronous long-running work;
- explicit confirmation for destructive/system-changing actions;
- no fake controls without backend support;
- stable rollback remains available until candidate verification succeeds.

## Branch and isolation

Implementation work is isolated on:

`feat/dpopcleaner-0.3.5-r1`

`main` is not changed while the candidate is being developed and verified.

The 0.3.5 implementation should use an explicit version-specific overlay or source directory rather than silently rewriting the already-published 0.3.4 source. Existing 0.3.3 and 0.3.4 release artifacts remain immutable rollback/reference points.

## Shell and navigation

The application keeps the historical compact horizontal interaction model.

Primary pages:

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

Settings opens from the gear button rather than becoming an eleventh primary page.

Layout requirements:

- target size: approximately `1200×850`;
- minimum supported size: `1100×700`;
- DPI-aware at 100%, 125% and 150%;
- content expands when maximized instead of producing decorative empty areas;
- no overlapping text, controls, lists or action bars;
- dense functional information is preferred over large decorative cards.

## Disk Analyzer — product behavior

The Disk page is no longer primarily a file browser. Its default purpose is to answer: **what consumes space, where, and in what proportion?**

The user-provided TreeSize screenshot is the layout reference for information density and parent-relative usage visualization.

### Main table/tree

The primary control is a hierarchical tree/list with expandable directory nodes.

Required columns:

- `Имя`
- `Размер`
- `Занято`
- `Файлы`
- `Папки`
- `% родителя`
- `Изменено`

`Имя` supports real tree expansion/collapse. Directories display recursively accumulated size instead of `—`.

### Green parent-relative bar

The `% родителя` column contains a green usage bar and a readable numeric percentage.

For child node `n` with parent `p`:

`percent = p.size > 0 ? 100 * n.size / p.size : 0`

Rules:

- clamp visual bar to 0–100%;
- retain a numeric value even for tiny percentages;
- use the application accent green for the filled portion;
- text remains readable over both filled and unfilled backgrounds;
- the root node displays 100%;
- sorting by percentage uses numeric values, not rendered text.

This visual is functional information, not decoration.

### Scanning model

A scan is recursive and asynchronous.

Required behavior:

- UI remains responsive;
- scan can be cancelled;
- partial results appear progressively rather than only at the end;
- inaccessible paths are skipped and counted/logged rather than aborting the scan;
- directory sizes are accumulated bottom-up;
- file and subdirectory counts are tracked;
- junction/reparse-point behavior is explicit and loop-safe;
- the scanner never follows a path pattern that can create recursive cycles;
- scan state survives list repaints and sorting;
- cancellation leaves the UI in a consistent partial-results state.

### Disk data model

The disk scanner uses a dedicated model rather than storing authoritative state only in list-view text.

Each node stores at minimum:

- stable node id;
- parent id;
- full path;
- display name;
- directory/file flag;
- logical file size total;
- allocated/on-disk size when available;
- descendant file count;
- descendant directory count;
- last modification time;
- access-denied/incomplete flag;
- protected/system path hint;
- scan status.

The UI reads from this model and can rebuild/sort visible rows without rescanning.

### Disk actions

Required top actions:

- `Назад`
- `C:\`
- `Выбрать каталог`
- `Сканировать`
- `Стоп`
- `Обновить`
- `Крупные файлы`
- `Проводник`

Behavior:

- `Сканировать` starts analysis for the selected root;
- `Стоп` requests cancellation and never blocks waiting on the UI thread;
- `Обновить` rescans the current root;
- double-clicking/expanding directories uses already-scanned hierarchy where possible;
- `Проводник` opens the selected item or current root;
- `Крупные файлы` is a secondary view derived from scan data or a dedicated safe scan, not the main Disk experience.

### Safety presentation

The current wide textual `Безопасность` column is removed from the default table.

System/protected locations use a compact warning indicator and tooltip/status explanation. The purpose is to preserve dense TreeSize-like presentation without hiding safety context.

The Disk page does not delete arbitrary files directly in 0.3.5 R1. Any future deletion action requires a separate explicit design and confirmation policy.

## Settings — complete redesign

Settings remains a gear-opened application surface, but the current flat group of checkboxes and labels is replaced by a structured settings center.

Top settings sections:

1. `Основное`
2. `Очистка`
3. `Память`
4. `Защита`
5. `Исключения`

The section selector is horizontal/compact to remain consistent with the 0.2.14 interaction model. No left navigation sidebar is introduced.

### Основное

Real settings only:

- run DPopCleaner with Windows;
- minimize/continue in tray;
- check DPopCleaner updates at startup;
- behavior when closing the main window;
- optional startup elevation behavior only when backed by the existing safe mechanism.

Language/theme controls are shown only if changing them actually changes application behavior. A static label such as `Язык: Русский • Тема: Midnight` is not considered a setting.

### Очистка

- confirm destructive cleanup actions;
- background junk monitoring, if real background scheduling is implemented;
- large-file threshold;
- duplicate minimum-size threshold;
- cleanup-related startup checks that have a real backend.

Values have explicit ranges and inline validation.

### Память

- automatic RAM cleanup enabled/disabled;
- threshold percentage;
- interval when interval-based automation is supported;
- safe/advanced scope where backed by the existing memory backend;
- aggressive/undocumented operations remain off by default and clearly separated.

### Защита

- Quick DPopGuard scan at application startup;
- notification behavior for Guard findings where supported;
- Windows Update cache startup check where it belongs operationally;
- administrator-mode preference only through the existing supported mechanism.

No setting may silently delete, quarantine or change system state at startup unless the user has explicitly enabled that exact behavior and it is within the existing safety policy.

### Исключения

A dedicated list supports cleanup exclusions.

Actions:

- `Добавить файл`
- `Добавить папку`
- `Удалить`

Requirements:

- normalize paths for duplicate detection;
- preserve original readable path for display;
- missing paths remain visible so the user can remove or restore them;
- exclusions are applied by cleanup analysis and cleanup execution, not merely stored cosmetically.

## Settings actions and state

Persistent bottom actions:

- `Применить`
- `Сохранить`
- `Отмена`
- `По умолчанию`

Semantics:

- `Применить`: validate, persist and activate changes without closing Settings;
- `Сохранить`: validate, persist, activate, then close Settings;
- `Отмена`: discard unsaved UI edits and restore the persisted model;
- `По умолчанию`: load safe defaults into the editor but do not persist until Apply/Save.

Dirty state is tracked. Closing Settings with unsaved changes prompts the user to save/discard/cancel.

## Settings storage

Canonical location:

`%LOCALAPPDATA%\DPopCleaner\settings.json`

Requirements:

- single typed settings model;
- schema version field;
- safe defaults;
- atomic write via temporary file + replace;
- corrupted JSON falls back to defaults and records a clear log/status event;
- unknown future fields do not crash the current version;
- existing compatible 0.3.x values are migrated where possible;
- no secrets are stored in this file.

The UI edits a working copy. Runtime systems consume the committed settings model.

## Backend integration contract

A control is considered complete only if all four conditions are true:

1. it displays the current persisted value;
2. user changes are validated;
3. Save/Apply persists the change;
4. the owning subsystem actually consumes the setting.

Controls that fail condition 4 are omitted from 0.3.5 R1 rather than displayed as placeholders.

## Async and threading

The existing principle remains mandatory: heavy operations never execute synchronously on the UI thread.

For the Disk analyzer specifically:

- scanning worker owns filesystem traversal;
- UI receives throttled batches/progress notifications;
- model mutation is synchronized or marshalled through the UI/application model boundary;
- cancellation uses cooperative stop tokens/events;
- closing the app requests cancellation and prevents workers from posting to destroyed windows.

## Logging

Continue using the application log under the DPopCleaner local application-data area.

Disk scan events include:

- root started;
- completed/cancelled;
- elapsed time;
- files/directories counted;
- access-denied/error count;
- no user file contents.

Settings log events include:

- load/migration/fallback state;
- successful save/apply;
- validation/persistence failures;
- no private file contents beyond paths the user explicitly added as exclusions.

## Testing strategy

### Disk scanner tests

Unit tests cover:

- bottom-up directory accumulation;
- parent percentages;
- empty directories;
- access-denied/incomplete nodes;
- cancellation;
- reparse/junction loop avoidance;
- file/subdirectory counts;
- deterministic sorting independent of rendered strings.

### Settings tests

Unit tests cover:

- defaults;
- load/save round trip;
- corrupted JSON fallback;
- atomic replacement failure handling;
- schema migration;
- validation ranges;
- path exclusion normalization;
- dirty/apply/cancel/default semantics at the model/controller boundary.

### UI contract tests

At supported viewport/DPI combinations:

- no control overlap;
- settings bottom action row remains visible;
- Disk columns remain usable and horizontal scrolling is available when required;
- `% родителя` bar rectangle remains inside its cell;
- text is not clipped into adjacent columns;
- gear opens Settings and returns to the previous main page cleanly.

### Windows candidate verification

Before publication:

1. migration/overlay tests pass;
2. MSVC x64 Release builds;
3. CTest passes;
4. real app starts and closes cleanly;
5. Disk scan smoke test completes on a controlled fixture directory;
6. Disk cancellation smoke test succeeds;
7. Settings save/reload smoke test succeeds;
8. screenshots are captured for Disk and every Settings section at 1100×700 and 1200×850;
9. Defender/SmartScreen-related release checks follow the existing safe pipeline;
10. public release is not created until screenshots and candidate evidence are reviewed.

## Implementation boundaries

0.3.5 R1 does not attempt to rewrite every DPopCleaner subsystem at once.

The shell is adjusted only as needed to restore the 0.2.14 interaction model and host the corrected pages. Existing working modules are retained unless a concrete test shows incompatibility.

Primary implementation order:

1. establish 0.3.5 isolated source/identity and regression tests;
2. recover/lock the 0.2.14-style shell behavior needed by 0.3.5;
3. implement the Disk data model and scanner tests;
4. implement TreeSize-style Disk UI including green parent bars;
5. implement typed Settings model/storage/migration tests;
6. implement the new Settings UI and connect every visible control to backend behavior;
7. integrate runtime consumers of settings;
8. run layout, CTest and Windows smoke verification;
9. produce candidate screenshots/evidence;
10. only after approval, publish release/site/update metadata.

## Definition of Done

0.3.5 BETA R1 is candidate-ready only when all of the following are true:

1. the application is recognizably based on the compact 0.2.14 interaction model rather than the current generic 0.3.4 presentation;
2. Disk defaults to a hierarchical size analyzer, not a normal file browser;
3. folder sizes are real recursive totals;
4. `% родителя` displays a green proportional bar plus numeric percentage;
5. scanning is asynchronous, progressive and cancellable;
6. access failures and reparse points cannot crash or loop the scanner;
7. Settings has the five approved horizontal sections;
8. no static fake language/theme setting is shown;
9. Apply/Save/Cancel/Defaults have distinct real semantics;
10. every visible setting is persisted and consumed by a real subsystem;
11. exclusions affect actual cleanup behavior;
12. supported sizes/DPI do not overlap controls;
13. MSVC build and CTest are green;
14. candidate Disk/Settings screenshots are reviewed before release;
15. `main` and published 0.3.4 artifacts remain rollback-safe until the 0.3.5 candidate is approved.
