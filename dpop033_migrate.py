#!/usr/bin/env python3
"""DPopCleaner 0.3.3 reverse-migration orchestrator.

This tool does not patch the historical 0.2.14 executable.  It uses the
repository's embedded faithful 0.2.14 recovery payload, applies it to an
isolated worktree, clones the recovered v032 overlay to v033, changes only the
target version identity, and optionally builds/tests the normalized source tree
on Windows.
"""
from __future__ import annotations

import argparse
import base64
import binascii
import gzip
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path, PurePosixPath
from typing import Iterable, Sequence

TARGET_VERSION = "0.3.3"
TARGET_DISPLAY_VERSION = "0.3.3 BETA R1"
TARGET_VERSION_CODE = "3031"
TARGET_REVISION = "1"
TARGET_RESOURCE_TUPLE = "0,3,3,1"
TARGET_RESOURCE_VERSION = "0.3.3.1"
DONOR_VERSION = "0.3.2"
DONOR_DISPLAY_VERSION = "0.3.2 BETA R1"
DONOR_VERSION_CODE = "3021"
EXPECTED_0214_SHA256 = "7d5e0a510189db31ef7ee1aca72dc182332a8020d994c81be40a519c5960515c"
RECOVERY_WORKFLOW = Path(
    ".github/workflows/DPopCleaner_0.3.2_FAITHFUL_0214_RECOVERY_ONE_CLICK.yml"
)
RECOVERY_POLICY_SCRIPTS = (
    Path("tests/CleanReleasePolicy.Tests.ps1"),
    Path("tests/R3ReleasePolicy.Tests.ps1"),
    Path("tests/R3WorkflowPolicy.Tests.ps1"),
    Path("v032/tests/Ui032Policy.Tests.ps1"),
    Path("v032/tests/FaithfulRecoveryPolicy.Tests.ps1"),
)


def _leading_spaces(line: str) -> int:
    return len(line) - len(line.lstrip(" "))


def extract_embedded_payload(workflow_text: str) -> dict:
    """Decode the gzip+base64 JSON recovery payload embedded in the workflow.

    The parser intentionally understands only the YAML literal block used by
    this repository.  It does not evaluate YAML or any PowerShell code.
    """
    lines = workflow_text.splitlines()
    marker_index = None
    marker_indent = None
    marker_re = re.compile(r"^(\s*)DPOP_PAYLOAD_B64_GZIP:\s*\|\s*$")
    for index, line in enumerate(lines):
        match = marker_re.match(line)
        if match:
            marker_index = index
            marker_indent = len(match.group(1))
            break
    if marker_index is None or marker_indent is None:
        raise ValueError("DPOP_PAYLOAD_B64_GZIP literal block not found")

    encoded_parts: list[str] = []
    for line in lines[marker_index + 1 :]:
        if not line.strip():
            if encoded_parts:
                continue
            continue
        if _leading_spaces(line) <= marker_indent:
            break
        token = line.strip()
        # Base64 payload lines contain no YAML syntax.  Fail here instead of
        # accidentally consuming the following run block.
        if not re.fullmatch(r"[A-Za-z0-9+/=]+", token):
            if encoded_parts:
                break
            raise ValueError("DPOP_PAYLOAD_B64_GZIP contains a non-base64 line")
        encoded_parts.append(token)

    if not encoded_parts:
        raise ValueError("DPOP_PAYLOAD_B64_GZIP literal block is empty")

    encoded = "".join(encoded_parts)
    try:
        compressed = base64.b64decode(encoded, validate=True)
    except (binascii.Error, ValueError) as exc:
        raise ValueError("DPOP_PAYLOAD_B64_GZIP is not valid base64") from exc

    try:
        raw = gzip.decompress(compressed)
    except (gzip.BadGzipFile, EOFError, OSError) as exc:
        raise ValueError("DPOP_PAYLOAD_B64_GZIP is not valid gzip data") from exc

    try:
        payload = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValueError("DPOP_PAYLOAD_B64_GZIP does not contain UTF-8 JSON") from exc

    if not isinstance(payload, dict) or not isinstance(payload.get("files"), list):
        raise ValueError("Recovery payload must be an object with a files array")
    if "delete" in payload and not isinstance(payload["delete"], list):
        raise ValueError("Recovery payload delete field must be an array")
    return payload


