#!/usr/bin/env python3
"""Mirror PipeWire endpoint attenuation into original Dolby VLLDP postgain.

Windows DAX derives VLLDP postgain from endpoint master-volume dB:
    postgain = round(master_volume_db * 16)
with the SP11 endpoint range bounded to -75..0 dB (-1200..0 units).

WirePlumber exposes a cubic UI value, but pw-dump's channelVolumes are the
actual linear per-channel gains.  This helper subscribes to pw-dump monitor
updates, converts the raw channel gain to dB, and writes only the postgain
request slot in the existing shared Dolby control page.  The LADSPA callback
never performs filesystem I/O.
"""

from __future__ import annotations

import argparse
import codecs
import json
import math
import os
import struct
import subprocess
import sys
from pathlib import Path
from typing import Any, Iterable

CONTROL_BYTES = 12
PROFILE_REQUEST_OFF = 0
PROFILE_ACK_OFF = 1
POSTGAIN_REQUEST_OFF = 4
POSTGAIN_ACK_OFF = 8
POSTGAIN_NONE = -(1 << 31)
POSTGAIN_MIN = -1200
POSTGAIN_MAX = 0
DEFAULT_NODE = "effect_input.sp11_windows_dolby"
DEFAULT_CONTROL_BASENAME = "sp11-dolby-profile.control"


def default_control_path() -> Path:
    runtime = os.environ.get("XDG_RUNTIME_DIR") or f"/run/user/{os.getuid()}"
    return Path(runtime) / DEFAULT_CONTROL_BASENAME


def postgain_from_linear_gain(gain: float, muted: bool = False) -> int:
    """Convert actual linear endpoint gain to SP11 DAX 1/16-dB postgain."""
    if muted or not math.isfinite(gain) or gain <= 0.0:
        return POSTGAIN_MIN
    db = 20.0 * math.log10(gain)
    db = max(-75.0, min(0.0, db))
    # Python round is nearest-even at exact half-way values. PipeWire's float
    # volume almost never lands exactly on a 1/32-dB tie; this matches the
    # recovered DAX round(master_volume_db * 16) relation for real states.
    return max(POSTGAIN_MIN, min(POSTGAIN_MAX, int(round(db * 16.0))))


def _props_for_node(entry: dict[str, Any]) -> Iterable[dict[str, Any]]:
    info = entry.get("info") or {}
    props = info.get("params", {}).get("Props") or []
    for item in props:
        if isinstance(item, dict):
            yield item


def extract_node_volume(entries: Any, node_name: str = DEFAULT_NODE) -> tuple[float, bool] | None:
    """Return (linear master gain proxy, muted) from a pw-dump JSON value.

    For ordinary stereo master-volume changes both channelVolumes are equal.
    If balance makes them unequal, use the maximum channel gain as the master
    proxy so a balance reduction on one side is not misinterpreted as master
    attenuation.
    """
    if not isinstance(entries, list):
        return None
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        info = entry.get("info") or {}
        node_props = info.get("props") or {}
        if node_props.get("node.name") != node_name:
            continue
        for props in _props_for_node(entry):
            volumes = props.get("channelVolumes")
            if isinstance(volumes, list) and volumes:
                finite = [float(v) for v in volumes if isinstance(v, (int, float)) and math.isfinite(float(v))]
                if finite:
                    return max(finite), bool(props.get("mute", False) or props.get("softMute", False))
            volume = props.get("volume")
            if isinstance(volume, (int, float)) and math.isfinite(float(volume)):
                return float(volume), bool(props.get("mute", False) or props.get("softMute", False))
    return None


def read_control_postgain(path: Path) -> tuple[int | None, int | None]:
    try:
        data = path.read_bytes()
    except FileNotFoundError:
        return None, None
    req = struct.unpack_from("<i", data, POSTGAIN_REQUEST_OFF)[0] if len(data) >= POSTGAIN_REQUEST_OFF + 4 else None
    ack = struct.unpack_from("<i", data, POSTGAIN_ACK_OFF)[0] if len(data) >= POSTGAIN_ACK_OFF + 4 else None
    return req, ack


