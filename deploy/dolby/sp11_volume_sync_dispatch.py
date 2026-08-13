#!/usr/bin/env python3
"""Select the rollback-safe SP11 endpoint-volume synchronizer for this boot."""

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


TRANSACTION_CONTROL = "SP11 Windows Volume Transaction"
SCRIPT_DIR = Path(__file__).resolve().parent


def has_control(amixer: str = "amixer", card: str = "hw:0") -> bool:
    cp = subprocess.run(
        [amixer, "-D", card, "controls"], capture_output=True, text=True
    )
    return cp.returncode == 0 and TRANSACTION_CONTROL.lower() in cp.stdout.lower()


def resolve_program(name: str) -> Path:
    installed = Path.home() / ".local/bin" / name
    if installed.exists():
        return installed
    source_name = name.replace("-", "_") + ".py"
    return SCRIPT_DIR / source_name


def main() -> int:
    if has_control():
        program = resolve_program("sp11-windows-volume-transaction-sync")
    else:
        program = resolve_program("sp11-dolby-volume-sync")
    if not program.exists():
        print(f"missing volume synchronizer: {program}", file=sys.stderr)
        return 2
    os.execv(str(program), [str(program), *sys.argv[1:]])
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
