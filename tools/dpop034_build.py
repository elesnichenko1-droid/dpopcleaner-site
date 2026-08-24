#!/usr/bin/env python3
from __future__ import annotations

import hashlib
import os
import shutil
import subprocess
from pathlib import Path
from typing import Sequence


def _insert_ctest_block(text: str, marker: str, block: str) -> str:
    if marker in text:
        return text
    closing = text.rfind('endif()')
    if closing < 0:
        raise ValueError('CMake donor drifted: BUILD_TESTING endif() missing')
    return text[:closing] + block + text[closing:]


def transform_cmake_for_page_layout(text: str) -> str:
    """Register the 0.3.4 shared page layout source and its CTest target."""
    if 'src/ui/PageLayout.cpp' not in text:
        anchor = '  src/ui/Layout.cpp\n'
        if anchor not in text:
            raise ValueError('CMake donor drifted: src/ui/Layout.cpp anchor missing')
        text = text.replace(anchor, anchor + '  src/ui/PageLayout.cpp\n', 1)
    block = '''\n  add_executable(PageLayoutTests tests/v034/PageLayoutTests.cpp src/ui/PageLayout.cpp)\n  target_include_directories(PageLayoutTests PRIVATE src)\n  target_compile_definitions(PageLayoutTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)\n  if(MSVC)\n    target_compile_options(PageLayoutTests PRIVATE /W4 /permissive- /utf-8)\n  endif()\n  add_test(NAME PageLayoutTests COMMAND PageLayoutTests)\n'''
    return _insert_ctest_block(text, 'add_executable(PageLayoutTests', block)


def transform_cmake_for_zapret_center(text: str) -> str:
    """Compile safe strategy discovery into the app and execute its behavior test."""
    if 'src/modules/ZapretCenterModel.cpp' not in text:
        anchor = '  src/modules/ZapretManager.cpp\n'
        if anchor not in text:
            raise ValueError('CMake donor drifted: src/modules/ZapretManager.cpp anchor missing')
        text = text.replace(anchor, anchor + '  src/modules/ZapretCenterModel.cpp\n', 1)
    block = '''\n  add_executable(ZapretCenterModelTests tests/v034/ZapretCenterModelTests.cpp src/modules/ZapretCenterModel.cpp)\n  target_include_directories(ZapretCenterModelTests PRIVATE src)\n  target_compile_definitions(ZapretCenterModelTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)\n  if(MSVC)\n    target_compile_options(ZapretCenterModelTests PRIVATE /W4 /permissive- /utf-8)\n  endif()\n  add_test(NAME ZapretCenterModelTests COMMAND ZapretCenterModelTests)\n'''
    return _insert_ctest_block(text, 'add_executable(ZapretCenterModelTests', block)


def transform_cmake_for_zapret_page_layout(text: str) -> str:
    """Compile the non-overlapping Zapret page geometry and its regression test."""
    if 'src/ui/pages/ZapretPageLayout.cpp' not in text:
        anchor = '  src/ui/pages/ZapretPage.cpp\n'
        if anchor not in text:
            raise ValueError('CMake donor drifted: src/ui/pages/ZapretPage.cpp anchor missing')
        text = text.replace(anchor, anchor + '  src/ui/pages/ZapretPageLayout.cpp\n', 1)
    block = '''\n  add_executable(ZapretPageLayoutTests tests/v034/ZapretPageLayoutTests.cpp src/ui/pages/ZapretPageLayout.cpp)\n  target_include_directories(ZapretPageLayoutTests PRIVATE src)\n  target_compile_definitions(ZapretPageLayoutTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)\n  if(MSVC)\n    target_compile_options(ZapretPageLayoutTests PRIVATE /W4 /permissive- /utf-8)\n  endif()\n  add_test(NAME ZapretPageLayoutTests COMMAND ZapretPageLayoutTests)\n'''
    return _insert_ctest_block(text, 'add_executable(ZapretPageLayoutTests', block)


