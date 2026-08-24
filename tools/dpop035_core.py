#!/usr/bin/env python3
"""DPopCleaner 0.3.5 migration core.

The recovered v033 tree is the user-visible 0.2.14-style UX host. Only the
allow-listed backend roots from verified v034 are copied into it; 0.3.4 UI
files are deliberately excluded. Version overlays are partial: core/update
may be inherited from the normalized R3 source prepared at build time.
"""
from __future__ import annotations

import importlib.util
import json
import shutil
import sys
from pathlib import Path
from types import ModuleType

TARGET_VERSION = "0.3.5"
TARGET_DISPLAY_VERSION = "0.3.5 BETA R1"
TARGET_VERSION_CODE = "3051"
TARGET_REVISION = "1"
TARGET_RESOURCE_VERSION = "0.3.5.1"
MODERN_BACKEND_ROOTS = ("core", "modules", "update")


def guarded_replace(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        raise ValueError(f"expected marker missing in {label}: {old}")
    return text.replace(old, new)


def apply_overlay(overlay_root: Path, target_root: Path) -> list[str]:
    overlay_root = overlay_root.resolve()
    target_root = target_root.resolve()
    changed: list[str] = []
    if not overlay_root.is_dir():
        return changed
    for source in sorted(overlay_root.rglob("*")):
        if source.is_symlink():
            raise ValueError(f"overlay symlinks are forbidden: {source}")
        if not source.is_file():
            continue
        relative = source.relative_to(overlay_root)
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"unsafe overlay path: {relative.as_posix()}")
        destination = target_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)
        changed.append(relative.as_posix())
    return changed


def copy_modern_backend(v034: Path, v035: Path) -> list[str]:
    """Copy backend roots that are actually present in the v034 overlay.

    v034 is a version overlay, not a normalized full source tree. Missing
    core/update roots intentionally inherit the tracked R3 baseline when the
    build helper prepares the normalized source. Modules must be present
    because 0.3.4 carries its modern functional overlay there.
    """
    copied: list[str] = []
    for root_name in MODERN_BACKEND_ROOTS:
        src = v034 / root_name
        if not src.is_dir():
            continue
        dst = v035 / root_name
        if dst.exists():
            shutil.rmtree(dst)
        shutil.copytree(src, dst)
        copied.append(root_name)
    if "modules" not in copied:
        raise ValueError("modern backend modules overlay missing")
    return copied


def _load_tool(repository: Path, name: str) -> ModuleType:
    tools = repository / "tools"
    path = tools / f"{name}.py"
    if not path.is_file():
        raise ValueError(f"required migration tool missing: {path}")
    tools_text = str(tools)
    if tools_text not in sys.path:
        sys.path.insert(0, tools_text)
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load migration tool: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def _find_generated(root: Path, version_dir: str) -> Path:
    preferred = root / "source-overlay" / version_dir
    if preferred.is_dir():
        return preferred
    matches = [p for p in root.rglob(version_dir) if p.is_dir() and (p / "CMakeLists.txt").is_file()]
    if len(matches) != 1:
        raise ValueError(f"expected exactly one generated {version_dir} tree below {root}; found {len(matches)}")
    return matches[0]


def _insert_ctest_block(text: str, marker: str, block: str) -> str:
    if marker in text:
        return text
    closing = text.rfind("endif()")
    if closing < 0:
        raise ValueError("CMake donor drifted: BUILD_TESTING endif() missing")
    return text[:closing] + block + text[closing:]


