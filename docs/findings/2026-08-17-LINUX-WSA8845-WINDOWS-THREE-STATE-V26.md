# Linux WSA8845 Windows three-state lifecycle v26 — live validation — 2026-08-17

## Result

**GREEN H03.** The SP11-specific Linux WSA8845 lifecycle now reproduces the shipping Windows three-state codec policy instead of replaying cold-only state on every DAPM cycle. Candidate v26 was built, signed with the existing render-parity-v4 kernel key, placed in an isolated initrd, one-shot booted with CPS-v3 retained as the saved fallback, and validated live on both physical WSA8845 amplifiers.

The loaded v26 module reports srcversion `86C5EDC920418F8B1306346` on `7.1.5-sp11-render-parity-v4+`. Both codecs logged `SP11 Windows WSA8845 three-state lifecycle enabled` at probe.

## Implemented state machine

The SP11/Denali profile is isolated from generic WSA884x behavior. It emits the recovered 63-write Windows cold-init sequence once from codec initialization, suppresses the Linux-only ordinary POST_PMU/PRE_PMD gain/DRE/current-limit/PDM-watchdog mutations, and uses `mute_stream()` for the exact Windows ordinary transitions.

START is exactly 10 writes per amp:

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

STOP is exactly 6 writes per amp:

1. `PA_FSM_EN 0x3430 = 0x00`
2. `INTR_MASK0 0x3581 = 0xff`
3. `INTR_MASK1 0x3582 = 0x03`
4. `INTR_CLEAR0 0x3585 = 0xff`
5. `INTR_CLEAR1 0x3586 = 0x03`
6. `CLSH_CTL_0 0x34d0 = 0x00`

## Binary and initrd gates

The compiled ARM64 module was inspected before boot. The three constant tables in `.rodata` matched the Windows oracle byte-for-byte: 63 cold-init records, 10 START records and 6 STOP records. The signed module uses the existing render-parity-v4 build key. The isolated v26 initrd was compared against the known v13 initrd and every regular file except `snd-soc-wsa884x.ko.zst` was byte-identical.

CPS-v3 remains the persistent GRUB fallback; v26 was armed only as a one-shot entry for the first boot.

## Live structural proof

A dedicated ftrace instance probed `_regmap_write` only for the WSA884x register range while all-zero 48-kHz stereo PCM traversed the production Dolby sink. The first complete natural lifecycle produced exactly 10 START writes and 6 STOP writes on each amplifier, with no other WSA-register mutation between them.

A three-cycle regression then started from SoundWire suspend and allowed each cycle to return naturally to idle. The trace contains exactly **96 WSA writes**:

`3 cycles × 2 amplifiers × (10 START + 6 STOP) = 96`.

For both regmaps, all three START sequences and all three STOP sequences match the Windows oracle exactly. No extra DSM, DRE, current-limit, PDM-watchdog, CKWD or analog-tail writes occurred in any ordinary cycle. No WSA/SoundWire/XRUN/fault/error kernel lines were emitted during the regression. Both SoundWire slaves ended `suspended` and ALSA PCM ended `closed`.

Machine-readable validation is in `artifacts/reviewed/2026-08-17-linux-wsa8845-three-state-v26-validation.json`. The implementation delta is preserved as `patches/0063-ASoC-wsa884x-SP11-Windows-three-state-lifecycle.patch`.

## Consequence

The v21 analog-tail replay remains useful historical evidence that state history mattered, but it is now superseded as an implementation direction. H03 is structurally closed: ordinary Linux codec lifecycle matches the complete shipping Windows START/STOP policy on repeated live cycles. The next physical gate is W03, using this corrected topology rather than any pre-v26 audible verdict.
