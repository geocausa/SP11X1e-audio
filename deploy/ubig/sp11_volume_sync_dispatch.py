#!/usr/bin/env python3
"""Select the rollback-safe SP11 endpoint-volume synchronizer for this boot."""

from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path


TRANSACTION_CONTROL = "SP11 Windows Volume Transaction"
SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_CONTROL_WAIT_SECONDS = 10.0
DEFAULT_CONTROL_POLL_SECONDS = 0.25


def has_control(amixer: str = "amixer", card: str = "hw:0") -> bool:
    try:
        cp = subprocess.run(
            [amixer, "-D", card, "controls"], capture_output=True, text=True
        )
    except OSError:
        return False
    return cp.returncode == 0 and TRANSACTION_CONTROL.lower() in cp.stdout.lower()


def wait_for_transaction_control(
    amixer: str = "amixer",
    card: str = "hw:0",
    timeout: float = DEFAULT_CONTROL_WAIT_SECONDS,
    interval: float = DEFAULT_CONTROL_POLL_SECONDS,
) -> bool:
    """Wait briefly for late ALSA/q6apm control registration at login.

    The user service can start several seconds before q6apm publishes the SP11
    transaction controls.  A one-shot probe permanently selected the rollback
    synchronizer for that boot even though the candidate kernel exposed the
    correct controls moments later.  Poll for a bounded interval, then retain
    the historical rollback behavior if the control truly never appears.
    """
    timeout = max(0.0, float(timeout))
    interval = max(0.01, float(interval))
    deadline = time.monotonic() + timeout

    while True:
        if has_control(amixer, card):
            return True
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return False
        time.sleep(min(interval, remaining))


def resolve_program(name: str) -> Path:
    helper_dir = os.environ.get("UBIG_VOLUME_HELPER_DIR")
    if helper_dir:
        candidate = Path(helper_dir) / name
        if candidate.exists():
            return candidate
    installed = Path.home() / ".local/bin" / name
    if installed.exists():
        return installed
    source_name = name.replace("-", "_") + ".py"
    return SCRIPT_DIR / source_name


def main() -> int:
    timeout = float(
        os.environ.get("SP11_VOLUME_CONTROL_WAIT_SECONDS", DEFAULT_CONTROL_WAIT_SECONDS)
    )
    interval = float(
        os.environ.get("SP11_VOLUME_CONTROL_POLL_SECONDS", DEFAULT_CONTROL_POLL_SECONDS)
    )
    if wait_for_transaction_control(timeout=timeout, interval=interval):
        program = resolve_program("sp11-windows-volume-transaction-sync")
        selected = "windows-transaction"
    else:
        program = resolve_program("sp11-ubig-volume-sync")
        selected = "rollback-ubig"
    if not program.exists():
        print(f"missing volume synchronizer: {program}", file=sys.stderr)
        return 2
    print(f"sp11-volume-sync-dispatch: selected={selected} program={program}", flush=True)
    os.execv(str(program), [str(program), *sys.argv[1:]])
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
