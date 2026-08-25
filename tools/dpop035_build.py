#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
from pathlib import Path
from typing import Sequence


def _powershell_executable() -> str:
    for candidate in ("pwsh", "powershell"):
        found = shutil.which(candidate)
        if found:
            return found
    raise RuntimeError("PowerShell is required to prepare DPopCleaner 0.3.5")


def _run(command: Sequence[str], *, cwd: Path | None = None) -> None:
    print("+", " ".join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), cwd=cwd, check=True)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _prepare_035_script_text() -> str:
    return r'''[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RepositoryRoot,
    [Parameter(Mandatory)][string]$V035Root,
    [Parameter(Mandatory)][string]$Destination
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$resolvedRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$resolvedV035 = [IO.Path]::GetFullPath($V035Root)
$resolvedDestination = [IO.Path]::GetFullPath($Destination)

& (Join-Path $resolvedRoot 'scripts/Prepare-R3Source.ps1') `
    -RepositoryRoot $resolvedRoot `
    -Destination $resolvedDestination

Copy-Item (Join-Path $resolvedV035 'CMakeLists.txt') (Join-Path $resolvedDestination 'CMakeLists.txt') -Force
Copy-Item (Join-Path $resolvedV035 'MainWindow.cpp') (Join-Path $resolvedDestination 'src/app/MainWindow.cpp') -Force
Copy-Item (Join-Path $resolvedV035 'Version.h') (Join-Path $resolvedDestination 'src/core/Version.h') -Force
Copy-Item (Join-Path $resolvedV035 'version.rc.in') (Join-Path $resolvedDestination 'resources/version.rc.in') -Force

foreach ($rootName in @('ui', 'core', 'modules', 'update')) {
    $sourceRoot = Join-Path $resolvedV035 $rootName
    if (Test-Path -LiteralPath $sourceRoot -PathType Container) {
        $destinationRoot = Join-Path $resolvedDestination ('src/' + $rootName)
        New-Item -ItemType Directory -Path $destinationRoot -Force | Out-Null
        Copy-Item (Join-Path $sourceRoot '*') $destinationRoot -Recurse -Force
    }
}

$testsRoot = Join-Path $resolvedV035 'tests'
if (Test-Path -LiteralPath $testsRoot -PathType Container) {
    $preparedTests = Join-Path $resolvedDestination 'tests/v035'
    New-Item -ItemType Directory -Path $preparedTests -Force | Out-Null
    Copy-Item (Join-Path $testsRoot '*') $preparedTests -Recurse -Force
}

Write-Host "Prepared DPopCleaner 0.3.5 source at $resolvedDestination"
'''


def run_windows_build(repository: Path, output: Path, workspace: Path) -> dict:
    if os.name != "nt":
        raise RuntimeError("DPopCleaner 0.3.5 --build requires Windows/MSVC")

    repository = repository.resolve()
    output = output.resolve()
    workspace = workspace.resolve()
    v035_root = output / "source-overlay" / "v035"
    if not v035_root.is_dir():
        raise RuntimeError(f"0.3.5 source overlay missing: {v035_root}")

    prepared = workspace / "prepared-035-src"
    build_root = workspace / "build-035"
    if prepared.exists():
        shutil.rmtree(prepared)
    if build_root.exists():
        shutil.rmtree(build_root)

    script = workspace / "Prepare-035Source.ps1"
    script.write_text(_prepare_035_script_text(), encoding="utf-8", newline="\n")
    powershell = _powershell_executable()
    _run([
        powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(script),
        "-RepositoryRoot", str(repository), "-V035Root", str(v035_root),
        "-Destination", str(prepared),
    ])
    _run([
        "cmake", "-S", str(prepared), "-B", str(build_root),
        "-G", "Visual Studio 17 2022", "-A", "x64", "-DBUILD_TESTING=ON",
    ])
    _run(["cmake", "--build", str(build_root), "--config", "Release", "--parallel"])
    _run(["ctest", "--test-dir", str(build_root), "-C", "Release", "--output-on-failure"])

    artifact_root = output / "artifacts"
    artifact_root.mkdir(parents=True, exist_ok=True)
    result: dict[str, object] = {"completed": True, "tests_passed": True, "artifacts": {}}
    for name in ("DPopCleaner.exe", "DPopUpdater.exe"):
        source = build_root / "bin" / "Release" / name
        if not source.is_file():
            raise RuntimeError(f"expected 0.3.5 build artifact missing: {source}")
        destination = artifact_root / name
        shutil.copy2(source, destination)
        result["artifacts"][name] = {
            "bytes": destination.stat().st_size,
            "sha256": sha256_file(destination),
        }
    return result
