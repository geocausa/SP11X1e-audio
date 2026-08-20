# 2026-08-20 — VI producer clocks + active Offset2: first stimulus-dependent Linux feedback

## Scope
Disposable Golden-v31 derivative only. Persistent saved GRUB entry remains `sp11-audio-golden-v31`.

## Proven Windows operation
Native qcaucd speaker resource types 7/8 execute the WSA speaker-protection TX lifecycle. For TX0/TX1 and TX2/TX3 Windows sequences reset, 8-kHz rate, CLK_EN, then reset release; stop asserts reset and clears CLK_EN.

## Golden gap
Golden render left WSA TX0..TX3 protection PATH_CTL at `0x04` (48-kHz rate nibble, CLK_EN clear). The inner WSA DAPM protection widget never powers, so upstream `wsa_macro_enable_vi_feedback()` never runs in the integrated companion-link lifecycle.

## Machine lifecycle candidate
Candidate machine module srcversion `943D430641888D2DC89938E` invokes the proven Windows resource7/8 WSA TX protection sequence from the machine `prepare()` lifecycle and reverses it from `hw_free()`.

Active render state is `0x10` on 0x244/0x264/0x284/0x2a4; teardown state is `0x20`.

## Interaction with active SoundWire Offset2
Adding the independently Windows-proven master active Offset2=0 candidate (`soundwire_qcom` srcversion `CE1DADE19E1CE61B7FC8843`) gives live master states:
- port10 `0x0300060f`
- port11 `0x03000d0f`
- port13 `0x0300001f`

The combined PROTCLK+Offset2 candidate produced forced tap2 logger frames at tap ID 2, 8 kHz, 64-byte payloads. The malformed near-full-scale/stale behavior seen with PROTCLK alone collapsed into stable packed low-amplitude data.

## Causal amplitude sweep
Using the same 997-Hz waveform with digital silence, -18 dB, and reference amplitude, the packed VI lane magnitudes moved monotonically with stimulus. Aggregate active-half mean |sample| was approximately:
- silence: 251
- -18 dB 997 Hz: 341
- reference 997 Hz: 432

High-sensitivity lanes moved approximately 323 -> 475 -> 604.

This is the first Linux VI stream in the project that responds monotonically to speaker stimulus. Treat as strong/provisional VI success pending packing characterization and fault-free lifecycle validation.

## Audible / PA-fault warning
During the recent experimental renders the right WSA8845 (`...:00:1`) repeatedly entered PA recovery with `err0=0x20`, and the user reported audible "static ghost" noise. Do not promote this candidate. The TX protection controls do shut down to `0x20` after teardown, and no stream remains running, so the effect appears transient. The PA-fault cause must be resolved before promotion or repeated high-level tests.

## CPS next step
PROTCLK+Offset2 does not itself produce tap3/CPS packets. A combined SoundWire module adding the previously proven CPS wake writes (`0x105c=0x0005000f`, DP13 `0x1d54=3`) has been built as srcversion `D008A3D6B585C11BE023992` under `v31-wsa-protclk-offset2-cpswake-20260820`, but should not be booted until the PA fault is understood or tests are constrained to reduced level.