def _prepare_034_script_text() -> str:
    return r'''[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RepositoryRoot,
    [Parameter(Mandatory)][string]$V034Root,
    [Parameter(Mandatory)][string]$Destination
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$resolvedRoot = [IO.Path]::GetFullPath($RepositoryRoot)
$resolvedV034 = [IO.Path]::GetFullPath($V034Root)
$resolvedDestination = [IO.Path]::GetFullPath($Destination)

& (Join-Path $resolvedRoot 'scripts/Prepare-R3Source.ps1') `
    -RepositoryRoot $resolvedRoot `
    -Destination $resolvedDestination

Copy-Item (Join-Path $resolvedV034 'CMakeLists.txt') (Join-Path $resolvedDestination 'CMakeLists.txt') -Force
Copy-Item (Join-Path $resolvedV034 'MainWindow.cpp') (Join-Path $resolvedDestination 'src/app/MainWindow.cpp') -Force
Copy-Item (Join-Path $resolvedV034 'Version.h') (Join-Path $resolvedDestination 'src/core/Version.h') -Force
Copy-Item (Join-Path $resolvedV034 'version.rc.in') (Join-Path $resolvedDestination 'resources/version.rc.in') -Force

$uiRoot = Join-Path $resolvedV034 'ui'
if (Test-Path -LiteralPath $uiRoot -PathType Container) {
    New-Item -ItemType Directory -Path (Join-Path $resolvedDestination 'src/ui') -Force | Out-Null
    Copy-Item (Join-Path $uiRoot '*') (Join-Path $resolvedDestination 'src/ui') -Recurse -Force
}

$moduleRoot = Join-Path $resolvedV034 'modules'
if (Test-Path -LiteralPath $moduleRoot -PathType Container) {
    New-Item -ItemType Directory -Path (Join-Path $resolvedDestination 'src/modules') -Force | Out-Null
    Copy-Item (Join-Path $moduleRoot '*') (Join-Path $resolvedDestination 'src/modules') -Recurse -Force
}

$testsRoot = Join-Path $resolvedV034 'tests'
if (Test-Path -LiteralPath $testsRoot -PathType Container) {
    $preparedTests = Join-Path $resolvedDestination 'tests/v034'
    New-Item -ItemType Directory -Path $preparedTests -Force | Out-Null
    Copy-Item (Join-Path $testsRoot '*') $preparedTests -Recurse -Force
}

Write-Host "Prepared DPopCleaner 0.3.4 source at $resolvedDestination"
'''


def _powershell_executable() -> str:
    for candidate in ('pwsh', 'powershell'):
        found = shutil.which(candidate)
        if found:
            return found
    raise RuntimeError('PowerShell is required to prepare DPopCleaner 0.3.4')


def _run(command: Sequence[str], *, cwd: Path | None = None) -> None:
    print('+', ' '.join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), cwd=cwd, check=True)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open('rb') as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def run_windows_build(repository: Path, output: Path, workspace: Path) -> dict:
    if os.name != 'nt':
        raise RuntimeError('DPopCleaner 0.3.4 --build requires Windows/MSVC')

    repository = repository.resolve()
    output = output.resolve()
    workspace = workspace.resolve()
    v034_root = output / 'source-overlay' / 'v034'
    if not v034_root.is_dir():
        raise RuntimeError(f'0.3.4 source overlay missing: {v034_root}')

    prepared = workspace / 'prepared-034-src'
    build_root = workspace / 'build-034'
    if prepared.exists():
        shutil.rmtree(prepared)
    if build_root.exists():
        shutil.rmtree(build_root)

    script = workspace / 'Prepare-034Source.ps1'
    script.write_text(_prepare_034_script_text(), encoding='utf-8', newline='\n')
    powershell = _powershell_executable()
    _run([
        powershell, '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', str(script),
        '-RepositoryRoot', str(repository), '-V034Root', str(v034_root),
        '-Destination', str(prepared),
    ])
    _run([
        'cmake', '-S', str(prepared), '-B', str(build_root),
        '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DBUILD_TESTING=ON',
    ])
    _run(['cmake', '--build', str(build_root), '--config', 'Release', '--parallel'])
    _run(['ctest', '--test-dir', str(build_root), '-C', 'Release', '--output-on-failure'])

    artifact_root = output / 'artifacts'
    artifact_root.mkdir(parents=True, exist_ok=True)
    result: dict[str, object] = {'completed': True, 'tests_passed': True, 'artifacts': {}}
    for name in ('DPopCleaner.exe', 'DPopUpdater.exe'):
        source = build_root / 'bin' / 'Release' / name
        if not source.is_file():
            raise RuntimeError(f'expected 0.3.4 build artifact missing: {source}')
        destination = artifact_root / name
        shutil.copy2(source, destination)
        result['artifacts'][name] = {
            'bytes': destination.stat().st_size,
            'sha256': sha256_file(destination),
        }
    return result