def _transform_cmake_for_disk(text: str) -> str:
    if "src/modules/DiskAnalyzer.cpp" not in text:
        anchor = "  src/modules/FullCore.cpp\n"
        if anchor not in text:
            raise ValueError("CMake donor drifted: FullCore source anchor missing")
        text = text.replace(anchor, anchor + "  src/modules/DiskAnalyzer.cpp\n", 1)

    if "src/ui/controls/DiskTreeList.cpp" not in text:
        anchor = "  src/ui/Controls.cpp\n"
        if anchor not in text:
            raise ValueError("CMake donor drifted: Controls source anchor missing")
        text = text.replace(anchor, anchor + "  src/ui/controls/DiskTreeList.cpp\n", 1)

    if "src/ui/pages/DiskPage.cpp" not in text:
        for anchor in ("  src/ui/pages/WorkspacePage.cpp\n", "  src/ui/pages/OverviewPage.cpp\n"):
            if anchor in text:
                text = text.replace(anchor, anchor + "  src/ui/pages/DiskPage.cpp\n", 1)
                break
        else:
            raise ValueError("CMake donor drifted: DiskPage source anchor missing")

    block = '''\n  add_executable(DiskAnalyzerTests tests/v035/DiskAnalyzerTests.cpp src/modules/DiskAnalyzer.cpp)\n  target_include_directories(DiskAnalyzerTests PRIVATE src)\n  target_compile_definitions(DiskAnalyzerTests PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)\n  target_link_libraries(DiskAnalyzerTests PRIVATE kernel32)\n  if(MSVC)\n    target_compile_options(DiskAnalyzerTests PRIVATE /W4 /permissive- /utf-8)\n  endif()\n  add_test(NAME DiskAnalyzerTests COMMAND DiskAnalyzerTests)\n'''
    return _insert_ctest_block(text, "add_executable(DiskAnalyzerTests", block)


def _overlay_v035_tests(repository: Path, stage: Path) -> list[str]:
    source = repository / "tests" / "v035"
    destination = stage / "tests"
    if not source.is_dir():
        return []
    destination.mkdir(parents=True, exist_ok=True)
    changed: list[str] = []
    for path in sorted(source.rglob("*")):
        if path.is_symlink():
            raise ValueError(f"test symlinks are forbidden: {path}")
        if not path.is_file():
            continue
        relative = path.relative_to(source)
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        changed.append(relative.as_posix())
    return changed


def _transform_identity(v035: Path) -> dict[str, str]:
    cmake_path = v035 / "CMakeLists.txt"
    header_path = v035 / "Version.h"
    resource_path = v035 / "version.rc.in"
    for path in (cmake_path, header_path, resource_path):
        if not path.is_file():
            raise ValueError(f"required 0.3.5 identity file missing: {path.name}")

    cmake = cmake_path.read_text(encoding="utf-8")
    if "project(DPopCleaner VERSION 0.3.3" in cmake:
        cmake = guarded_replace(cmake, "project(DPopCleaner VERSION 0.3.3", "project(DPopCleaner VERSION 0.3.5", "CMakeLists.txt")
    elif "project(DPopCleaner VERSION 0.3.4" in cmake:
        cmake = guarded_replace(cmake, "project(DPopCleaner VERSION 0.3.4", "project(DPopCleaner VERSION 0.3.5", "CMakeLists.txt")
    else:
        raise ValueError("expected 0.3.3/0.3.4 CMake product identity missing")
    cmake = cmake.replace("tests/v033/", "tests/v035/").replace("tests/v034/", "tests/v035/")
    cmake = _transform_cmake_for_disk(cmake)

    header = header_path.read_text(encoding="utf-8")
    for old in ('kVersion[] = L"0.3.3"', 'kVersion[] = L"0.3.4"'):
        if old in header:
            header = guarded_replace(header, old, 'kVersion[] = L"0.3.5"', "Version.h")
            break
    else:
        raise ValueError("expected donor kVersion missing")
    for old in ('kDisplayVersion[] = L"0.3.3 BETA R1"', 'kDisplayVersion[] = L"0.3.4 BETA R1"', 'kDisplayVersion[] = L"0.3.4 BETA R2"'):
        if old in header:
            header = guarded_replace(header, old, 'kDisplayVersion[] = L"0.3.5 BETA R1"', "Version.h")
            break
    else:
        raise ValueError("expected donor display version missing")
    for old in ("kVersionCode = 3031", "kVersionCode = 3041", "kVersionCode = 3042"):
        if old in header:
            header = guarded_replace(header, old, "kVersionCode = 3051", "Version.h")
            break
    for old in ("kRevision = 1", "kRevision = 2"):
        if old in header:
            header = guarded_replace(header, old, "kRevision = 1", "Version.h")
            break

    resource = resource_path.read_text(encoding="utf-8")
    for old, new in (
        ("FILEVERSION 0,3,3,1", "FILEVERSION 0,3,5,1"),
        ("PRODUCTVERSION 0,3,3,1", "PRODUCTVERSION 0,3,5,1"),
        ('"0.3.3.1\\0"', '"0.3.5.1\\0"'),
        ('"0.3.3 BETA R1\\0"', '"0.3.5 BETA R1\\0"'),
        ("FILEVERSION 0,3,4,1", "FILEVERSION 0,3,5,1"),
        ("PRODUCTVERSION 0,3,4,1", "PRODUCTVERSION 0,3,5,1"),
        ('"0.3.4.1\\0"', '"0.3.5.1\\0"'),
        ('"0.3.4 BETA R1\\0"', '"0.3.5 BETA R1\\0"'),
        ("FILEVERSION 0,3,4,2", "FILEVERSION 0,3,5,1"),
        ("PRODUCTVERSION 0,3,4,2", "PRODUCTVERSION 0,3,5,1"),
        ('"0.3.4.2\\0"', '"0.3.5.1\\0"'),
        ('"0.3.4 BETA R2\\0"', '"0.3.5 BETA R1\\0"'),
    ):
        resource = resource.replace(old, new)
    if "0.3.5.1" not in resource and "0,3,5,1" not in resource:
        raise ValueError("resource identity transformation failed")

    cmake_path.write_text(cmake, encoding="utf-8", newline="\n")
    header_path.write_text(header, encoding="utf-8", newline="\n")
    resource_path.write_text(resource, encoding="utf-8", newline="\n")
    return {
        "version": TARGET_VERSION,
        "display_version": TARGET_DISPLAY_VERSION,
        "version_code": TARGET_VERSION_CODE,
        "revision": TARGET_REVISION,
        "resource_version": TARGET_RESOURCE_VERSION,
    }


