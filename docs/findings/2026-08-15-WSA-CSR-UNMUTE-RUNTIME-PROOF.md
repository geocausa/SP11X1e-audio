# WSA8845 CSR-assist unmute runtime proof

Date: 2026-08-15
Status: CLOSED for native Linux CSR lifecycle; Windows COMP-only acoustic root cause remains OPEN

## Objective

Prove on the untouched accepted CPS-v3 path whether the generic WSA8845 driver actually re-enables `CSR_GAIN_EN` immediately before the speaker PA is enabled. Earlier source analysis predicted this, but the decisive positive runtime write sequence had not been preserved as a dedicated artifact.

## Safe capture method

A boot-bounded kprobe observed only `_regmap_write()` calls whose register argument was inside the WSA8845 register aperture (`0x3000..0x35ff`). It recorded stack traces and performed no codec/MMIO reads, ALSA writes, PipeWire/WirePlumber operations, playback, or module replacement.

The tracer started at monotonic `11.76 s`, before the relevant ALSA/WirePlumber/PA lifecycle, ran for 40 seconds, then disabled and removed its probe. The machine remained on the exact persistent CPS-v3 fallback.

Reviewed artifacts:

- `artifacts/reviewed/2026-08-15-cps-v3-wsa884x-write-boot.trace`
  - SHA-256 `416cb69e825d5ffb723ed7d813f27b2a0ff547883d9909a0947f68ae5ea9669a`
- `artifacts/reviewed/2026-08-15-cps-v3-wsa884x-write-boot.meta`
  - SHA-256 `2b220e246907be864ee7bd9ae5ad25ad578d2ee173865510e7628a6386677f45`
- boot ID `ad67ff49-8396-45fe-a066-67e50f87d7ea`
- kernel `7.1.5-sp11-cps-v3+`
- WSA8845 srcversion `E084BC31719EE85BB8DEABD`
- WSA-macro srcversion `F681186BB3D24B32621905D`

## Exact `DRE_CTL_1` lifecycle

Both amplifier regmaps show the same three-stage behavior.

### 1. ALSA persisted-state restore

At about `11.7936 s`, `alsactl` writes:

```text
DRE_CTL_1 0x34b1 = 0x0f
```

for both amplifiers.

### 2. COMP-aware speaker POST_PMU

At about `12.4713 s`, WirePlumber/DAPM reaches the WSA8845 speaker setup. The driver writes:

```text
DRE_CTL_1 0x34b1 = 0x0e
DRE_CTL_0 0x34b0 = 0xf0
```

for both amplifiers. `0x0e` is the COMP-aware state with `CSR_GAIN_EN` clear while retaining the stored CSR gain-field bits.

### 3. Real stream unmute immediately before PA

At `12.975943 s` and `12.977813 s`, the real PipeWire data-loop unmute path writes:

```text
DRE_CTL_1 0x34b1 = 0x0f
PA_FSM_EN 0x3430 = 0x01
```

on left and right respectively.

Thus native Linux **positively re-enables CSR assistance immediately before `GLOBAL_PA_EN`**. This is not merely an inference from source or a stale steady-state read.

The full surrounding start sequence also retains the already-proven class-H / power-stage restoration:

```text
CLSH_CTL_0      0x34d0 = 0x67
PWR_STAGE_DBG   0x3067 = 0x08
PDRV_HS_CTL     0x304d = 0x52
PA_FSM_EN       0x3430 = 0x01
PWR_STAGE_DBG   0x3067 = 0x0c
PDRV_HS_CTL     0x304d = 0x5a
```

## Relation to Windows and rejected patch 0055

Fresh Windows KDNET evidence already established that Windows initializes both amplifiers with `DRE_CTL_1=0x00` and performs no runtime `0x34b1` rewrite across ordinary PA cycles.

Patch `0055-ASoC-wsa884x-match-Windows-DRE-CTL1-lifecycle.patch` removes exactly this generic Linux unmute behavior for the SP11 2S profile: it leaves `CSR_GAIN_EN=0` instead of writing it back to one before PA enable. The cold validation of the fully Windows-like `DRE_CTL_1=0x00` candidate produced ugly/unsafe speaker noise and remains permanently rejected.

This runtime capture therefore sharpens the failure boundary:

```text
Linux safe path:
COMP producer ready -> DRE_CTL_1 0x0e -> unmute writes 0x0f -> PA enable

Windows:
COMP producer/runtime policy -> DRE_CTL_1 stays 0x00 -> PA enable
```

The extra Linux `0x0f` write is demonstrably a safety/compatibility crutch in the current native stack. Removing it exposes a lower producer/consumer mismatch that is still unresolved.

## Diagnostic tooling correction

An experimental initramfs-armed kprobe/save design was also tried later. The userspace save service found that its initramfs probe no longer existed after switch-root, so that method did not capture WSA8845 writes and is rejected as a diagnostic design. The working tracer is self-contained in one systemd service and now writes boot-ID-specific files so one capture cannot overwrite another.

## Conclusion

The native Linux CSR lifecycle is now evidence-closed:

- COMP-aware setup clears `CSR_GAIN_EN`;
- generic WSA8845 unmute re-enables it;
- that write occurs immediately before PA enable;
- Windows does not perform the corresponding runtime rewrite.

Do not promote `DRE_CTL_1=0x00` again until the active WSA-macro COMP producer semantics are proven sufficiently Windows-equivalent. The next investigation remains producer-side, not SoundWire routing, PA ordering, or Dolby/AudioReach volume logic.
