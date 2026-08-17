# Windows WSA8845 complete lifecycle state machine — live KD + WPP + Ghidra oracle

Date: 2026-08-17

## Verdict

**GREEN Windows oracle.** A software-only KDNET trace attached before qcaucd audio bring-up, combined with qcaucd WPP and Ghidra decompilation of the exact live `qcaucd8380.sys`, captures the complete WSA8845 register-write lifecycle for both physical Surface Pro 11 amplifiers through cold initialization, boot/login activation, two real MP3 playback cycles, two stops, SoundWire idle and return to Linux. No direct WSA-macro physical-MMIO reads were used.

The decisive result is not another register snapshot. The shipping Windows driver has one explicit three-state codec policy function:

- `FUN_140032e10(..., state=0, ...)` — full codec/board initialization;
- `state=1` — ordinary speaker START;
- `state=2` — ordinary speaker STOP.

Ghidra identifies `FUN_1400328b8` as the register-level START/STOP state machine and `FUN_140031188` as the common codec-write builder. The higher speaker owners are `FUN_140036510` for START and `FUN_140036bc0` for STOP.

## Completeness proof

The traced shipping binary is `qcaucd8380.sys` version `1.0.0.10344`, SHA-256 `BD0C8276C51FC7A020C616E904DD613B6CCF187EC3E1FE6F94C2C811C8ADC8BF`. SP7's Ghidra input is byte-for-byte the same image.

Per physical WSA8845 amplifier, the complete session contains exactly:

- **63 cold-init writes, once**;
- **3 START transactions × 10 writes** (boot/login activation + MP3 A + MP3 B);
- **3 STOP transactions × 6 writes**;
- **0 unclassified WSA8845 register writes**.

Left/right sequences are identical. The machine-derived oracle is `artifacts/reviewed/2026-08-17-windows-wsa8845-complete-state-machine-oracle.json`, SHA-256 `8becf8c61c93f0be671c75940c88e8e08ed59788c0ef5a9a005f099e0a0ff0ee`.

## Ordinary Windows START — exact order per amp

1. `ISENSE2 0x3021 = 0x07`
2. `VSENSE1 0x3020 = 0x67`
3. `INTR_MASK0 0x3581 = 0x90`
4. `INTR_MASK1 0x3582 = 0x00`
5. `CLSH_CTL_0 0x34d0 = 0x67`
6. `PWRSTG_DBG 0x3067 = 0x08`
7. `PDRV_HS_CTL 0x304d = 0x52`
8. `PA_FSM_EN 0x3430 = 0x01`
9. `PWRSTG_DBG 0x3067 = 0x0c`
10. `PDRV_HS_CTL 0x304d = 0x5a`

The Linux SP11 gain choices already correspond to the observed sensing values (`ISENSE_6_DB` -> full `0x07`, `VSENSE_M21_DB` -> full `0x67`). The mismatch is lifecycle placement, not those gain selections.

## Ordinary Windows STOP — exact order per amp

1. `PA_FSM_EN 0x3430 = 0x00`
2. `INTR_MASK0 0x3581 = 0xff`
3. `INTR_MASK1 0x3582 = 0x03`
4. `INTR_CLEAR0 0x3585 = 0xff`
5. `INTR_CLEAR1 0x3586 = 0x03`
6. `CLSH_CTL_0 0x34d0 = 0x00`

## What Windows does *not* do on ordinary playback

The 63-write initialization transaction is cold-only. Windows does **not** replay DSM coefficients, DRE setup, PDM watchdog enable, current-limit history, CKWD history, gain-ramping minimum, Class-H board defaults or the analog-tail board configuration on every playback cycle.

This materially changes the interpretation of the Linux experiments. v21's per-cycle replay of Windows positions 44-63 was a useful diagnostic compensation because it prevented the catastrophic v13 second-cycle degradation, but it is **not Windows lifecycle parity** and should not become production policy.

## Linux structural mismatch now proven

The exact v13 Linux source still performs several cold-only hardware mutations in ordinary DAPM playback:

- speaker POST_PMU re-runs `wsa884x_set_gain_parameters()`;
- POST_PMU rewrites DRE programming/current-limit state;
- POST_PMU/PRE_PMD toggles the PDM watchdog;
- `mute_stream()` mutates the DRE CSR bit;
- Linux implements only the middle six PA/Class-H START writes and only two STOP writes;
- Linux omits the Windows START interrupt-mask writes and STOP mask/clear transaction at this boundary.

KD/Ghidra place Windows START/STOP after the supporting speaker resources are acquired. Linux's `mute_stream()` is the corresponding practical ASoC boundary after SoundWire prepare/enable. Therefore the next implementation should make the SP11 path use the exact Windows START/STOP state machine there and stop reapplying cold-only policy during ordinary DAPM cycles.

## Timing note

Stack-heavy KD breakpoints intentionally perturb wall-clock timing, so KD is authoritative for control flow, order, arguments and callers—not real-time latency. qcaucd WPP remains the timing oracle; separate low-overhead tracing already proves the Surface 500-ms delayed SoundWire clock-stop policy.

## Implementation direction

Treat this as an SP11/Denali hardware profile inside the upstream WSA884x framework rather than continuing value-by-value experiments:

1. establish the Windows board/codec initialization once;
2. ordinary speaker START executes only the exact 10-write transaction;
3. ordinary speaker STOP executes only the exact 6-write transaction;
4. generic non-SP11 behavior remains unchanged;
5. structural write tracing must prove no DRE/current-limit/PDM-watchdog/CKWD/DSM writes occur during normal SP11 START/STOP before acoustic validation.

This is the new H03 source of truth.
