#!/usr/bin/env python3
"""DPopCleaner 0.3.4 entrypoint over the verified repaired 0.3.3 donor."""
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path
from typing import Sequence

_CORE_PATH = Path(__file__).with_name('dpop034_core.py')
_SPEC = importlib.util.spec_from_file_location('dpop034_core', _CORE_PATH)
if _SPEC is None or _SPEC.loader is None:
    raise RuntimeError(f'cannot load 0.3.4 migration core: {_CORE_PATH}')
_core = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = _core
_SPEC.loader.exec_module(_core)

_BUILD_PATH = Path(__file__).with_name('dpop034_build.py')
_BUILD_SPEC = importlib.util.spec_from_file_location('dpop034_build', _BUILD_PATH)
if _BUILD_SPEC is None or _BUILD_SPEC.loader is None:
    raise RuntimeError(f'cannot load 0.3.4 build helpers: {_BUILD_PATH}')
_build = importlib.util.module_from_spec(_BUILD_SPEC)
sys.modules[_BUILD_SPEC.name] = _build
_BUILD_SPEC.loader.exec_module(_build)

_original_transform_v034_overlay = _core.transform_v034_overlay
_original_migrate_034 = _core.migrate_034


def _run_repaired_donor(repository: Path, donor_output: Path, donor_workspace: Path) -> dict:
    import dpop033_migrate
    return dpop033_migrate.migrate(
        repository,
        donor_output,
        donor_workspace,
        build=False,
        keep_worktree=True,
    )


_run_donor_migration = _run_repaired_donor
_core._run_donor_migration = _run_donor_migration

transform_cmake_for_page_layout = _build.transform_cmake_for_page_layout
transform_cmake_for_zapret_center = _build.transform_cmake_for_zapret_center
transform_cmake_for_zapret_page_layout = _build.transform_cmake_for_zapret_page_layout
transform_cmake_for_shell_parity = _build.transform_cmake_for_shell_parity
_prepare_034_script_text = _build._prepare_034_script_text


def _insert_page_layout_include(text: str) -> str:
    include = '#include "ui/PageLayout.h"'
    if include in text:
        return text
    lines = text.splitlines()
    include_indexes = [i for i, line in enumerate(lines) if line.startswith('#include ')]
    if include_indexes:
        lines.insert(include_indexes[-1] + 1, include)
    else:
        lines.insert(0, include)
    suffix = '\n' if text.endswith('\n') else ''
    return '\n'.join(lines) + suffix


def transform_page_content_top(text: str) -> str:
    """Replace the recovered hard-coded content origin with the shared DPI boundary."""
    legacy = 'const int top = 54;'
    if legacy not in text:
        return text
    updated = _insert_page_layout_include(text)
    updated = updated.replace(
        legacy,
        'const int top = dpop::ui::ComputePageContentTop(GetDpiForWindow(Hwnd()));',
    )
    return updated


def transform_v034_overlay(v034_root: Path) -> dict[str, str]:
    summary = _original_transform_v034_overlay(v034_root)
    cmake_path = v034_root / 'CMakeLists.txt'
    original = cmake_path.read_text(encoding='utf-8')
    updated = original

    page_layout = v034_root / 'ui' / 'PageLayout.cpp'
    if page_layout.is_file():
        updated = transform_cmake_for_page_layout(updated)
        summary['page_layout_cmake_registered'] = 'true'
    else:
        summary['page_layout_cmake_registered'] = 'false'

    zapret_model = v034_root / 'modules' / 'ZapretCenterModel.cpp'
    if zapret_model.is_file():
        updated = transform_cmake_for_zapret_center(updated)
        summary['zapret_center_cmake_registered'] = 'true'
    else:
        summary['zapret_center_cmake_registered'] = 'false'

    zapret_layout = v034_root / 'ui' / 'pages' / 'ZapretPageLayout.cpp'
    if zapret_layout.is_file():
        updated = transform_cmake_for_zapret_page_layout(updated)
        summary['zapret_page_layout_cmake_registered'] = 'true'
    else:
        summary['zapret_page_layout_cmake_registered'] = 'false'

    startup_page = v034_root / 'ui' / 'pages' / 'StartupPage.cpp'
    updates_page = v034_root / 'ui' / 'pages' / 'UpdatesPage.cpp'
    if startup_page.is_file() and updates_page.is_file():
        updated = transform_cmake_for_shell_parity(updated)
        summary['shell_parity_cmake_registered'] = 'true'
    else:
        summary['shell_parity_cmake_registered'] = 'false'

    if updated != original:
        cmake_path.write_text(updated, encoding='utf-8', newline='\n')

    legacy_pages = (
        'MemoryPage.cpp',
        'GuardPage.cpp',
        'DiskPage.cpp',
        'ApplicationsPage.cpp',
        'WindowsPage.cpp',
        'DuplicatesPage.cpp',
        'ToolsPage.cpp',
        'SettingsPage.cpp',
    )
    migrated_pages = 0
    for name in legacy_pages:
        path = v034_root / 'ui' / 'pages' / name
        if not path.is_file():
            continue
        page_text = path.read_text(encoding='utf-8')
        page_updated = transform_page_content_top(page_text)
        if page_updated != page_text:
            path.write_text(page_updated, encoding='utf-8', newline='\n')
            migrated_pages += 1
    summary['shared_page_layout_migrated'] = str(migrated_pages)
    return summary


_core.transform_v034_overlay = transform_v034_overlay


def migrate_034(repository: Path, output: Path, workspace: Path, *, build: bool = False) -> dict:
    _core._run_donor_migration = _run_donor_migration
    _core.transform_v034_overlay = transform_v034_overlay
    report = _original_migrate_034(repository, output, workspace, build=False)
    if build:
        build_report = _build.run_windows_build(repository, output, workspace)
        report['build'] = {'requested': True, **build_report}
    else:
        report['build'] = {'requested': False, 'completed': False, 'tests_passed': False}
    return report


_core.migrate_034 = migrate_034


def __getattr__(name: str):
    return getattr(_core, name)


def main(argv: Sequence[str] | None = None) -> int:
    return _core.main(argv)


if __name__ == '__main__':
    raise SystemExit(main())
