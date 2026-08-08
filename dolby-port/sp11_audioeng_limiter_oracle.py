#!/usr/bin/env python3
"""Offline replica of Windows AudioEng!CAudioLimiter for SP11 parity work.

Derived from the exact ARM64 AudioEng.dll SHA-256
1e2cc764cae6ebfb6985d8503bb83a36022852fbbf1841c377c5ad2fa2d6795b.
This is an analysis/oracle tool, not production deployment code.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import numpy as np

CEILING = np.float32(0.9850000143051147)
RELEASE_K = 2.205
CATASTROPHIC_PEAK = np.float32(128.0)


def lookahead_frames(sample_rate: float) -> int:
    # AudioEng FUN_180045E30 exact rate buckets.
    if sample_rate <= 16000.0:
        return 16
    if sample_rate <= 32000.0:
        return 32
    if sample_rate <= 64000.0:
        return 64
    return 128


def process(x: np.ndarray, sample_rate: float = 48000.0, flush: bool = True):
    """Process interleaved-channel float audio with the decoded CAudioLimiter.

    Returns (output, stats). Output includes the limiter's look-ahead latency;
    when flush=True it has len(x)+lookahead frames, matching a zero-tail flush.
    """
    x = np.asarray(x, dtype=np.float32)
    if x.ndim != 2:
        raise ValueError("audio must be [frames, channels]")
    n, ch = x.shape
    la = lookahead_frames(sample_rate)
    release_up = np.exp(RELEASE_K / sample_rate)
    release_down = np.exp(-RELEASE_K / sample_rate)

    gain = np.float32(1.0)
    envelope = np.float32(CEILING)
    target = np.float32(1.0)
    step = np.float32(0.0)
    attack_left = 0
    bad = False
    limited_frames = 0
    min_gain = 1.0
    first_limited = None

    # At engine time t the detector sees input[t], while the audio being emitted
    # is input[t-lookahead]. This is the semantic equivalent of FUN_18000AD20's
    # delay-buffer + FUN_18000B500/FUN_18000B200 pipeline.
    total = n + (la if flush else 0)
    out = np.zeros((total, ch), dtype=np.float32)

    for t in range(total):
        if t < n:
            p = np.float32(np.max(np.abs(x[t])))
        else:
            p = np.float32(0.0)

        current_before = gain
        if p > envelope:
            if p > CATASTROPHIC_PEAK:
                bad = True
                break
            new_target = np.float32(CEILING / p)
            if new_target < gain:
                if attack_left == 0:
                    target = new_target
                    step = np.float32((gain - new_target) / la)
                    attack_left = la
                else:
                    new_step = np.float32((gain - new_target) / la)
                    # ARM64 branch at 0x18000B490..4D0: if the newly proposed
                    # ramp would reach/bypass the old target, replace it with a
                    # fresh N-sample ramp. Otherwise extend the existing ramp by
                    # ceil((old_target-new_target)/old_step).
                    if np.float32(gain - attack_left * new_step) <= target:
                        step = new_step
                        attack_left = la
                    else:
                        if step > 0.0:
                            attack_left += int(np.ceil(float((target - new_target) / step)))
                        target = new_target
                target = new_target
            envelope = p

        # Exact order in FUN_18000B200: attack/release state is updated before
        # multiplying the delayed frame.
        if attack_left == 0:
            if (2.0 * float(p) <= float(envelope)) and gain < 1.0:
                envelope = np.float32(float(envelope) * release_down)
                gain = np.float32(float(gain) * release_up)
                if gain >= 1.0 or envelope <= CEILING:
                    gain = np.float32(1.0)
                    envelope = np.float32(CEILING)
        else:
            attack_left -= 1
            gain = np.float32(gain - step)

        if gain < 1.0:
            limited_frames += 1
            if first_limited is None:
                first_limited = t
            if float(gain) < min_gain:
                min_gain = float(gain)

        src = t - la
        if 0 <= src < n:
            out[t] = x[src] * gain

    stats = {
        "sample_rate": float(sample_rate),
        "lookahead_frames": la,
        "ceiling": float(CEILING),
        "limited_frames": limited_frames,
        "first_limited_engine_frame": first_limited,
        "min_gain": min_gain,
        "final_gain": float(gain),
        "final_envelope": float(envelope),
        "catastrophic_guard": bad,
    }
    return out, stats


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("input_f32", type=Path)
    ap.add_argument("output_f32", type=Path)
    ap.add_argument("--channels", type=int, default=2)
    ap.add_argument("--rate", type=float, default=48000.0)
    ap.add_argument("--no-flush", action="store_true")
    args = ap.parse_args()
    raw = np.fromfile(args.input_f32, dtype="<f4")
    if raw.size % args.channels:
        raise SystemExit("input length is not a whole number of frames")
    x = raw.reshape(-1, args.channels)
    y, stats = process(x, args.rate, not args.no_flush)
    y.astype("<f4").tofile(args.output_f32)
    for k, v in stats.items():
        print(f"{k}={v}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
