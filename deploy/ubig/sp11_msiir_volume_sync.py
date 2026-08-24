#!/usr/bin/env python3
"""Keep SP11 AudioReach MSIIR 0x489e on the Windows volume-dependent ACDB row.

Windows qcadcm8380.sys converts endpoint gain to Q28, performs nearest-neighbour
selection over the endpoint's ACDB gain-step table, maps internal index 0..29
to CKV GainStep 1..30, and reapplies graph calibration. Linux originally froze
this stage at CKV 30 (full-volume unity) for every user volume.

This service follows the already recovered DAX/VLLDP postgain request in the
shared control page. The postgain is endpoint dB in 1/16-dB units, so it is a
single gain source shared with UbiG. Whenever playback enters RUNNING, or the
selected CKV changes, the exact reviewed REV_0D 0x489e/0x08001022 payload is
sent through the existing allowlisted ALSA TLV control.

Provenance:
  REV_0D ACDB sha256 a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde
  qcadcm8380.sys sha256 37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429
  internal speaker endpoint key 0x01000006 = 1
  MSIIR iid 0x489e, coefficient param 0x08001022

The table below is generated from the reviewed Windows ACDB. Do not hand-tune it.
"""
from __future__ import annotations

import argparse
import math
import os
import struct
import subprocess
import sys
import time
from pathlib import Path

CONTROL_NAME = "SP11 MSIIR Inject"
TRANSACTION_CONTROL_NAME = "SP11 Windows Volume Transaction"
DEFAULT_CARD = "hw:0"
DEFAULT_PCM_STATUS = Path("/proc/asound/card0/pcm0p/sub0/status")
DEFAULT_CONTROL_BASENAME = "sp11-ubig-profile.control"
DEFAULT_TLV_WRITE = Path.home() / ".local/lib/ubig-private/tlv_write"
POSTGAIN_REQUEST_OFF = 4
POSTGAIN_NONE = -(1 << 31)
POSTGAIN_MIN = -1200
POSTGAIN_MAX = 0

UBIG_CONTROL_MAGIC = 0x55424947
UBIG_CONTROL_ABI = 2
UBIG_CONTROL_BYTES = 172
UBIG_DESIRED_POSTGAIN_OFF = 116
CONTROL_FORMAT_AUTO = "auto"
CONTROL_FORMAT_LEGACY = "legacy"
CONTROL_FORMAT_UBIG_V2 = "ubig-v2"
MSIIR_IID = 0x489E
PARAM_COEFFS = 0x08001022
Q28_ONE = 1 << 28

Q28_GAIN_STEPS = (
    0x00000000,
    0x016d0e6f,
    0x018dfa93,
    0x01b1dece,
    0x01d8ffab,
    0x0203a7e6,
    0x023228f6,
    0x0264dbae,
    0x029c20e0,
    0x02d8621c,
    0x031a1276,
    0x0361af63,
    0x03afc1a9,
    0x0404de62,
    0x0461a81c,
    0x04c6d00e,
    0x0535176a,
    0x05ad50ce,
    0x063061d6,
    0x06bf44d5,
    0x075b0ab0,
    0x0804dce8,
    0x08bdffd3,
    0x0987d507,
    0x0a63ddfe,
    0x0b53bef5,
    0x0c59420f,
    0x0d765ac1,
    0x0ead2988,
    0x10000000,
)

