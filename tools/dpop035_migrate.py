#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
from typing import Sequence

from dpop035_core import prepare_v035


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="Prepare isolated DPopCleaner 0.3.5 source")
    p.add_argument("--repository", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--workspace", type=Path, required=True)
    p.add_argument("--build", action="store_true")
    return p


def main(argv: Sequence[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    prepare_v035(args.repository, args.output, args.workspace, build=args.build)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