def validate_payload_path(path: str) -> PurePosixPath:
    if not isinstance(path, str):
        raise ValueError("payload path must be text")
    normalized = path.replace("\\", "/").strip()
    if not normalized:
        raise ValueError("payload path is empty")
    if normalized.startswith("/") or re.match(r"^[A-Za-z]:/", normalized):
        raise ValueError(f"absolute payload path is forbidden: {path}")

    pure = PurePosixPath(normalized)
    if any(part in ("", ".", "..") for part in pure.parts):
        raise ValueError(f"unsafe payload path: {path}")
    if not pure.parts or pure.parts[0] not in {"v032", "scripts"}:
        raise ValueError(f"payload path outside v032/scripts: {path}")

    # A normalized path beginning in v032/scripts is safe only if the original
    # spelling itself did not contain traversal syntax.
    if "../" in normalized or "/.." in normalized or normalized.endswith(".."):
        raise ValueError(f"unsafe payload path: {path}")
    return pure


def _decode_file_content(value: object, path: str) -> bytes:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"missing content_base64 for {path}")
    try:
        return base64.b64decode(value, validate=True)
    except (binascii.Error, ValueError) as exc:
        raise ValueError(f"invalid content_base64 for {path}") from exc


def apply_payload(repo_root: Path, payload: dict) -> tuple[int, int]:
    """Apply validated file changes under repo_root; return writes, deletes."""
    root = repo_root.resolve()
    files = payload.get("files")
    deletes = payload.get("delete", [])
    if not isinstance(files, list) or not isinstance(deletes, list):
        raise ValueError("invalid recovery payload collections")

    prepared_writes: list[tuple[Path, bytes]] = []
    prepared_deletes: list[Path] = []

    for entry in files:
        if not isinstance(entry, dict):
            raise ValueError("payload files entry must be an object")
        pure = validate_payload_path(entry.get("path", ""))
        data = _decode_file_content(entry.get("content_base64"), str(pure))
        destination = root.joinpath(*pure.parts)
        prepared_writes.append((destination, data))

    for entry in deletes:
        pure = validate_payload_path(entry)
        prepared_deletes.append(root.joinpath(*pure.parts))

    # Validation is complete before the first mutation.
    writes = 0
    for destination, data in prepared_writes:
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
        writes += 1

    delete_count = 0
    for destination in prepared_deletes:
        if destination.is_dir():
            shutil.rmtree(destination)
            delete_count += 1
        elif destination.exists():
            destination.unlink()
            delete_count += 1

    return writes, delete_count


