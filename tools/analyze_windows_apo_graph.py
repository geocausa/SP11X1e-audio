#!/usr/bin/env python3
"""Read ARM64 audiodg full-memory minidumps and report known APO graph edges.

This intentionally implements only the MINIDUMP Memory64 stream needed for the
SP11 state-pinned captures, so it does not depend on third-party parsers knowing
Windows' ARM64 processor-architecture enum value.
"""
from __future__ import annotations

import argparse
import math
import struct
import uuid
from dataclasses import dataclass
from pathlib import Path

MEMORY64_LIST_STREAM = 9

APO_GUIDS = {
    "audio_cleanup": "0f92ff8d-2f19-4b9a-b9dd-3efc2b3becec",
    "virtual_surround": "4be8a061-c73b-4f23-8114-317aae3e8698",
    "dolby_dax_sfx": "0ebd8605-17bb-4ae7-ad76-e86f99a425e9",
    "audio_meter": "3dc09436-7d83-4ba0-addc-cd47f996c5ba",
    "audio_volume": "06587e71-f043-403a-bf49-cb591ba6e103",
    "audio_constrictor": "07252659-bb6b-4b79-b78b-623f6699a579",
    "audio_mixer": "12dd4dbb-532b-4fce-8653-74cdb9c8fe5a",
    "adaptive_spatial": "5bbc2c71-dec2-4ba3-961a-36f37d1cc8a5",
    "dolby_dax_mfx": "0ebd8606-17bb-4ae7-ad76-e86f99a425e9",
    "surface_render_mfx": "34d30cd8-370e-4229-85be-3346c594c805",
    "audio_limiter": "d69e0717-dd4b-4b25-997a-da813833b8ac",
    "audio_format_convert": "3fd7f233-a716-472e-8f2f-c25954f34e96",
}


@dataclass(frozen=True)
class Segment:
    start: int
    size: int
    file_offset: int


class Memory64Dump:
    def __init__(self, path: Path):
        self.path = path
        self._fh = path.open("rb")
        header = self._fh.read(32)
        if len(header) < 32 or header[:4] != b"MDMP":
            raise ValueError(f"{path}: not a minidump")
        stream_count, directory_rva = struct.unpack_from("<II", header, 8)
        self._fh.seek(directory_rva)
        memory64_rva = None
        for _ in range(stream_count):
            stream_type, data_size, rva = struct.unpack("<III", self._fh.read(12))
            if stream_type == MEMORY64_LIST_STREAM:
                memory64_rva = rva
        if memory64_rva is None:
            raise ValueError(f"{path}: no Memory64ListStream")
        self._fh.seek(memory64_rva)
        count, base_rva = struct.unpack("<QQ", self._fh.read(16))
        desc = [struct.unpack("<QQ", self._fh.read(16)) for _ in range(count)]
        file_off = base_rva
        self.segments = []
        for start, size in desc:
            self.segments.append(Segment(start, size, file_off))
            file_off += size

    def close(self):
        self._fh.close()

    def read(self, address: int, size: int) -> bytes:
        for seg in self.segments:
            if seg.start <= address and address + size <= seg.start + seg.size:
                self._fh.seek(seg.file_offset + address - seg.start)
                data = self._fh.read(size)
                if len(data) != size:
                    raise ValueError("short dump read")
                return data
        raise ValueError(f"address not present: {address:#x}+{size:#x}")

    def search(self, needle: bytes) -> list[int]:
        hits = []
        for seg in self.segments:
            self._fh.seek(seg.file_offset)
            data = self._fh.read(seg.size)
            pos = 0
            while True:
                pos = data.find(needle, pos)
                if pos < 0:
                    break
                hits.append(seg.start + pos)
                pos += 1
        return hits


def _array_values(dump: Memory64Dump, object_bytes: bytes, offset: int) -> list[int]:
    data, count, capacity, _grow = struct.unpack_from("<QQQQ", object_bytes, offset)
    if count > capacity or count > 16 or not data:
        return []
    return list(struct.unpack("<" + "Q" * count, dump.read(data, count * 8)))


