#!/usr/bin/env python3
"""Isolated SP11 endpoint-volume actuator A/B wrapper.

This candidate reuses the production Windows taper / Dolby control / monitor
logic, but moves endpoint attenuation from the hidden PipeWire/ALSA sink to the
final protected-DSP VOL_CTRL 0x4a63 via the candidate `SP11 Final Volume Q28`
bytes-TLV control.  It is intentionally not installed as a service.

Safety ordering:
  enter DSP actuator: write attenuated DSP Q28 first, then set hidden sink unity;
  restore host actuator: apply hidden-sink attenuation first, then set DSP unity.
Thus actuator switching can transiently double-attenuate, but never intentionally
creates a full-volume interval.
"""
from __future__ import annotations

import argparse
import importlib.util
import math
import struct
import subprocess
import sys
from pathlib import Path

BASE_SCRIPT = (
    Path(__file__).resolve().parents[1]
    / "deploy/dolby/sp11_dolby_volume_sync.py"
)
spec = importlib.util.spec_from_file_location("sp11_volume_sync_base", BASE_SCRIPT)
base = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(base)

FINAL_CONTROL_NAME = "SP11 Final Volume Q28"
DEFAULT_CARD = "hw:0"
DEFAULT_TLV_WRITE = Path.home() / ".local/lib/sp11-dolby/tlv_write"
Q28_ONE = 1 << 28


def endpoint_q28_from_db(db: float) -> int:
    if not math.isfinite(db):
        raise ValueError("non-finite endpoint dB")
    db = max(-75.0, min(0.0, float(db)))
    return max(0, min(Q28_ONE, int(round((10.0 ** (db / 20.0)) * Q28_ONE))))


def find_control_numid(card: str = DEFAULT_CARD, amixer: str = "amixer") -> int | None:
    cp = subprocess.run([amixer, "-D", card, "controls"], capture_output=True, text=True)
    if cp.returncode:
        return None
    for line in cp.stdout.splitlines():
        if FINAL_CONTROL_NAME.lower() in line.lower() and "numid=" in line:
            return int(line.split("numid=", 1)[1].split(",", 1)[0])
    return None


def write_final_q28(q28: int, *, card: str = DEFAULT_CARD,
                    helper: Path = DEFAULT_TLV_WRITE, amixer: str = "amixer") -> None:
    if not 0 <= int(q28) <= Q28_ONE:
        raise ValueError("final VOL_CTRL Q28 outside 0..unity")
    numid = find_control_numid(card, amixer)
    if numid is None:
        raise RuntimeError(f"missing ALSA control: {FINAL_CONTROL_NAME}")
    if not helper.exists():
        raise RuntimeError(f"missing TLV helper: {helper}")
    payload = struct.pack("<I", int(q28)).hex()
    cp = subprocess.run([str(helper), card, str(numid), payload], capture_output=True, text=True)
    if cp.returncode:
        raise RuntimeError((cp.stdout + cp.stderr).strip() or f"tlv_write rc={cp.returncode}")


def dsp_apply_state(state: tuple[float, bool], hardware_id: int | None, control: Path,
                    dry_run: bool, last: tuple[int, int] | None,
                    wpctl: str = "wpctl") -> tuple[int, int] | None:
    pipewire_gain, muted = state
    ui_scalar, endpoint_db, postgain, _host_scalar = base.derive_windows_state(pipewire_gain, muted)
    q28 = endpoint_q28_from_db(endpoint_db)
    signature = (postgain, q28)
    if signature == last:
        return last
    if hardware_id is None and not dry_run:
        raise RuntimeError(f"hardware node not found: {base.DEFAULT_HARDWARE_NODE}")
    if not dry_run:
        base.write_postgain_request(control, postgain)
        # Safe handover: attenuate inside final DSP first. The topology already
        # carries Windows 0x1037 ramp policy 10 ms / 1000 us / curve 3.
        write_final_q28(q28)
        # Remove the former host actuator only after DSP attenuation is active.
        base.set_hardware_volume(hardware_id, 1.0, wpctl)
    print(
        f"pipewire_gain={pipewire_gain:.9g} ui_scalar={ui_scalar:.6f} "
        f"windows_db={endpoint_db:.3f} postgain={postgain} "
        f"final_q28=0x{q28:08x} hardware_scalar=1.000000 "
        f"muted={'yes' if muted else 'no'}",
        flush=True,
    )
    return signature


def restore_host_actuator(args: argparse.Namespace) -> int:
    entries = base.snapshot(args.pw_dump)
    state = base.extract_node_volume(entries, args.node)
    if state is None:
        print(f"node not found or has no volume Props: {args.node}", file=sys.stderr)
        return 3
    hardware_id = base.extract_node_id(entries, args.hardware_node)
    if hardware_id is None:
        print(f"hardware node not found: {args.hardware_node}", file=sys.stderr)
        return 3
    pipewire_gain, muted = state
    ui_scalar, endpoint_db, postgain, host_scalar = base.derive_windows_state(pipewire_gain, muted)
    if not args.dry_run:
        base.write_postgain_request(args.control, postgain)
        # Safe return: establish host attenuation before removing DSP attenuation.
        base.set_hardware_volume(hardware_id, host_scalar, args.wpctl)
        write_final_q28(Q28_ONE)
    print(
        f"restore ui_scalar={ui_scalar:.6f} windows_db={endpoint_db:.3f} "
        f"postgain={postgain} hardware_scalar={host_scalar:.6f} "
        f"final_q28=0x{Q28_ONE:08x}",
        flush=True,
    )
    return 0


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--node", default=base.DEFAULT_NODE)
    p.add_argument("--hardware-node", default=base.DEFAULT_HARDWARE_NODE)
    p.add_argument("--control", type=Path, default=base.default_control_path())
    p.add_argument("--pw-dump", default="pw-dump")
    p.add_argument("--wpctl", default="wpctl")
    p.add_argument("--once", action="store_true")
    p.add_argument("--restore-host-actuator", action="store_true")
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("--settle-ms", type=int, default=200)
    p.add_argument("--bootstrap-ms", type=int, default=5000)
    p.add_argument("--bootstrap-guard-ms", type=int, default=1000)
    return p


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    try:
        if args.restore_host_actuator:
            return restore_host_actuator(args)
        # Reuse the production monitor/bootstrap implementation but substitute
        # only the endpoint actuator operation.
        base.apply_state = dsp_apply_state
        return base.run_once(args) if args.once else base.run_monitor(args)
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as exc:
        print(f"sp11-volume-actuator-ab: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
