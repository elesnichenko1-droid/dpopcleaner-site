# DPopCleaner 0.3.4 Feature Parity and Release Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Audit old-series capabilities, restore only safely backed missing functions, and publish one verified 0.3.4 installer/site/update chain.

**Architecture:** Feature parity is explicit documentation plus tests, not inferred marketing. Each missing capability gets a real backend or is marked not safely recoverable. Publication reuses the verified 0.3.3 release architecture with 0.3.4 identity and live SHA verification.

**Tech Stack:** C++20/Win32, Python, PowerShell, CMake/CTest, Inno Setup, GitHub Actions/Pages.

**Spec:** `docs/superpowers/specs/2026-08-24-dpopcleaner-0.3.4-golden-0214-design.md`

## Global Constraints
- Do not add fake controls for behavior not backed by code.
- Public manifest/site/download must point to the same immutable 0.3.4 R1 asset.
- Release only after candidate UI/build verification is green.

---

### Task 1: Commit feature parity matrix

**Files:** Create `docs/0.3.4-feature-parity.md`.

- [ ] Inventory Cleaning, Memory, Guard, Disk, Applications/WinGet/default apps, Windows Update, Duplicates, Tools, Zapret, logs/quarantine and Settings.
- [ ] Classify each capability as complete/partial/missing/not safely recoverable with supporting file/function references.
- [ ] List concrete R1 restoration items only where safe backend work is feasible.
- [ ] Review the matrix for unsupported claims.
- [ ] Commit `docs: audit DPopCleaner 0.2.14 to 0.3.4 feature parity`.

### Task 2: Restore feasible missing R1 functions

**Files:** Determined by the parity matrix; every change is paired with a focused C++ or Python test.

- [ ] For each R1 restoration item, write the failing test first.
- [ ] Implement the minimal real backend and UI action.
- [ ] Run focused tests after each capability.
- [ ] Run full CTest after the batch.
- [ ] Commit per capability rather than as one monolithic change.

### Task 3: 0.3.4 installer and release pipeline

**Files:**
- Create: `release/DPopCleaner_0.3.4_R1.iss`
- Create: `.github/workflows/publish-dpopcleaner-0.3.4.yml`
- Create: `release/RELEASE_NOTES_0.3.4_R1.md`
- Modify after candidate approval: `index.html`, `script.js`, `release-manifest.js`, `update/beta.json`, site screenshot.

- [ ] Add contract tests for 0.3.4 tag/asset/version_code and fail-closed live SHA verification.
- [ ] Build candidate installer from verified 0.3.4 binaries and pinned Zapret bundle.
- [ ] Verify installer size/SHA in artifact metadata.
- [ ] Publish immutable prerelease only on main after candidate approval.
- [ ] Deploy Pages with exact 0.3.4 screenshot/manifest/download URL and verify public SHA.

### Task 4: Final cleanup

**Files:** Old 0.3.3-only temporary migration/release files that have no rollback/test-fixture role.

- [ ] Search references before deletion.
- [ ] Keep 0.3.3 release tag and rollback artifacts immutable.
- [ ] Move legacy policy fixtures out of live workflow paths where tests permit.
- [ ] Run full candidate + publish contract tests after cleanup.
- [ ] Commit `chore: finalize DPopCleaner 0.3.4 repository layout`.
