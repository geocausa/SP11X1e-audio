#!/usr/bin/env python3
"""Generate a deterministic 75 Hz stereo-matrix probe WAV.

Layout (17 s total at defaults):
  0..5 s   silence
  5..8 s   in-phase L=R tone
  8..9 s   silence
  9..12 s  left-only tone
 12..13 s  silence
 13..16 s  anti-phase L=-R tone
 16..17 s  silence

The three tone cases discriminate a hidden stereo sum/matrix from ordinary
per-channel gain. Raw probe WAVs are generated locally and are not versioned.
"""

from __future__ import annotations

import argparse
import hashlib
import math
from pathlib import Path
import struct
import wave


def write_probe(path: Path, rate: int = 48000, frequency: float = 75.0, amplitude: float = 0.25) -> None:
    sections = [
        (5.0, "silence"),
        (3.0, "in_phase"),
        (1.0, "silence"),
        (3.0, "left_only"),
        (1.0, "silence"),
        (3.0, "anti_phase"),
        (1.0, "silence"),
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as wav:
        wav.setnchannels(2)
        wav.setsampwidth(2)
        wav.setframerate(rate)
        for duration, mode in sections:
            frames = int(round(duration * rate))
            payload = bytearray()
            for index in range(frames):
                if mode == "silence":
                    left = right = 0.0
                else:
                    tone = amplitude * math.sin(2.0 * math.pi * frequency * index / rate)
                    if mode == "in_phase":
                        left = right = tone
                    elif mode == "left_only":
                        left, right = tone, 0.0
                    elif mode == "anti_phase":
                        left, right = tone, -tone
                    else:
                        raise AssertionError(mode)
                li = max(-32768, min(32767, int(round(left * 32768.0))))
                ri = max(-32768, min(32767, int(round(right * 32768.0))))
                payload += struct.pack("<hh", li, ri)
            wav.writeframes(payload)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--rate", type=int, default=48000)
    parser.add_argument("--frequency", type=float, default=75.0)
    parser.add_argument("--amplitude", type=float, default=0.25)
    args = parser.parse_args()
    write_probe(args.output, args.rate, args.frequency, args.amplitude)
    print(f"wrote {args.output}")
    print(f"sha256 {sha256(args.output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
