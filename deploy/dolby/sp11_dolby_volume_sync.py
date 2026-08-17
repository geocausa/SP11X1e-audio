#!/usr/bin/env python3
"""Reproduce the SP11 Windows endpoint taper and feed its dB state to Dolby.

Windows DAX derives VLLDP postgain from endpoint master-volume dB:
    postgain = round(master_volume_db * 16)
with the SP11 endpoint range bounded to -75..0 dB (-1200..0 units).

PipeWire/WirePlumber exposes a cubic UI control: pw-dump channelVolumes contain
UI_scalar**3. Windows IAudioEndpointVolume instead exposes a nonlinear,
audio-tapered scalar. Fresh SP11 Windows measurement on 2026-08-12 captured the
live built-in-speaker endpoint at scalar increments of 0.005 from 0..1. This
helper therefore treats the virtual Dolby sink as the user-facing scalar,
recovers that scalar from PipeWire's cubic gain, applies the pinned Windows
scalar->dB curve, and then does two coordinated things:

  1. writes DAX/VLLDP postgain from that Windows endpoint dB;
  2. sets the downstream ALSA sink to the matching linear endpoint gain.

The virtual Dolby sink retains the user's visible slider value. Only the hidden
downstream hardware sink is re-tapered, so endpoint attenuation remains after
Dolby/AudioEngine processing as it is on Windows. The separate MSIIR service
reads the same postgain slot and therefore selects the matching Windows CKV.

Pinned Windows endpoint evidence:
  endpoint {0.0.0.00000000}.{5bb689e6-2c6b-4357-b4c1-beb815638f88}
  dB range -75..0, hardware increment 0.5 dB, 51 software step positions
  original Windows CSV sha256 cbaa8bf2149becf82d6eeac2613ba1acb3d7244fe101ff82070b15f591a471fe
  normalized SP7 mirror sha256 c4a0e0d93ffc40765ea2c9c861ac201ad60be0620f2675a1879608b48a143faf
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
import time
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
DEFAULT_HARDWARE_NODE = "alsa_output.platform-sound.HiFi__Speaker__sink"
DEFAULT_CONTROL_BASENAME = "sp11-dolby-profile.control"
WINDOWS_TAPER_SPACING = 0.005

# Fresh live SP11 Windows IAudioEndpointVolume measurements at scalar
# 0.000, 0.005, ... 1.000. Keep this table evidence-pinned; do not replace it
# with a guessed power law. Linear interpolation is used only between measured
# scalar positions; ordinary integer-percent UI positions land exactly on rows.
WINDOWS_ENDPOINT_DB = (
    -75, -67.026237487793, -61.8230285644531, -57.9539031982422,
    -54.8725051879883, -52.3117065429688, -50.1207695007324,
    -48.2062034606934, -46.5060386657715, -44.97705078125,
    -43.5879058837891, -42.3151512145996, -41.1407814025879,
    -40.0506706237793, -39.0335350036621, -38.0802116394043,
    -37.1831703186035, -36.3361282348633, -35.533805847168,
    -34.7717170715332, -34.0460205078125, -33.353401184082,
    -32.6909675598145, -32.0562019348145, -31.4468841552734,
    -30.8610534667969, -30.2969665527344, -29.7530632019043,
    -29.227954864502, -28.7203826904297, -28.2292098999023,
    -27.7534160614014, -27.292064666748, -26.8443031311035,
    -26.4093551635742, -25.9865074157715, -25.5751037597656,
    -25.1745452880859, -24.7842693328857, -24.4037647247314,
    -24.0325527191162, -23.6701908111572, -23.3162651062012,
    -22.9703941345215, -22.6322193145752, -22.3014030456543,
    -21.9776344299316, -21.6606178283691, -21.3500804901123,
    -21.0457572937012, -20.7474098205566, -20.4548034667969,
    -20.1677265167236, -19.8859691619873, -19.609338760376,
    -19.3376541137695, -19.0707397460938, -18.8084335327148,
    -18.5505752563477, -18.2970180511475, -18.0476207733154,
    -17.8022518157959, -17.5607795715332, -17.3230838775635,
    -17.0890483856201, -16.8585605621338, -16.6315155029297,
    -16.4078121185303, -16.1873531341553, -15.9700469970703,
    -15.7558031082153, -15.5445375442505, -15.3361663818359,
    -15.1306142807007, -14.9278049468994, -14.7276668548584,
    -14.5301284790039, -14.3351249694824, -14.1425914764404,
    -13.9524641036987, -13.764687538147, -13.5792026519775,
    -13.3959531784058, -13.2148876190186, -13.0359525680542,
    -12.8590984344482, -12.6842803955078, -12.5114488601685,
    -12.3405609130859, -12.1715726852417, -12.0044431686401,
    -11.839129447937, -11.6755952835083, -11.5138025283813,
    -11.3537130355835, -11.1952924728394, -11.0385055541992,
    -10.883318901062, -10.7297019958496, -10.5776214599609,
    -10.4270467758179, -10.2779502868652, -10.1303014755249,
    -9.9840726852417, -9.83923816680908, -9.69577026367188,
    -9.55364322662354, -9.41283226013184, -9.27331447601318,
    -9.13506603240967, -8.99806308746338, -8.86228466033936,
    -8.727707862854, -8.59431266784668, -8.46207809448242,
    -8.33098316192627, -8.20100975036621, -8.0721378326416,
    -7.94434976577759, -7.81762552261353, -7.69195032119751,
    -7.56730604171753, -7.44367504119873, -7.32104206085205,
    -7.19938993453979, -7.07870388031006, -6.95896863937378,
    -6.84016942977905, -6.72229099273682, -6.60531997680664,
    -6.48924207687378, -6.3740439414978, -6.25971126556396,
    -6.14623308181763, -6.03359603881836, -5.92178726196289,
    -5.81079530715942, -5.70060729980469, -5.5912127494812,
    -5.48260021209717, -5.37475776672363, -5.26767492294312,
    -5.16134166717529, -5.05574655532837, -4.95087909698486,
    -4.84673118591309, -4.74329233169556, -4.64055252075195,
    -4.53850221633911, -4.43713283538818, -4.33643484115601,
    -4.23639917373657, -4.13701820373535, -4.03828239440918,
    -3.94018387794495, -3.84271430969238, -3.74586582183838,
    -3.64962935447693, -3.55399966239929, -3.45896768569946,
    -3.36452627182007, -3.27066779136658, -3.17738556861877,
    -3.08467221260071, -2.99252080917358, -2.90092492103577,
    -2.80987739562988, -2.71937227249146, -2.62940263748169,
    -2.53996157646179, -2.45104455947876, -2.3626446723938,
    -2.27475595474243, -2.18737244606018, -2.10048866271973,
    -2.01409840583801, -1.9281964302063, -1.84277725219727,
    -1.75783538818359, -1.67336547374725, -1.58936250209808,
    -1.50582110881805, -1.42273545265198, -1.34010243415833,
    -1.25791621208191, -1.17617189884186, -1.09486496448517,
    -1.01399052143097, -0.933544158935547, -0.853521287441254,
    -0.773917496204376, -0.694728434085846, -0.615949749946594,
    -0.537577271461487, -0.459605902433395, -0.382033348083496,
    -0.304854691028595, -0.228065893054008, -0.151663094758987,
    -0.075642392039299, 0,
)

if len(WINDOWS_ENDPOINT_DB) != 201:
    raise RuntimeError("Windows endpoint taper table must contain 201 samples")


def default_control_path() -> Path:
    runtime = os.environ.get("XDG_RUNTIME_DIR") or f"/run/user/{os.getuid()}"
    return Path(runtime) / DEFAULT_CONTROL_BASENAME


def pipewire_ui_scalar_from_linear_gain(gain: float) -> float:
    """Recover WirePlumber's visible slider scalar from its cubic linear gain."""
    if not math.isfinite(gain) or gain <= 0.0:
        return 0.0
    return max(0.0, min(1.0, gain ** (1.0 / 3.0)))