def _connection(dump: Memory64Dump, address: int) -> dict:
    raw = dump.read(address, 0xA0)
    return {
        "node": address,
        "handle": struct.unpack_from("<Q", raw, 0x10)[0],
        "pcm": struct.unpack_from("<Q", raw, 0x48)[0],
        "frames": struct.unpack_from("<Q", raw, 0x50)[0],
        "properties": struct.unpack_from("<Q", raw, 0x68)[0],
    }


def _pcm_stats(dump: Memory64Dump, address: int, frames: int) -> tuple[float, bytes]:
    # SP11 graph edges here are stereo float32.
    raw = dump.read(address, frames * 2 * 4)
    floats = struct.unpack("<" + "f" * (frames * 2), raw)
    finite = [value for value in floats if math.isfinite(value)]
    peak = max((abs(value) for value in finite), default=0.0)
    return peak, raw


def find_apo_edges(dump: Memory64Dump) -> list[dict]:
    edges = []
    for name, guid_text in APO_GUIDS.items():
        guid = uuid.UUID(guid_text).bytes_le
        for guid_address in dump.search(guid):
            # CAPONode stores its APO GUID at +0x138.
            caponode = guid_address - 0x138
            try:
                raw = dump.read(caponode, 0x1C0)
                inputs = [_connection(dump, item) for item in _array_values(dump, raw, 0x38)]
                outputs = [_connection(dump, item) for item in _array_values(dump, raw, 0x58)]
                if not inputs or not outputs:
                    continue
                if any(not item["handle"] for item in inputs + outputs):
                    continue
                input_peaks = []
                output_peaks = []
                for item in inputs:
                    peak, _ = _pcm_stats(dump, item["pcm"], item["frames"])
                    input_peaks.append(peak)
                for item in outputs:
                    peak, _ = _pcm_stats(dump, item["pcm"], item["frames"])
                    output_peaks.append(peak)
                exact_pcm = None
                compared_floats = 0
                if len(inputs) == 1 and len(outputs) == 1:
                    frames = min(inputs[0]["frames"], outputs[0]["frames"])
                    _, input_raw = _pcm_stats(dump, inputs[0]["pcm"], frames)
                    _, output_raw = _pcm_stats(dump, outputs[0]["pcm"], frames)
                    exact_pcm = input_raw == output_raw
                    compared_floats = frames * 2
            except (ValueError, struct.error):
                continue
            edges.append(
                {
                    "name": name,
                    "guid": guid_text,
                    "caponode": caponode,
                    "inputs": inputs,
                    "outputs": outputs,
                    "input_peaks": input_peaks,
                    "output_peaks": output_peaks,
                    "exact_pcm": exact_pcm,
                    "compared_floats": compared_floats,
                }
            )
    return edges


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path, nargs="+")
    args = parser.parse_args()
    for path in args.dump:
        dump = Memory64Dump(path)
        try:
            print(f"DUMP {path}")
            for edge in find_apo_edges(dump):
                ins, outs = edge["inputs"], edge["outputs"]
                input_handles = ",".join(f"h{item['handle']}" for item in ins)
                output_handles = ",".join(f"h{item['handle']}" for item in outs)
                input_peaks = ",".join(f"{peak:.9f}" for peak in edge["input_peaks"])
                output_peaks = ",".join(f"{peak:.9f}" for peak in edge["output_peaks"])
                exact = "n/a" if edge["exact_pcm"] is None else str(edge["exact_pcm"]).lower()
                print(
                    f"  {edge['name']:20s} "
                    f"{input_handles} -> {output_handles} "
                    f"peak={input_peaks}->{output_peaks} "
                    f"exact_pcm={exact} caponode={edge['caponode']:#x}"
                )
        finally:
            dump.close()


if __name__ == "__main__":
    main()
