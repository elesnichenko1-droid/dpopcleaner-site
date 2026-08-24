# DPopCleaner 0.3.4 UI Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Establish a reproducible 0.3.4 migration/build path and replace fragile fixed page geometry with a tested non-overlapping layout contract.

**Architecture:** Reuse the verified 0.3.3 recovery as donor, copy the recovered overlay to `v034`, apply explicit files from `v034_overlay`, then bump product identity to 0.3.4. Shared layout metrics are pure C++ geometry so runtime code and tests use the same rules.

**Tech Stack:** Python 3.12, C++20/Win32, CMake/CTest, PowerShell, GitHub Actions Windows 2022.

**Spec:** `docs/superpowers/specs/2026-08-24-dpopcleaner-0.3.4-golden-0214-design.md`

## Global Constraints
- Display version is exactly `0.3.4 BETA R1`.
- Version code is exactly `3041`; revision is `1`.
- 0.2.14 is the visual/interaction reference; 0.3.3 remains rollback.
- No public release from this plan; candidate evidence only.
- Supported UI viewports include 1100×700, 1200×850 and maximized.
- Visible page rectangles must not overlap.

---

### Task 1: 0.3.4 migration harness

**Files:**
- Create: `tools/dpop034_migrate.py`
- Create: `tests/test_dpop034_migrate.py`
- Create: `v034_overlay/README.md`

**Interfaces:**
- Consumes: `tools/dpop033_core.py::migrate()` and exported `source-overlay/v033` donor.
- Produces: `transform_v034_overlay(v034_root: Path) -> dict[str, str]`, traversal-safe overlay application, and `output/source-overlay/v034`.

- [ ] **Step 1: Write the failing identity/overlay tests**

Create tests that prove a temporary donor containing 0.3.3 identity becomes 0.3.4/3041/0.3.4.1 and that overlay application accepts only relative paths below the overlay root.

- [ ] **Step 2: Run the new test and verify RED**

Run: `python tests/test_dpop034_migrate.py -v`
Expected: FAIL because `tools/dpop034_migrate.py` does not exist.

- [ ] **Step 3: Implement minimal migration/identity functions**

Implement guarded replacements for `CMakeLists.txt`, `Version.h`, `version.rc.in`, `v033 -> v034` copy, and traversal-safe `v034_overlay/` application. Never mutate v033.

- [ ] **Step 4: Run tests and verify GREEN**

Run: `python tests/test_dpop034_migrate.py -v`
Expected: PASS with zero failures.

- [ ] **Step 5: Commit**

Commit: `feat: add DPopCleaner 0.3.4 migration harness`

### Task 2: Shared page layout geometry

**Files:**
- Create: `v034_overlay/ui/PageLayout.h`
- Create: `v034_overlay/ui/PageLayout.cpp`
- Create: `v034_overlay/tests/PageLayoutTests.cpp`
- Create/modify overlay: `v034_overlay/CMakeLists.txt`

**Interfaces:**
- Produces: `PageLayoutMetrics`, `PageRegions`, and `ComputePageRegions(int width, int height, int dpi, int actionCount)`.
- Guarantees: heading/description/content/actions remain in bounds with at least 8 logical px separation.

- [ ] **Step 1: Write failing geometry tests**

Cover 1100×700 and 1200×850 at DPI 96/120/144 plus a maximized sample. Assert positive dimensions, in-bounds rectangles and no pairwise intersection.

- [ ] **Step 2: Verify RED in Windows candidate compile**

Expected: compile/test failure because `ComputePageRegions` is missing.

- [ ] **Step 3: Implement DPI-scaled geometry**

Use integer DPI scaling equivalent to `MulDiv(value, dpi, 96)`. Reserve wrap-capable description height and allow actions to wrap to two rows when required.

- [ ] **Step 4: Run CTest and verify GREEN**

Expected: `PageLayoutTests` PASS.

- [ ] **Step 5: Commit**

Commit: `feat: add non-overlapping 0.3.4 page layout contract`

### Task 3: Apply layout contract to recovered workspace UI

**Files:**
- Create/modify overlay: `v034_overlay/ui/pages/WorkspacePage.h`
- Create/modify overlay: `v034_overlay/ui/pages/WorkspacePage.cpp`
- Create: `tests/test_dpop034_layout_contract.py`

**Interfaces:**
- Consumes: `ComputePageRegions()`.
- Produces: runtime page layout with measured description/status area and action row(s).

- [ ] **Step 1: Write static regression test**

Reject the old fixed `status y=52 / content y=86` layout and require `ComputePageRegions` usage.

- [ ] **Step 2: Run and verify RED**

Run: `python tests/test_dpop034_layout_contract.py -v`
Expected: FAIL before WorkspacePage overlay exists.

- [ ] **Step 3: Integrate shared layout**

Place heading, wrapped status/description, content and buttons using `PageRegions`; keep existing page actions/async behavior unchanged.

- [ ] **Step 4: Verify contracts + CTest + UI smoke**

Expected: all pass and screenshots show no text/content overlap at supported sizes.

- [ ] **Step 5: Commit**

Commit: `fix: prevent 0.3.4 workspace text and content overlap`

### Task 4: Candidate workflow

**Files:**
- Create: `.github/workflows/DPopCleaner_0.3.4_CANDIDATE.yml`
- Create: `tools/dpop034_ui_smoke.ps1`

**Interfaces:**
- Consumes: `tools/dpop034_migrate.py`.
- Produces: artifact `dpopcleaner-0.3.4-candidate` with binaries, migration report and screenshots.

- [ ] **Step 1: Add workflow contract assertions**

Require Windows 2022, migration tests, MSVC build, CTest, UI smoke, 1100×700/1200×850/maximized captures and artifact upload. Reject release creation/site mutation.

- [ ] **Step 2: Verify RED before workflow exists**

Run the Python migration/workflow contract tests; expected FAIL.

- [ ] **Step 3: Implement candidate workflow and UI smoke**

Workflow runs on pull requests for 0.3.4 paths and never creates a GitHub Release or updates `update/beta.json`.

- [ ] **Step 4: Open draft PR and verify Windows candidate**

Expected: migration tests PASS, MSVC PASS, CTest PASS, UI smoke PASS, artifact uploaded.

- [ ] **Step 5: Commit**

Commit: `ci: add DPopCleaner 0.3.4 candidate verification`
