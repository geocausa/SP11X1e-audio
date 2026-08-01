#!/usr/bin/env python3
"""Build and send MSIIR biquad coefficients to the live SP11 DSP.

This is the audible follow-up to the unity test. The unity test proved the
delivery path works (rc=0); this proves the DSP actually APPLIES what it
accepts, by sending a filter you can hear.

ARCHITECTURE (why this exists)
------------------------------
Windows does not filter audio in user space. DolbyAPOvlldp150 analyses the
stream, computes MSIIR coefficients, and injects them into the MSIIR modules
already present in the DSP graph. The DSP filters, ahead of speaker
protection. Earlier Linux attempts filtered samples in PipeWire/LADSPA, which
is architecturally different: the DSP's own MSIIR stayed flat and the
processing happened in the wrong place.

The kernel control 'SP11 MSIIR Inject' provides the missing delivery path.

SAFETY
------
Speaker protection is live and verified on this machine (VI feedback at 8 kHz
on both amps, R0/T0 calibration byte-identical to Windows). That is what makes
a bass-lift test acceptable at all. Even so:

  * gains here are deliberately modest
  * every biquad is stability-checked before sending
  * the low-shelf option is omitted on purpose; a shelf lifts everything below
    its corner, which is where these small drivers have least excursion
    headroom. Peaking filters are bounded on both sides.

Do not raise the gains casually. Ask what protection is actually doing first.

USAGE
    ./sp11_msiir_filter.py --list
    ./sp11_msiir_filter.py --preset warmth --dry-run
    ./sp11_msiir_filter.py --preset warmth
    ./sp11_msiir_filter.py --preset flat        # restore pass-through
"""

from __future__ import annotations

import argparse
import math
import struct
import subprocess
import sys
from pathlib import Path

CARD = "hw:0"
CONTROL = "SP11 MSIIR Inject"
TLV_WRITE = Path(__file__).resolve().parent / "bin" / "tlv_write"

MSIIR_A = 0x489E          # last stage before the SoundWire sink
MSIIR_B = 0x48A1          # earlier stage in the chain
PARAM_COEFFS = 0x08001022

FS = 48000
Q30 = 1 << 30

# Structural constants decoded from the shipped topology blob. See build_payload().
Q_FIELD = 0x00030000
TRAILER = 0x00020002

# Presets are (freq_hz, gain_db, Q) tuples. Kept conservative on purpose.
PRESETS: dict[str, list[tuple[float, float, float]]] = {
    "flat": [],
    "warmth": [(250.0, 4.0, 0.9), (2500.0, -3.0, 1.0)],
    "presence": [(3500.0, 3.0, 1.0)],
    "bass-probe": [(250.0, 6.0, 0.9)],
}


def peaking(f0: float, gain_db: float, q: float, fs: int = FS) -> list[float]:
    """RBJ peaking EQ, normalised, returned as [b0, b1, b2, a1, a2].

    Sign convention: a1/a2 are returned already negated, i.e. the DSP form
    y = b0*x0 + b1*x1 + b2*x2 + a1*y1 + a2*y2.
    """
    a = 10 ** (gain_db / 40)
    w0 = 2 * math.pi * f0 / fs
    alpha = math.sin(w0) / (2 * q)
    cos_w0 = math.cos(w0)

    b0, b1, b2 = 1 + alpha * a, -2 * cos_w0, 1 - alpha * a
    a0, a1, a2 = 1 + alpha / a, -2 * cos_w0, 1 - alpha / a

    return [b0 / a0, b1 / a0, b2 / a0, -a1 / a0, -a2 / a0]


def check_stable(coeffs: list[float], label: str) -> bool:
    """Reject anything whose poles are outside the unit circle.

    An unstable biquad does not merely sound wrong, it diverges. Given these
    coefficients drive a filter feeding real speakers, this check is not
    optional.
    """
    a1, a2 = -coeffs[3], -coeffs[4]
    ok_a2 = abs(a2) < 1.0
    ok_a1 = abs(a1) < 1.0 + a2
    if not (ok_a2 and ok_a1):
        print(f"  UNSTABLE {label}: |a2|={abs(a2):.4f} |a1|={abs(a1):.4f}", file=sys.stderr)
        return False
    q30 = [int(round(c * Q30)) for c in coeffs]
    if not all(-2**31 <= v < 2**31 for v in q30):
        print(f"  OVERFLOW {label}: does not fit int32 in Q30", file=sys.stderr)
        return False
    return True