def write_postgain_request(path: Path, value: int) -> None:
    if not POSTGAIN_MIN <= value <= POSTGAIN_MAX:
        raise ValueError(f"postgain out of range: {value}")
    path.parent.mkdir(parents=True, exist_ok=True)
    fd = os.open(path, os.O_RDWR | os.O_CREAT | os.O_CLOEXEC, 0o600)
    try:
        os.fchmod(fd, 0o600)
        old_size = os.fstat(fd).st_size
        if old_size < CONTROL_BYTES:
            os.ftruncate(fd, CONTROL_BYTES)
            # Initialize only slots that did not previously exist. Never
            # overwrite queued/applied profile bytes from the existing helper.
            if old_size < POSTGAIN_REQUEST_OFF + 4:
                os.pwrite(fd, struct.pack("<i", POSTGAIN_NONE), POSTGAIN_REQUEST_OFF)
            if old_size < POSTGAIN_ACK_OFF + 4:
                os.pwrite(fd, struct.pack("<i", POSTGAIN_NONE), POSTGAIN_ACK_OFF)
        os.pwrite(fd, struct.pack("<i", value), POSTGAIN_REQUEST_OFF)
    finally:
        os.close(fd)


def iter_json_stream(fd: int) -> Iterable[Any]:
    decoder = json.JSONDecoder()
    utf8 = codecs.getincrementaldecoder("utf-8")()
    buf = ""
    while True:
        chunk = os.read(fd, 65536)
        if not chunk:
            break
        buf += utf8.decode(chunk)
        while True:
            stripped = buf.lstrip()
            if len(stripped) != len(buf):
                buf = stripped
            if not buf:
                break
            try:
                value, end = decoder.raw_decode(buf)
            except json.JSONDecodeError:
                break
            yield value
            buf = buf[end:]
    buf += utf8.decode(b"", final=True)
    if buf.strip():
        value, end = decoder.raw_decode(buf.lstrip())
        yield value


def describe(gain: float, muted: bool, postgain: int) -> str:
    db = -math.inf if gain <= 0.0 else 20.0 * math.log10(gain)
    db_text = "-inf" if not math.isfinite(db) else f"{db:.3f}"
    return f"linear_gain={gain:.9g} endpoint_db={db_text} postgain={postgain} muted={'yes' if muted else 'no'}"


def sync_value(entries: Any, node_name: str, control: Path, dry_run: bool, last: int | None) -> int | None:
    state = extract_node_volume(entries, node_name)
    if state is None:
        return last
    gain, muted = state
    value = postgain_from_linear_gain(gain, muted)
    if value == last:
        return last
    if not dry_run:
        write_postgain_request(control, value)
    print(describe(gain, muted, value), flush=True)
    return value


def run_once(args: argparse.Namespace) -> int:
    cp = subprocess.run([args.pw_dump], check=True, stdout=subprocess.PIPE, text=True)
    entries = json.loads(cp.stdout)
    state = extract_node_volume(entries, args.node)
    if state is None:
        print(f"node not found or has no volume Props: {args.node}", file=sys.stderr)
        return 3
    gain, muted = state
    value = postgain_from_linear_gain(gain, muted)
    if not args.dry_run:
        write_postgain_request(args.control, value)
    print(describe(gain, muted, value))
    return 0


def run_monitor(args: argparse.Namespace) -> int:
    proc = subprocess.Popen([args.pw_dump, "-m"], stdout=subprocess.PIPE, stderr=None)
    assert proc.stdout is not None
    last: int | None = None
    try:
        for value in iter_json_stream(proc.stdout.fileno()):
            last = sync_value(value, args.node, args.control, args.dry_run, last)
    finally:
        if proc.poll() is None:
            proc.terminate()
        proc.wait()
    return proc.returncode or 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--node", default=DEFAULT_NODE)
    p.add_argument("--control", type=Path, default=default_control_path())
    p.add_argument("--pw-dump", default="pw-dump")
    p.add_argument("--once", action="store_true", help="read one pw-dump snapshot and exit")
    p.add_argument("--dry-run", action="store_true", help="print conversion without writing the control page")
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return run_once(args) if args.once else run_monitor(args)
    except (OSError, subprocess.SubprocessError, json.JSONDecodeError) as exc:
        print(f"sp11-dolby-volume-sync: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
