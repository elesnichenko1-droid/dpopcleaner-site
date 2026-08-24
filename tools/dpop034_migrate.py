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


def migrate_034(repository: Path, output: Path, workspace: Path, *, build: bool = False) -> dict:
    _core._run_donor_migration = _run_donor_migration
    return _core.migrate_034(repository, output, workspace, build=build)


def __getattr__(name: str):
    return getattr(_core, name)


def main(argv: Sequence[str] | None = None) -> int:
    return _core.main(argv)


if __name__ == '__main__':
    raise SystemExit(main())
