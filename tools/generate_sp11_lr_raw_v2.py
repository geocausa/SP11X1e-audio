#!/usr/bin/env python3
"""Generate the versioned SP11 fixed-fixture L/R RAW acoustic oracle."""
from __future__ import annotations
import hashlib
import wave
from pathlib import Path
import numpy as np

FS = 48_000
FREQS = np.array([100,125,160,200,250,315,400,500,630,800,1000,1250,1600,2000,2500,3150,4000,5000,6300], dtype=np.float64)
ACTIVE_S = 10
SILENCE_S = 2
PEAK = 0.08
RAMP_S = 0.100
SEED = 20260818

def tone_bed() -> np.ndarray:
    n = ACTIVE_S * FS
    t = np.arange(n, dtype=np.float64) / FS
    rng = np.random.default_rng(SEED)
    phases = rng.uniform(0.0, 2.0*np.pi, len(FREQS))
    x = np.zeros(n, dtype=np.float64)
    for f, p in zip(FREQS, phases):
        x += np.sin(2.0*np.pi*f*t + p)
    x *= PEAK / np.max(np.abs(x))
    r = int(round(RAMP_S * FS))
    ramp = 0.5 - 0.5*np.cos(np.pi*np.arange(r, dtype=np.float64)/r)
    x[:r] *= ramp
    x[-r:] *= ramp[::-1]
    return x

def build() -> np.ndarray:
    bed = tone_bed()
    z = np.zeros(SILENCE_S*FS, dtype=np.float64)
    segments = [
        np.column_stack((z,z)),
        np.column_stack((bed,np.zeros_like(bed))),
        np.column_stack((z,z)),
        np.column_stack((np.zeros_like(bed),bed)),
        np.column_stack((z,z)),
        np.column_stack((bed,bed)),
        np.column_stack((z,z)),
    ]
    return np.concatenate(segments, axis=0)

def write(path: Path) -> None:
    x = build()
    pcm = np.clip(np.rint(x*32767.0), -32768, 32767).astype('<i2')
    with wave.open(str(path), 'wb') as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(FS); w.writeframes(pcm.tobytes())
    h = hashlib.sha256(path.read_bytes()).hexdigest()
    print(f'{h}  {path}')
    print(f'duration={len(x)/FS:.3f}s peak={np.max(np.abs(x)):.9f} freqs={",".join(map(str,FREQS.astype(int)))}')

if __name__ == '__main__':
    import argparse
    ap=argparse.ArgumentParser(); ap.add_argument('output', type=Path); a=ap.parse_args(); write(a.output)
