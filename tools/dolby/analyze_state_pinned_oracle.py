#!/usr/bin/env python3
"""Analyze a state-pinned SP11 Windows Dolby oracle capture.

The tool deliberately works from raw bytes instead of debugger symbols. It can:

* scan PCM16 stereo loopback WAVs for the strongest 75 Hz window and report
  fundamental/H3/H5;
* parse a Windows full minidump (MINIDUMP_MEMORY64_LIST + module list);
* locate the unique live DolbyAPOvlldp150 inner wrapper from its relocated
  vtable pointer;
* report production-relevant VLLDP scalar state, final-limiter state and the
  256-frame input/output staging buffers.

Raw WAV/DMP evidence is intentionally not stored in Git. Pass local paths whose
hashes are recorded in the corresponding findings document.
"""

from __future__ import annotations

import argparse
import array
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import wave
from typing import Any

MINIDUMP_SIGNATURE = 0x504D444D
MODULE_LIST_STREAM = 4
MEMORY64_LIST_STREAM = 9
MODULE_ENTRY_SIZE = 108
VLLDP_VTABLE_RVA = 0x10B9A8
VLLDP_CORE_FROM_WRAPPER = 0x168


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_pcm16_stereo(path: Path) -> tuple[int, array.array, int]:
    with wave.open(str(path), "rb") as wav:
        if wav.getnchannels() != 2 or wav.getsampwidth() != 2:
            raise ValueError(f"expected PCM16 stereo WAV: {path}")
        rate = wav.getframerate()
        frames = wav.getnframes()
        samples = array.array("h")
        samples.frombytes(wav.readframes(frames))
    if sys.byteorder != "little":
        samples.byteswap()
    return rate, samples, frames


def mono_dft_amplitude(
    samples: array.array,
    rate: int,
    start_seconds: float,
    duration_seconds: float,
    frequency: float,
) -> float:
    start = max(0, int(round(start_seconds * rate)))
    frame_count = min(
        int(round(duration_seconds * rate)), len(samples) // 2 - start
    )
    if frame_count <= 0:
        return 0.0
    cos_sum = 0.0
    sin_sum = 0.0
    for j in range(frame_count):
        i = start + j
        value = (samples[2 * i] + samples[2 * i + 1]) / 65536.0
        angle = 2.0 * math.pi * frequency * j / rate
        cos_sum += value * math.cos(angle)
        sin_sum += value * math.sin(angle)
    return 2.0 * math.hypot(cos_sum, sin_sum) / frame_count


def db(value: float) -> float:
    return 20.0 * math.log10(max(abs(value), 1e-30))


def waveform_window(
    path: Path,
    start: float,
    duration: float,
    fundamental: float = 75.0,
) -> dict[str, Any]:
    rate, samples, frames = read_pcm16_stereo(path)
    amplitudes = [
        mono_dft_amplitude(samples, rate, start, duration, frequency)
        for frequency in (fundamental, 3 * fundamental, 5 * fundamental)
    ]
    f0 = amplitudes[0]
    return {
        "path": str(path),
        "sha256": sha256_file(path),
        "sample_rate": rate,
        "duration_seconds": frames / rate,
        "window_start_seconds": start,
        "window_duration_seconds": duration,
        "fundamental_hz": fundamental,
        "fundamental_dbfs": db(f0),
        "h3_dbc": db(amplitudes[1] / max(f0, 1e-30)),
        "h5_dbc": db(amplitudes[2] / max(f0, 1e-30)),
    }


def strongest_fundamental_window(
    path: Path,
    scan_start: float,
    scan_end: float,
    duration: float = 0.4,
    step: float = 0.05,
    fundamental: float = 75.0,
) -> dict[str, Any]:
    rate, samples, frames = read_pcm16_stereo(path)
    best_start = scan_start
    best_amp = -1.0
    cursor = scan_start
    while cursor + duration <= min(scan_end, frames / rate) + 1e-12:
        amp = mono_dft_amplitude(samples, rate, cursor, duration, fundamental)
        if amp > best_amp:
            best_amp = amp
            best_start = cursor
        cursor += step
    result = waveform_window(path, best_start, duration, fundamental)
    result["scan_start_seconds"] = scan_start
    result["scan_end_seconds"] = scan_end
    result["scan_step_seconds"] = step
    return result


