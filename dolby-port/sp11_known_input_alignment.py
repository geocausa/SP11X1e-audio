#!/usr/bin/env python3
"""Robust time alignment for the SP11 known-input Windows loopback oracle.

The original 2026-05-18 helper correlated a long periodic 1-kHz reference and
could lock exactly one second late.  This helper aligns *changes in log RMS
envelope* across the complete non-periodic stimulus instead.  The coarse pass
uses 10-ms blocks; a 1-ms pass refines the winning delay.
"""
from __future__ import annotations

import math
import wave
from pathlib import Path
from typing import Tuple

import numpy as np


def read_pcm16_wav(path: Path | str) -> Tuple[int, np.ndarray]:
    with wave.open(str(path), "rb") as wf:
        rate = wf.getframerate()
        channels = wf.getnchannels()
        width = wf.getsampwidth()
        raw = wf.readframes(wf.getnframes())
    if width != 2:
        raise ValueError(f"{path}: expected PCM16, got {width * 8}-bit")
    x = np.frombuffer(raw, dtype="<i2").astype(np.float64) / 32768.0
    return rate, x.reshape(-1, channels)


def _log_rms_feature(x: np.ndarray, block: int, floor: float = 1e-7) -> np.ndarray:
    """Return first difference of log10 block RMS for a mono signal."""
    n = len(x) // block
    if n < 3:
        raise ValueError("signal too short for alignment feature")
    z = x[: n * block].reshape(n, block)
    rms = np.sqrt(np.mean(z * z, axis=1))
    return np.diff(np.log10(rms + floor))


def _best_lag_blocks(ref: np.ndarray, target: np.ndarray, lo: int, hi: int) -> tuple[int, float]:
    best_lag = lo
    best_corr = -math.inf
    for lag in range(lo, hi + 1):
        n = min(len(ref), len(target) - lag)
        if n < 32:
            continue
        a = ref[:n]
        b = target[lag : lag + n]
        # Pearson correlation rejects overall gain changes and concentrates on
        # event timing.  Dolby's dynamics may alter magnitudes but not the order
        # of the stimulus transitions.
        sa = float(np.std(a))
        sb = float(np.std(b))
        if sa == 0.0 or sb == 0.0:
            continue
        c = float(np.mean((a - np.mean(a)) * (b - np.mean(b))) / (sa * sb))
        if c > best_corr:
            best_corr = c
            best_lag = lag
    return best_lag, best_corr


def _waveform_refine(
    input_mono: np.ndarray,
    output_mono: np.ndarray,
    center_samples: int,
    rate: int,
    radius_ms: float = 2.0,
) -> tuple[int, float]:
    """Refine an event-aligned lag to individual samples.

    Use the broad, non-periodic 1..14-s program region and modest decimation.
    The preceding envelope pass guarantees we are already in the correct event
    basin, so this stage cannot jump by an integer tone period.
    """
    radius = max(1, int(round(rate * radius_ms / 1000.0)))
    start = min(rate, max(0, len(input_mono) // 20))
    end = min(len(input_mono), 14 * rate)
    stride = 4
    a0 = input_mono[start:end:stride]
    a0 = a0 - np.mean(a0)
    na = float(np.linalg.norm(a0))
    best_lag = center_samples
    best_corr = -math.inf
    for lag in range(max(0, center_samples - radius), center_samples + radius + 1):
        b0 = output_mono[start + lag : end + lag : stride]
        if len(b0) != len(a0):
            continue
        b0 = b0 - np.mean(b0)
        nb = float(np.linalg.norm(b0))
        if na == 0.0 or nb == 0.0:
            continue
        c = float(np.dot(a0, b0) / (na * nb))
        if c > best_corr:
            best_corr = c
            best_lag = lag
    return best_lag, best_corr


def robust_loopback_lag(
    input_mono: np.ndarray,
    output_mono: np.ndarray,
    rate: int,
    max_lag_seconds: float = 3.0,
) -> tuple[int, float]:
    """Return (lag_samples, refined_feature_correlation).

    Positive lag means output event time = input event time + lag/rate.
    A 1-ms event pass avoids periodic false locks; a final +/-2-ms waveform
    search then refines that correct event basin to individual samples.
    """
    coarse_ms = 10
    fine_ms = 1
    coarse_block = max(1, int(round(rate * coarse_ms / 1000.0)))
    fine_block = max(1, int(round(rate * fine_ms / 1000.0)))

    rc = _log_rms_feature(input_mono, coarse_block)
    tc = _log_rms_feature(output_mono, coarse_block)
    max_coarse = int(math.ceil(max_lag_seconds * 1000.0 / coarse_ms))
    coarse_lag, _ = _best_lag_blocks(rc, tc, 0, max_coarse)

    rf = _log_rms_feature(input_mono, fine_block)
    tf = _log_rms_feature(output_mono, fine_block)
    center = int(round(coarse_lag * coarse_ms / fine_ms))
    radius = int(math.ceil(30.0 / fine_ms))
    lo = max(0, center - radius)
    hi = min(int(math.ceil(max_lag_seconds * 1000.0 / fine_ms)), center + radius)
    fine_lag, event_corr = _best_lag_blocks(rf, tf, lo, hi)
    event_samples = fine_lag * fine_block
    exact_samples, waveform_corr = _waveform_refine(
        input_mono, output_mono, event_samples, rate
    )
    # Return the sample-refined lag.  The score is the waveform-refinement
    # correlation; callers needing the coarse event score can recompute it.
    return exact_samples, waveform_corr


def main() -> int:
    import argparse

    ap = argparse.ArgumentParser()
    ap.add_argument("input_wav", type=Path)
    ap.add_argument("output_wav", type=Path)
    ap.add_argument("--max-lag", type=float, default=3.0)
    args = ap.parse_args()
    sr, x = read_pcm16_wav(args.input_wav)
    sr2, y = read_pcm16_wav(args.output_wav)
    if sr != sr2:
        raise SystemExit("sample rates differ")
    lag, corr = robust_loopback_lag(x.mean(axis=1), y.mean(axis=1), sr, args.max_lag)
    print(f"lag_samples={lag}")
    print(f"lag_seconds={lag / sr:.9f}")
    print(f"feature_corr={corr:.9f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
