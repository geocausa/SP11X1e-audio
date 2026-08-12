# Windows POPLESS/VOL headroom control link restored — 2026-08-13

## Finding

The reviewed Windows DEFAULT AudioReach control-link evidence contains **four** links. The deployed CPS-V3 Linux topology contained only three because `tools/build_sp11_protected_topology.py` selected only `record_blocks[0]` and carried a stale comment claiming that block already contained the EQ/VOL link.

The omitted Windows link is:

- peer 1: `POPLESS_EQ` iid `0x4664`, control port `0x80000000`
- peer 2: `VOL_CTRL` iid `0x4663`, control port `0x80000000`
- intent: `INTENT_ID_P_EQ_VOL_HEADROOM` = `0x08001118`
- reviewed record payload size: 52 bytes

The reviewed aggregate payload is 196 bytes and encodes all four links.

## Source correction

`windows_default_control_payload()` now validates:

1. reviewed `link_count == 4`;
2. aggregate byte size matches provenance;
3. aggregate leading count is four;
4. the exact `0x4664 <-> 0x4663` identity is present;
5. intent `0x08001118` is present.

The builder now serializes the reviewed four-link aggregate rather than selecting one positional record block. Regression tests explicitly reject a three-link payload and a payload missing the headroom intent.

## Validation

- targeted topology/control tests: `18 passed`
- full repository suite: `114 passed, 3 skipped`
- original deployed topology SHA-256: `f385a5d83127cf8f83dab0cbc86f418514f9c8839f2da6aac97e3e2ee782d121`
- four-link candidate topology SHA-256: `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`
- isolated DT model: `X1E80100-Microsoft-Surface-Pro-11-CPS-Headroom-Test`

The candidate was installed under a separate topology filename and separate GRUB entry. The known-good `sp11-audio-cps-v3` entry remained the saved default and its topology/DT were not overwritten.

## Boot result

The isolated candidate booted successfully. ALSA `MultiMedia1 Playback`, PipeWire, WirePlumber, Dolby, Windows endpoint taper and MSIIR volume-sync services all came up. Kernel evidence showed the SP/SPVI/VI/CPS calibration sequence accepted and `GRAPH_START accepted`. No AudioReach graph-open failure was observed.

## Remaining gate

This proves the four-link graph is structurally valid and DSP-accepted. It does **not yet prove** that the physical YouTube seek spike is eliminated. The next gate is physical/WSA-side A/B around real YouTube seeks; if smoothing is still incomplete, downstream SAL_V2 dynamic limiting and SOFT_PAUSE lifecycle remain the next evidence-backed candidates.