def _guarded_replace(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise ValueError(f"expected marker missing in {label}: {old}")
    return text.replace(old, new)


def transform_v033_overlay(v033_root: Path) -> dict[str, str]:
    """Change recovered v032 identity to 0.3.3, failing closed on drift."""
    cmake_path = v033_root / "CMakeLists.txt"
    header_path = v033_root / "Version.h"
    resource_path = v033_root / "version.rc.in"
    required = (cmake_path, header_path, resource_path)
    for path in required:
        if not path.is_file():
            raise ValueError(f"required v033 file missing: {path.name}")

    cmake = cmake_path.read_text(encoding="utf-8")
    header = header_path.read_text(encoding="utf-8")
    resource = resource_path.read_text(encoding="utf-8")

    cmake = _guarded_replace(
        cmake,
        "project(DPopCleaner VERSION 0.3.2",
        "project(DPopCleaner VERSION 0.3.3",
        "CMakeLists.txt",
    )
    if "tests/v032/" in cmake:
        cmake = cmake.replace("tests/v032/", "tests/v033/")

    header = _guarded_replace(
        header,
        'kVersion[] = L"0.3.2"',
        'kVersion[] = L"0.3.3"',
        "Version.h",
    )
    header = _guarded_replace(
        header,
        'kDisplayVersion[] = L"0.3.2 BETA R1"',
        'kDisplayVersion[] = L"0.3.3 BETA R1"',
        "Version.h",
    )
    header = _guarded_replace(
        header,
        "kVersionCode = 3021",
        "kVersionCode = 3031",
        "Version.h",
    )
    # R1 remains revision 1.  We still require the marker so drift is visible.
    if "kRevision = 1" not in header:
        raise ValueError("expected marker missing in Version.h: kRevision = 1")

    resource = _guarded_replace(
        resource, "FILEVERSION 0,3,2,1", "FILEVERSION 0,3,3,1", "version.rc.in"
    )
    resource = _guarded_replace(
        resource,
        "PRODUCTVERSION 0,3,2,1",
        "PRODUCTVERSION 0,3,3,1",
        "version.rc.in",
    )
    resource = _guarded_replace(
        resource,
        '"0.3.2.1\\0"',
        '"0.3.3.1\\0"',
        "version.rc.in",
    )
    resource = _guarded_replace(
        resource,
        '"0.3.2 BETA R1\\0"',
        '"0.3.3 BETA R1\\0"',
        "version.rc.in",
    )

    # Collect user-visible product identity rewrites in source and C++ tests.
    # Internal symbols such as DPopCleaner032ShellWindow intentionally stay
    # unchanged because they are implementation identifiers, not UI strings.
    source_updates: dict[Path, str] = {}
    identity_pairs = (
        ("DPopCleaner 0.3.2 BETA R1", "DPopCleaner 0.3.3 BETA R1"),
        ("v0.3.2 BETA", "v0.3.3 BETA"),
        ("DPopCleaner 0.3.2", "DPopCleaner 0.3.3"),
    )
    identity_files_changed = 0
    for pattern in ("*.cpp", "*.h"):
        for path in v033_root.rglob(pattern):
            if path == header_path:
                continue
            original = path.read_text(encoding="utf-8")
            updated = original
            for old, new in identity_pairs:
                updated = updated.replace(old, new)
            if updated != original:
                source_updates[path] = updated
                identity_files_changed += 1

    # Only write after every expected marker has been validated.
    cmake_path.write_text(cmake, encoding="utf-8", newline="\n")
    header_path.write_text(header, encoding="utf-8", newline="\n")
    resource_path.write_text(resource, encoding="utf-8", newline="\n")
    for path, updated in source_updates.items():
        path.write_text(updated, encoding="utf-8", newline="\n")

    return {
        "version": TARGET_VERSION,
        "display_version": TARGET_DISPLAY_VERSION,
        "version_code": TARGET_VERSION_CODE,
        "revision": TARGET_REVISION,
        "resource_version": TARGET_RESOURCE_VERSION,
        "identity_source_files_changed": str(identity_files_changed),
    }


def validate_workspace(repository: Path, workspace: Path) -> None:
    repo = repository.resolve()
    work = workspace.resolve()
    if work == repo or repo in work.parents:
        raise ValueError("workspace must be outside the repository checkout")


def recovery_policy_commands(stage_root: Path, powershell: str) -> list[list[str]]:
    commands: list[list[str]] = []
    for relative in RECOVERY_POLICY_SCRIPTS:
        path = stage_root / relative
        if not path.is_file():
            raise RuntimeError(f"required recovery policy test missing: {relative.as_posix()}")
        commands.append(
            [
                powershell,
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-File",
                str(path),
            ]
        )
    return commands


def windows_build_commands(source_root: Path, build_root: Path) -> list[list[str]]:
    return [
        [
            "cmake",
            "-S",
            str(source_root),
            "-B",
            str(build_root),
            "-G",
            "Visual Studio 17 2022",
            "-A",
            "x64",
            "-DBUILD_TESTING=ON",
        ],
        ["cmake", "--build", str(build_root), "--config", "Release", "--parallel"],
        ["ctest", "--test-dir", str(build_root), "-C", "Release", "--output-on-failure"],
    ]


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _run(command: Sequence[str], *, cwd: Path | None = None) -> None:
    print("+", " ".join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), cwd=cwd, check=True)


