#!/usr/bin/env python3
"""Apply the recovered SP11 Windows endpoint-volume transaction atomically.

This synchronizer is selected only when the candidate kernel exposes
``SP11 Windows Volume Transaction``.  The live endpoint transaction sends final
VOL_CTRL followed by the complete four-frame GainStep OOB delta.  Only after
that succeeds does it move the hidden ALSA sink to unity.

Fresh same-process Windows speaker-branch memory capture proves that VLLDP
postgain follows endpoint master volume live: the staged/applied fields are
exactly ``round(endpoint_dB * 16)`` at every tested 8..50% point and return to
the original value on the downward sweep.  The control is therefore updated on
ordinary live volume changes as well as forced once for every replacement UbiG
engine generation.

Steady-state volume changes are consumed from ``pw-dump -m`` rather than a
timer.  This preserves the recovered Windows ordering without allowing the
desktop preview sound to run ahead of a 100 ms userspace poll.  The bootstrap
snapshot and stale-event guard are the same proven mechanism used by the
rollback host-volume actuator.

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
import os
import signal
import struct
import subprocess
import sys
import time
from importlib.machinery import SourceFileLoader
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parents[1] if SCRIPT_DIR.name == "ubig" else None


def load_module(source_name: str, installed_name: str, module_name: str):
    candidates = []
    helper_dir = os.environ.get("UBIG_VOLUME_HELPER_DIR")
    if helper_dir:
        candidates.append(Path(helper_dir) / installed_name)
    if ROOT is not None:
        candidates.append(ROOT / "deploy/ubig" / source_name)
    candidates.append(Path.home() / ".local/bin" / installed_name)
    path = next((candidate for candidate in candidates if candidate.exists()), None)
    if path is None:
        raise RuntimeError(f"missing dependency: {installed_name}")
    # Installed helpers intentionally have no .py suffix.  Python cannot infer
    # a loader for those paths, so bind the source loader explicitly.
    loader = SourceFileLoader(module_name, str(path))
    spec = importlib.util.spec_from_file_location(
        module_name, path, loader=loader
    )
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module


base = load_module(
    "sp11_ubig_volume_sync.py", "sp11-ubig-volume-sync", "sp11_volume_base"
)
msiir = load_module(
    "sp11_msiir_volume_sync.py", "sp11-msiir-volume-sync", "sp11_msiir_base"
)

CONTROL_NAME = "SP11 Windows Volume Transaction"
VOLUME_ONLY_CONTROL_NAME = "SP11 Windows Volume Only"
MUTE_CONTROL_NAME = "SP11 Windows Endpoint Mute"
DEFAULT_CARD = "hw:0"
DEFAULT_PCM_STATUS = Path("/proc/asound/card0/pcm0p/sub0/status")
DEFAULT_TLV_WRITE = Path.home() / ".local/lib/ubig-private/tlv_write"
DEFAULT_DELTA_TABLE = (
    ROOT / "deploy/ubig/sp11_gainstep_runtime_deltas.json"
    if ROOT is not None
    else Path.home() / ".local/share/sp11-audio/sp11-gainstep-runtime-deltas.json"
)
Q28_ONE = 1 << 28
SND_SOC_TLV_HDR = 2 * 4
STEREO_GAIN_WORDS = 2 * 4
STEREO_TRANSACTION_CONTROL_VALUES = SND_SOC_TLV_HDR + STEREO_GAIN_WORDS + 272
VOLUME_ONLY_CONTROL_VALUES = SND_SOC_TLV_HDR + STEREO_GAIN_WORDS
EXPECTED_ACDB_SHA256 = (
    "a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde"
)
WSA_RX_CONTROL_NAMES = (
    "WSA WSA_RX0 Digital Volume",
    "WSA WSA_RX1 Digital Volume",
)
WSA_RX_WINDOWS_VALUE = 84
WSA_RX_GOLDEN_BASELINE_VALUE = 81


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


def find_named_control_numid(name: str, card: str = DEFAULT_CARD,
                             amixer: str = "amixer") -> int | None:
    cp = subprocess.run(
        [amixer, "-D", card, "controls"], capture_output=True, text=True
    )
    if cp.returncode:
        return None
    for line in cp.stdout.splitlines():
        if name.lower() in line.lower() and "numid=" in line:
            return int(line.split("numid=", 1)[1].split(",", 1)[0])
    return None


def find_control_numid(card: str = DEFAULT_CARD, amixer: str = "amixer") -> int | None:
    return find_named_control_numid(CONTROL_NAME, card, amixer)


def find_volume_only_control_numid(card: str = DEFAULT_CARD,
                                     amixer: str = "amixer") -> int | None:
    return find_named_control_numid(VOLUME_ONLY_CONTROL_NAME, card, amixer)


def find_mute_control_numid(card: str = DEFAULT_CARD,
                            amixer: str = "amixer") -> int | None:
    return find_named_control_numid(MUTE_CONTROL_NAME, card, amixer)


def find_control_values(card: str, numid: int, amixer: str = "amixer") -> int | None:
    cp = subprocess.run(
        [amixer, "-D", card, "cget", f"numid={numid}"],
        capture_output=True, text=True,
    )
    if cp.returncode:
        return None
    for line in cp.stdout.splitlines():
        if "type=BYTES" not in line or "values=" not in line:
            continue
        try:
            return int(line.split("values=", 1)[1].split()[0])
        except (ValueError, IndexError):
            return None
    return None


def set_wsa_rx_volume(value: int, *, card: str = DEFAULT_CARD,
                      amixer: str = "amixer") -> None:
    """Set both SP11 WSA-macro producer RX controls to one exact value.

    Native Windows qcaucd programming proves RX0/RX1 RX_VOL_CTL=0x00, which
    corresponds to ALSA value 84 / 0 dB on the RX84-capable SP11 machine
    driver.  Golden v31 intentionally keeps the old Linux value 81 / -3 dB
    while idle; the candidate policy applies 84 only after the protected
    producer is active, because POST_PMU can overwrite the active register
    after an idle-time mixer write.
    """
    if not 0 <= value <= WSA_RX_WINDOWS_VALUE:
        raise ValueError("WSA RX volume outside supported 0..84 range")
    for name in WSA_RX_CONTROL_NAMES:
        cp = subprocess.run(
            [amixer, "-D", card, "cset", f"name='{name}'", str(value)],
            capture_output=True, text=True,
        )
        if cp.returncode:
            output = (cp.stdout + cp.stderr).strip()
            raise RuntimeError(output or f"could not set {name}={value}")


def write_transaction(left_q28: int, delta: bytes, *, helper: Path, card: str,
                      numid: int, right_q28: int | None = None) -> None:
    if not 0 <= left_q28 <= Q28_ONE:
        raise ValueError("left final VOL_CTRL Q28 outside 0..unity")
    if right_q28 is not None and not 0 <= right_q28 <= Q28_ONE:
        raise ValueError("right final VOL_CTRL Q28 outside 0..unity")
    if len(delta) not in (216, 272):
        raise ValueError("invalid GainStep delta size")
    if right_q28 is None:
        blob = struct.pack("<I", left_q28) + delta
    else:
        blob = struct.pack("<II", left_q28, right_q28) + delta
    cp = subprocess.run(
        [str(helper), card, str(numid), blob.hex()], capture_output=True, text=True
    )
    if cp.returncode:
        output = (cp.stdout + cp.stderr).strip()
        raise RuntimeError(output or f"tlv_write rc={cp.returncode}")


def write_volume_only(left_q28: int, right_q28: int, *, helper: Path,
                      card: str, numid: int) -> None:
    """Send only final stereo VOL_CTRL gain with no GainStep calibration.

    Windows GSL computes runtime calibration from prior/new CKVs.  When the
    selected GainStep CKV does not change, only the final VOL_CTRL gain body is
    required; the non-persistent 0x489e calibration group is absent.
    """
    if not 0 <= left_q28 <= Q28_ONE or not 0 <= right_q28 <= Q28_ONE:
        raise ValueError("final VOL_CTRL Q28 outside 0..unity")
    blob = struct.pack("<II", left_q28, right_q28)
    cp = subprocess.run(
        [str(helper), card, str(numid), blob.hex()], capture_output=True, text=True
    )
    if cp.returncode:
        output = (cp.stdout + cp.stderr).strip()
        raise RuntimeError(output or f"volume-only tlv_write rc={cp.returncode}")


def write_endpoint_mute(muted: bool, *, helper: Path, card: str,
                        numid: int) -> None:
    """Send the exact qcadcm final VOL_CTRL multichannel mute selector.

    The kernel constructs the captured 104-byte 0x4a63/0x08001039 body.
    Userspace supplies only the validated Windows mute selector 0/1.
    """
    blob = struct.pack("<I", 1 if muted else 0)
    cp = subprocess.run(
        [str(helper), card, str(numid), blob.hex()], capture_output=True, text=True
    )
    if cp.returncode:
        output = (cp.stdout + cp.stderr).strip()
        raise RuntimeError(output or f"endpoint mute tlv_write rc={cp.returncode}")


def restore_host_attenuation(state: tuple[float, bool], hardware_id: int,
                             wpctl: str) -> tuple[int, int, bool]:
    pipewire_gain, muted = state
    _ui, _db, postgain, host_scalar = base.derive_windows_state(
        pipewire_gain, muted
    )
    base.set_hardware_volume(hardware_id, host_scalar, wpctl)
    base.set_hardware_mute(hardware_id, muted, wpctl)
    return postgain, round(host_scalar * 1_000_000_000), muted


def restore_visible_control_state(state: tuple[float, bool], node_id: int,
                                  wpctl: str) -> float:
    """Restore the last user scalar on a recreated visible control sink.

    PipeWire publishes a new filter sink at unity before WirePlumber restores
    its saved Props.  That transient value is node lifecycle state, not a user
    volume command, and must never reach final VOL_CTRL/GainStep.  Apply volume
    before mute/unmute so recreation always fails toward attenuation.
    """
    pipewire_gain, muted = state
    ui_scalar = base.pipewire_ui_scalar_from_linear_gain(pipewire_gain)
    subprocess.run(
        [wpctl, "set-volume", str(node_id), f"{ui_scalar:.12f}"], check=True,
        stdout=subprocess.DEVNULL,
    )
    subprocess.run(
        [wpctl, "set-mute", str(node_id), "1" if muted else "0"], check=True,
        stdout=subprocess.DEVNULL,
    )
    print(
        f"recreated visible sink id={node_id} restored ui_scalar={ui_scalar:.6f} "
        f"muted={'yes' if muted else 'no'}",
        flush=True,
    )
    return ui_scalar


def default_generation_state() -> Path:
    runtime = os.environ.get("XDG_RUNTIME_DIR")
    if runtime:
        return Path(runtime) / "sp11-ubig-volume-generation"
    return Path(f"/run/user/{os.getuid()}/sp11-ubig-volume-generation")


def read_generation_postgain(path: Path) -> tuple[int, int | None] | None:
    """Read the last queued ``(node generation, postgain)`` state.

    Older deployments stored only the node id.  Treat that legacy form as an
    unknown postgain so the first reconciliation safely re-queues the current
    endpoint value without breaking an in-place upgrade.
    """
    try:
        fields = path.read_text().split()
        if not fields:
            return None
        node_id = int(fields[0])
        postgain = int(fields[1]) if len(fields) > 1 else None
        return node_id, postgain
    except (FileNotFoundError, OSError, ValueError):
        return None


def queue_ubig_postgain_for_generation(
    state: tuple[float, bool], node_id: int, control: Path, generation_state: Path,
    control_format: str = base.CONTROL_FORMAT_AUTO,
) -> int | None:
    """Queue exact Windows VLLDP postgain when node or endpoint state changes.

    Same-process native-Windows speaker-branch dumps on 2026-08-23 directly
    measured VLLDP ``core+0xBB0`` (applied) and ``core+0xBB4`` (staged) across
    an 8/10/15/17/20/25/30/40/50% up/down sweep.  Both fields followed
    ``round(endpoint_dB * 16)`` exactly while the VLLDP object identity and
    profile controls remained unchanged.  Therefore a live endpoint-volume
    gesture must update this lane; a replacement node also forces a re-queue
    even when the numerical postgain is unchanged.
    """
    pipewire_gain, muted = state
    ui_scalar, endpoint_db, postgain, _host = base.derive_windows_state(
        pipewire_gain, muted
    )
    previous = read_generation_postgain(generation_state)
    if previous == (node_id, postgain):
        return None
    if control_format == base.CONTROL_FORMAT_AUTO:
        base.write_postgain_request(control, postgain)
    else:
        base.write_postgain_request(control, postgain, control_format)
    generation_state.parent.mkdir(parents=True, exist_ok=True)
    generation_state.write_text(f"{node_id} {postgain}\n")
    reason = "generation" if previous is None or previous[0] != node_id else "endpoint"
    print(
        f"ubig_generation={node_id} reason={reason} ui_scalar={ui_scalar:.6f} "
        f"windows_db={endpoint_db:.3f} queued_postgain={postgain} "
        f"muted={'yes' if muted else 'no'}",
        flush=True,
    )
    return postgain


def apply_transaction(state: tuple[float, bool], hardware_id: int,
                      deltas: tuple[bytes, ...], helper: Path,
                      card: str, numid: int, wpctl: str,
                      previous: tuple[int, int] | None = None,
                      stereo_sequence: bool = False,
                      volume_only_numid: int | None = None) -> tuple[int, int]:
    pipewire_gain, muted = state
    ui_scalar, endpoint_db, _postgain, _host_scalar = base.derive_windows_state(
        pipewire_gain, muted
    )
    q28 = qcadcm_q28_from_db(endpoint_db)
    step = msiir.select_ckv_step_q28(q28)
    sequence = "legacy-stereo"

    if stereo_sequence:
        if previous is not None and previous[0] != q28:
            old_q28, old_step = previous
            # Fresh Windows KDNET shows master volume is delivered as left
            # channel first and right channel second. GetGraphCkv keeps the
            # louder of the two channel states during that intermediate body.
            mixed_step = max(old_step, step)
            if volume_only_numid is not None:
                # Qualcomm GSL sends non-persistent calibration from a
                # prior/new CKV delta.  A channel call whose selected GainStep
                # did not change carries final VOL_CTRL only; re-sending the
                # same 0x489e row is not Windows-equivalent.
                first_cal = mixed_step != old_step
                if first_cal:
                    write_transaction(
                        q28, deltas[mixed_step - 1], helper=helper, card=card,
                        numid=numid, right_q28=old_q28,
                    )
                else:
                    write_volume_only(
                        q28, old_q28, helper=helper, card=card,
                        numid=volume_only_numid,
                    )

                second_cal = step != mixed_step
                if second_cal:
                    write_transaction(
                        q28, deltas[step - 1], helper=helper, card=card,
                        numid=numid, right_q28=q28,
                    )
                else:
                    write_volume_only(
                        q28, q28, helper=helper, card=card,
                        numid=volume_only_numid,
                    )
                sequence = (
                    f"windows-ckv:{'cal' if first_cal else 'vol'}->"
                    f"{'cal' if second_cal else 'vol'}:{mixed_step}->{step}"
                )
            else:
                write_transaction(
                    q28, deltas[mixed_step - 1], helper=helper, card=card,
                    numid=numid, right_q28=old_q28,
                )
                write_transaction(
                    q28, deltas[step - 1], helper=helper, card=card,
                    numid=numid, right_q28=q28,
                )
                sequence = f"windows-lr:{mixed_step}->{step}"
        else:
            # Unknown prior CKV at graph handover: establish one validated
            # baseline row before prior/new delta semantics take over.
            write_transaction(
                q28, deltas[step - 1], helper=helper, card=card,
                numid=numid, right_q28=q28,
            )
            sequence = "windows-lr:init"
    else:
        write_transaction(q28, deltas[step - 1], helper=helper, card=card, numid=numid)

    # The hidden sink is moved to unity only when entering DSP-owned volume
    # after idle/fail-quiet host attenuation.  Rewriting unity on every live
    # endpoint step is redundant and creates a second downstream control edge.
    if previous is None:
        base.set_hardware_volume(hardware_id, 1.0, wpctl)
    print(
        f"pipewire_gain={pipewire_gain:.9g} ui_scalar={ui_scalar:.6f} "
        f"windows_db={endpoint_db:.3f} final_q28=0x{q28:08x} "
        f"gainstep={step} delta={len(deltas[step - 1])} "
        f"sequence={sequence} hardware_scalar=1.000000 "
        f"muted={'yes' if muted else 'no'}",
        flush=True,
    )
    return q28, step


def event_mentions_node(value: object, node_name: str) -> bool:
    entries = value if isinstance(value, list) else [value]
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        info = entry.get("info") or {}
        props = info.get("props") or {}
        if props.get("node.name") == node_name:
            return True
    return False


def run(args: argparse.Namespace) -> int:
    deltas = load_deltas(args.delta_table)
    numid = find_control_numid(args.card, args.amixer)
    if numid is None:
        print(f"missing ALSA control: {CONTROL_NAME}", file=sys.stderr)
        return 4
    if not args.tlv_write.exists():
        print(f"missing TLV helper: {args.tlv_write}", file=sys.stderr)
        return 5
    mute_numid = find_mute_control_numid(args.card, args.amixer)
    volume_only_numid = find_volume_only_control_numid(args.card, args.amixer)
    control_values = find_control_values(args.card, numid, args.amixer)
    volume_only_values = (
        find_control_values(args.card, volume_only_numid, args.amixer)
        if volume_only_numid is not None else None
    )
    stereo_sequence = (
        control_values is not None
        and control_values >= STEREO_TRANSACTION_CONTROL_VALUES
    )
    prior_new_ckv = (
        stereo_sequence
        and volume_only_numid is not None
        and volume_only_values == VOLUME_ONLY_CONTROL_VALUES
    )
    if not prior_new_ckv:
        volume_only_numid = None
    print(
        f"volume_transaction_control_values={control_values} "
        f"mode={'windows-lr' if stereo_sequence else 'legacy-stereo'} "
        f"ckv_delta={'prior-new' if prior_new_ckv else 'legacy-resend'} "
        f"volume_only_control_values={volume_only_values} "
        f"endpoint_mute={'exact-dsp' if mute_numid is not None else 'hardware-fallback'} "
        f"wsa_rx={'windows-0db-active' if prior_new_ckv else 'legacy-unchanged'}",
        flush=True,
    )

    last: tuple[int, int] | None = None
    last_dsp_mute: bool | None = None
    last_host: tuple[int, int, bool] | None = None
    transaction_active = False
    rx_gain_active = False
    current_state: tuple[float, bool] | None = None
    current_node_id: int | None = None
    current_hardware_id: int | None = None
    node_settle_ms = getattr(args, "node_settle_ms", 0)
    generation_state = getattr(args, "generation_state", default_generation_state())

    def reconcile(state: tuple[float, bool], hardware_id: int) -> None:
        nonlocal last, last_host, last_dsp_mute, transaction_active
        nonlocal rx_gain_active, current_state, current_hardware_id
        current_state = state
        current_hardware_id = hardware_id

        if current_node_id is not None:
            queue_ubig_postgain_for_generation(
                state, current_node_id, args.control, generation_state,
                getattr(args, "control_format", base.CONTROL_FORMAT_AUTO),
            )

        if not msiir.graph_running(args.pcm_status):
            if prior_new_ckv and rx_gain_active:
                try:
                    set_wsa_rx_volume(
                        WSA_RX_GOLDEN_BASELINE_VALUE,
                        card=args.card, amixer=args.amixer,
                    )
                    print("wsa_rx_active=81 (-3 dB Golden idle baseline)", flush=True)
                except RuntimeError as exc:
                    print(
                        f"could not restore idle WSA RX baseline: {exc}",
                        file=sys.stderr, flush=True,
                    )
                rx_gain_active = False
            pipewire_gain, muted = state
            _ui, _db, postgain, host_scalar = base.derive_windows_state(
                pipewire_gain, muted
            )
            host_signature = (postgain, round(host_scalar * 1_000_000_000), muted)
            if transaction_active or host_signature != last_host:
                last_host = restore_host_attenuation(state, hardware_id, args.wpctl)
            transaction_active = False
            last_dsp_mute = None
            return

        pipewire_gain, muted = state
        _ui, endpoint_db, _postgain, _host = base.derive_windows_state(
            pipewire_gain, muted
        )
        q28 = qcadcm_q28_from_db(endpoint_db)
        step = msiir.select_ckv_step_q28(q28)
        signature = (q28, step)

        mute_changed = last_dsp_mute is None or muted != last_dsp_mute
        if mute_numid is not None and mute_changed:
            try:
                write_endpoint_mute(
                    muted, helper=args.tlv_write, card=args.card, numid=mute_numid
                )
                last_dsp_mute = muted
                # Exact Windows endpoint mute is the normal actuator on kernels
                # that expose 0x4a63/0x08001039.  Do not mirror successful
                # transitions into the physical PipeWire sink: a synchronized
                # SP7 external-mic A/B showed that the second actuator sharpens
                # rare high-rate mute edges.  The exception path below remains
                # fail-closed, and rollback kernels without this control keep the
                # proven downstream-only mute actuator.
                print(
                    f"endpoint_dsp_mute={'1' if muted else '0'} hardware_backstop="
                    "fail-closed-only", flush=True
                )
            except RuntimeError as exc:
                _ui, _db, _postgain, host_scalar = base.derive_windows_state(
                    pipewire_gain, muted
                )
                base.set_hardware_volume(hardware_id, host_scalar, args.wpctl)
                # A failed DSP unmute must fail closed instead of exposing audio
                # whose graph mute state is unknown.
                base.set_hardware_mute(hardware_id, True, args.wpctl)
                transaction_active = False
                last_dsp_mute = None
                print(
                    f"endpoint DSP mute failed closed: {exc}",
                    file=sys.stderr,
                    flush=True,
                )
                return
        elif mute_numid is None:
            # Rollback kernels retain the proven downstream-only mute actuator.
            base.set_hardware_mute(hardware_id, muted, args.wpctl)

        if transaction_active and signature == last:
            return
        try:
            last = apply_transaction(
                state, hardware_id, deltas, args.tlv_write,
                args.card, numid, args.wpctl,
                previous=last if transaction_active else None,
                stereo_sequence=stereo_sequence,
                volume_only_numid=volume_only_numid,
            )
            if prior_new_ckv and not rx_gain_active:
                try:
                    set_wsa_rx_volume(
                        WSA_RX_WINDOWS_VALUE, card=args.card, amixer=args.amixer
                    )
                    rx_gain_active = True
                    print("wsa_rx_active=84 (0 dB native-Windows producer)", flush=True)
                except RuntimeError as exc:
                    # Producer gain is a parity/quality actuator, not a safety
                    # prerequisite.  Keep rendering on the proven RX81 Golden
                    # baseline if the mixer write is unavailable.
                    print(
                        f"WSA RX 0 dB parity write failed; keeping Golden baseline: {exc}",
                        file=sys.stderr, flush=True,
                    )
            last_host = None
            transaction_active = True
        except RuntimeError as exc:
            last_host = restore_host_attenuation(state, hardware_id, args.wpctl)
            transaction_active = False
            last_dsp_mute = None
            print(
                f"transaction failed safely: {exc}",
                file=sys.stderr,
                flush=True,
            )

    def settled() -> tuple[tuple[float, bool] | None, int | None, int | None]:
        entries = base.snapshot(args.pw_dump)
        return (
            base.extract_node_volume(entries, args.node),
            base.extract_node_id(entries, args.node),
            base.extract_node_id(entries, args.hardware_node),
        )

    def adopt_recreated_node(node_id: int | None, hardware_id: int | None) -> bool:
        """Return True when a replacement node was restored and reconciled."""
        nonlocal current_node_id
        if node_id is None:
            return False
        if current_node_id is None:
            current_node_id = node_id
            return False
        if node_id == current_node_id:
            return False
        if current_state is None:
            current_node_id = node_id
            return False
        # Do not consume the new node's default-unity Props.  The old user state
        # is authoritative across object recreation and is restored first.
        restore_visible_control_state(current_state, node_id, args.wpctl)
        current_node_id = node_id
        target_hardware = hardware_id or current_hardware_id
        if target_hardware is not None:
            reconcile(current_state, target_hardware)
        return True

    if args.once:
        state, node_id, hardware_id = settled()
        if state is None or node_id is None or hardware_id is None:
            print("volume or hardware node unavailable", file=sys.stderr)
            return 3
        current_node_id = node_id
        reconcile(state, hardware_id)
        if not transaction_active:
            print("graph idle; host attenuation established")
        return 0

    # Subscribe before the initial snapshot so node creation cannot fall into
    # a snapshot/monitor attachment gap during a user-session cold start.
    proc = subprocess.Popen([args.pw_dump, "-m"], stdout=subprocess.PIPE)
    assert proc.stdout is not None
    try:
        if args.settle_ms > 0:
            time.sleep(args.settle_ms / 1000.0)
        deadline = time.monotonic() + (args.bootstrap_ms / 1000.0)
        while (
            current_state is None
            or current_node_id is None
            or current_hardware_id is None
        ):
            state, node_id, hardware_id = settled()
            if state is not None and node_id is not None and hardware_id is not None:
                # A brand-new PipeWire node is born at unity before WirePlumber
                # restores stream properties.  Give that restoration one
                # bounded window before the first DSP transaction of a login.
                if node_settle_ms > 0:
                    time.sleep(node_settle_ms / 1000.0)
                    state2, node_id2, hardware_id2 = settled()
                    if (
                        state2 is None
                        or node_id2 != node_id
                        or hardware_id2 is None
                    ):
                        if time.monotonic() >= deadline:
                            break
                        continue
                    state, node_id, hardware_id = state2, node_id2, hardware_id2
                current_node_id = node_id
                reconcile(state, hardware_id)
                break
            if time.monotonic() >= deadline:
                break
            time.sleep(0.1)

        guard_until = (
            time.monotonic() + (args.bootstrap_guard_ms / 1000.0)
            if current_state is not None else 0.0
        )
        for value in base.iter_json_stream(proc.stdout.fileno()):
            event_state = base.extract_node_volume(value, args.node)
            found_node_id = base.extract_node_id(value, args.node)
            found_hardware_id = base.extract_node_id(value, args.hardware_node)
            relevant = (
                event_state is not None
                or found_node_id is not None
                or found_hardware_id is not None
                or event_mentions_node(value, args.node)
                or event_mentions_node(value, args.hardware_node)
            )
            if not relevant:
                continue

            # A filter-chain recreation is not a volume gesture.  Restore the
            # previous scalar on the replacement object before considering any
            # of its default Props, then ignore the queued unity event.
            if adopt_recreated_node(
                found_node_id, found_hardware_id or current_hardware_id
            ):
                guard_until = time.monotonic() + (
                    args.bootstrap_guard_ms / 1000.0
                )
                continue

            # Resolve queued bootstrap values and partial node-state deltas
            # against a complete snapshot. Outside the guard, a complete Props
            # event is used directly so slider changes have no polling delay.
            if (
                event_state is None
                or current_hardware_id is None
                or time.monotonic() < guard_until
            ):
                snap_state, snap_node_id, snap_hardware_id = settled()
                event_state = snap_state or event_state
                found_node_id = snap_node_id or found_node_id
                found_hardware_id = snap_hardware_id or found_hardware_id

                if adopt_recreated_node(
                    found_node_id, found_hardware_id or current_hardware_id
                ):
                    guard_until = time.monotonic() + (
                        args.bootstrap_guard_ms / 1000.0
                    )
                    continue

            state = event_state or current_state
            hardware_id = found_hardware_id or current_hardware_id
            if state is None or hardware_id is None:
                continue
            if found_node_id is not None and current_node_id is None:
                current_node_id = found_node_id
            reconcile(state, hardware_id)
    finally:
        if proc.poll() is None:
            proc.terminate()
        proc.wait()
        if prior_new_ckv and rx_gain_active:
            try:
                set_wsa_rx_volume(
                    WSA_RX_GOLDEN_BASELINE_VALUE,
                    card=args.card, amixer=args.amixer,
                )
            except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
                print(
                    f"could not restore WSA RX Golden baseline on exit: {exc}",
                    file=sys.stderr, flush=True,
                )
        if (
            transaction_active
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
    return proc.returncode or 0

def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--node", default=base.DEFAULT_NODE)
    p.add_argument("--hardware-node", default=base.DEFAULT_HARDWARE_NODE)
    p.add_argument("--control", type=Path, default=base.default_control_path())
    p.add_argument("--control-format", choices=(base.CONTROL_FORMAT_AUTO, base.CONTROL_FORMAT_LEGACY, base.CONTROL_FORMAT_UBIG_V2),
                   default=base.default_control_format())
    p.add_argument("--card", default=DEFAULT_CARD)
    p.add_argument("--pcm-status", type=Path, default=DEFAULT_PCM_STATUS)
    p.add_argument("--tlv-write", type=Path, default=DEFAULT_TLV_WRITE)
    p.add_argument("--delta-table", type=Path, default=DEFAULT_DELTA_TABLE)
    p.add_argument("--pw-dump", default="pw-dump")
    p.add_argument("--wpctl", default="wpctl")
    p.add_argument("--amixer", default="amixer")
    p.add_argument("--settle-ms", type=int, default=200)
    p.add_argument("--bootstrap-ms", type=int, default=5000)
    p.add_argument("--bootstrap-guard-ms", type=int, default=1000)
    p.add_argument("--node-settle-ms", type=int, default=300,
                   help="wait for WirePlumber Props restore before first transaction")
    p.add_argument("--generation-state", type=Path,
                   default=default_generation_state(), help=argparse.SUPPRESS)
    # Kept only so older unit/service invocations do not fail argument parsing;
    # the transaction path is no longer timer-driven.
    p.add_argument("--interval-ms", type=int, default=100,
                   help=argparse.SUPPRESS)
    p.add_argument("--once", action="store_true")
    return p


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    if min(args.settle_ms, args.bootstrap_ms, args.bootstrap_guard_ms,
           args.node_settle_ms) < 0:
        print("monitor timing values must be non-negative", file=sys.stderr)
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
