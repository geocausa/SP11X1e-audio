#!/usr/bin/python3
"""Small, dependency-free client for the UbiG v2 realtime control page."""

from __future__ import annotations

from dataclasses import dataclass
import fcntl
import json
import mmap
import os
from pathlib import Path
import struct
import tempfile
from typing import Sequence

MAGIC = 0x55424947
ABI_VERSION = 2
CONTROL_BYTES = 172
EQ_BANDS = 20
EQ_RAW_MIN = -192
EQ_RAW_MAX = 192
CUSTOM_EQ_VALID = 1

PROFILE_NAMES = ("Dynamic", "Movie", "Music", "Game", "Voice", "Course", "Custom")
PROFILE_CUSTOM = 6
BAND_FREQUENCIES = (
    47, 141, 234, 328, 469, 656, 844, 1031, 1313, 1688,
    2250, 3000, 3750, 4688, 5813, 7125, 9000, 11250, 13875, 19688,
)

REQUEST_GENERATION_OFF = 16
ACK_GENERATION_OFF = 20
DESIRED_PROFILE_OFF = 24
ACTIVE_PROFILE_OFF = 28
DESIRED_FLAGS_OFF = 32
CUSTOM_EQ_OFF = 36
DESIRED_POSTGAIN_OFF = 116
ACTIVE_POSTGAIN_OFF = 120
POSTGAIN_REQUEST_GENERATION_OFF = 124
POSTGAIN_ACK_GENERATION_OFF = 128
LAST_ERROR_OFF = 132
ENGINE_FLAGS_OFF = 136


def default_control_path() -> Path:
    override = os.environ.get("UBIG_CONTROL_PATH")
    if override:
        return Path(override)
    runtime = os.environ.get("XDG_RUNTIME_DIR") or f"/run/user/{os.getuid()}"
    return Path(runtime) / "ubig-control-v2"


def default_state_path() -> Path:
    config = os.environ.get("XDG_CONFIG_HOME")
    root = Path(config) if config else Path.home() / ".config"
    return root / "ubig" / "control.json"


def raw_to_db(value: int) -> float:
    if value < EQ_RAW_MIN or value > EQ_RAW_MAX:
        raise ValueError("GEQ value outside -12..+12 dB")
    return value / 16.0


def db_to_raw(value: float) -> int:
    if value < -12.0 or value > 12.0:
        raise ValueError("GEQ value outside -12..+12 dB")
    return max(EQ_RAW_MIN, min(EQ_RAW_MAX, round(value * 16.0)))


def profile_index(value: int | str) -> int:
    if isinstance(value, int):
        if 0 <= value < len(PROFILE_NAMES):
            return value
        raise ValueError("invalid UbiG profile")
    folded = value.casefold()
    for index, name in enumerate(PROFILE_NAMES):
        if name.casefold() == folded:
            return index
    raise ValueError("invalid UbiG profile")


@dataclass(frozen=True)
class ControlSnapshot:
    request_generation: int
    ack_generation: int
    desired_profile: int
    active_profile: int
    desired_flags: int
    custom_eq: tuple[int, ...]
    desired_postgain: int
    active_postgain: int
    postgain_request_generation: int
    postgain_ack_generation: int
    last_error: int
    engine_flags: int

    @property
    def request_pending(self) -> bool:
        return self.request_generation != self.ack_generation