def prepare_v035(repository: Path, output: Path, workspace: Path, *, build: bool = False) -> dict:
    repository = repository.resolve()
    output = output.resolve()
    workspace = workspace.resolve()
    if workspace == repository or repository in workspace.parents:
        raise ValueError("workspace must be outside repository checkout")
    output.mkdir(parents=True, exist_ok=True)
    workspace.mkdir(parents=True, exist_ok=True)

    d033 = _load_tool(repository, "dpop033_migrate")
    d034 = _load_tool(repository, "dpop034_migrate")

    ux_output = workspace / "ux-donor-output"
    ux_workspace = workspace / "ux-donor-workspace"
    ux_report = d033.migrate(repository, ux_output, ux_workspace, build=False, keep_worktree=True)
    v033 = _find_generated(ux_output, "v033") if (ux_output / "source-overlay").exists() else _find_generated(ux_workspace, "v033")

    backend_output = workspace / "backend-output"
    backend_workspace = workspace / "backend-workspace"
    migrate034 = getattr(d034, "migrate", None) or getattr(d034, "migrate_034", None)
    if migrate034 is None:
        raise RuntimeError("0.3.4 migration entrypoint is unavailable")
    backend_report = migrate034(repository, backend_output, backend_workspace, build=False)
    v034 = _find_generated(backend_output, "v034") if (backend_output / "source-overlay").exists() else _find_generated(backend_workspace, "v034")

    stage = workspace / "v035-stage"
    if stage.exists():
        shutil.rmtree(stage)
    shutil.copytree(v033, stage)
    copied_backend = copy_modern_backend(v034, stage)
    overlay_files = apply_overlay(repository / "v035_overlay", stage)
    test_files = _overlay_v035_tests(repository, stage)
    identity = _transform_identity(stage)

    export_root = output / "source-overlay"
    if export_root.exists():
        shutil.rmtree(export_root)
    export_root.mkdir(parents=True, exist_ok=True)
    shutil.copytree(stage, export_root / "v035")

    report = {
        "target": TARGET_DISPLAY_VERSION,
        "target_version": TARGET_VERSION,
        "identity": identity,
        "ux_donor": ux_report,
        "backend_donor": backend_report,
        "modern_backend_roots": list(copied_backend),
        "inherited_backend_roots": [name for name in MODERN_BACKEND_ROOTS if name not in copied_backend],
        "overlay_files": overlay_files,
        "v035_test_files": test_files,
        "build": {"requested": build, "completed": False, "tests_passed": False},
    }

    if build:
        builder = _load_tool(repository, "dpop035_build")
        report["build"] = {"requested": True, **builder.run_windows_build(repository, output, workspace)}

    (output / "migration-report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2, default=str), encoding="utf-8")
    return report
