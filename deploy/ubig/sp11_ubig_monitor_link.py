#!/usr/bin/env python3
"""Keep the SP11 control-sink unity monitors linked to the hidden UbiG engine.

The visible sink intentionally owns the desktop volume scalar while its monitor
ports remain unity.  Only those monitor ports may feed the UbiG
processor.  This small keeper survives node recreation and never guesses a
session-manager target.
"""
from __future__ import annotations

import argparse
import json
import re
import signal
import subprocess
import time

ENGINE_NODE = "effect_input.sp11_ubig_engine"
ENGINE_UNITY_GUARD_SECONDS = 5.0
ENGINE_PROBE_SECONDS = 1.0

LINKS = (
    (
        "effect_input.sp11_ubig:monitor_FL",
        "effect_input.sp11_ubig_engine:input_FL",
    ),
    (
        "effect_input.sp11_ubig:monitor_FR",
        "effect_input.sp11_ubig_engine:input_FR",
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


def node_id(pw_dump: str, node_name: str) -> int | None:
    cp = _run([pw_dump])
    if cp.returncode:
        return None
    try:
        entries = json.loads(cp.stdout)
    except json.JSONDecodeError:
        return None
    for entry in entries if isinstance(entries, list) else ():
        info = entry.get("info") or {}
        props = info.get("props") or {}
        if props.get("node.name") == node_name and isinstance(entry.get("id"), int):
            return int(entry["id"])
    return None


def ensure_engine_unity(engine_id: int, wpctl: str) -> bool:
    cp = _run([wpctl, "get-volume", str(engine_id)])
    if cp.returncode:
        return False
    match = re.search(r"Volume:\s*([0-9.]+)", cp.stdout)
    if not match:
        return False
    volume = float(match.group(1))
    muted = "[MUTED]" in cp.stdout
    if abs(volume - 1.0) > 5e-4:
        cp = _run([wpctl, "set-volume", str(engine_id), "1.0"] )
        if cp.returncode:
            return False
    if muted:
        cp = _run([wpctl, "set-mute", str(engine_id), "0"] )
        if cp.returncode:
            return False
    return True


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


def run(interval: float, pw_link: str, pw_dump: str, wpctl: str) -> int:
    stopping = False

    def stop(_signum: int, _frame: object) -> None:
        nonlocal stopping
        stopping = True

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    announced: bool | None = None
    guarded_engine_id: int | None = None
    guard_deadline = 0.0
    next_engine_probe = 0.0
    while not stopping:
        now = time.monotonic()
        if now >= next_engine_probe:
            current_engine_id = node_id(pw_dump, ENGINE_NODE)
            if current_engine_id != guarded_engine_id:
                guarded_engine_id = current_engine_id
                guard_deadline = (now + ENGINE_UNITY_GUARD_SECONDS) if current_engine_id is not None else 0.0
                if current_engine_id is not None:
                    print(
                        f"sp11-ubig-monitor-link: guarding hidden engine {current_engine_id} at unity",
                        flush=True,
                    )
            next_engine_probe = now + ENGINE_PROBE_SECONDS

        if guarded_engine_id is not None and now < guard_deadline:
            if not ensure_engine_unity(guarded_engine_id, wpctl):
                # Node recreation can race a wpctl transaction. A fresh ID probe
                # on the next loop re-arms the same bounded bootstrap guard.
                next_engine_probe = 0.0

        ready = reconcile(pw_link)
        if ready != announced:
            print(
                "sp11-ubig-monitor-link: "
                + ("unity monitor path linked" if ready else "waiting for UbiG ports"),
                flush=True,
            )
            announced = ready
        time.sleep(interval)
    return 0


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--interval", type=float, default=0.25)
    p.add_argument("--pw-link", default="pw-link")
    p.add_argument("--pw-dump", default="pw-dump")
    p.add_argument("--wpctl", default="wpctl")
    return p


def main() -> int:
    args = parser().parse_args()
    if args.interval <= 0:
        raise SystemExit("--interval must be positive")
    return run(args.interval, args.pw_link, args.pw_dump, args.wpctl)


if __name__ == "__main__":
    raise SystemExit(main())
