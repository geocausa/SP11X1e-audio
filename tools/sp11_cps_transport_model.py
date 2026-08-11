#!/usr/bin/env python3
"""Deterministic model of the SP11 WSA8845 CPS SoundWire transport.

This is an evidence ledger, not a hardware access tool.  It derives the slave
banked register bytes from the already-reviewed Windows capture and the normal
Qualcomm master-port-13 board parameters used by the Linux implementation.
"""
from __future__ import annotations

from dataclasses import dataclass, asdict
import json

@dataclass(frozen=True)
class MasterPortConfig:
    port: int = 13
    sample_interval_minus_1: int = 0x031F
    offset1: int = 0x00
    hstart: int = 0x0F
    hstop: int = 0x0F
    word_length_minus_1: int = 0x18
    block_pack_mode: int = 0x00
    block_group_count_valid: bool = False
    offset2_programmed_for_simple_cps: bool = False

@dataclass(frozen=True)
class SlaveCpsConfig:
    name: str
    identity: str
    local_port: int
    master_port: int
    channel_enable: int
    offset1: int

LEFT = SlaveCpsConfig(
    name="left", identity="0x0000000402170220", local_port=6,
    master_port=13, channel_enable=0x03, offset1=0,
)
RIGHT = SlaveCpsConfig(
    name="right", identity="0x0000000402170221", local_port=6,
    master_port=13, channel_enable=0x03, offset1=25,
)
MASTER = MasterPortConfig()


def derive_slave_registers(master: MasterPortConfig, slave: SlaveCpsConfig) -> dict[str, int]:
    interval = master.sample_interval_minus_1 + 1
    return {
        "ChannelEnable": slave.channel_enable,
        "SampleCtrl1": master.sample_interval_minus_1 & 0xFF,
        "SampleCtrl2": (master.sample_interval_minus_1 >> 8) & 0xFF,
        "OffsetCtrl1": slave.offset1,
        "HCtrl": ((master.hstart & 0xF) << 4) | (master.hstop & 0xF),
        "BlockCtrl1": master.word_length_minus_1,
        "BlockCtrl3": master.block_pack_mode,
        "sample_interval_clocks": interval,
    }


def dedupe_master_ports(slaves: tuple[SlaveCpsConfig, ...]) -> list[dict[str, int]]:
    ports: dict[int, int] = {}
    for slave in slaves:
        ports[slave.master_port] = ports.get(slave.master_port, 0) | slave.channel_enable
    return [{"port": p, "channel_mask": ports[p]} for p in sorted(ports)]


def build_model() -> dict:
    slaves = (LEFT, RIGHT)
    return {
        "purpose": "SP11 Windows-parity CPS SoundWire transport derivation",
        "master_source": {
            "description": "Qualcomm WSA SoundWire controller port-13 board schedule",
            **asdict(MASTER),
        },
        "slaves": [
            {**asdict(s), "derived_registers": derive_slave_registers(MASTER, s)}
            for s in slaves
        ],
        "deduped_master_runtime_ports": dedupe_master_ports(slaves),
        "registers_intentionally_not_programmed": ["OffsetCtrl2", "BlockCtrl2"],
        "windows_expected": {
            "left": {
                "ChannelEnable": 0x03, "SampleCtrl1": 0x1F,
                "SampleCtrl2": 0x03, "OffsetCtrl1": 0x00,
                "HCtrl": 0xFF, "BlockCtrl1": 0x18, "BlockCtrl3": 0x00,
            },
            "right": {
                "ChannelEnable": 0x03, "SampleCtrl1": 0x1F,
                "SampleCtrl2": 0x03, "OffsetCtrl1": 0x19,
                "HCtrl": 0xFF, "BlockCtrl1": 0x18, "BlockCtrl3": 0x00,
            },
        },
    }


def main() -> None:
    print(json.dumps(build_model(), indent=2))

if __name__ == "__main__":
    main()
