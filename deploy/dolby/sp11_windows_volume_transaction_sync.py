#!/usr/bin/env python3
"""Apply the recovered SP11 Windows endpoint-volume transaction atomically.

This synchronizer is selected only when the candidate kernel exposes
``SP11 Windows Volume Transaction``.  It writes Dolby postgain, then asks the
kernel to send final VOL_CTRL followed by the complete four-frame GainStep OOB
delta.  Only after that succeeds does it move the hidden ALSA sink to unity.

When the graph is idle, or if the transaction fails, the hidden sink retains
the Windows endpoint attenuation.  This makes handover fail quiet rather than
creating a full-volume interval.  A rollback kernel lacks the candidate
control and the dispatcher automatically runs the established host actuator.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import math
import signal
import struct
import subprocess
import sys
import time
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parents[1] if SCRIPT_DIR.name == "dolby" else None


def load_module(source_name: str, installed_name: str, module_name: str):
    candidates = []
    if ROOT is not None:
        candidates.append(ROOT / "deploy/dolby" / source_name)
    candidates.append(Path.home() / ".local/bin" / installed_name)
    path = next((candidate for candidate in candidates if candidate.exists()), None)
    if path is None:
        raise RuntimeError(f"missing dependency: {installed_name}")
    spec = importlib.util.spec_from_file_location(module_name, path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


base = load_module(
    "sp11_dolby_volume_sync.py", "sp11-dolby-volume-sync", "sp11_volume_base"
)
msiir = load_module(
    "sp11_msiir_volume_sync.py", "sp11-msiir-volume-sync", "sp11_msiir_base"
)

CONTROL_NAME = "SP11 Windows Volume Transaction"
DEFAULT_CARD = "hw:0"
DEFAULT_PCM_STATUS = Path("/proc/asound/card0/pcm0p/sub0/status")
DEFAULT_TLV_WRITE = Path.home() / ".local/lib/sp11-dolby/tlv_write"
DEFAULT_DELTA_TABLE = (
    ROOT / "deploy/dolby/sp11_gainstep_runtime_deltas.json"
    if ROOT is not None
    else Path.home() / ".local/share/sp11-audio/sp11-gainstep-runtime-deltas.json"
)
Q28_ONE = 1 << 28
EXPECTED_ACDB_SHA256 = (
    "a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde"
)


def qcadcm_q28_from_db(db: float) -> int:
    if not math.isfinite(db):
        raise ValueError("non-finite endpoint dB")
    db = max(-75.0, min(0.0, float(db)))
    q16 = round(db * 65536.0)
    quarter_index = ((-q16) >> 14) if q16 < 0 else 0
    effective_db = -(quarter_index / 4.0)
    return max(
        0, min(Q28_ONE, round((10.0 ** (effective_db / 20.0)) * Q28_ONE))
    )


def load_deltas(path: Path) -> tuple[bytes, ...]:
    data = json.loads(path.read_text())
    if data.get("source_sha256") != EXPECTED_ACDB_SHA256:
        raise ValueError("unexpected GainStep delta ACDB provenance")
    steps = data.get("steps")
    if not isinstance(steps, list) or len(steps) != 30:
        raise ValueError("GainStep delta table must contain 30 rows")
    result = []
    for expected_step, row in enumerate(steps, 1):
        if row.get("gain_step") != expected_step:
            raise ValueError("GainStep delta rows are not ordered 1..30")
        blob = bytes.fromhex(row.get("serialized_hex", ""))
        expected_size = 272 if expected_step in (3, 9, 24) else 216
        if len(blob) != expected_size:
            raise ValueError(f"invalid GainStep {expected_step} delta size")
        if hashlib.sha256(blob).hexdigest() != row.get("serialized_sha256"):
            raise ValueError(f"invalid GainStep {expected_step} delta hash")
        result.append(blob)
    return tuple(result)


def find_control_numid(card: str = DEFAULT_CARD, amixer: str = "amixer") -> int | None:
    cp = subprocess.run(
        [amixer, "-D", card, "controls"], capture_output=True, text=True
    )
    if cp.returncode:
        return None
    for line in cp.stdout.splitlines():
        if CONTROL_NAME.lower() in line.lower() and "numid=" in line:
            return int(line.split("numid=", 1)[1].split(",", 1)[0])
    return None


def write_transaction(q28: int, delta: bytes, *, helper: Path, card: str,
                      numid: int) -> None:
    if not 0 <= q28 <= Q28_ONE:
        raise ValueError("final VOL_CTRL Q28 outside 0..unity")
    if len(delta) not in (216, 272):
        raise ValueError("invalid GainStep delta size")
    blob = struct.pack("<I", q28) + delta
    cp = subprocess.run(
        [str(helper), card, str(numid), blob.hex()], capture_output=True, text=True
    )
    if cp.returncode:
        output = (cp.stdout + cp.stderr).strip()
        raise RuntimeError(output or f"tlv_write rc={cp.returncode}")


def restore_host_attenuation(state: tuple[float, bool], hardware_id: int,
                             wpctl: str) -> tuple[int, int]:
    pipewire_gain, muted = state
    _ui, _db, postgain, host_scalar = base.derive_windows_state(
        pipewire_gain, muted
    )
    base.set_hardware_volume(hardware_id, host_scalar, wpctl)
    return postgain, round(host_scalar * 1_000_000_000)


def apply_transaction(state: tuple[float, bool], hardware_id: int,
                      control: Path, deltas: tuple[bytes, ...], helper: Path,
                      card: str, numid: int, wpctl: str) -> tuple[int, int]:
    pipewire_gain, muted = state
    ui_scalar, endpoint_db, postgain, _host_scalar = base.derive_windows_state(
        pipewire_gain, muted
    )
    q28 = qcadcm_q28_from_db(endpoint_db)
    step = msiir.select_ckv_step_q28(q28)
    base.write_postgain_request(control, postgain)
    write_transaction(q28, deltas[step - 1], helper=helper, card=card, numid=numid)
    base.set_hardware_volume(hardware_id, 1.0, wpctl)
    print(
        f"pipewire_gain={pipewire_gain:.9g} ui_scalar={ui_scalar:.6f} "
        f"windows_db={endpoint_db:.3f} postgain={postgain} "
        f"final_q28=0x{q28:08x} gainstep={step} "
        f"delta={len(deltas[step - 1])} hardware_scalar=1.000000 "
        f"muted={'yes' if muted else 'no'}",
        flush=True,
    )
    return postgain, q28


def run(args: argparse.Namespace) -> int:
    deltas = load_deltas(args.delta_table)
    numid = find_control_numid(args.card, args.amixer)
    if numid is None:
        print(f"missing ALSA control: {CONTROL_NAME}", file=sys.stderr)
        return 4
    if not args.tlv_write.exists():
        print(f"missing TLV helper: {args.tlv_write}", file=sys.stderr)
        return 5

    last: tuple[int, int] | None = None
    last_host: tuple[int, int] | None = None
    transaction_active = False
    current_state: tuple[float, bool] | None = None
    current_hardware_id: int | None = None
    try:
        while True:
            entries = base.snapshot(args.pw_dump)
            state = base.extract_node_volume(entries, args.node)
            hardware_id = base.extract_node_id(entries, args.hardware_node)
            if state is None or hardware_id is None:
                time.sleep(args.interval_ms / 1000.0)
                continue
            current_state = state
            current_hardware_id = hardware_id

            running = msiir.graph_running(args.pcm_status)
            if not running:
                pipewire_gain, muted = state
                _ui, _db, postgain, host_scalar = base.derive_windows_state(
                    pipewire_gain, muted
                )
                host_signature = (postgain, round(host_scalar * 1_000_000_000))
                if transaction_active or host_signature != last_host:
                    last_host = restore_host_attenuation(
                        state, hardware_id, args.wpctl
                    )
                transaction_active = False
                if args.once:
                    print("graph idle; host attenuation established")
                    return 0
                time.sleep(args.interval_ms / 1000.0)
                continue

            pipewire_gain, muted = state
            _ui, endpoint_db, postgain, _host = base.derive_windows_state(
                pipewire_gain, muted
            )
            signature = (postgain, qcadcm_q28_from_db(endpoint_db))
            if not transaction_active or signature != last:
                try:
                    last = apply_transaction(
                        state, hardware_id, args.control, deltas, args.tlv_write,
                        args.card, numid, args.wpctl
                    )
                    last_host = None
                    transaction_active = True
                except RuntimeError as exc:
                    last_host = restore_host_attenuation(
                        state, hardware_id, args.wpctl
                    )
                    transaction_active = False
                    print(
                        f"transaction failed safely: {exc}",
                        file=sys.stderr,
                        flush=True,
                    )
            if args.once:
                return 0
            time.sleep(args.interval_ms / 1000.0)
    finally:
        if (
            transaction_active
            and not args.once
            and current_state is not None
            and current_hardware_id is not None
        ):
            try:
                restore_host_attenuation(
                    current_state, current_hardware_id, args.wpctl
                )
            except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
                print(
                    f"could not restore host attenuation on exit: {exc}",
                    file=sys.stderr,
                    flush=True,
                )


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--node", default=base.DEFAULT_NODE)
    p.add_argument("--hardware-node", default=base.DEFAULT_HARDWARE_NODE)
    p.add_argument("--control", type=Path, default=base.default_control_path())
    p.add_argument("--card", default=DEFAULT_CARD)
    p.add_argument("--pcm-status", type=Path, default=DEFAULT_PCM_STATUS)
    p.add_argument("--tlv-write", type=Path, default=DEFAULT_TLV_WRITE)
    p.add_argument("--delta-table", type=Path, default=DEFAULT_DELTA_TABLE)
    p.add_argument("--pw-dump", default="pw-dump")
    p.add_argument("--wpctl", default="wpctl")
    p.add_argument("--amixer", default="amixer")
    p.add_argument("--interval-ms", type=int, default=100)
    p.add_argument("--once", action="store_true")
    return p


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    if args.interval_ms < 20:
        print("interval must be >=20 ms", file=sys.stderr)
        return 2
    if not args.once:
        signal.signal(signal.SIGTERM, lambda _signum, _frame: sys.exit(0))
    try:
        return run(args)
    except KeyboardInterrupt:
        return 0
    except (OSError, RuntimeError, subprocess.SubprocessError,
            ValueError, json.JSONDecodeError) as exc:
        print(f"sp11-windows-volume-transaction-sync: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
