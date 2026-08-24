#!/usr/bin/env python3
"""DPopCleaner 0.3.3 migration entrypoint and recovered-source repairs.

The original reverse-migration implementation is kept in dpop033_core.py.
This thin layer applies narrowly-scoped repairs that are specific to the
faithful 0.2.14 recovery before the recovered v033 tree is compiled.
"""
from __future__ import annotations

import importlib.util
import re
import sys
from pathlib import Path
from typing import Sequence

_CORE_PATH = Path(__file__).with_name("dpop033_core.py")
_SPEC = importlib.util.spec_from_file_location("dpop033_core", _CORE_PATH)
if _SPEC is None or _SPEC.loader is None:
    raise RuntimeError(f"cannot load migration core: {_CORE_PATH}")
_core = importlib.util.module_from_spec(_SPEC)
sys.modules[_SPEC.name] = _core
_SPEC.loader.exec_module(_core)

_original_transform_v033_overlay = _core.transform_v033_overlay


def _repair_recovery_small_font_identifier(v033_root: Path) -> int:
    """Rename RecoveryFonts.small, which collides with a Windows SDK macro.

    rpcndr.h may define ``small`` as ``char``.  The faithful recovered source
    uses ``small`` as an HFONT member, so MSVC preprocesses expressions such as
    ``small = CreateUiFont(...)`` into invalid ``char = ...`` syntax.
    """
    header = v033_root / "ui" / "RecoveryControls.h"
    source = v033_root / "ui" / "RecoveryControls.cpp"
    exists = (header.is_file(), source.is_file())
    if any(exists) and not all(exists):
        raise ValueError("RecoveryControls.h/.cpp must either both exist or both be absent")
    if not all(exists):
        return 0

    changed: set[Path] = set()

    header_text = header.read_text(encoding="utf-8")
    repaired_header, declarations = re.subn(
        r"(\bHFONT\s+)small\b", r"\1smallFont", header_text
    )
    if declarations != 1:
        raise ValueError(
            "expected exactly one RecoveryFonts HFONT small declaration; "
            f"found {declarations}"
        )
    if repaired_header != header_text:
        header.write_text(repaired_header, encoding="utf-8", newline="\n")
        changed.add(header)

    source_text = source.read_text(encoding="utf-8")
    repaired_source, source_uses = re.subn(r"\bsmall\b", "smallFont", source_text)
    if source_uses == 0:
        raise ValueError("expected RecoveryControls.cpp to reference RecoveryFonts.small")
    if repaired_source != source_text:
        source.write_text(repaired_source, encoding="utf-8", newline="\n")
        changed.add(source)

    ui_root = v033_root / "ui"
    for pattern in ("*.cpp", "*.h"):
        for path in ui_root.rglob(pattern):
            if path in {header, source}:
                continue
            original = path.read_text(encoding="utf-8")
            updated = re.sub(r"(?<=\.)small\b", "smallFont", original)
            updated = re.sub(r"(?<=->)small\b", "smallFont", updated)
            if updated != original:
                path.write_text(updated, encoding="utf-8", newline="\n")
                changed.add(path)

    return len(changed)


def transform_v033_overlay(v033_root: Path) -> dict[str, str]:
    summary = _original_transform_v033_overlay(v033_root)
    changed = _repair_recovery_small_font_identifier(v033_root)
    summary["recovery_small_fix_files_changed"] = str(changed)
    return summary


# Core migrate() resolves this global on the core module at runtime.  Patching
# it here makes both CLI builds and callers of core.migrate use the repair.
_core.transform_v033_overlay = transform_v033_overlay


def __getattr__(name: str):
    return getattr(_core, name)


def main(argv: Sequence[str] | None = None) -> int:
    return _core.main(argv)


if __name__ == "__main__":
    raise SystemExit(main())