class MiniDump:
    def __init__(self, path: Path):
        self.path = path
        self._stream = path.open("rb")
        header = self._read(0, 32)
        (
            signature,
            _version,
            stream_count,
            directory_rva,
            _checksum,
            _timestamp,
            _flags,
        ) = struct.unpack("<IIIIIIQ", header)
        if signature != MINIDUMP_SIGNATURE:
            raise ValueError(f"not a minidump: {path}")

        self.streams: dict[int, tuple[int, int]] = {}
        for index in range(stream_count):
            stream_type, size, rva = struct.unpack(
                "<III", self._read(directory_rva + 12 * index, 12)
            )
            self.streams[stream_type] = (size, rva)

        if MODULE_LIST_STREAM not in self.streams:
            raise ValueError("minidump has no module list stream")
        if MEMORY64_LIST_STREAM not in self.streams:
            raise ValueError("minidump has no Memory64 stream")

        self.modules = self._read_modules()
        self.ranges = self._read_memory64_ranges()

    def close(self) -> None:
        self._stream.close()

    def __enter__(self) -> "MiniDump":
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def _read(self, offset: int, size: int) -> bytes:
        self._stream.seek(offset)
        data = self._stream.read(size)
        if len(data) != size:
            raise EOFError(f"short read at file offset 0x{offset:x}")
        return data

    def _u32(self, offset: int) -> int:
        return struct.unpack("<I", self._read(offset, 4))[0]

    def _read_modules(self) -> list[dict[str, Any]]:
        _size, rva = self.streams[MODULE_LIST_STREAM]
        count = self._u32(rva)
        modules = []
        for index in range(count):
            entry = self._read(rva + 4 + index * MODULE_ENTRY_SIZE, MODULE_ENTRY_SIZE)
            base, image_size, _checksum, _timestamp, name_rva = struct.unpack_from(
                "<QIIII", entry, 0
            )
            name_bytes = self._u32(name_rva)
            name = self._read(name_rva + 4, name_bytes).decode(
                "utf-16le", errors="replace"
            )
            modules.append({"base": base, "size": image_size, "name": name})
        return modules

    def _read_memory64_ranges(self) -> list[tuple[int, int, int]]:
        _size, rva = self.streams[MEMORY64_LIST_STREAM]
        count, data_rva = struct.unpack("<QQ", self._read(rva, 16))
        file_offset = data_rva
        ranges = []
        for index in range(count):
            address, size = struct.unpack(
                "<QQ", self._read(rva + 16 + index * 16, 16)
            )
            ranges.append((address, size, file_offset))
            file_offset += size
        return ranges

    def virtual_read(self, address: int, size: int) -> bytes:
        for start, length, file_offset in self.ranges:
            if start <= address and address + size <= start + length:
                return self._read(file_offset + address - start, size)
        raise KeyError(f"virtual address not captured: 0x{address:x}+0x{size:x}")

    def u32(self, address: int) -> int:
        return struct.unpack("<I", self.virtual_read(address, 4))[0]

    def i32(self, address: int) -> int:
        return struct.unpack("<i", self.virtual_read(address, 4))[0]

    def u64(self, address: int) -> int:
        return struct.unpack("<Q", self.virtual_read(address, 8))[0]

    def f32(self, address: int) -> float:
        return struct.unpack("<f", self.virtual_read(address, 4))[0]

    def find_u64(self, value: int) -> list[int]:
        needle = struct.pack("<Q", value)
        hits: list[int] = []
        for start, length, file_offset in self.ranges:
            self._stream.seek(file_offset)
            remaining = length
            consumed = 0
            overlap = b""
            while remaining:
                chunk = self._stream.read(min(4 * 1024 * 1024, remaining))
                if not chunk:
                    break
                data = overlap + chunk
                cursor = 0
                while True:
                    index = data.find(needle, cursor)
                    if index < 0:
                        break
                    hits.append(start + consumed - len(overlap) + index)
                    cursor = index + 1
                overlap = data[-7:]
                consumed += len(chunk)
                remaining -= len(chunk)
        return hits


def rms_peak(values: tuple[float, ...]) -> dict[str, float]:
    if not values:
        return {"rms": 0.0, "peak": 0.0}
    return {
        "rms": math.sqrt(sum(value * value for value in values) / len(values)),
        "peak": max(abs(value) for value in values),
    }