def windows_endpoint_db_from_ui_scalar(scalar: float) -> float:
    """Interpolate the pinned live-Windows SP11 scalar->endpoint-dB taper."""
    if not math.isfinite(scalar) or scalar <= 0.0:
        return -75.0
    if scalar >= 1.0:
        return 0.0
    pos = scalar / WINDOWS_TAPER_SPACING
    lo = int(math.floor(pos))
    hi = min(lo + 1, len(WINDOWS_ENDPOINT_DB) - 1)
    frac = pos - lo
    return WINDOWS_ENDPOINT_DB[lo] + (WINDOWS_ENDPOINT_DB[hi] - WINDOWS_ENDPOINT_DB[lo]) * frac


def windows_endpoint_linear_gain_from_ui_scalar(scalar: float) -> float:
    return 10.0 ** (windows_endpoint_db_from_ui_scalar(scalar) / 20.0)


def hardware_wpctl_scalar_from_ui_scalar(scalar: float) -> float:
    """Scalar wpctl needs so its cubic sink gain equals Windows endpoint gain."""
    return windows_endpoint_linear_gain_from_ui_scalar(scalar) ** (1.0 / 3.0)


def postgain_from_linear_gain(gain: float, muted: bool = False) -> int:
    """Convert an actual linear endpoint gain to recovered DAX 1/16-dB units."""
    if muted or not math.isfinite(gain) or gain <= 0.0:
        return POSTGAIN_MIN
    db = max(-75.0, min(0.0, 20.0 * math.log10(gain)))
    return max(POSTGAIN_MIN, min(POSTGAIN_MAX, int(round(db * 16.0))))


