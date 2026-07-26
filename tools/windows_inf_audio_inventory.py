#!/usr/bin/env python3
"""Inventory Windows speaker and SP_VI formats from hash-bound driver INFs."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


REGISTRY_LINE = re.compile(
    r"^HKR,(?P<key>[^,]+),(?P<name>\"[^\"]+\"|[^,]+),"
    r"(?P<kind>[^,]*),(?P<value>.*?)(?:\s+;.*)?$",
    re.IGNORECASE,
)
SPEAKER_KEY = re.compile(
    r"^QCAUD\\WaveSpeaker\\FormatsAndModes(?P<group>\d+)"
    r"(?:\\ModeAndDefaultFormat(?P<mode>\d+))?"
    r"(?:\\WaveFormat(?P<format>\d+))?"
    r"(?:\\Effects)?$",
    re.IGNORECASE,
)

FORMAT_FIELDS = {
    "nChannels": "channels",
    "nSamplesPerSec": "sample_rate",
    "nBitsPerSample": "bits_per_sample",
    "nValidBitsPerSample": "valid_bits_per_sample",
    "dwChannelMask": "channel_mask",
}


def read_inf(path: Path) -> str:
    raw = path.read_bytes()
    if raw.startswith((b"\xff\xfe", b"\xfe\xff")):
        return raw.decode("utf-16")
    if b"\x00" in raw[:256]:
        return raw.decode("utf-16-le")
    return raw.decode("utf-8-sig")


def parse_value(value: str) -> int | str:
    value = value.strip()
    if value.startswith('"') and value.endswith('"'):
        return value[1:-1]
    if re.fullmatch(r"0[xX][0-9a-fA-F]+", value):
        return int(value, 16)
    if re.fullmatch(r"\d+", value):
        return int(value, 10)
    return value


def registry_entries(text: str) -> list[dict]:
    entries = []
    for line_number, raw_line in enumerate(text.splitlines(), 1):
        match = REGISTRY_LINE.match(raw_line.strip())
        if not match:
            continue
        entries.append(
            {
                "line": line_number,
                "key": match.group("key"),
                "name": match.group("name").strip('"'),
                "kind": match.group("kind"),
                "value": parse_value(match.group("value")),
            }
        )
    return entries


def speaker_inventory(entries: list[dict]) -> list[dict]:
    groups: dict[int, dict] = {}
    for entry in entries:
        match = SPEAKER_KEY.match(entry["key"])
        if not match:
            continue
        group_index = int(match.group("group"))
        group = groups.setdefault(
            group_index,
            {
                "group_index": group_index,
                "type": None,
                "modes": {},
                "pin_formats": {},
            },
        )
        mode_text = match.group("mode")
        format_text = match.group("format")
        name = entry["name"]
        value = entry["value"]
        if mode_text is None and format_text is None and name.lower() == "type":
            group["type"] = value
            continue
        if mode_text is not None:
            mode_index = int(mode_text)
            mode = group["modes"].setdefault(
                mode_index,
                {"mode_index": mode_index, "name": None, "formats": {}},
            )
            if format_text is None and name.lower() == "mode":
                mode["name"] = value
                continue
            target_formats = mode["formats"]
        else:
            target_formats = group["pin_formats"]
        if format_text is None or name not in FORMAT_FIELDS:
            continue
        format_index = int(format_text)
        audio_format = target_formats.setdefault(
            format_index, {"format_index": format_index}
        )
        field = FORMAT_FIELDS[name]
        if field == "channel_mask" and isinstance(value, int):
            audio_format[field] = f"0x{value:08x}"
        else:
            audio_format[field] = value

    result = []
    for group_index in sorted(groups):
        group = groups[group_index]
        group["pin_formats"] = [
            group["pin_formats"][index] for index in sorted(group["pin_formats"])
        ]
        group["modes"] = [
            {
                **group["modes"][index],
                "formats": [
                    group["modes"][index]["formats"][format_index]
                    for format_index in sorted(group["modes"][index]["formats"])
                ],
            }
            for index in sorted(group["modes"])
        ]
        result.append(group)
    return result


def all_formats(groups: list[dict], include_loopback: bool) -> list[dict]:
    formats = []
    for group in groups:
        if not include_loopback and str(group["type"]).lower() == "loopback":
            continue
        formats.extend(group["pin_formats"])
        for mode in group["modes"]:
            formats.extend(mode["formats"])
    return formats


def inventory(miniport_inf: Path, qcadcm_inf: Path) -> dict:
    miniport_entries = registry_entries(read_inf(miniport_inf))
    qcadcm_entries = registry_entries(read_inf(qcadcm_inf))
    groups = speaker_inventory(miniport_entries)
    vi_values = {
        entry["name"]: entry["value"]
        for entry in qcadcm_entries
        if entry["key"].lower() == r"adcm\spkrprotviinfo"
    }
    non_loopback = all_formats(groups, include_loopback=False)
    loopback = [
        audio_format
        for group in groups
        if str(group["type"]).lower() == "loopback"
        for audio_format in group["pin_formats"]
    ]
    return {
        "format": "SP11 Windows INF audio-format inventory",
        "format_version": 1,
        "evidence_class": (
            "B: installed Windows INF policy; driver code consumes these values, "
            "but this is not a captured DSP command body"
        ),
        "sources": {
            "surface_audio_miniport_extension_inf": {
                "path": str(miniport_inf.resolve()),
                "sha256": hashlib.sha256(miniport_inf.read_bytes()).hexdigest(),
            },
            "qcadcm_inf": {
                "path": str(qcadcm_inf.resolve()),
                "sha256": hashlib.sha256(qcadcm_inf.read_bytes()).hexdigest(),
            },
        },
        "speaker_format_groups": groups,
        "speaker_non_loopback_summary": {
            "format_count": len(non_loopback),
            "bits_per_sample": sorted(
                {
                    item["bits_per_sample"]
                    for item in non_loopback
                    if "bits_per_sample" in item
                }
            ),
            "valid_bits_per_sample": sorted(
                {
                    item["valid_bits_per_sample"]
                    for item in non_loopback
                    if "valid_bits_per_sample" in item
                }
            ),
        },
        "speaker_loopback_summary": {
            "format_count": len(loopback),
            "bits_per_sample": sorted(
                {
                    item["bits_per_sample"]
                    for item in loopback
                    if "bits_per_sample" in item
                }
            ),
        },
        "sp_vi_endpoint_registry": {
            "sample_rate": vi_values.get("SamplesPerSecond"),
            "bits_per_sample": vi_values.get("BitsPerSample"),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("surface_audio_miniport_extension_inf", type=Path)
    parser.add_argument("qcadcm_inf", type=Path)
    parser.add_argument("--json", type=Path)
    args = parser.parse_args()
    result = inventory(
        args.surface_audio_miniport_extension_inf,
        args.qcadcm_inf,
    )
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.json:
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