def analyze_vlldp_dump(path: Path) -> dict[str, Any]:
    with MiniDump(path) as dump:
        module = next(
            (
                item
                for item in dump.modules
                if item["name"].lower().endswith("dolbyapovlldp150.dll")
            ),
            None,
        )
        if module is None:
            raise ValueError("DolbyAPOvlldp150.dll is not present in the dump")

        vtable = module["base"] + VLLDP_VTABLE_RVA
        wrappers = dump.find_u64(vtable)
        if len(wrappers) != 1:
            raise ValueError(
                f"expected one live VLLDP wrapper, found {len(wrappers)}: "
                + ", ".join(hex(item) for item in wrappers)
            )
        wrapper = wrappers[0]
        core = wrapper + VLLDP_CORE_FROM_WRAPPER
        limiter = dump.u64(core + 0x88)

        block = dump.u32(wrapper + 0x3C)
        fill = dump.u32(wrapper + 0x20)
        input_staging = dump.u64(wrapper + 0x10)
        output_staging = dump.u64(wrapper + 0x18)
        input_values = struct.unpack(
            f"<{2 * block}f", dump.virtual_read(input_staging, 2 * block * 4)
        )
        output_values = struct.unpack(
            f"<{2 * block}f", dump.virtual_read(output_staging, 2 * block * 4)
        )

        return {
            "path": str(path),
            "sha256": sha256_file(path),
            "vlldp_module": {
                "path": module["name"],
                "base": f"0x{module['base']:x}",
                "vtable": f"0x{vtable:x}",
            },
            "wrapper": f"0x{wrapper:x}",
            "core": f"0x{core:x}",
            "block_frames": block,
            "fill_frames": fill,
            "scalars": {
                "system_gain": dump.i32(core + 0x94),
                "postgain_applied": dump.i32(core + 0xBB0),
                "postgain_staged": dump.i32(core + 0xBB4),
                "peak_level": dump.i32(core + 0xDD4),
                "limiter_ceiling": dump.f32(core + 0xDD8),
            },
            "final_limiter": {
                "address": f"0x{limiter:x}",
                "envelope": dump.f32(limiter + 0x64),
                "stored_peak": dump.f32(limiter + 0x68),
                "current_gain": dump.f32(limiter + 0x78),
                "previous_gain": dump.f32(limiter + 0x7C),
                "target_gain": dump.f32(limiter + 0x80),
            },
            "input_staging": {
                "address": f"0x{input_staging:x}",
                **rms_peak(input_values),
                "left": rms_peak(input_values[0::2]),
                "right": rms_peak(input_values[1::2]),
            },
            "output_staging": {
                "address": f"0x{output_staging:x}",
                **rms_peak(output_values),
                "left": rms_peak(output_values[0::2]),
                "right": rms_peak(output_values[1::2]),
            },
        }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--dump", type=Path, help="state-pinned audiodg full minidump")
    parser.add_argument("--source-wav", type=Path, help="known input PCM16 stereo WAV")
    parser.add_argument("--loopback-wav", type=Path, help="Windows loopback PCM16 stereo WAV")
    parser.add_argument("--source-start", type=float, default=27.4)
    parser.add_argument("--source-window", type=float, default=0.4)
    parser.add_argument("--loopback-scan-start", type=float, default=25.0)
    parser.add_argument("--loopback-scan-end", type=float, default=32.0)
    parser.add_argument("--loopback-window", type=float, default=0.4)
    parser.add_argument("--loopback-step", type=float, default=0.05)
    parser.add_argument("--fundamental", type=float, default=75.0)
    parser.add_argument("-o", "--output", type=Path)
    args = parser.parse_args()

    if not any((args.dump, args.source_wav, args.loopback_wav)):
        parser.error("provide --dump, --source-wav and/or --loopback-wav")

    result: dict[str, Any] = {"format": "sp11-state-pinned-dolby-oracle", "version": 1}
    if args.source_wav:
        result["source"] = waveform_window(
            args.source_wav,
            args.source_start,
            args.source_window,
            args.fundamental,
        )
    if args.loopback_wav:
        result["loopback"] = strongest_fundamental_window(
            args.loopback_wav,
            args.loopback_scan_start,
            args.loopback_scan_end,
            args.loopback_window,
            args.loopback_step,
            args.fundamental,
        )
    if args.dump:
        result["dump"] = analyze_vlldp_dump(args.dump)

    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
