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
_prepare_034_script_text = _build._prepare_034_script_text


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

    if updated != original:
        cmake_path.write_text(updated, encoding='utf-8', newline='\n')
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
