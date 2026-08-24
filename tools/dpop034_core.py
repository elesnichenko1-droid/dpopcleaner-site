#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
import tempfile
from pathlib import Path
from typing import Sequence

TARGET_VERSION = '0.3.4'
TARGET_DISPLAY_VERSION = '0.3.4 BETA R1'
TARGET_VERSION_CODE = '3041'
TARGET_REVISION = '1'
TARGET_RESOURCE_VERSION = '0.3.4.1'
DONOR_VERSION = '0.3.3'
DONOR_DISPLAY_VERSION = '0.3.3 BETA R1'
DONOR_VERSION_CODE = '3031'


def _guarded_replace(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise ValueError(f'expected marker missing in {label}: {old}')
    return text.replace(old, new)


def apply_overlay(overlay_root: Path, target_root: Path) -> list[str]:
    overlay_root = overlay_root.resolve()
    target_root = target_root.resolve()
    if not overlay_root.is_dir():
        return []
    changed: list[str] = []
    for source in sorted(overlay_root.rglob('*')):
        if source.is_symlink():
            raise ValueError(f'overlay symlinks are forbidden: {source.relative_to(overlay_root).as_posix()}')
        if not source.is_file():
            continue
        relative = source.relative_to(overlay_root)
        if relative.is_absolute() or '..' in relative.parts:
            raise ValueError(f'unsafe overlay path: {relative.as_posix()}')
        destination = target_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        changed.append(relative.as_posix())
    return changed


def transform_v034_overlay(v034_root: Path) -> dict[str, str]:
    cmake_path = v034_root / 'CMakeLists.txt'
    header_path = v034_root / 'Version.h'
    resource_path = v034_root / 'version.rc.in'
    for path in (cmake_path, header_path, resource_path):
        if not path.is_file():
            raise ValueError(f'required v034 file missing: {path.name}')

    cmake = cmake_path.read_text(encoding='utf-8')
    header = header_path.read_text(encoding='utf-8')
    resource = resource_path.read_text(encoding='utf-8')

    cmake = _guarded_replace(cmake, 'project(DPopCleaner VERSION 0.3.3', 'project(DPopCleaner VERSION 0.3.4', 'CMakeLists.txt')
    cmake = cmake.replace('tests/v033/', 'tests/v034/')

    header = _guarded_replace(header, 'kVersion[] = L"0.3.3"', 'kVersion[] = L"0.3.4"', 'Version.h')
    header = _guarded_replace(header, 'kDisplayVersion[] = L"0.3.3 BETA R1"', 'kDisplayVersion[] = L"0.3.4 BETA R1"', 'Version.h')
    header = _guarded_replace(header, 'kVersionCode = 3031', 'kVersionCode = 3041', 'Version.h')
    if 'kRevision = 1' not in header:
        raise ValueError('expected marker missing in Version.h: kRevision = 1')

    resource = _guarded_replace(resource, 'FILEVERSION 0,3,3,1', 'FILEVERSION 0,3,4,1', 'version.rc.in')
    resource = _guarded_replace(resource, 'PRODUCTVERSION 0,3,3,1', 'PRODUCTVERSION 0,3,4,1', 'version.rc.in')
    resource = _guarded_replace(resource, '"0.3.3.1\\0"', '"0.3.4.1\\0"', 'version.rc.in')
    resource = _guarded_replace(resource, '"0.3.3 BETA R1\\0"', '"0.3.4 BETA R1\\0"', 'version.rc.in')

    source_updates: dict[Path, str] = {}
    identity_pairs = (
        ('DPopCleaner 0.3.3 BETA R1', 'DPopCleaner 0.3.4 BETA R1'),
        ('v0.3.3 BETA', 'v0.3.4 BETA'),
        ('DPopCleaner 0.3.3', 'DPopCleaner 0.3.4'),
    )
    for pattern in ('*.cpp', '*.h'):
        for path in v034_root.rglob(pattern):
            if path == header_path:
                continue
            original = path.read_text(encoding='utf-8')
            updated = original
            for old, new in identity_pairs:
                updated = updated.replace(old, new)
            if updated != original:
                source_updates[path] = updated

    cmake_path.write_text(cmake, encoding='utf-8', newline='\n')
    header_path.write_text(header, encoding='utf-8', newline='\n')
    resource_path.write_text(resource, encoding='utf-8', newline='\n')
    for path, updated in source_updates.items():
        path.write_text(updated, encoding='utf-8', newline='\n')

    return {
        'version': TARGET_VERSION,
        'display_version': TARGET_DISPLAY_VERSION,
        'version_code': TARGET_VERSION_CODE,
        'revision': TARGET_REVISION,
        'resource_version': TARGET_RESOURCE_VERSION,
        'identity_source_files_changed': str(len(source_updates)),
    }


def prepare_v034_from_donor(donor_root: Path, overlay_root: Path, target_root: Path) -> dict:
    donor_root = donor_root.resolve()
    target_root = target_root.resolve()
    if not donor_root.is_dir():
        raise ValueError(f'v033 donor directory is missing: {donor_root}')
    if target_root == donor_root or donor_root in target_root.parents:
        raise ValueError('v034 target must not be inside v033 donor')
    if target_root.exists():
        shutil.rmtree(target_root)
    shutil.copytree(donor_root, target_root)
    overlay_files = apply_overlay(overlay_root, target_root)
    layout_path = None
    for relative in (Path('ui/pages/WorkspacePage.cpp'), Path('ui/WorkspacePage.cpp')):
        candidate = target_root / relative
        if candidate.is_file():
            candidate.write_text(
                transform_workspace_layout_text(candidate.read_text(encoding='utf-8')),
                encoding='utf-8',
                newline='\n',
            )
            layout_path = relative.as_posix()
            break
    version = transform_v034_overlay(target_root)
    return {'version': version, 'overlay_files': overlay_files, 'layout_transformed': layout_path}


def _run_donor_migration(repository: Path, donor_output: Path, donor_workspace: Path) -> dict:
    import dpop033_core
    return dpop033_core.migrate(
        repository,
        donor_output,
        donor_workspace,
        build=False,
        keep_worktree=True,
    )


def migrate_034(repository: Path, output: Path, workspace: Path, *, build: bool = False) -> dict:
    repository = repository.resolve()
    output = output.resolve()
    workspace = workspace.resolve()
    if workspace == repository or repository in workspace.parents:
        raise ValueError('workspace must be outside repository checkout')
    output.mkdir(parents=True, exist_ok=True)
    workspace.mkdir(parents=True, exist_ok=True)

    donor_output = workspace / 'donor-output'
    donor_workspace = workspace / 'donor-workspace'
    donor_report = _run_donor_migration(repository, donor_output, donor_workspace)
    stage = donor_workspace / 'repo-stage'
    donor = stage / 'v033'
    target = stage / 'v034'
    overlay = repository / 'v034_overlay'
    prepared_report = prepare_v034_from_donor(donor, overlay, target)

    export_root = output / 'source-overlay'
    if export_root.exists():
        shutil.rmtree(export_root)
    export_root.mkdir(parents=True, exist_ok=True)
    shutil.copytree(target, export_root / 'v034')

    if build:
        raise RuntimeError('0.3.4 Windows build orchestration is added by the candidate workflow task')

    return {
        'target': TARGET_DISPLAY_VERSION,
        'target_version': TARGET_VERSION,
        'donor': donor_report,
        'overlay': prepared_report,
        'build': {'requested': build, 'completed': False, 'tests_passed': False},
    }


def _function_span(text: str, signature: str) -> tuple[int, int]:
    start = text.find(signature)
    if start < 0:
        raise ValueError(f'expected function signature missing: {signature}')
    brace = text.find('{', start + len(signature))
    if brace < 0:
        raise ValueError(f'opening brace missing for: {signature}')
    depth = 0
    for index in range(brace, len(text)):
        ch = text[index]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                return start, index + 1
    raise ValueError(f'unbalanced function body: {signature}')


def transform_workspace_layout_text(text: str) -> str:
    signature = 'void WorkspacePage::LayoutChildren() noexcept'
    start, end = _function_span(text, signature)
    old_function = text[start:end]
    required_markers = (
        'MoveWindow(status_, margin, 52',
        'MoveWindow(list_, margin, 86',
        'const int buttonHeight = 38',
    )
    missing = [marker for marker in required_markers if marker not in old_function]
    if missing:
        raise ValueError('workspace layout donor drifted; missing: ' + ', '.join(missing))

    if '#include "ui/PageLayout.h"' not in text:
        include_marker = '#include "ui/Theme.h"'
        if include_marker not in text:
            raise ValueError('WorkspacePage include anchor missing: ui/Theme.h')
        text = text.replace(include_marker, include_marker + '\n#include "ui/PageLayout.h"', 1)
        start, end = _function_span(text, signature)

    replacement = '''void WorkspacePage::LayoutChildren() noexcept {
    if (!hwnd_) return;
    RECT rc{};
    GetClientRect(hwnd_, &rc);
    const int width = std::max(0, rc.right - rc.left);
    const int height = std::max(0, rc.bottom - rc.top);
    const int visibleButtons = static_cast<int>(std::count_if(
        buttons_.begin(), buttons_.end(), [](HWND h) { return h && IsWindowVisible(h); }));
    const UINT windowDpi = GetDpiForWindow(hwnd_);
    const PageRegions regions = ComputePageRegions(
        width, height, windowDpi ? static_cast<int>(windowDpi) : 96, visibleButtons);

    MoveWindow(
        heading_, regions.heading.x, regions.heading.y,
        regions.heading.width, regions.heading.height, TRUE);
    MoveWindow(
        status_, regions.description.x, regions.description.y,
        regions.description.width, regions.description.height, TRUE);
    MoveWindow(
        list_, regions.content.x, regions.content.y,
        regions.content.width, regions.content.height, TRUE);

    if (visibleButtons <= 0) return;
    const int columns = (visibleButtons + regions.actionRows - 1) / regions.actionRows;
    const int buttonWidth = std::max(
        1, (regions.actions.width - regions.gap * (columns - 1)) / columns);
    int visibleIndex = 0;
    for (HWND button : buttons_) {
        if (!button || !IsWindowVisible(button)) continue;
        const int row = visibleIndex / columns;
        const int column = visibleIndex % columns;
        const int x = regions.actions.x + column * (buttonWidth + regions.gap);
        const int y = regions.actions.y + row * (regions.actionRowHeight + regions.gap);
        MoveWindow(button, x, y, buttonWidth, regions.actionRowHeight, TRUE);
        ++visibleIndex;
    }
}'''
    return text[:start] + replacement + text[end:]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description='Build DPopCleaner 0.3.4 from the verified 0.3.3 recovered donor.'
    )
    parser.add_argument('--repository', type=Path, default=Path.cwd())
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument(
        '--workspace',
        type=Path,
        default=Path(tempfile.gettempdir()) / 'dpopcleaner-0.3.4-migration',
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--build', action='store_true')
    mode.add_argument('--no-build', action='store_true')
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    build_requested = bool(args.build)
    if not args.build and not args.no_build:
        build_requested = os.name == 'nt'
    try:
        report = migrate_034(
            args.repository,
            args.output,
            args.workspace,
            build=build_requested,
        )
    except Exception as exc:
        print(f'DPopCleaner 0.3.4 migration FAILED: {exc}', file=sys.stderr)
        return 1
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