COEFF_PAYLOADS = (
    bytes.fromhex("02000000060000000000000000000200d10c1641e9f238818918c73dd14a2f81427dd33e3efb14452acab9a80c3d6d2024d5e5c8c52f921302000300f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 1, 96 bytes
    bytes.fromhex("02000000060000000000000000000200040c1241a4274881e042bc3dba743e81fb9bc43e6f792a43c124eeab4464231f24d5e5c8c52f921302000300f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 2, 96 bytes
    bytes.fromhex("03000000020000000000000000000200fb57174147d75781ec9ea83d999e4d8139beb53e8d444f41aba506af3b18e61d24d5e5c8c52f921302000300040000000000000000000200e3911c41411e5881feaba33d999e4d8139beb53e8d444f41aba506af3b18e61d24d5e5c8c52f921302000300f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 3, 152 bytes
    bytes.fromhex("02000000060000000000000000000200de671941445d6781f310983d70c85c81fde3a63e8bdd823ff43e04b2f4e5b41c24d5e5c8c52f921302000300f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 4, 96 bytes
    bytes.fromhex("02000000060000000000000000000200e9bc174107c579817823883d98fa6e81f315953e92c9c43d70dae7b4715e8f1b24d5e5c8c52f921302000300f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 5, 96 bytes
    bytes.fromhex("0200000006000000000000000000020026061541f61c8c811737793db22c8181f94c833edd91143c045ab2b79316751a24d5e5c8c52f921302000300f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 6, 96 bytes
    bytes.fromhex("02000000060000000000000000000200a953114160659e81063c6b3dbc5e93810c89713e96c3713ae79764baf7a6651924d5e5c8c52f921302000300f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 7, 96 bytes
    bytes.fromhex("0200000006000000000000000000020089a70e4195cfb3818664593d0a99a88184d55c3eb5efdb38eb66ffbcd4ab601824d5e5c8c52f921302000300f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 8, 96 bytes
    bytes.fromhex("03000000020000000000000000000200bbe00a41c527c981a19c483d42d3bd81d928483edfaa5237b79283bfdec4651724d5e5c8c52f921302000300040000000000000000000200562fef400899c78149bf623d42d3bd81d928483edfaa5237b79283bfdec4651724d5e5c8c52f921302000300f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 9, 152 bytes
    bytes.fromhex("020000000600000000000000000002003d100641406ede81a9d3383d620dd3810983333e891aab6b15c0e3834c2ae92c24d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 10, 96 bytes
    bytes.fromhex("020000000600000000000000000002004946004171a3f381cff9293d6947e88110e41e3e0d65c868e7199688f585192b24d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 11, 96 bytes
    bytes.fromhex("020000000600000000000000000002005a11fb4050f20b82aeb2173d9b890082545b073e2d73fc6520a21f8d9bef5b2924d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 12, 96 bytes
    bytes.fromhex("0200000006000000000000000000020016ccf4407a2d24823c71063daacb188282dbef3d3889466380bb819131c0af2724d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 13, 96 bytes
    bytes.fromhex("020000000600000000000000000002001fd2ee40ab7c3f82170bf23cce1534825976d53dacf1a560bebcbd958056142624d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 14, 96 bytes
    bytes.fromhex("0200000006000000000000000000020003b7e740c0b55a826bbbde3cc25f4f82701cbb3d07fd195eecf0d499f616892424d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 15, 96 bytes
    bytes.fromhex("02000000060000000000000000000200c5a6e0403dfd7882ed86c83cb2b16d8227e29d3d9301a25be197c89d756b0d2324d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 16, 96 bytes
    bytes.fromhex("02000000060000000000000000000200d969d840352c9782b174b33c5f038c82b5b5803d2f5b3d5998e699a122c3a02124d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 17, 96 bytes
    bytes.fromhex("02000000060000000000000000000200b512cf401d43b582b3729f3cc854aa821397633d256beb568e074aa53592422024d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 18, 96 bytes
    bytes.fromhex("02000000060000000000000000000200638ac5408e61d68210c8883c01aecb82e69e433dfb97ab541e1bdaa8d051f21e24d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 19, 96 bytes
    bytes.fromhex("020000000600000000000000000002005aa3bb40f782fa8260a26f3cef0ef082b3d1203d424d7d52d8374baccf7faf1d24d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 20, 96 bytes
    bytes.fromhex("02000000060000000000000000000200e630a040db711d83cde9663c6b6f14834418fe3c6ffb5f50d96a9eafa19e791c24d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 21, 96 bytes
    bytes.fromhex("02000000060000000000000000000200d1509440686b448378d34c3c6cd73b834e90d83cb017534e19b8d4b21f35501b24d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 22, 96 bytes
    bytes.fromhex("02000000060000000000000000000200b18279409417958358ce183c95ad8d830ae78a3cc31b564cbf1aefb566ce321a24d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 23, 96 bytes
    bytes.fromhex("03000000020000000000000000000200a10f694067fd93833b27283c95ad8d830ae78a3cd085684a6985eeb8b0f9201924d5e5c8c52f921302000200040000000000000000000200b18279409417958358ce183c95ad8d830ae78a3cd085684a6985eeb8b0f9201924d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 24, 152 bytes
    bytes.fromhex("0200000006000000000000000000020065435a407bb8c0836d3e0b3c6a23bb83c2ec5f3c41d889487ae2d3bb2e4a1a1824d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 25, 96 bytes
    bytes.fromhex("020000000600000000000000000002009e484a40b550ed83af80ef3b6298e883fc10353ca199b9465f14a0bee9561e1724d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 26, 96 bytes
    bytes.fromhex("020000000600000000000000000002008b87394006e11f84b4e0ce3bdc1b1c8415a3043c7754f744d5f553c19dba2c1624d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 27, 96 bytes
    bytes.fromhex("02000000060000000000000000000200d0822740634852849783af3b1f9e4f84235cd43b239742432d5af0c39913451524d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 28, 96 bytes
    bytes.fromhex("02000000060000000000000000000200134f1440678784843d55913b1e1f8384073ca43bc0f39a418b0d76c69e03671424d5e5c8c52f921302000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 29, 96 bytes
    bytes.fromhex("020000000600000000000000000002000000004000000000000000000000000000000000000000400000000000000000000000000000000002000200f8ffffff0700ffff00000100000000400000000000000000000000000000000002000000"),  # CKV 30, 96 bytes
)

if len(Q28_GAIN_STEPS) != 30 or len(COEFF_PAYLOADS) != 30:
    raise RuntimeError("reviewed volume table must contain exactly 30 steps")


def default_control_path() -> Path:
    override = os.environ.get("UBIG_CONTROL_PATH")
    if override:
        return Path(override)
    runtime = os.environ.get("XDG_RUNTIME_DIR") or f"/run/user/{os.getuid()}"
    return Path(runtime) / DEFAULT_CONTROL_BASENAME


def default_control_format() -> str:
    value = os.environ.get("UBIG_CONTROL_FORMAT", CONTROL_FORMAT_AUTO).strip().lower()
    return value if value in {CONTROL_FORMAT_AUTO, CONTROL_FORMAT_LEGACY, CONTROL_FORMAT_UBIG_V2} else CONTROL_FORMAT_AUTO


def postgain_to_q28(postgain: int) -> int:
    """Convert DAX 1/16-dB endpoint postgain to unsigned Q28 linear gain."""
    if postgain == POSTGAIN_NONE:
        raise ValueError("postgain request is uninitialized")
    postgain = max(POSTGAIN_MIN, min(POSTGAIN_MAX, int(postgain)))
    gain = 10.0 ** ((postgain / 16.0) / 20.0)
    return max(0, min(Q28_ONE, int(round(gain * Q28_ONE))))


def select_ckv_step_q28(q28_gain: int) -> int:
    """Match qcadcm GetGainTableStepFrmQ28Gain nearest-neighbour semantics."""
    q = max(0, min(Q28_ONE, int(q28_gain)))
    # Exact ties are not expected from real endpoint gains; choose the lower
    # CKV deterministically. qcadcm's normal values are well away from ties.
    idx = min(range(len(Q28_GAIN_STEPS)), key=lambda i: (abs(Q28_GAIN_STEPS[i] - q), i))
    return idx + 1


def select_ckv_step_postgain(postgain: int) -> int:
    return select_ckv_step_q28(postgain_to_q28(postgain))


def detect_control_format(path: Path, requested: str = CONTROL_FORMAT_AUTO) -> str:
    if requested != CONTROL_FORMAT_AUTO:
        return requested
    try:
        data = path.read_bytes()[:12]
    except FileNotFoundError:
        return CONTROL_FORMAT_LEGACY
    if len(data) >= 12:
        magic, abi, struct_bytes = struct.unpack_from("<III", data, 0)
        if magic == UBIG_CONTROL_MAGIC and abi == UBIG_CONTROL_ABI and struct_bytes == UBIG_CONTROL_BYTES:
            return CONTROL_FORMAT_UBIG_V2
    return CONTROL_FORMAT_LEGACY


def read_postgain(path: Path, control_format: str = CONTROL_FORMAT_AUTO) -> int | None:
    try:
        data = path.read_bytes()
    except FileNotFoundError:
        return None
    if detect_control_format(path, control_format) == CONTROL_FORMAT_UBIG_V2:
        if len(data) < UBIG_DESIRED_POSTGAIN_OFF + 4:
            return None
        value = struct.unpack_from("<i", data, UBIG_DESIRED_POSTGAIN_OFF)[0]
        return value if POSTGAIN_MIN <= value <= POSTGAIN_MAX else None
    if len(data) < POSTGAIN_REQUEST_OFF + 4:
        return None
    value = struct.unpack_from("<i", data, POSTGAIN_REQUEST_OFF)[0]
    return None if value == POSTGAIN_NONE else value


def graph_running(path: Path = DEFAULT_PCM_STATUS) -> bool:
    try:
        with path.open() as fh:
            return fh.readline().strip() == "state: RUNNING"
    except OSError:
        return False


def find_control_numid(card: str = DEFAULT_CARD, amixer: str = "amixer") -> int | None:
    cp = subprocess.run([amixer, "-D", card, "controls"], capture_output=True, text=True)
    if cp.returncode:
        return None
    for line in cp.stdout.splitlines():
        if CONTROL_NAME.lower() in line.lower() and "numid=" in line:
            return int(line.split("numid=", 1)[1].split(",", 1)[0])
    return None


def control_present(name: str, card: str = DEFAULT_CARD,
                    amixer: str = "amixer") -> bool:
    cp = subprocess.run([amixer, "-D", card, "controls"],
                        capture_output=True, text=True)
    return cp.returncode == 0 and name.lower() in cp.stdout.lower()


def build_tlv_blob(step: int) -> bytes:
    if not 1 <= step <= 30:
        raise ValueError("CKV step must be 1..30")
    payload = COEFF_PAYLOADS[step - 1]
    return struct.pack("<III", MSIIR_IID, PARAM_COEFFS, len(payload)) + payload


def inject_step(step: int, helper: Path, card: str, numid: int, dry_run: bool = False) -> tuple[int, str]:
    blob = build_tlv_blob(step)
    if dry_run:
        return 0, f"dry-run payload={len(blob)}"
    cp = subprocess.run([str(helper), card, str(numid), blob.hex()], capture_output=True, text=True)
    return cp.returncode, (cp.stdout + cp.stderr).strip()


def describe(postgain: int, step: int) -> str:
    db = postgain / 16.0
    return f"endpoint_db={db:.3f} postgain={postgain} msiir_ckv={step} payload={len(COEFF_PAYLOADS[step-1])}"


def run_once(args: argparse.Namespace) -> int:
    pg = read_postgain(args.control, args.control_format)
    if pg is None:
        print("postgain unavailable", file=sys.stderr)
        return 3
    step = select_ckv_step_postgain(pg)
    print(describe(pg, step))
    if not graph_running(args.pcm_status):
        print("graph idle; selection computed but not injected")
        return 0
    numid = find_control_numid(args.card, args.amixer)
    if numid is None:
        print(f"missing ALSA control: {CONTROL_NAME}", file=sys.stderr)
        return 4
    if not args.dry_run and not args.tlv_write.exists():
        print(f"missing TLV helper: {args.tlv_write}", file=sys.stderr)
        return 5
    rc, out = inject_step(step, args.tlv_write, args.card, numid, args.dry_run)
    if out:
        print(out)
    return rc


def run_monitor(args: argparse.Namespace) -> int:
    numid = find_control_numid(args.card, args.amixer)
    if numid is None:
        print(f"missing ALSA control: {CONTROL_NAME}", file=sys.stderr)
        return 4
    if not args.dry_run and not args.tlv_write.exists():
        print(f"missing TLV helper: {args.tlv_write}", file=sys.stderr)
        return 5

    last_running = False
    last_step: int | None = None
    last_error: tuple[int, int] | None = None
    while True:
        pg = read_postgain(args.control, args.control_format)
        running = graph_running(args.pcm_status)
        if not running:
            last_running = False
            last_step = None
            time.sleep(args.interval_ms / 1000.0)
            continue
        if pg is None:
            time.sleep(args.interval_ms / 1000.0)
            continue
        step = select_ckv_step_postgain(pg)
        if (not last_running) or step != last_step:
            rc, out = inject_step(step, args.tlv_write, args.card, numid, args.dry_run)
            if rc == 0:
                print(describe(pg, step) + " applied", flush=True)
                last_step = step
                last_error = None
            else:
                err = (step, rc)
                if err != last_error:
                    print(describe(pg, step) + f" inject_rc={rc} {out}", file=sys.stderr, flush=True)
                    last_error = err
                # Do not mark the step applied. Retry while the graph remains
                # RUNNING; this covers the short CAPI-init window at start.
        last_running = True
        time.sleep(args.interval_ms / 1000.0)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--control", type=Path, default=default_control_path())
    p.add_argument("--control-format", choices=(CONTROL_FORMAT_AUTO, CONTROL_FORMAT_LEGACY, CONTROL_FORMAT_UBIG_V2),
                   default=default_control_format())
    p.add_argument("--card", default=DEFAULT_CARD)
    p.add_argument("--pcm-status", type=Path, default=DEFAULT_PCM_STATUS)
    p.add_argument("--tlv-write", type=Path, default=DEFAULT_TLV_WRITE)
    p.add_argument("--amixer", default="amixer")
    p.add_argument("--interval-ms", type=int, default=100)
    p.add_argument("--once", action="store_true")
    p.add_argument("--dry-run", action="store_true")
    return p


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.interval_ms < 20:
        print("interval must be >=20 ms", file=sys.stderr)
        return 2
    if control_present(TRANSACTION_CONTROL_NAME, args.card, args.amixer):
        print("combined Windows volume transaction owns GainStep updates")
        return 0
    try:
        return run_once(args) if args.once else run_monitor(args)
    except KeyboardInterrupt:
        return 0
    except (OSError, subprocess.SubprocessError, ValueError) as exc:
        print(f"sp11-msiir-volume-sync: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