def postgain_from_ui_scalar(scalar: float, muted: bool = False) -> int:
    if muted:
        return POSTGAIN_MIN
    db = windows_endpoint_db_from_ui_scalar(scalar)
    return max(POSTGAIN_MIN, min(POSTGAIN_MAX, int(round(db * 16.0))))


def _props_for_node(entry: dict[str, Any]) -> Iterable[dict[str, Any]]:
    info = entry.get("info") or {}
    props = info.get("params", {}).get("Props") or []
    for item in props:
        if isinstance(item, dict):
            yield item


def extract_node_volume(entries: Any, node_name: str = DEFAULT_NODE) -> tuple[float, bool] | None:
    """Return (PipeWire linear master-gain proxy, muted) for one node."""
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


def extract_node_id(entries: Any, node_name: str) -> int | None:
    if not isinstance(entries, list):
        return None
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        info = entry.get("info") or {}
        props = info.get("props") or {}
        if props.get("node.name") == node_name and isinstance(entry.get("id"), int):
            return int(entry["id"])
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
        value, _end = decoder.raw_decode(buf.lstrip())
        yield value


def snapshot(pw_dump: str) -> Any:
    cp = subprocess.run([pw_dump], check=True, stdout=subprocess.PIPE, text=True)
    return json.loads(cp.stdout)


def set_hardware_volume(node_id: int, scalar: float, wpctl: str = "wpctl") -> None:
    scalar = max(0.0, min(1.0, float(scalar)))
    subprocess.run([wpctl, "set-volume", str(node_id), f"{scalar:.12f}"], check=True,
                   stdout=subprocess.DEVNULL)


def set_hardware_mute(node_id: int, muted: bool, wpctl: str = "wpctl") -> None:
    """Fail-safe endpoint mute below the protected DSP graph.

    Windows has a dedicated final VOL_CTRL multichannel-mute transaction
    (0x4a63/0x08001039).  The current Linux kernel transaction control carries
    gain + GainStep only, so until that exact DSP mute actuator is promoted we
    mirror the endpoint mute at the hidden downstream sink.  Volume is always
    established before this switch is changed so unmute cannot expose unity.
    """
    subprocess.run([wpctl, "set-mute", str(node_id), "1" if muted else "0"],
                   check=True, stdout=subprocess.DEVNULL)


