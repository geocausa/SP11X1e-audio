#!/usr/bin/env python3
"""Send one parameter to a live AudioReach module via the SP11 inject control.

WHAT THIS IS FOR
----------------
On Windows, Dolby's VLLDP processor does not filter audio in user space. It
analyses the stream, computes MSIIR filter coefficients, and injects them into
the MSIIR modules that already exist in the DSP graph. The DSP does the
filtering, ahead of speaker protection.

Linux had no equivalent path: module parameters were only ever sent during
graph setup, so MSIIR sits at whatever the topology loaded for the life of the
stream. The kernel control 'SP11 MSIIR Inject' provides that missing delivery
path. This script is the user-space side of it.

Write format expected by the kernel (little-endian):

    u32 instance_id
    u32 param_id
    u32 payload_size
    u8  payload[payload_size]

The kernel allowlists both instance_id and param_id. Anything else is refused
with EPERM.

IMPORTANT: audio must be PLAYING when you inject. Firmware analysis (June 2026,
Ghidra on the ADSP image) established that MSIIR gates its tuning parameters
behind the module's CAPI-initialised flag, which is only set once the graph has
started and media format has been applied. Injecting into a stopped graph is
expected to fail with EINVAL (-22).

Usage:
    # verify the control exists
    ./sp11_msiir_inject.py --probe

    # unity test: send a known-safe pass-through filter and check acceptance
    ./sp11_msiir_inject.py --unity --iid 0x489e

    # send raw coefficients from a file
    ./sp11_msiir_inject.py --iid 0x489e --param 0x08001022 --file coeffs.bin
"""

from __future__ import annotations

import argparse
import struct
import subprocess
import sys

CONTROL_NAME = "SP11 MSIIR Inject"

# Instances allowlisted in the kernel. These are the MSIIR modules present in
# the currently deployed SP11 topology. Note they differ from historical
# project values: June 2026 work used 0x4b00/0x4b01, and Windows uses
# 0x47ee/0x47ef. Always target what is actually deployed.
ALLOWED_IIDS = {0x489E, 0x48A1}

PARAM_ENABLE = 0x08001020
PARAM_PREGAIN = 0x08001021
PARAM_COEFFS = 0x08001022
ALLOWED_PARAMS = {PARAM_ENABLE, PARAM_PREGAIN, PARAM_COEFFS}

Q30_ONE = 1 << 30


def run(cmd: list[str]) -> tuple[int, str]:
    p = subprocess.run(cmd, capture_output=True, text=True)
    return p.returncode, (p.stdout + p.stderr).strip()


def find_control(card: str) -> str | None:
    rc, out = run(["amixer", "-D", card, "controls"])
    if rc:
        return None
    for line in out.splitlines():
        if CONTROL_NAME.lower() in line.lower():
            return line.strip()
    return None


def graph_is_running() -> bool:
    """A live graph is required or MSIIR will reject the parameter."""
    try:
        with open("/proc/asound/card0/pcm0p/sub0/status") as fh:
            return not fh.readline().startswith("closed")
    except OSError:
        return False


def unity_coefficients(stages: int = 2, channels: int = 2) -> bytes:
    """A pass-through filter: b0 = 1.0 in Q30, everything else zero.

    Deliberately audibly neutral. The point of the unity test is to prove the
    DELIVERY PATH works (rc=0), not to change the sound. If unity is accepted,
    real coefficients can follow. If unity is rejected, no coefficient set
    would have worked and the problem is initialisation, not the values.
    """
    body = struct.pack("<IIII", 0, channels, stages, 0)
    for _ in range(stages):
        # b0, b1, b2, a1, a2
        body += struct.pack("<iiiii", Q30_ONE, 0, 0, 0, 0)
    return body


def inject(card: str, iid: int, param_id: int, payload: bytes, dry_run: bool) -> int:
    if iid not in ALLOWED_IIDS:
        print(f"refusing: iid {iid:#x} is not allowlisted in the kernel", file=sys.stderr)
        return 2
    if param_id not in ALLOWED_PARAMS:
        print(f"refusing: param {param_id:#x} is not allowlisted", file=sys.stderr)
        return 2

    blob = struct.pack("<III", iid, param_id, len(payload)) + payload

    print(f"  target      iid={iid:#06x} param={param_id:#010x}")
    print(f"  payload     {len(payload)} bytes")
    print(f"  total write {len(blob)} bytes")

    if dry_run:
        print("  DRY RUN - nothing sent")
        return 0

    if not graph_is_running():
        print("\n  WARNING: no playback stream is open.")
        print("  MSIIR gates tuning behind CAPI init, which only happens once the")
        print("  graph is running. Expect this to fail with -22. Start audio first.")

    hexstr = ",".join(f"0x{b:02x}" for b in blob)
    rc, out = run(["amixer", "-D", card, "cset", f"name={CONTROL_NAME}", hexstr])
    if rc == 0:
        print("\n  ACCEPTED (rc=0) - the DSP took the parameter")
    else:
        print(f"\n  REJECTED (rc={rc})")
        if out:
            print("  " + out.replace("\n", "\n  "))
        print("\n  -22/EINVAL usually means MSIIR was not CAPI-initialised:")
        print("  confirm audio is actually playing, then retry.")
        print("  -1/EPERM means the kernel allowlist refused the target.")
    return rc


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--card", default="hw:0")
    ap.add_argument("--probe", action="store_true", help="check the control exists, then exit")
    ap.add_argument("--unity", action="store_true", help="send a pass-through coefficient set")
    ap.add_argument("--iid", type=lambda x: int(x, 0), default=0x489E)
    ap.add_argument("--param", type=lambda x: int(x, 0), default=PARAM_COEFFS)
    ap.add_argument("--file", help="raw payload file")
    ap.add_argument("--stages", type=int, default=2)
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    ctl = find_control(args.card)
    print(f"control     {ctl if ctl else 'NOT FOUND'}")
    if not ctl:
        print("\nThe 'SP11 MSIIR Inject' control is not present on this card.", file=sys.stderr)
        print("Are you booted into the diagnostic kernel?", file=sys.stderr)
        print(f"Running: {subprocess.run(['uname','-r'],capture_output=True,text=True).stdout.strip()}",
              file=sys.stderr)
        return 1

    print(f"graph       {'running' if graph_is_running() else 'idle'}")

    if args.probe:
        return 0

    if args.file:
        with open(args.file, "rb") as fh:
            payload = fh.read()
    elif args.unity:
        payload = unity_coefficients(stages=args.stages)
    else:
        ap.error("give --unity or --file")

    return inject(args.card, args.iid, args.param, payload, args.dry_run)


if __name__ == "__main__":
    raise SystemExit(main())
