# Full Windows WSA8845 codec initialization history — 2026-08-16

## Result

The original pre-playback qcaucd command-FIFO capture contains a complete ordered WSA8845 codec-programming transaction for each physical amplifier. Decoding the hash-pinned raw log produces **63 direct codec-register writes per amp**, and the left/right sequences are byte-for-byte identical.

Raw capture:

`/home/geoca/Documents/SP11-PROJECT/01-audio/11.08.2026/CPS_SWR_RUNTIME_20260810_1909Z_2d7c_2026-08-10_19-09-12-880.log`

SHA-256:

`EE8CB66EB3D7A44BF7FE4AADD61F04BB29BA85520DF5E15E5DED95D2C1B3DC36`

Decoder:

`tools/decode_qcaucd_command_fifo.py --records`

Reviewed machine-readable sequence:

`artifacts/reviewed/2026-08-16-windows-wsa8845-full-init-sequence.json`

This changes the interpretation of the earlier patch-0052 closure. Patch 0052 correctly reproduced a set of important SP11 board values and the Windows PA start/stop transaction, but it did **not** reproduce the complete Windows initialization history. H08 remains green for the proven board-value subset and SoundWire transport; the remaining initialization-history gap belongs to H03.

## 2026-08-17 register-symbol audit correction

A mechanical address audit against the exact v5 `wsa884x.c` register definitions found four incorrect symbolic labels in the first reviewed JSON. The **raw qcaucd addresses, values and ordering were always correct**; only these names were wrong:

- `0x34d2` is `CLSH_V_HD_PA`, not `CLSH_CTL_1`;
- `0x300b` is `REF_CTRL`, not `BOP2_PROG`;
- `0x3040` is `TOP_CTRL1`, not `REF_CTRL`;
- `0x306a` is `DAC_VCM_CTRL_REG7`, not `PA_FSM_TIMER0` (`PA_FSM_TIMER0` is `0x3433`).

The reviewed JSON is corrected in place and now records the source hash/method. `0x3581/0x3582` remain classified as interrupt-mask registers from the independent regmap-IRQ analysis even though those exact addresses are not named in the local codec register define block. No behavioral conclusion may use an un-audited symbolic label when the raw address is available.

## Ordered Windows transaction

The 63 writes per amp are:

```text
01 3067=0c   02 304d=5a   03 3414=07   04 3581=ff
05 3582=03   06 343c=00   07 3014=20   08 30ba=1b
09 3473=0a   10 3474=08   11 3475=f3   12 3476=07
13 3477=79   14 3478=02   15 3479=0b   16 347a=02
17 347b=8a   18 347c=9b   19 347d=68   20 347f=f2
21 3480=20   22 3481=83   23 3482=7f   24 3483=9d
25 3484=82   26 3485=8b   27 3486=9b   28 3487=3f
29 34b2=0f   30 348b=01   31 34b0=f0   32 34b1=00
33 3461=11   34 3459=05   35 3458=79   36 34b9=0e
37 34d0=00   38 34d2=13   39 34d6=ff   40 3504=9d
41 3505=00   42 38a6=08   43 38a8=20   44 300b=d1
45 3005=33   46 3006=60   47 3003=22   48 3004=44
49 30a0=f8   50 3090=6a   51 30aa=e3   52 3091=d4
53 3040=d2   54 304c=f6   55 3045=06   56 3046=14
57 3047=19   58 3048=1b   59 3049=1c   60 306a=02
61 30ba=13   62 30a5=f1   63 3091=44
```

The important point is not only the final register values. Windows deliberately performs transitions such as:

```text
CKWD_CTL_1       0x1b -> 0x13
CURRENT_LIMIT    0xd4 -> 0x44
```

inside the codec configuration transaction.

## Static qcaucd confirmation

The corresponding Windows qcaucd decompile is `FUN_140032e10` in the retained SP7 `qcaucd-wsa-vbat-adjacent-path.txt` analysis.

It independently proves that this is a board/supply configuration function rather than an accidental FIFO ordering:

- configuration branch `local_74 == 2` selects the 2S supply values, including `DRE_CTL_0=0xf0`, `ANA_WO_CTL_0=0x9d`, and DAC VCM `06/14/19/1b/1c`;
- load values 4 or 6 select `OCP_CTL=0xf6`;
- `PWRSTAGE_CTRL2=0xf1` is written before the final current-limit specialization;
- codec-config object field `+0x28 == 1` selects final `CURRENT_LIMIT=0x44`.