def derive_windows_state(pipewire_gain: float, muted: bool = False) -> tuple[float, float, int, float]:
    ui_scalar = pipewire_ui_scalar_from_linear_gain(pipewire_gain)
    endpoint_db = windows_endpoint_db_from_ui_scalar(ui_scalar)
    postgain = postgain_from_ui_scalar(ui_scalar, muted)
    hardware_scalar = hardware_wpctl_scalar_from_ui_scalar(ui_scalar)
    return ui_scalar, endpoint_db, postgain, hardware_scalar


def describe(pipewire_gain: float, muted: bool, ui_scalar: float, endpoint_db: float,
             postgain: int, hardware_scalar: float) -> str:
    return (f"pipewire_gain={pipewire_gain:.9g} ui_scalar={ui_scalar:.6f} "
            f"windows_db={endpoint_db:.3f} postgain={postgain} "
            f"hardware_scalar={hardware_scalar:.6f} muted={'yes' if muted else 'no'}")


def apply_state(state: tuple[float, bool], hardware_id: int | None, control: Path,
                dry_run: bool, last: tuple[int, int, bool] | None,
                wpctl: str = "wpctl") -> tuple[int, int, bool] | None:
    pipewire_gain, muted = state
    ui_scalar, endpoint_db, postgain, hardware_scalar = derive_windows_state(pipewire_gain, muted)
    # Signature is stable enough to suppress monitor echoes from our hardware
    # update while still responding immediately to a user slider/mute change.
    signature = (postgain, int(round(hardware_scalar * 1_000_000_000)), muted)
    if signature == last:
        return last
    if hardware_id is None and not dry_run:
        raise RuntimeError(f"hardware node not found: {DEFAULT_HARDWARE_NODE}")
    if not dry_run:
        write_postgain_request(control, postgain)
        # Endpoint attenuation is downstream of Dolby/AudioEngine on Windows.
        # Directly overriding the hidden ALSA sink leaves the virtual sink's
        # visible user scalar untouched; passive PipeWire forwarding is thus
        # converted from its cubic taper to the pinned Windows taper here.
        set_hardware_volume(hardware_id, hardware_scalar, wpctl)
        set_hardware_mute(hardware_id, muted, wpctl)
    print(describe(pipewire_gain, muted, ui_scalar, endpoint_db, postgain, hardware_scalar), flush=True)
    return signature


def settled_state(pw_dump: str, node_name: str, hardware_name: str, delay_ms: int) -> tuple[tuple[float, bool] | None, int | None]:
    if delay_ms > 0:
        time.sleep(delay_ms / 1000.0)
    entries = snapshot(pw_dump)
    return extract_node_volume(entries, node_name), extract_node_id(entries, hardware_name)


def run_once(args: argparse.Namespace) -> int:
    entries = snapshot(args.pw_dump)
    state = extract_node_volume(entries, args.node)
    if state is None:
        print(f"node not found or has no volume Props: {args.node}", file=sys.stderr)
        return 3
    hardware_id = extract_node_id(entries, args.hardware_node)
    apply_state(state, hardware_id, args.control, args.dry_run, None, args.wpctl)
    return 0


