# WSA8845 Windows analog-tail pre-SoundWire re-arm v21 — first stable repeat-cycle improvement

## Question

v13 proved that the recovered Windows positions 44--63 analog tail materially improves the first CSR-off speaker wake, but Linux loses that state on the next cycle. Can Linux deterministically recreate the useful state at the correct lifecycle boundary rather than trying to preserve individual latches across teardown?

## Candidate

v21 starts from exact v13 and adds one SP11-only behavior in speaker `hw_params()`:

`wsa884x_apply_sp11_windows_analog_tail()` is replayed **before SoundWire slave/port add/prepare** on every speaker route construction.

The existing v13 helper remains the single source of the audited twenty direct writes:

`300b=d1 3005=33 3006=60 3003=22 3004=44 30a0=f8 3090=6a 30aa=e3 3091=d4 3040=d2 304c=f6 3045=06 3046=14 3047=19 3048=1b 3049=1c 306a=02 30ba=13 30a5=f1 3091=44`.

v21 does not inherit v16--v19 transport experiments. Its initramfs contains exact v13 producer, qcom ASoC SoundWire helper, x1e machine driver and qcom SoundWire master bytes. The force-load list is byte-identical to v13.

### Provenance

- source v13-before SHA-256 `5b747b52a4733604d994bf7a37831d168691a2296d1f8c8b9a7eb26608491f1e`
- source v21-after SHA-256 `bdfa893918aa0056e17d4a91891a922b50cbd4bc04d23c35558a21bb49be1dfc`
- patch SHA-256 `9611e867044298666dab04b276666b84526bc1dad48c1c9d87fd9f216a1ba4a4`
- unsigned module SHA-256 `d97705ce0870e257c357cf0ed32ce4cbb8d13e6b34b02abf231ab78ae5536ab9`
- independent fresh audit rebuild: byte-for-byte identical to the saved unsigned module
- signed/compressed module SHA-256 `d00fd87a4e5f732922368e2f2da38d193627db6eee89913c2fc918263b41b853`
- srcversion `9215F2AB055FE5E03F04D9D`
- initramfs SHA-256 `e7cb390b520af6a9c1f8b121b33b4c4b8383cfc4925ce6b23ee0c6cb3174f35f`
- v13 force-list SHA-256 `63924610ab0def67421078432948696856fc15cedada16d95e9bd7b53cec2c7e`

## Structural gate

At 1% muted with the exact 10 s digital-zero source, both physical amplifiers logged the tail replay before SoundWire selection. PCM entered RUNNING and returned closed with no WSA/PA/SoundWire/XRUN fault.

## Acoustic repeatability

The important comparison is not the first cold wake; it is repeated wakes on the same boot after prior teardown.

### Cycle B

SP7 capture SHA-256 `976EE73281D72B29B76ACB5C718444F232B4BF8D0E314AB94BC8F6825CAF612C`.

Median steady diff-RMS: **5.666342067e-4**.

- 31.0x Windows;
- 0.838x v5;
- 0.144x v13 catastrophic cycle 2.

### Cycle C

SP7 capture SHA-256 `F66260A94447D78AC9C079ACE1CF4B153C63AC48E282FBE2E8C02703820862E4`.

Median steady diff-RMS: **4.804193274e-4**.

- 26.3x Windows;
- 0.710x v5;
- 0.122x v13 catastrophic cycle 2;
- 0.848x cycle B.

The catastrophic v13 cycle-to-cycle collapse is therefore absent in these two repeat cycles. v21 remains far from the Windows physical noise floor, but it is the first stable repeat-cycle CSR-off direction that is also quieter than v5.

## Read-only write-history proof

Reviewed trace `2026-08-17-v21-pre-sdw-rearm-regwrite-cycleD.trace`, SHA-256 `de11dc66b833c055474217b0543963ca05387481ee39394d672a292691239b4d`, records exactly 44 WSA register events per physical amp during another zero-stream cycle.

Per amp:

1. `hw_params`: exact 20-write Windows analog tail.
2. About 10 ms later Linux speaker POST_PMU still performs generic gain/DRE/current/watchdog updates: `GAIN_RAMPING_MIN=0x0e`, DRE0 updates to `0xf0`, DRE1 bit0 clear, `CURRENT_LIMIT=0x44`, and `PDM_WD=1`.
3. About 50 ms after the tail, PA start is `CLSH67 -> PWR08 -> PDRV52 -> PA=1 -> PWR0c -> PDRV5a`.
4. Stop is PA off, Class-H off, then PDM watchdog off.

The remaining ordering gap is therefore **before the analog tail**: Windows establishes its DSM/DRE/watchdog/Class-H prerequisite block before positions 44--63, while v21 currently replays positions 44--63 first and lets generic Linux POST_PMU establish several prerequisites later.

## Decision

**Keep v21 AMBER/promising; do not reject it.** It proves that the manufacturer analog state can be recreated deterministically enough to prevent the v13 repeat-cycle collapse when replayed before SoundWire programming.

Do not escalate to normal-volume music/chirp yet. The next work is to move only the proven prerequisite state that Linux currently establishes late to the pre-SoundWire boundary, preserving the Windows ordering while avoiding a blind full-63-write replay. In particular, do not overwrite the full `DRE_CTL_1` gain field without a separate safety argument; route-time stored-gain-zero experiments were noisy.