The symbolic name of the `+0x28` field is still unresolved. It is intentionally **not** guessed here.

## Linux v5 reaches several Windows values at a different lifecycle boundary

The exact v5 source-to-binary mapping was re-audited after the v6 packaging bug. A forced fresh rebuild from the exact v5 source is byte-for-byte identical to the packaged v5 `.ko`; the full executable `.text` is identical. Thus the following source comparison is valid for the tested v5 binary.

Key history differences are:

### DRE_CTL_0 `0x34b0`

Windows explicitly writes `0xf0` during coherent codec initialization.

Linux has regmap default `0x70` and generic init explicitly writes `0x70`. The SP11 supply correction does not write `0xf0`. Only speaker POST_PMU later changes `PROG_DELAY` to `0xf`, yielding active `0xf0` immediately before PA use.

### DRE_CTL_1 `0x34b1`

Windows explicitly writes `0x00` during codec initialization and then performs zero ordinary runtime writes across PA cycles.

Linux starts from regmap default `0x04`; generic init does not write the register. Initial `wsa884x_set_gain_parameters()` runs before the COMP port is enabled and sets `CSR_GAIN_EN=1`, producing `0x05` at that point. UCM later programs the stored PA/CSR gain bits. COMP-aware speaker POST_PMU finally clears bit 0.

### PDM_WD_CTL `0x348b`

Windows explicitly writes `0x01` during codec initialization.

Linux starts at `0x00` and does not enable it in generic or SP11 supply init. DAPM speaker POST_PMU enables it and PRE_PMD disables it.

### CURRENT_LIMIT `0x3091`

Windows explicitly performs `0xd4 -> 0x44` inside the board/supply configuration function.

Linux generic init computes `0x24` from current-limit code 9. The SP11 supply correction does not reproduce the Windows two-write transition. Only PBR-enabled speaker POST_PMU later writes code `0x11` with override clear, yielding the familiar active `0x44`.

Thus the old live observation “Linux current-limit is 0x44 like Windows” is a final-state fact, not proof of equivalent initialization history.

### CKWD_CTL_1 `0x30ba`

Windows performs `0x1b -> 0x13` during init. Linux generic init writes `0x13` directly and never reproduces the `0x1b` precursor in the SP11 correction.

### DRE_IDLE_DET_CTL `0x34b2`

Windows writes `0x0f` immediately before the `DRE_CTL_0/DRE_CTL_1` pair. Linux default is `0x2f`; generic init omits the register; the SP11 supply correction writes `0x0f` later in a different sequence.

### Other explicit-vs-default differences

Windows explicitly writes DSM `A4_1=0x02`, `A5_1=0x02`, and `PA_FSM_BYP_CTL=0x00`; Linux largely relies on matching defaults for those fields. Windows also writes `GAIN_RAMPING_MIN=0x0e` inside the same DRE/DSM/Class-H initialization block, whereas Linux begins at default `0x12` and reaches the 0-dB minimum later through gain setup.

The Windows `0x3581=ff / 0x3582=03` writes are interrupt-mask initialization and are not promoted as the current acoustic root cause.

## Why this now matters physically

The four-way active-noise oracle in `2026-08-16-CSR-OFF-ACTIVE-NOISE-ORACLE.md` proves:

- Windows CSR-off is quiet under a continuously non-zero active stream;
- Linux CPS-v3 CSR-on is equally quiet;
- Linux v5/v8 CSR-off are about 37--43x Windows in steady diff-RMS.

Therefore final DRE register equality and exact ordinary PA start/stop ordering are not sufficient. The leading remaining hypothesis class is **initialization/latch history**: Windows establishes the complete 2S/4-ohm DSM/DRE/watchdog/current-limit state coherently before normal playback, while Linux defers several of those transitions until route/DAPM activation.

Do not respond by copying the full Windows transaction blindly. The next step is to isolate which transition(s) are stateful and required for quiet CSR-off operation, preferably first through read-only Linux write-history capture and static Qualcomm code analysis. Any behavioral candidate remains one-variable and low-level-gated.
