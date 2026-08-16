# RPV4 RX84 + WSA half-dB policy isolation

Date: 2026-08-16  
Status: REJECTED as a Windows-parity improvement; safe test completed

## Question

After synchronized acoustic testing made WSA RX value 84 (`0 dB`) the best current producer-side match to Windows, the next independent generic Linux policy was `WSA_MACRO_GAIN_OFFSET_M1P5_DB` in `lpass-wsa-macro.c`. With compander enabled that policy controls the half-dB PGA mode used by the RX paths.

The question was whether combining the proven render-parity-v4 stack and RX84 with `WSA_MACRO_GAIN_OFFSET_0_DB` would improve the Windows acoustic residual.

## Candidate isolation

The candidate changed exactly one source assignment:

```diff
- wsa->spkr_gain_offset = WSA_MACRO_GAIN_OFFSET_M1P5_DB;
+ wsa->spkr_gain_offset = WSA_MACRO_GAIN_OFFSET_0_DB;
```

DRE/CSR policy, WSA8845 initialization, protection, Dolby, AudioReach, endpoint volume, RX value and all other render-parity state were unchanged.

The rebuilt `snd-soc-lpass-wsa-macro` module:

- has vermagic `7.1.5-sp11-render-parity-v4+`;
- is signed by the same build-time kernel key as the live RPV4 modules;
- has srcversion `842E1604E1CC981A2E50659`;
- differs in function-size inventory only at `wsa_macro_component_probe` (`0x10c -> 0x110`), where disassembly shows the intended `spkr_gain_offset = 1` store replacing the baseline zero store.

The source tree was restored to the baseline M1P5 assignment after the module was built.

## Initramfs provenance trap found and fixed

The first combined initramfs contained the correct RX84-capable `snd-soc-x1e80100.ko.zst` bytes, but runtime ALSA still exposed maximum 81. Comparison against the known-good RPV4 macro0db initramfs found the precise cause:

```text
known-good conf/modules: snd_soc_x1e80100 present
fresh combined conf/modules: snd_soc_x1e80100 missing
```

Without that line the machine driver was not forced to load before switch-root, so the root filesystem's baseline 81-capped module won despite the candidate binary being present in the initramfs.

The regenerated candidate explicitly restores `snd_soc_x1e80100` to `conf/modules`. Preboot validation then proved:

- exact RX84 x1e binary inside initrd;
- exact signed HalfDB0 WSA macro binary inside initrd;
- x1e disassembly contains four `#84` limit immediates;
- live ALSA exposes `max=84`;
- live lpass-wsa-macro srcversion is the HalfDB0 candidate value.

This is a reusable deployment lesson: **module presence in an initramfs is not proof that it wins the boot race; the force-load list is part of provenance.**

## Runtime safety result

The corrected candidate booted normally with the protected render path healthy. Three synchronized RX84/endpoint-12% chirp runs completed with:

- zero new WSA/PA faults;
- zero DSP/GPR runtime failures beyond the already-known harmless boot-time graph-calibration returns;
- zero SoundWire/XRUN/underrun/overrun faults;
- RX controls restored to 81 after each measurement.

No DRE/CSR experiment was performed.

## Acoustic result

The same external SP7 recorder geometry and exact 40 Hz -> 16 kHz chirp used for the Windows and RX84 baseline comparison were retained.

One HalfDB0 run contained a large one-off 1.25 kHz outlier. Two subsequent repeats did not reproduce it. Therefore the decision uses the **per-frequency median of three HalfDB0 captures** and also reports a stable-bin subset where HalfDB0 run-to-run spread is <=2 dB.

### 630 Hz--6.3 kHz

Stable-bin normalized response-shape error versus synchronized Windows:

```text
RX84 baseline M1P5: MAE ~0.425 dB, RMSE ~0.507 dB
RX84 HalfDB0 median: MAE ~0.437 dB, RMSE ~0.600 dB
```

### 1--5 kHz

Stable-bin normalized response-shape error:

```text
RX84 baseline M1P5: MAE ~0.419 dB, RMSE ~0.489 dB
RX84 HalfDB0 median: MAE ~0.431 dB, RMSE ~0.604 dB
```

The difference is small, but HalfDB0 does **not** improve either MAE or RMSE. The existing M1P5 policy therefore remains the preferred setting.

## Decision

**Reject HalfDB0 as a parity improvement.**

Keep:

```text
WSA RX = 84 / 0 dB for the current acoustic oracle
spkr_gain_offset = WSA_MACRO_GAIN_OFFSET_M1P5_DB
```

Do not merge or promote the HalfDB0 module into the normal render-parity stack.

The HalfDB0 candidate remains useful as a preserved negative experiment only.

## Evidence

SP7 captures:

```text
HalfDB0 A SHA-256 C9ECFB859472A9E94DD80D3C7B6553A6361E2D264EED4DC5FCEC70A737A5963D
HalfDB0 B SHA-256 5A7561531311A0AB5C4F93682E1A0CEBF7A86D43B87C502FB24AE82E158FC05B
HalfDB0 C SHA-256 C9382FA5B920B5AEDA600A87EE933DA4C2B6164CEBBDF704AF114C1E25BB452F
```

Three-run ridge-aligned analysis:

```text
C:\Users\SurfacePro7\Documents\KDNET\Codex\ridge-halfdb0-three-run-median-20260816.json
SHA-256 3182BDC4AF024F7874D27E06B5C0DB9A765E2FDE6B0417CAB7AB4F42CA4624E1
```

Candidate build directory:

```text
/home/geoca/Documents/SP11-PROJECT/02-kernel/candidates/rpv4-macro84-halfdb0-oracle-20260816
```