def run_monitor(args: argparse.Namespace) -> int:
    # Subscribe before taking the initial full snapshot so a filter/ALSA node
    # created during startup cannot fall into the gap between snapshot and
    # monitor attachment.  Cold boot proved that relying on pw-dump -m to emit
    # a complete initial Props object is not sufficient.
    proc = subprocess.Popen([args.pw_dump, "-m"], stdout=subprocess.PIPE, stderr=None)
    assert proc.stdout is not None
    last: tuple[int, int, bool] | None = None
    hardware_id: int | None = None
    try:
        # filter-chain.service starts in the same user-session transaction and
        # can publish its virtual sink more than a few hundred milliseconds
        # after this daemon.  Retry complete snapshots during bootstrap instead
        # of depending on one timing point or on pw-dump -m to replay Props.
        if args.settle_ms > 0:
            time.sleep(args.settle_ms / 1000.0)
        deadline = time.monotonic() + (args.bootstrap_ms / 1000.0)
        while last is None:
            initial, snap_hw = settled_state(
                args.pw_dump, args.node, args.hardware_node, 0
            )
            hardware_id = snap_hw or hardware_id
            if initial is not None and hardware_id is not None:
                last = apply_state(initial, hardware_id, args.control, args.dry_run, None, args.wpctl)
                break
            if time.monotonic() >= deadline:
                break
            time.sleep(0.1)

        # pw-dump -m can replay stale transient-unity Props that were queued
        # while WirePlumber was still restoring the node. During a short guard
        # window, resolve any volume delta against a fresh full snapshot. This
        # preserves genuine user changes while preventing old queued values from
        # temporarily undoing the settled Windows taper at autoplay time.
        guard_until = time.monotonic() + (args.bootstrap_guard_ms / 1000.0) if last is not None else 0.0

        for value in iter_json_stream(proc.stdout.fileno()):
            found_hw = extract_node_id(value, args.hardware_node)
            if found_hw is not None and found_hw != hardware_id:
                hardware_id = found_hw
                # A recreated ALSA node starts with WirePlumber's saved cubic
                # volume. Force the Windows endpoint correction back onto it.
                if last is not None:
                    state, snap_hw = settled_state(args.pw_dump, args.node, args.hardware_node, 0)
                    hardware_id = snap_hw or hardware_id
                    if state is not None:
                        last = apply_state(state, hardware_id, args.control, args.dry_run, None, args.wpctl)

            state = extract_node_volume(value, args.node)
            if state is not None and last is not None and time.monotonic() < guard_until:
                current, snap_hw = settled_state(args.pw_dump, args.node, args.hardware_node, 0)
                hardware_id = snap_hw or hardware_id
                state = current or state
            if state is None and last is None:
                # Some PipeWire monitor deltas contain only changed object
                # fields and are not self-sufficient for volume extraction. On
                # the first such startup event, recover from a full snapshot.
                state, snap_hw = settled_state(args.pw_dump, args.node, args.hardware_node, 0)
                hardware_id = snap_hw or hardware_id
            if state is None:
                continue
            last = apply_state(state, hardware_id, args.control, args.dry_run, last, args.wpctl)
    finally:
        if proc.poll() is None:
            proc.terminate()
        proc.wait()
    return proc.returncode or 0


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--node", default=DEFAULT_NODE)
    p.add_argument("--hardware-node", default=DEFAULT_HARDWARE_NODE)
    p.add_argument("--control", type=Path, default=default_control_path())
    p.add_argument("--pw-dump", default="pw-dump")
    p.add_argument("--wpctl", default="wpctl")
    p.add_argument("--once", action="store_true", help="read one pw-dump snapshot and exit")
    p.add_argument("--settle-ms", type=int, default=200,
                   help="initial delay before bootstrap snapshots")
    p.add_argument("--bootstrap-ms", type=int, default=5000,
                   help="maximum startup window to wait for virtual/hardware nodes")
    p.add_argument("--bootstrap-guard-ms", type=int, default=1000,
                   help="window to resolve queued monitor deltas against a fresh snapshot")
    p.add_argument("--dry-run", action="store_true",
                   help="print Windows conversion without writing control page or hardware sink")
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        return run_once(args) if args.once else run_monitor(args)
    except (OSError, RuntimeError, subprocess.SubprocessError, json.JSONDecodeError) as exc:
        print(f"sp11-dolby-volume-sync: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
