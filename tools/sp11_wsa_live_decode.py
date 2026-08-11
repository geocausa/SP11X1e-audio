#!/usr/bin/env python3
"""Decode bounded SP11 WSA884x live-observer lines from a kernel journal."""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path


SAMPLE_RE = re.compile(
    r"\[\s*(?P<monotonic>[0-9.]+)\].*"
    r"sdw:[^\s:]+(?::[^\s:]+)*:00:(?P<amplifier>\d+): "
    r"SP11 WSA live sample=(?P<sequence>\d+) "
    r"failed=0x(?P<failed>[0-9a-fA-F]+) "
    r"pa=(?P<pa>[0-9a-fA-F]{2}) "
    r"sta=(?P<sta0>[0-9a-fA-F]{2})/(?P<sta1>[0-9a-fA-F]{2}) "
    r"err=(?P<err0>[0-9a-fA-F]{2})/(?P<err1>[0-9a-fA-F]{2}) "
    r"intr=(?P<intr0>[0-9a-fA-F]{2})/(?P<intr1>[0-9a-fA-F]{2}) "
    r"adc=(?P<adc>[0-9a-fA-F]{4}) "
    r"temp=(?P<temp>[0-9a-fA-F]{4}) "
    r"vbat=(?P<vbat>[0-9a-fA-F]{4}) "
    r"wavg=(?P<wavg>[0-9a-fA-F]{2}) "
    r"cps=(?P<cps>[0-9a-fA-F]{2}) "
    r"ilim=(?P<ilim>[0-9a-fA-F]{2})"
)

BYTE_FIELDS = ("pa", "sta0", "sta1", "err0", "err1", "intr0", "intr1",
               "wavg", "cps", "ilim")
WORD_FIELDS = {
    "adc": "adc_raw",
    "temp": "temperature_raw",
    "vbat": "vbat_raw",
}


def _hex(value: int, width: int) -> str:
    return f"0x{value:0{width}x}"


def samples_from_journal(text: str) -> list[dict]:
    samples = []
    for line in text.splitlines():
        match = SAMPLE_RE.search(line)
        if not match:
            continue
        groups = match.groupdict()
        item = {
            "monotonic_seconds": float(groups["monotonic"]),
            "amplifier": int(groups["amplifier"]),
            "sequence": int(groups["sequence"]),
            "failed_mask": _hex(int(groups["failed"], 16), 1),
        }
        item.update({name: _hex(int(groups[name], 16), 2)
                     for name in BYTE_FIELDS})
        item.update({output: _hex(int(groups[source], 16), 4)
                     for source, output in WORD_FIELDS.items()})
        samples.append(item)
    return samples


def _unique_hex(samples: list[dict], field: str) -> list[str]:
    return sorted({sample[field] for sample in samples}, key=lambda value: int(value, 16))


def _raw_summary(samples: list[dict], field: str) -> dict:
    values = [int(sample[field], 16) for sample in samples]
    return {
        "minimum": _hex(min(values), 4),
        "maximum": _hex(max(values), 4),
        "unique_count": len(set(values)),
        "values": [_hex(value, 4) for value in sorted(set(values))],
    }


def summarize(samples: list[dict]) -> dict:
    grouped = defaultdict(list)
    for sample in samples:
        grouped[sample["amplifier"]].append(sample)

    amplifiers = {}
    for amplifier, items in sorted(grouped.items()):
        items.sort(key=lambda item: item["sequence"])
        ilim_values = _unique_hex(items, "ilim")
        ilim_decode = []
        for value_hex in ilim_values:
            value = int(value_hex, 16)
            ilim_decode.append({
                "register": value_hex,
                "override_enabled": bool(value & 0x80),
                "current_limit_code": (value & 0x7c) >> 2,
            })
        amplifiers[str(amplifier)] = {
            "sample_count": len(items),
            "sequences": [item["sequence"] for item in items],
            "failed_masks": _unique_hex(items, "failed_mask"),
            "pa_values": _unique_hex(items, "pa"),
            "status_values": {
                "sta0": _unique_hex(items, "sta0"),
                "sta1": _unique_hex(items, "sta1"),
            },
            "error_values": {
                "err0": _unique_hex(items, "err0"),
                "err1": _unique_hex(items, "err1"),
            },
            "interrupt_values": {
                "intr0": _unique_hex(items, "intr0"),
                "intr1": _unique_hex(items, "intr1"),
            },
            "adc_raw": _raw_summary(items, "adc_raw"),
            "temperature_raw": _raw_summary(items, "temperature_raw"),
            "vbat_raw": _raw_summary(items, "vbat_raw"),
            "wavg_values": _unique_hex(items, "wavg"),
            "cps_control_values": _unique_hex(items, "cps"),
            "current_limit": ilim_decode,
        }

    return {
        "sample_count": len(samples),
        "amplifier_count": len(grouped),
        "amplifiers": amplifiers,
    }


def decode_journal(text: str) -> dict:
    samples = samples_from_journal(text)
    if not samples:
        raise ValueError("no SP11 WSA live-observer samples found")
    return {
        "schema": "sp11-wsa884x-live-observer-v1",
        "units_note": (
            "adc_raw, temperature_raw and vbat_raw are uncalibrated 16-bit "
            "register words; no physical-unit conversion is asserted."
        ),
        "summary": summarize(samples),
        "samples": samples,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("journal", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = decode_journal(args.journal.read_text(
        encoding="utf-8", errors="replace"))
    rendered = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