def build_payload(stages: list[tuple[float, float, float]], channels: int = 2) -> bytes:
    """MSIIR param 0x08001022 body, matching the deployed topology exactly.

    LAYOUT (decoded 2026-08-01 from the shipped topology blob for 0x48a1;
    do not guess this, it was wrong twice before):

        u32 channels          2
        u32 num_stages        n - 1   (count is one LESS than stages present)
        u32 reserved          0
        u32 q_field           0x00030000
        int32 coeffs[5] x n           b0, b1, b2, a1, a2 in Q30
        u32 trailer           0x00020002

    ...and the whole structure appears TWICE, once per channel. The shipped
    0x48a1 payload is 164 bytes = two 82-byte blocks. An earlier version of
    this function emitted a single 36-48 byte block with q_field=0 and no
    trailer; the DSP returned rc=0 and silently ignored it, which is why the
    first listening tests heard nothing.
    """
    n = max(len(stages), 1)

    if stages:
        coeff_words = []
        for f0, gain, q in stages:
            c = peaking(f0, gain, q)
            if not check_stable(c, f"{gain:+g}dB @{f0:g}Hz"):
                raise SystemExit("refusing to send an unstable filter")
            coeff_words.append([int(round(x * Q30)) for x in c])
    else:
        coeff_words = [[Q30, 0, 0, 0, 0]]

    def block() -> bytes:
        out = struct.pack("<IIII", channels, max(len(coeff_words) - 1, 0), 0, Q_FIELD)
        for cw in coeff_words:
            out += struct.pack("<iiiii", *cw)
        out += struct.pack("<I", TRAILER)
        return out

    return block() + block()


def graph_running() -> bool:
    try:
        with open("/proc/asound/card0/pcm0p/sub0/status") as fh:
            return fh.readline().startswith("state:")
    except OSError:
        return False


def numid() -> int | None:
    out = subprocess.run(["amixer", "-D", CARD, "controls"],
                         capture_output=True, text=True).stdout
    for line in out.splitlines():
        if CONTROL.lower() in line.lower():
            return int(line.split("numid=")[1].split(",")[0])
    return None


def send(iid: int, payload: bytes, nid: int, dry: bool) -> int:
    blob = struct.pack("<III", iid, PARAM_COEFFS, len(payload)) + payload
    print(f"  iid {iid:#06x}  {len(payload)} byte payload  ({len(blob)} total)")
    if dry:
        print("    DRY RUN")
        return 0
    if not TLV_WRITE.exists():
        raise SystemExit(f"missing helper {TLV_WRITE}; build it with:\n"
                         f"  gcc -o {TLV_WRITE} {TLV_WRITE.parent.parent}/tlv_write.c -lasound")
    r = subprocess.run(["sudo", str(TLV_WRITE), CARD, str(nid), blob.hex()],
                       capture_output=True, text=True)
    print("   ", r.stdout.strip() or r.stderr.strip())
    return r.returncode


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--preset", default="warmth", choices=sorted(PRESETS))
    ap.add_argument("--iid", type=lambda x: int(x, 0), default=MSIIR_A)
    ap.add_argument("--both", action="store_true", help="send to both MSIIR instances")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    if args.list:
        for name, stages in PRESETS.items():
            desc = ", ".join(f"{g:+g}dB @{f:g}Hz Q{q:g}" for f, g, q in stages) or "pass-through"
            print(f"  {name:<12} {desc}")
        return 0

    nid = numid()
    if nid is None:
        print(f"'{CONTROL}' not found. Booted into the diagnostic kernel?", file=sys.stderr)
        return 1

    print(f"control numid={nid}   graph {'running' if graph_running() else 'IDLE'}")
    if not graph_running():
        print("\nStart audio first: MSIIR gates tuning behind CAPI init, so an\n"
              "idle graph will reject the parameter.", file=sys.stderr)
        return 1

    stages = PRESETS[args.preset]
    print(f"preset  {args.preset}: " +
          (", ".join(f"{g:+g}dB @{f:g}Hz Q{q:g}" for f, g, q in stages) or "pass-through"))

    payload = build_payload(stages)
    targets = [MSIIR_A, MSIIR_B] if args.both else [args.iid]

    rc = 0
    for iid in targets:
        rc |= send(iid, payload, nid, args.dry_run)

    if rc == 0 and not args.dry_run:
        print("\nSent. If the DSP applies coefficients, this should be audible.")
        print("Restore with:  ./sp11_msiir_filter.py --preset flat")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
