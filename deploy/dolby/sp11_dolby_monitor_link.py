#!/usr/bin/env python3
"""Keep the SP11 control-sink unity monitors linked to the hidden Dolby engine.

The visible sink intentionally owns the desktop volume scalar while its monitor
ports remain unity.  Only those monitor ports may feed the recovered Dolby
processor.  This small keeper survives node recreation and never guesses a
session-manager target.
"""
from __future__ import annotations

import argparse
import signal
import subprocess
import time

LINKS = (
    (
        "effect_input.sp11_windows_dolby:monitor_FL",
        "effect_input.sp11_windows_dolby_engine:input_FL",
    ),
    (
        "effect_input.sp11_windows_dolby:monitor_FR",
        "effect_input.sp11_windows_dolby_engine:input_FR",
    ),
)


def _run(args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(args, capture_output=True, text=True)


def available_ports(pw_link: str) -> tuple[set[str], set[str]]:
    outputs = _run([pw_link, "-o"])
    inputs = _run([pw_link, "-i"])
    if outputs.returncode or inputs.returncode:
        return set(), set()
    return set(outputs.stdout.splitlines()), set(inputs.stdout.splitlines())


def current_links(pw_link: str) -> set[tuple[str, str]]:
    cp = _run([pw_link, "-l"])
    if cp.returncode:
        return set()
    found: set[tuple[str, str]] = set()
    source: str | None = None
    for raw in cp.stdout.splitlines():
        if raw and not raw[0].isspace():
            source = raw.strip()
            continue
        line = raw.strip()
        if source and line.startswith("|->"):
            found.add((source, line[3:].strip()))
    return found


def reconcile(pw_link: str) -> bool:
    outputs, inputs = available_ports(pw_link)
    if not outputs or not inputs:
        return False
    linked = current_links(pw_link)
    all_ready = True
    for source, sink in LINKS:
        if source not in outputs or sink not in inputs:
            all_ready = False
            continue
        if (source, sink) in linked:
            continue
        cp = _run([pw_link, "-L", source, sink])
        if cp.returncode:
            # A concurrent recreation can race us; the next pass rechecks.
            all_ready = False
    return all_ready and LINKS[0] in current_links(pw_link) and LINKS[1] in current_links(pw_link)


def run(interval: float, pw_link: str) -> int:
    stopping = False

    def stop(_signum: int, _frame: object) -> None:
        nonlocal stopping
        stopping = True

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    announced: bool | None = None
    while not stopping:
        ready = reconcile(pw_link)
        if ready != announced:
            print(
                "sp11-dolby-monitor-link: "
                + ("unity monitor path linked" if ready else "waiting for Dolby ports"),
                flush=True,
            )
            announced = ready
        time.sleep(interval)
    return 0


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--interval", type=float, default=0.25)
    p.add_argument("--pw-link", default="pw-link")
    return p


def main() -> int:
    args = parser().parse_args()
    if args.interval <= 0:
        raise SystemExit("--interval must be positive")
    return run(args.interval, args.pw_link)


if __name__ == "__main__":
    raise SystemExit(main())
