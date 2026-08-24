# DPopCleaner 0.3.3 Reverse Migration Design

## Goal

Build DPopCleaner 0.3.3 with the user-visible UX and behavior of DPopCleaner 0.2.14 as the compatibility baseline, while retaining the useful 0.3.2 core functionality.

## Ground truth discovered in the repository

The published clean 0.2.14 R1 release is based on the standalone native Windows executable `downloads/DPopCleaner_0.2.14_BETA.exe`; the original C++ source tree for that exact executable is not preserved byte-for-byte. The repository contains a maintainable reconstruction based on observed 0.2.14 behavior and an embedded faithful-recovery payload in `.github/workflows/DPopCleaner_0.3.2_FAITHFUL_0214_RECOVERY_ONE_CLICK.yml` whose stated purpose is to restore the 0.2.14 UX on the 0.3.2 core.

Therefore 0.3.3 uses the faithful recovery payload as the UI/behavior host and the recovered 0.3.2 modules as the functional donor. It does not attempt to patch the compiled 0.2.14 EXE.

## Source of truth

- UX/behavior reference: standalone DPopCleaner 0.2.14 BETA, expected SHA-256 `7d5e0a510189db31ef7ee1aca72dc182332a8020d994c81be40a519c5960515c` when present.
- Maintainable base mapping: `scripts/Prepare-R3Source.ps1` and `scripts/R3ReleasePolicy.psm1`.
- Faithful recovery payload: `.github/workflows/DPopCleaner_0.3.2_FAITHFUL_0214_RECOVERY_ONE_CLICK.yml`.
- Functional donor overlay: recovered `v032/`.
- Target overlay: `v033/`.
- Target product version: `0.3.3 BETA R1`, version code `3031`, resource version `0.3.3.1`.

## Migration rules

1. Never modify the original `v032/`, `r4/`, site, downloads, update manifests, or published release files in-place during preparation.
2. Apply the faithful-recovery payload only inside an isolated staging worktree.
3. Copy the recovered `v032/` overlay to `v033/` and make all 0.3.3-specific changes there.
4. Keep the recovered 0.2.14-style shell and layout as the UI regression baseline.
5. Retain 0.3.2 core modules, including FullCore diagnostics and its tests, unless a test proves a donor component incompatible with the recovered shell.
6. Update every target-visible version identifier from 0.3.2 to 0.3.3.
7. Build and test from a prepared normalized source tree, not directly from the repository's flattened source layout.

## Isolation

`tools/dpop033_migrate.py` creates a detached Git worktree beneath the selected workspace. The embedded recovery payload is decoded and applied only there. The original checkout remains unchanged. The tool exports a portable `v033/` overlay, a `Prepare-033Source.ps1` helper, build binaries, test results, and a JSON report.

## Payload safety

The embedded JSON recovery payload may only write/delete paths below `v032/` or `scripts/`. Absolute paths, traversal (`..`), protected `r4/`, `downloads/`, `update/`, and site files are rejected. The decoded payload is never executed as code; only declared file writes/deletes are applied.

## Versioning

The migration transforms:

- CMake project version: `0.3.2` -> `0.3.3`.
- `kVersion`: `0.3.3`.
- `kDisplayVersion`: `0.3.3 BETA R1`.
- `kVersionCode`: `3031`.
- `kRevision`: `1`.
- Windows resource file/product version: `0,3,3,1` and `0.3.3.1`.
- Test path namespace in prepared source: `tests/v033/`.

No final target file may advertise `0.3.2 BETA R1` as the product version.

## Build and verification

On Windows with Visual Studio 2022 build tools:

1. Prepare the R3 normalized source tree.
2. Overlay `v033/CMakeLists.txt`, `MainWindow.cpp`, `Version.h`, `version.rc.in`, `ui/`, `modules/`, and tests.
3. Configure CMake x64 Release with `BUILD_TESTING=ON`.
4. Build DPopCleaner, DPopUpdater, and tests.
5. Run CTest with `--output-on-failure`.
6. Verify the produced EXEs exist and compute SHA-256.
7. Emit `migration-report.json` containing baseline/recovery hashes, payload file count, version checks, test state, and artifact hashes.

The migration is not considered release-ready until a Windows Release build and all tests pass.

## Out of scope for this first 0.3.3 migration wave

- New features not present in 0.2.14 or 0.3.2.
- Replacing the faithful recovered shell with the current 0.3.2 shell.
- Publishing or changing the live website/update manifest before build verification.
- Altering R4 or existing public release artifacts.
