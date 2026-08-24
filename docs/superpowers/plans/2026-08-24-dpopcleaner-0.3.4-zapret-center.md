# DPopCleaner 0.3.4 Zapret Center Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore a full, safe Zapret Center with strategy discovery/selection, safe start/stop, diagnostics, service-manager access and old-series information density.

**Architecture:** Extend the recovered Zapret backend with pure strategy/path policy helpers and keep system mutations behind explicit functions. The UI renders real status, strategy selection and grouped actions using the shared 0.3.4 page layout contract.

**Tech Stack:** C++20/Win32, filesystem/process/service APIs, CMake/CTest, Python contract tests.

**Spec:** `docs/superpowers/specs/2026-08-24-dpopcleaner-0.3.4-golden-0214-design.md`

## Global Constraints
- Only strategies inside the verified bundled Zapret root are launchable.
- Foreign `winws.exe` processes are never terminated.
- No silent service installation/removal or automatic strategy start.
- Every system-changing action is explicit and logged.

---

### Task 1: Strategy discovery policy

**Files:**
- Create/modify: `v034_overlay/modules/ZapretPolicy.h`
- Create/modify: `v034_overlay/modules/ZapretPolicy.cpp`
- Create: `v034_overlay/tests/ZapretPolicy034Tests.cpp`
- Modify overlay CMake test list.

- [ ] Write failing tests for `.bat` discovery, traversal rejection, service-script exclusion, deterministic sort and `general.bat` default preference.
- [ ] Run candidate CTest and verify RED.
- [ ] Implement pure discovery/policy helpers.
- [ ] Run CTest and verify GREEN.
- [ ] Commit `feat: restore Zapret strategy discovery in 0.3.4`.

### Task 2: Safe bundled process control

**Files:**
- Create/modify: `v034_overlay/modules/ZapretManager.h`
- Create/modify: `v034_overlay/modules/ZapretManager.cpp`
- Create: `v034_overlay/tests/ZapretManager034Tests.cpp` for pure ownership helpers.

- [ ] Add failing ownership tests: bundled path=true, same filename outside bundle=false.
- [ ] Implement `LaunchStrategy(relativePath)`, `StopBundledWinws`, richer status and diagnostics.
- [ ] Preserve compatibility with `LaunchDefaultStrategy`, `OpenServiceManager`, `OpenBundledFolder`.
- [ ] Run CTest and verify GREEN.
- [ ] Commit `feat: add safe Zapret strategy launch and stop controls`.

### Task 3: Full Zapret Center UI

**Files:**
- Create/modify: `v034_overlay/ui/pages/WorkspacePage.h/.cpp`, or create dedicated `v034_overlay/ui/pages/ZapretPage.*` if that produces a cleaner isolated component.
- Create: `tests/test_dpop034_zapret_contract.py`.

- [ ] Add failing UI contract requiring strategy selector and actions: Refresh, Start selected, Default, Stop, Service Manager, Open bundle, Diagnostics.
- [ ] Implement the page using 0.3.4 shared layout metrics; no overlap at minimum viewport.
- [ ] Log each action and show actionable errors.
- [ ] Run Python contract, CTest and UI smoke.
- [ ] Commit `feat: restore full Zapret Center UX for 0.3.4`.

### Task 4: Candidate evidence

**Files:** `tools/dpop034_ui_smoke.ps1`, `.github/workflows/DPopCleaner_0.3.4_CANDIDATE.yml`.

- [ ] Add Zapret-page screenshot capture at 1100×700, 1200×850 and maximized.
- [ ] Add assertion/evidence that the expected action controls are present.
- [ ] Run full Windows candidate.
- [ ] Review artifact screenshots and logs.
- [ ] Commit `test: verify DPopCleaner 0.3.4 Zapret Center UI`.
