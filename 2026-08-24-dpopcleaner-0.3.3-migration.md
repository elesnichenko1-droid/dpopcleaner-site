# DPopCleaner 0.3.3 Reverse Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produce a safe, reproducible 0.3.3 source/build staging flow that restores the faithful 0.2.14 UX onto the 0.3.2 core and exports it as `v033` without mutating the existing releases.

**Architecture:** A Python migration tool parses the existing embedded faithful-recovery payload, applies it in a detached Git worktree, clones the recovered `v032` overlay into `v033`, rewrites version identity, prepares a normalized source tree, and optionally builds/tests on Windows. A thin GitHub Actions workflow runs the same tool and uploads evidence.

**Tech Stack:** Python 3.11+ standard library, Git, PowerShell, CMake 3.24+, Visual Studio 2022/MSVC, GitHub Actions Windows 2022.

**Spec:** `docs/superpowers/specs/2026-08-24-dpopcleaner-0.3.3-migration-design.md`

## Global Constraints

- UX/behavior reference is DPopCleaner 0.2.14 BETA.
- Recovery payload source is `.github/workflows/DPopCleaner_0.3.2_FAITHFUL_0214_RECOVERY_ONE_CLICK.yml`.
- Target is `0.3.3 BETA R1`, version code `3031`, resource version `0.3.3.1`.
- Existing `v032/`, `r4/`, `downloads/`, `update/`, and site files must not be changed during staging.
- Recovery payload writes/deletes are restricted to `v032/` and `scripts/` inside an isolated worktree.
- Final acceptance requires a Windows x64 Release build and passing CTest suite.

---

### Task 1: Recovery-payload parser and safety boundary

**Files:**
- Create: `tools/dpop033_migrate.py`
- Test: `tests/test_dpop033_migrate.py`

**Interfaces:**
- Produces: `extract_embedded_payload(workflow_text: str) -> dict`
- Produces: `validate_payload_path(path: str) -> pathlib.PurePosixPath`
- Produces: `apply_payload(repo_root: pathlib.Path, payload: dict) -> tuple[int, int]`

- [ ] **Step 1: Write failing tests** for payload extraction, malformed base64/gzip/JSON, path traversal rejection, protected path rejection, and a valid file write/delete.
- [ ] **Step 2: Run** `python -m unittest -v tests/test_dpop033_migrate.py` and verify import/function failures.
- [ ] **Step 3: Implement** strict YAML-block extraction, base64+gzip+JSON decoding, path validation, and file application.
- [ ] **Step 4: Re-run tests** and require all Task 1 tests to pass.
- [ ] **Step 5: Keep changes isolated**; no repository write occurs in the parser tests.

### Task 2: v033 version transformation

**Files:**
- Modify: `tools/dpop033_migrate.py`
- Test: `tests/test_dpop033_migrate.py`

**Interfaces:**
- Produces: `transform_v033_overlay(v033_root: pathlib.Path) -> dict[str, str]`

- [ ] **Step 1: Write failing fixture tests** for CMake, Version.h, resource version, and `tests/v032/` -> `tests/v033/` transformation.
- [ ] **Step 2: Run tests** and verify they fail before implementation.
- [ ] **Step 3: Implement exact guarded replacements** that fail closed if expected 0.3.2 markers are absent.
- [ ] **Step 4: Re-run tests** and require exact 0.3.3 markers.

### Task 3: isolated worktree migration and source preparation

**Files:**
- Modify: `tools/dpop033_migrate.py`
- Generated in staging: `scripts/Prepare-033Source.ps1`

**Interfaces:**
- Consumes: recovery parser and version transformer.
- Produces: `v033/`, `prepared-src/`, `migration-report.json`.

- [ ] **Step 1: Add tests** for command construction and refusal to use a workspace inside the repository.
- [ ] **Step 2: Implement detached `git worktree add --detach` staging** with guaranteed cleanup unless `--keep-worktree` is specified.
- [ ] **Step 3: Apply recovery payload in stage**, clone recovered `v032` to `v033`, transform version, and generate `Prepare-033Source.ps1`.
- [ ] **Step 4: Run preparation helper** on Windows or export source without building when `--no-build` is used.

### Task 4: Windows build/test/evidence

**Files:**
- Modify: `tools/dpop033_migrate.py`
- Create: `.github/workflows/DPopCleaner_0.3.3_REVERSE_MIGRATION.yml`

**Interfaces:**
- Produces: `artifacts/DPopCleaner.exe`, `artifacts/DPopUpdater.exe`, `artifacts/migration-report.json`, CTest output.

- [ ] **Step 1: Add command-generation tests** for CMake configure/build/CTest.
- [ ] **Step 2: Implement Windows build path** using Visual Studio 17 2022 x64 and `BUILD_TESTING=ON`.
- [ ] **Step 3: Verify required executables** and hash them with SHA-256.
- [ ] **Step 4: Create workflow** that runs parser unit tests first, then performs migration/build, and uploads the artifacts.

### Task 5: final static verification and handoff

**Files:**
- Create: `README_0.3.3_MIGRATION_RU.md`

**Interfaces:**
- Consumes all previous outputs.
- Produces a human-readable execution/rollback guide.

- [ ] **Step 1: Run** `python -m unittest -v tests/test_dpop033_migrate.py`.
- [ ] **Step 2: Run** `python -m py_compile tools/dpop033_migrate.py`.
- [ ] **Step 3: Scan generated bundle** for accidental target writes outside allowed paths and stale target version markers.
- [ ] **Step 4: Package the migration bundle** for GitHub upload once connector write access is available.