class ControlPage:
    def __init__(self, path: Path | str | None = None, create: bool = True):
        self.path = Path(path) if path is not None else default_control_path()
        self.path.parent.mkdir(parents=True, exist_ok=True)
        flags = os.O_RDWR | os.O_CLOEXEC | (os.O_CREAT if create else 0)
        self.fd = os.open(self.path, flags, 0o600)
        os.fchmod(self.fd, 0o600)
        self._lock()
        try:
            size = os.fstat(self.fd).st_size
            if create and size != CONTROL_BYTES:
                os.ftruncate(self.fd, CONTROL_BYTES)
            elif not create and size < CONTROL_BYTES:
                raise RuntimeError("short UbiG control page")
            self.mapping = mmap.mmap(self.fd, CONTROL_BYTES, access=mmap.ACCESS_WRITE)
            if create and not self._valid():
                self._initialize()
            elif not self._valid():
                raise RuntimeError("invalid UbiG control page")
        except Exception:
            os.close(self.fd)
            self.fd = -1
            raise
        finally:
            if self.fd >= 0:
                self._unlock()

    def _lock(self) -> None:
        fcntl.flock(self.fd, fcntl.LOCK_EX)

    def _unlock(self) -> None:
        fcntl.flock(self.fd, fcntl.LOCK_UN)

    def _valid(self) -> bool:
        return struct.unpack_from("<III", self.mapping, 0) == (
            MAGIC, ABI_VERSION, CONTROL_BYTES,
        )

    def _initialize(self) -> None:
        self.mapping[:] = bytes(CONTROL_BYTES)
        struct.pack_into("<II", self.mapping, 4, ABI_VERSION, CONTROL_BYTES)
        struct.pack_into("<II", self.mapping, DESIRED_PROFILE_OFF, 0, 0)
        struct.pack_into("<I", self.mapping, 0, MAGIC)
        self.mapping.flush()

    def close(self) -> None:
        mapping = getattr(self, "mapping", None)
        if mapping is not None:
            mapping.close()
            self.mapping = None
        if self.fd >= 0:
            os.close(self.fd)
            self.fd = -1

    def __enter__(self) -> "ControlPage":
        return self

    def __exit__(self, *_exc: object) -> None:
        self.close()

    def snapshot(self) -> ControlSnapshot:
        for _ in range(8):
            request_a = struct.unpack_from("<I", self.mapping, REQUEST_GENERATION_OFF)[0]
            postgain_a = struct.unpack_from("<I", self.mapping, POSTGAIN_REQUEST_GENERATION_OFF)[0]
            data = self.mapping[:CONTROL_BYTES]
            request_b = struct.unpack_from("<I", self.mapping, REQUEST_GENERATION_OFF)[0]
            postgain_b = struct.unpack_from("<I", self.mapping, POSTGAIN_REQUEST_GENERATION_OFF)[0]
            if request_a == request_b and postgain_a == postgain_b:
                return ControlSnapshot(
                    request_a,
                    struct.unpack_from("<I", data, ACK_GENERATION_OFF)[0],
                    struct.unpack_from("<I", data, DESIRED_PROFILE_OFF)[0],
                    struct.unpack_from("<I", data, ACTIVE_PROFILE_OFF)[0],
                    struct.unpack_from("<I", data, DESIRED_FLAGS_OFF)[0],
                    struct.unpack_from("<20i", data, CUSTOM_EQ_OFF),
                    struct.unpack_from("<i", data, DESIRED_POSTGAIN_OFF)[0],
                    struct.unpack_from("<i", data, ACTIVE_POSTGAIN_OFF)[0],
                    postgain_a,
                    struct.unpack_from("<I", data, POSTGAIN_ACK_GENERATION_OFF)[0],
                    struct.unpack_from("<i", data, LAST_ERROR_OFF)[0],
                    struct.unpack_from("<I", data, ENGINE_FLAGS_OFF)[0],
                )
        raise RuntimeError("UbiG control page changed during every snapshot")

    def request_profile(self, profile: int | str) -> int:
        selected = profile_index(profile)
        self._lock()
        try:
            generation = struct.unpack_from("<I", self.mapping, REQUEST_GENERATION_OFF)[0]
            struct.pack_into("<I", self.mapping, DESIRED_PROFILE_OFF, selected)
            generation = (generation + 1) & 0xFFFFFFFF
            struct.pack_into("<I", self.mapping, REQUEST_GENERATION_OFF, generation)
            self.mapping.flush()
            return generation
        finally:
            self._unlock()

    def request_custom_eq(self, values: Sequence[int]) -> int:
        if len(values) != EQ_BANDS:
            raise ValueError("Custom EQ requires exactly 20 bands")
        raw = tuple(int(value) for value in values)
        if any(value < EQ_RAW_MIN or value > EQ_RAW_MAX for value in raw):
            raise ValueError("Custom EQ value outside -192..+192")
        self._lock()
        try:
            generation = struct.unpack_from("<I", self.mapping, REQUEST_GENERATION_OFF)[0]
            struct.pack_into("<20i", self.mapping, CUSTOM_EQ_OFF, *raw)
            flags = struct.unpack_from("<I", self.mapping, DESIRED_FLAGS_OFF)[0]
            struct.pack_into("<I", self.mapping, DESIRED_FLAGS_OFF, flags | CUSTOM_EQ_VALID)
            struct.pack_into("<I", self.mapping, DESIRED_PROFILE_OFF, PROFILE_CUSTOM)
            generation = (generation + 1) & 0xFFFFFFFF
            struct.pack_into("<I", self.mapping, REQUEST_GENERATION_OFF, generation)
            self.mapping.flush()
            return generation
        finally:
            self._unlock()


def load_saved_state(path: Path | str | None = None) -> tuple[int, tuple[int, ...]]:
    target = Path(path) if path is not None else default_state_path()
    if not target.exists():
        return 0, (0,) * EQ_BANDS
    data = json.loads(target.read_text(encoding="utf-8"))
    profile = profile_index(data.get("profile", "Dynamic"))
    eq = tuple(int(value) for value in data.get("custom_eq", (0,) * EQ_BANDS))
    if len(eq) != EQ_BANDS or any(value < EQ_RAW_MIN or value > EQ_RAW_MAX for value in eq):
        raise ValueError("invalid saved Custom EQ")
    return profile, eq


def save_state(profile: int | str, custom_eq: Sequence[int], path: Path | str | None = None) -> Path:
    selected = profile_index(profile)
    eq = tuple(int(value) for value in custom_eq)
    if len(eq) != EQ_BANDS or any(value < EQ_RAW_MIN or value > EQ_RAW_MAX for value in eq):
        raise ValueError("invalid Custom EQ")
    target = Path(path) if path is not None else default_state_path()
    target.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    payload = {"version": 1, "profile": PROFILE_NAMES[selected], "custom_eq": list(eq)}
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=target.parent, delete=False) as handle:
        json.dump(payload, handle, indent=2)
        handle.write("\n")
        temporary = Path(handle.name)
    os.chmod(temporary, 0o600)
    os.replace(temporary, target)
    return target
