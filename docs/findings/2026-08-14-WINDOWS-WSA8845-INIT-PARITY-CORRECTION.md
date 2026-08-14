# Windows WSA8845 initialization parity correction

Date: 2026-08-14

## Result

The retained August 10 Windows KDNET log contains a complete amplifier
initialization transaction which the earlier `CODEX_DP6BRIDGE` decoder did not
parse.  Its marker is `CODEX_QCAUCD_V2CMD`, and only controller FIFO address
`0x06b15020` contains packed direct slave writes.

Decoding that second marker exposes 133 direct writes: 66 attributed to each
logical amplifier and one controller tail.  Excluding the expected logical
device selector, Windows applies the same 63-register codec transaction to
both physical amplifiers.

This invalidates the earlier claim that the retained 328-write data-port
capture closed the complete amplifier profile.  That earlier capture closed
the SoundWire data-port transport, especially DP5, but did not contain this
codec initialization sequence.

## Concrete Linux error

Both amplifiers report `VPHX_SYS_EN_STATUS=2`.  Windows consequently writes
`ANA_WO_CTL_0=0x9d`.  Linux instead did:

```c
wo_ctl_0 |= WSA884X_ANA_WO_CTL_0_VPHX_SYS_EN_MASK;
```

which ORed the whole two-bit mask (`0xc0`) and produced live value `0xdd`—the
encoding for a different supply state.  Patch `0052` now encodes the observed
state with `FIELD_PREP`, producing `0x9d`.

## Other proven profile differences

The Windows trace and adjacent hash-bound qcaucd decompile agree on the exact
SP11 state-2 values.  Linux's earlier generic/downstream-derived values differ
at BOP/UVLO, power-stage, DSM, DRE-idle, class-H soft-max, and OTP trim
registers.  Patch `0052` applies the captured values only when the physical
codec reports supply state 2; other WSA884x systems retain their prior path.

The Windows PA-start function also proves an ordered transition:

1. class-H `0x34d0=0x67`, power-stage `0x3067=0x08`, high-side drive
   `0x304d=0x52`;
2. enable `PA_FSM_EN`;
3. restore `0x3067=0x0c` and `0x304d=0x5a`;
4. clear class-H to `0x00` after PA disable.

Linux previously toggled only the PA and CSR-gain enable bits.  Patch `0052`
adds the exact observed ordering.

## Deliberate exclusions

- Windows initializes `DRE_CTL_1`'s fallback CSR-gain field to zero, whereas
  Linux UCM retains PA Volume 24.  This field is inactive while the compander
  is enabled, so it is not changed here.
- Linux's regmap-IRQ layer owns interrupt masks `0x3581/0x3582`.  Its active
  `0x90/0x00` state already matches Windows; direct writes would desynchronise
  the IRQ abstraction.
- SoundWire controller records `0x0040/0x0041/0x0046` are not codec-profile
  settings.

## Evidence and implementation

- raw Windows log SHA-256:
  `ee8cb66eb3d7a44bf7fe4aadd61f04bb29ba85520df5e15e5ded95d2c1b3dc36`;
- reviewed comparison:
  [`2026-08-14-windows-wsa-init-vs-linux.json`](../../artifacts/reviewed/2026-08-14-windows-wsa-init-vs-linux.json);
- correction:
  [`0052-ASoC-wsa884x-match-SP11-Windows-codec-init.patch`](../../patches/0052-ASoC-wsa884x-match-SP11-Windows-codec-init.patch).

This is a proven hardware-profile correction, but it is not called an audible
success until the new isolated kernel boots, both codecs read back correctly,
protected playback remains fault-free, and the user performs the physical
comparison.

## Isolated candidate

The correction is built and staged as `7.1.5-sp11-render-parity-v3+` under
GRUB id `sp11-audio-wsa-init-parity-v3`.  It retains the byte-identical DTB
and topology from the live-tested VISENSE candidate.  All 7,886 installed
modules have exact v3 vermagic and signatures; the Phase91 touch overrides,
Wi-Fi, OLED/display and complete audio dependency set are present in the
initramfs.  Strict patch checking and all 142 repository tests pass.  The
preboot manifest and artifact hashes are recorded in
[`deploy/render-parity-v3/README.md`](../../deploy/render-parity-v3/README.md).
