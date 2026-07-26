#!/usr/bin/env python3
"""Create a deterministic inventory of Windows audio ETL captures.

The Linux ``dissect.etl`` reader can expose event headers without the
Microsoft-Windows-Audio manifest.  This tool deliberately records only
header-level facts: provider/event identities, process IDs, payload sizes,
and event counts inside timestamped WASAPI probe windows.  It does not invent
event names or decode undocumented payloads.
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from datetime import datetime, timedelta, timezone
import hashlib
import json
from pathlib import Path
from typing import Any, Iterable
from uuid import UUID


AUDIO_PROVIDER = "ae4bd3be-f36f-45b6-8d21-bdd6fb832853"


def canonical_provider_id(header: Any) -> str:
    """Return a conventional GUID string for an ETL event header.

    ``dissect.etl`` currently constructs EventHeader provider UUIDs from the
    on-disk little-endian GUID bytes as if they were network byte order.
    SystemHeader provider IDs are already rendered conventionally.
    """
    provider_id = header.provider_id
    if header.__class__.__name__ == "EventHeader":
        return str(UUID(bytes_le=provider_id.bytes))
    return str(provider_id)


def descriptor_value(header: Any, name: str, default: int = 0) -> int:
    """Read an EventHeader descriptor field, or return a stable default."""
    descriptor = getattr(header, "header", None)
    value = getattr(descriptor, name, default)
    return int(value)


def parse_probe(path: Path) -> dict[str, Any]:
    """Parse one key=value WASAPI probe result."""
    fields: dict[str, str] = {}
    for raw_line in path.read_text(errors="replace").splitlines():
        if "=" not in raw_line:
            continue
        key, value = raw_line.split("=", 1)
        fields[key] = value

    timestamp = datetime.fromisoformat(fields["Timestamp"])
    seconds = int(fields["Seconds"])
    supported = fields.get("IsFormatSupportedHr") == "0x00000000"
    initialized = fields.get("InitializeHr") == "0x00000000"
    started = fields.get("Started") == "1"
    completed = (
        supported
        and initialized
        and started
        and fields.get("Stopped") == "1"
        and fields.get("LoopTimedOut") == "False"
    )
    return {
        "file": path.name,
        "timestamp": timestamp,
        "start_utc": timestamp.astimezone(timezone.utc).isoformat(),
        "end_utc": (timestamp + timedelta(seconds=seconds))
        .astimezone(timezone.utc)
        .isoformat(),
        "seconds": seconds,
        "mode": fields.get("Mode"),
        "rate": int(fields["Rate"]),
        "channels": int(fields["Channels"]),
        "format": fields.get("Format"),
        "raw": fields.get("Raw") == "True",
        "format_supported": supported,
        "initialized": initialized,
        "started": started,
        "completed": completed,
        "frames_written": int(fields.get("FramesWritten", "0")),
        "evidence": {
            key: fields[key]
            for key in (
                "IsFormatSupportedHr",
                "SetClientPropertiesRawHr",
                "InitializeHr",
                "UnsupportedFormatNoInitialize",
                "LoopTimedOut",
                "Stopped",
            )
            if key in fields
        },
    }


def _counter(counter: Counter[Any]) -> dict[str, int]:
    return {
        str(key): value
        for key, value in sorted(counter.items(), key=lambda item: item[0])
    }


def _event_identity(header: Any) -> tuple[int, int, int, int, int]:
    return (
        descriptor_value(header, "Id"),
        descriptor_value(header, "Version"),
        descriptor_value(header, "Task"),
        descriptor_value(header, "OpCode"),
        descriptor_value(header, "Level"),
    )


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def inventory_etl(
    path: Path, probes: Iterable[dict[str, Any]]
) -> dict[str, Any]:
    """Inventory one ETL file using only stable header-level facts."""
    try:
        from dissect.etl import ETL
    except ImportError as error:
        raise SystemExit(
            "dissect.etl is required; install the dissect.etl package"
        ) from error

    with path.open("rb") as metadata_stream:
        metadata_etl = ETL(metadata_stream)
        trace_start = metadata_etl.start
        trace_end = metadata_etl.end

    probe_list = []
    for probe in probes:
        start = probe["timestamp"].astimezone(trace_start.tzinfo)
        end = start + timedelta(seconds=probe["seconds"])
        if start <= trace_end and end >= trace_start:
            probe_list.append(probe)
    provider_counts: Counter[str] = Counter()
    provider_pid_counts: dict[str, Counter[int]] = defaultdict(Counter)
    audio_ids: Counter[int] = Counter()
    audio_identities: Counter[tuple[int, int, int, int, int]] = Counter()
    audio_payload_sizes: dict[int, Counter[int]] = defaultdict(Counter)
    audio_pids: Counter[int] = Counter()
    audio_activity_ids: set[str] = set()
    probe_audio_ids: list[Counter[int]] = [
        Counter() for _ in probe_list
    ]
    probe_audio_totals = [0] * len(probe_list)
    event_count = 0

    with path.open("rb") as stream:
        etl = ETL(stream)
        for record in etl:
            event_count += 1
            header = record.header
            provider = canonical_provider_id(header)
            pid = int(getattr(header, "process_id", -1))
            provider_counts[provider] += 1
            provider_pid_counts[provider][pid] += 1

            if provider != AUDIO_PROVIDER:
                continue

            event_id = descriptor_value(header, "Id")
            audio_ids[event_id] += 1
            audio_identities[_event_identity(header)] += 1
            audio_payload_sizes[event_id][int(header.data_size)] += 1
            audio_pids[pid] += 1
            activity_id = getattr(header, "activity_id", None)
            if activity_id and activity_id.int:
                audio_activity_ids.add(str(activity_id))

            timestamp = header.timestamp
            for index, probe in enumerate(probe_list):
                start = probe["timestamp"].astimezone(trace_start.tzinfo)
                end = start + timedelta(seconds=probe["seconds"])
                if start <= timestamp <= end:
                    probe_audio_totals[index] += 1
                    probe_audio_ids[index][event_id] += 1

    audio_events = []
    for identity, count in sorted(audio_identities.items()):
        event_id, version, task, opcode, level = identity
        audio_events.append(
            {
                "event_id": event_id,
                "version": version,
                "task": task,
                "opcode": opcode,
                "level": level,
                "count": count,
                "payload_sizes": _counter(audio_payload_sizes[event_id]),
            }
        )

    probe_windows = []
    for probe, total, ids in zip(
        probe_list, probe_audio_totals, probe_audio_ids
    ):
        item = {
            key: value
            for key, value in probe.items()
            if key != "timestamp"
        }
        item["inside_trace"] = (
            datetime.fromisoformat(item["start_utc"]) >= trace_start
            and datetime.fromisoformat(item["end_utc"]) <= trace_end
        )
        item["audio_event_count"] = total
        item["audio_event_ids"] = _counter(ids)
        probe_windows.append(item)

    digest = sha256_file(path)
    return {
        "file": path.name,
        "sha256": digest,
        "size": path.stat().st_size,
        "trace_start_utc": trace_start.isoformat(),
        "trace_end_utc": trace_end.isoformat(),
        "event_count": event_count,
        "provider_counts": _counter(provider_counts),
        "provider_top_pids": {
            provider: [
                {"pid": pid, "count": count}
                for pid, count in counts.most_common(12)
            ]
            for provider, counts in sorted(provider_pid_counts.items())
        },
        "audio_provider": {
            "guid": AUDIO_PROVIDER,
            "event_count": sum(audio_ids.values()),
            "unique_activity_ids": len(audio_activity_ids),
            "event_id_counts": _counter(audio_ids),
            "top_pids": [
                {"pid": pid, "count": count}
                for pid, count in audio_pids.most_common(20)
            ],
            "event_headers": audio_events,
        },
        "probe_windows": probe_windows,
    }


def discover_probes(etl_path: Path) -> list[dict[str, Any]]:
    probes = []
    for path in sorted(etl_path.parent.glob("scenario*.txt")):
        if path.name == "scenario-notes.txt":
            continue
        probes.append(parse_probe(path))
    return probes


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("etl", type=Path, nargs="+")
    parser.add_argument("-o", "--output", type=Path)
    args = parser.parse_args()

    traces = []
    for path in args.etl:
        traces.append(inventory_etl(path, discover_probes(path)))
    document = {
        "format": "sp11-windows-audio-etl-header-inventory",
        "format_version": 1,
        "method": {
            "reader": "dissect.etl",
            "semantic_limit": (
                "Microsoft-Windows-Audio manifest was not present; event "
                "names and payload meanings are intentionally not inferred"
            ),
            "time_window_rule": (
                "probe Timestamp through Timestamp + requested Seconds, "
                "inclusive"
            ),
        },
        "traces": traces,
    }
    rendered = json.dumps(document, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered)
    else:
        print(rendered, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