def _capture(command: Sequence[str], *, cwd: Path | None = None) -> str:
    result = subprocess.run(
        list(map(str, command)),
        cwd=cwd,
        check=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    return result.stdout.strip()


def _powershell_executable() -> str:
    for candidate in ("pwsh", "powershell"):
        found = shutil.which(candidate)
        if found:
            return found
    raise RuntimeError("PowerShell (pwsh or powershell) is required to prepare the source tree")


def _prepare_033_script_text() -> str:
    return r'''[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RepositoryRoot,
    [Parameter(Mandatory)][string]$Destination
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$resolvedRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$resolvedDestination = [IO.Path]::GetFullPath($Destination)

& (Join-Path $resolvedRoot 'scripts/Prepare-R3Source.ps1') `
    -RepositoryRoot $resolvedRoot `
    -Destination $resolvedDestination

Copy-Item (Join-Path $resolvedRoot 'v033/CMakeLists.txt') (Join-Path $resolvedDestination 'CMakeLists.txt') -Force
Copy-Item (Join-Path $resolvedRoot 'v033/MainWindow.cpp') (Join-Path $resolvedDestination 'src/app/MainWindow.cpp') -Force
Copy-Item (Join-Path $resolvedRoot 'v033/Version.h') (Join-Path $resolvedDestination 'src/core/Version.h') -Force
Copy-Item (Join-Path $resolvedRoot 'v033/version.rc.in') (Join-Path $resolvedDestination 'resources/version.rc.in') -Force

$uiRoot = Join-Path $resolvedRoot 'v033/ui'
if (Test-Path -LiteralPath $uiRoot -PathType Container) {
    Copy-Item $uiRoot (Join-Path $resolvedDestination 'src/ui') -Recurse -Force
}

$moduleRoot = Join-Path $resolvedRoot 'v033/modules'
if (Test-Path -LiteralPath $moduleRoot -PathType Container) {
    New-Item -ItemType Directory -Path (Join-Path $resolvedDestination 'src/modules') -Force | Out-Null
    Copy-Item (Join-Path $moduleRoot '*') (Join-Path $resolvedDestination 'src/modules') -Recurse -Force
}

$testsRoot = Join-Path $resolvedRoot 'v033/tests'
if (Test-Path -LiteralPath $testsRoot -PathType Container) {
    $preparedTests = Join-Path $resolvedDestination 'tests/v033'
    New-Item -ItemType Directory -Path $preparedTests -Force | Out-Null
    Copy-Item (Join-Path $testsRoot '*') $preparedTests -Recurse -Force
}

$overlayRoot = Join-Path $resolvedRoot 'v033'
$inventory = @(
    Get-ChildItem $overlayRoot -File -Recurse |
        Sort-Object FullName |
        ForEach-Object {
            [pscustomobject][ordered]@{
                path = $_.FullName.Substring($overlayRoot.Length + 1).Replace('\', '/')
                sha256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
            }
        }
)
$inventory | ConvertTo-Json -Depth 4 |
    Set-Content (Join-Path $resolvedDestination 'v033-overlay-inventory.json') -Encoding utf8
Write-Host "Prepared DPopCleaner 0.3.3 reverse-migration source at $resolvedDestination"
'''


def _assert_repository_inputs(repo_root: Path) -> None:
    required = [
        RECOVERY_WORKFLOW,
        Path("scripts/Prepare-R3Source.ps1"),
        Path("scripts/R3ReleasePolicy.psm1"),
        Path("v032/CMakeLists.txt"),
        Path("v032/Version.h"),
        Path("v032/version.rc.in"),
    ]
    missing = [str(path) for path in required if not (repo_root / path).is_file()]
    if missing:
        raise RuntimeError("required repository inputs are missing: " + ", ".join(missing))


def _copy_tree_clean(source: Path, destination: Path) -> None:
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(source, destination)


def _verify_v033_identity(v033_root: Path) -> None:
    cmake = (v033_root / "CMakeLists.txt").read_text(encoding="utf-8")
    header = (v033_root / "Version.h").read_text(encoding="utf-8")
    resource = (v033_root / "version.rc.in").read_text(encoding="utf-8")
    required = [
        ("CMakeLists.txt", cmake, "project(DPopCleaner VERSION 0.3.3"),
        ("Version.h", header, 'kVersion[] = L"0.3.3"'),
        ("Version.h", header, 'kDisplayVersion[] = L"0.3.3 BETA R1"'),
        ("Version.h", header, "kVersionCode = 3031"),
        ("version.rc.in", resource, "FILEVERSION 0,3,3,1"),
        ("version.rc.in", resource, '"0.3.3 BETA R1\\0"'),
    ]
    for label, text, marker in required:
        if marker not in text:
            raise RuntimeError(f"0.3.3 identity verification failed in {label}: {marker}")
    if "tests/v032/" in cmake:
        raise RuntimeError("CMakeLists.txt still references tests/v032/")

    forbidden_ui_markers = (
        "DPopCleaner 0.3.2 BETA R1",
        "v0.3.2 BETA",
        "DPopCleaner 0.3.2",
    )
    stale: list[str] = []
    for pattern in ("*.cpp", "*.h"):
        for path in v033_root.rglob(pattern):
            text = path.read_text(encoding="utf-8")
            if any(marker in text for marker in forbidden_ui_markers):
                stale.append(str(path.relative_to(v033_root)))
    if stale:
        raise RuntimeError(
            "stale 0.3.2 user-visible identity remains in v033: " + ", ".join(sorted(stale))
        )


def _artifact_candidates(build_root: Path) -> tuple[Path, Path]:
    return (
        build_root / "bin" / "Release" / "DPopCleaner.exe",
        build_root / "bin" / "Release" / "DPopUpdater.exe",
    )


def migrate(
    repository: Path,
    output: Path,
    workspace: Path,
    *,
    build: bool,
    keep_worktree: bool,
) -> dict:
    repository = repository.resolve()
    output = output.resolve()
    workspace = workspace.resolve()
    validate_workspace(repository, workspace)
    _assert_repository_inputs(repository)

    output.mkdir(parents=True, exist_ok=True)
    workspace.mkdir(parents=True, exist_ok=True)
    stage = workspace / "repo-stage"
    prepared = workspace / "prepared-src"
    build_root = workspace / "build"

    if stage.exists():
        raise RuntimeError(f"staging worktree already exists: {stage}")

    head = _capture(["git", "rev-parse", "HEAD"], cwd=repository)
    workflow_path = repository / RECOVERY_WORKFLOW
    workflow_sha = sha256_file(workflow_path)

    baseline_path = repository / "downloads" / "DPopCleaner_0.2.14_BETA.exe"
    baseline_sha: str | None = None
    if baseline_path.is_file():
        baseline_sha = sha256_file(baseline_path)
        if baseline_sha.lower() != EXPECTED_0214_SHA256:
            raise RuntimeError(
                "0.2.14 standalone baseline hash mismatch: "
                f"expected {EXPECTED_0214_SHA256}, got {baseline_sha}"
            )

    report: dict = {
        "target": TARGET_DISPLAY_VERSION,
        "target_version": TARGET_VERSION,
        "source_commit": head,
        "recovery_workflow_sha256": workflow_sha,
        "baseline_0214_sha256": baseline_sha,
        "baseline_0214_expected_sha256": EXPECTED_0214_SHA256,
        "payload": {},
        "version": {},
        "build": {
            "requested": build,
            "recovery_policy_tests_passed": False,
            "completed": False,
            "tests_passed": False,
        },
        "artifacts": {},
    }

    worktree_added = False
    try:
        _run(["git", "worktree", "add", "--detach", str(stage), head], cwd=repository)
        worktree_added = True
        _assert_repository_inputs(stage)

        payload = extract_embedded_payload((stage / RECOVERY_WORKFLOW).read_text(encoding="utf-8"))
        writes, deletes = apply_payload(stage, payload)
        report["payload"] = {
            "declared_files": len(payload.get("files", [])),
            "declared_deletes": len(payload.get("delete", [])),
            "writes": writes,
            "deletes_applied": deletes,
        }

        if build:
            if os.name != "nt":
                raise RuntimeError("--build requires Windows/MSVC; use --no-build on non-Windows hosts")
            powershell = _powershell_executable()
            for command in recovery_policy_commands(stage, powershell):
                _run(command, cwd=stage)
            report["build"]["recovery_policy_tests_passed"] = True

        donor = stage / "v032"
        target = stage / "v033"
        if not donor.is_dir():
            raise RuntimeError("recovered v032 donor directory is missing")
        _copy_tree_clean(donor, target)
        report["version"] = transform_v033_overlay(target)
        _verify_v033_identity(target)

        prepare_script = stage / "scripts" / "Prepare-033Source.ps1"
        prepare_script.write_text(_prepare_033_script_text(), encoding="utf-8", newline="\n")

        export_root = output / "source-overlay"
        if export_root.exists():
            shutil.rmtree(export_root)
        (export_root / "scripts").mkdir(parents=True, exist_ok=True)
        shutil.copytree(target, export_root / "v033")
        shutil.copy2(prepare_script, export_root / "scripts" / "Prepare-033Source.ps1")

        if build:
            if prepared.exists():
                shutil.rmtree(prepared)
            _run(
                [
                    powershell,
                    "-NoProfile",
                    "-ExecutionPolicy",
                    "Bypass",
                    "-File",
                    str(prepare_script),
                    "-RepositoryRoot",
                    str(stage),
                    "-Destination",
                    str(prepared),
                ]
            )
            for command in windows_build_commands(prepared, build_root):
                _run(command)
            report["build"]["completed"] = True
            report["build"]["tests_passed"] = True

            artifact_root = output / "artifacts"
            artifact_root.mkdir(parents=True, exist_ok=True)
            app_exe, updater_exe = _artifact_candidates(build_root)
            for candidate in (app_exe, updater_exe):
                if not candidate.is_file():
                    raise RuntimeError(f"expected build artifact missing: {candidate}")
                destination = artifact_root / candidate.name
                shutil.copy2(candidate, destination)
                report["artifacts"][candidate.name] = {
                    "sha256": sha256_file(destination),
                    "bytes": destination.stat().st_size,
                }

        report_path = output / "migration-report.json"
        report_path.write_text(
            json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
        )
        return report
    finally:
        if worktree_added and not keep_worktree:
            subprocess.run(
                ["git", "worktree", "remove", "--force", str(stage)],
                cwd=repository,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )
        if not keep_worktree:
            subprocess.run(
                ["git", "worktree", "prune"],
                cwd=repository,
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
            )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Build a DPopCleaner 0.3.3 overlay from the faithful 0.2.14 recovery + 0.3.2 core."
    )
    parser.add_argument("--repository", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--workspace",
        type=Path,
        default=Path(tempfile.gettempdir()) / "dpopcleaner-0.3.3-migration",
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--build", action="store_true", help="prepare, build and run CTest on Windows")
    mode.add_argument("--no-build", action="store_true", help="export v033 overlay without compiling")
    parser.add_argument("--keep-worktree", action="store_true")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    build_requested = bool(args.build)
    if not args.build and not args.no_build:
        build_requested = os.name == "nt"
    try:
        report = migrate(
            args.repository,
            args.output,
            args.workspace,
            build=build_requested,
            keep_worktree=args.keep_worktree,
        )
    except Exception as exc:  # CLI boundary: print a concise actionable failure.
        print(f"DPopCleaner 0.3.3 migration FAILED: {exc}", file=sys.stderr)
        return 1

    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
