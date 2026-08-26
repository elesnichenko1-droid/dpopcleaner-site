# DPopCleaner 0.4.17 Implementation Plan Index

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement these plans in dependency order. Steps inside each plan use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Execute the approved 0.4.17 design without using reconstructed C++ DPopCleaner code.

**Architecture:** The preserved original 0.2.14 EXE remains byte-for-byte untouched. New code is limited to isolated C#/.NET Framework 4.8 companion modules and release tooling under the 0.4.17 line. The package/site version is 0.4.17; any `0.4.17.0` Windows file-version claim applies to the new installer/companion binaries only, never to the preserved original core.

**Tech Stack:** Original native 0.2.14 EXE + C#/.NET Framework 4.8 companions + Python/PowerShell contracts + Inno Setup + GitHub Actions + static HTML/CSS/JS site.

**Spec:** `docs/superpowers/specs/2026-08-25-dpopcleaner-0.4.17-original-0214-design.md`

## Global Constraints

- Do not compile, copy, stage, or use reconstructed C++ DPopCleaner application sources as a runtime base or donor.
- Do not patch the preserved core's executable logic or PE identity in the first 0.4.17 scope.
- The checked-in original core must remain Git blob `efd0eff1f4962319282363fa85595c25e0cebe11`, size `389632` bytes.
- New companion modules use C#/.NET Framework 4.8 and external `Languages/*.json` only.
- `Documentation` is writable runtime data. In an `{app}` Program Files installation, the installer grants modify permission only to `{app}\Documentation`; executable/module/language/shell directories remain non-user-writable.
- `Documentation\History`, `Documentation\Backups`, and `Documentation\Logs` survive ordinary uninstall/upgrade.
- Release identity is `DPopCleaner 0.4.17`, installer `DPopCleaner_Setup_0.4.17.exe`, tag `v0.4.17`.

## Dependency Order

1. `2026-08-25-dpopcleaner-0.4.17-foundation.md`
2. `2026-08-25-dpopcleaner-0.4.17-disk-analyzer.md`
3. `2026-08-25-dpopcleaner-0.4.17-restore-center.md`
4. `2026-08-25-dpopcleaner-0.4.17-site-release.md`

Each earlier plan must be GREEN before the next one begins.

## Test Fixture Files Required by the Plans

These are test-only helpers and must never be copied to the runtime payload:

```text
v0417/tests/DPop.Common.Tests/TestFixtures/TestLanguageFixture.cs
v0417/tests/DPop.Common.Tests/TestFixtures/HistoryFixture.cs
v0417/tests/DiskAnalyzer.Tests/TestFixtures/DiskFixture.cs
v0417/tests/DiskAnalyzer.Tests/TestFixtures/FakeAllocationProvider.cs
v0417/tests/DiskAnalyzer.Tests/TestFixtures/FakeFileSystemView.cs
v0417/tests/DiskAnalyzer.Tests/TestFixtures/DiskTreeFixture.cs
```

`DiskScanner` therefore has two injectable dependencies:

```csharp
public interface IAllocationSizeProvider
{
    long? GetAllocatedBytes(string path);
}

public interface IFileSystemView
{
    IEnumerable<FileSystemEntry> Enumerate(string directory);
}
```

Production uses `WindowsAllocationSizeProvider` + `PhysicalFileSystemView`; tests use fakes. `FileSystemEntry` contains `FullPath`, `Name`, `IsDirectory`, `IsReparsePoint`, `Length`, and `ModifiedUtc`. This is the seam used to prove reparse points are not followed without requiring privileged symlink creation in CI.

## Localization Visual Gate

Before packaging, both new EXEs must support deterministic diagnostic switches:

```text
--lang ru
--lang en
--layout-report <json-path>
```

`--layout-report` starts the normal form, performs layout, and writes visible textual controls as:

```json
{
  "language": "ru",
  "controls": [
    {"name":"scanButton","text":"Сканировать","x":10,"y":10,"width":120,"height":30}
  ]
}
```

`tools/dpop0417_language_smoke.ps1` runs both Disk Analyzer and Restore Center in Russian and English, then fails when any two visible textual controls on the same container have intersecting rectangles with positive area. It also requires the language field to match the requested pack. This is the release gate for the user's requirement that translated texts do not overlap.

## Site Screenshot Rule

The site hero uses a screenshot captured from the real preserved `DPopCleaner.exe` during Windows candidate CI. Companion screenshots may appear only in their own feature sections. No generated/mock application screenshot can replace the real-core hero image.

## Self-Review Result

- Spec coverage: original core, Languages, Shell, Documentation/rollback, Disk Analyzer, Restore Center, installer, site, legacy publisher isolation, live SHA verification are all assigned to plans.
- Version ambiguity resolved: 0.4.17 package/installer/companions are new; original 0.2.14 core bytes and embedded identity remain untouched.
- Runtime-write ambiguity resolved: only Documentation receives user modify permission under Program Files.
- Localization overlap gate is explicit and applies to both new modules.
- Test seams for reparse points and allocation-size failure are explicit; no privileged junction creation is required.
