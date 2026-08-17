# W02 strict-identity residual localization and AudioCleanup closure — 2026-08-17

## Scope

This pass revisits W02, the remaining non-bit-identical Windows-vs-Linux userspace render residual after the v28 physical-static closure. It does **not** change the production render chain.

## Exact Aug-12 oracle replay reproduced

Using the pinned deterministic source (`FD5898DB...431CD`), clean Windows loopback (`1D73D8FE...6A709`), Movie profile, VLLDP generation postgain `-545`, and the current original-code VR -> VLLDP -> exact AudioEng limiter host, the published Aug-12 result was reproduced:

- Windows lag: `76480` samples
- Linux/native lag: `1264` samples
- fitted scale: `1.000156535277`
- correlation: `0.999999473855`
- residual SNR: `59.778327 dB`
- cold first 3 s residual SNR: `84.75 dB`
- regenerated Linux peak: `0.985905945`, matching the value recorded by the Aug-12 audit

Therefore W02 is reproducible from the surviving source/state contract.

## Residual is a microscopic LF complex-transfer difference

50-ms energy localization and a complex-transfer sweep show the residual is concentrated in the low-bass program sections rather than distributed across the file:

- largest cluster: the ~55-Hz burst around 12--14 s;
- second cluster: the loud end of the ~75-Hz staircase around 24--28 s;
- 90 Hz is smaller;
- 140 Hz is much closer;
- above roughly 500 Hz, the fitted phase difference is effectively zero and magnitude error is only a few ten-thousandths of a dB.

Representative Windows/Linux complex-transfer difference:

- ~40--60 Hz: about `-0.012..0 dB`, `+0.09..+0.11 deg`;
- ~75 Hz: about `+0.0034 dB`, `+0.07 deg`;
- ~200 Hz: about `+0.0008 dB`, `+0.009 deg`;
- >500 Hz: phase approximately zero at the precision of this test.

H3/H5 and other harmonics on the loud 75-Hz steps already agree to roughly hundredths of a dB or better. This is not evidence for a missing bass enhancer, limiter, EQ, or gross nonlinear stage.

## Windows loopback PCM16 conversion is not the cause

The retained `Record-WindowsLoopback.ps1` converts float32 loopback to PCM16 with clamp plus `Math.Round(v * 32767f)`. Re-scoring common float->S16 policies changes the full-file result by only ~0.001 dB. The ~59.78-dB residual and its LF phase signature remain.

## Current AudioEng CAudioCleanup is not an LF filter

The current SP11 Windows partition was mounted read-only and the installed `Windows/System32/AudioEng.dll` inspected:

- SHA-256 `843430c1516a2867fe716e89bcc35399e59e5040d992bfaff7468eab1cb63a93`
- public CodeView `AUDIOENG.pdb`
- GUID `{86BD4A63-EA96-E509-EA46-34121370ED6E}`

Matching public Microsoft symbols locate native ARM64:

`CAudioCleanup::APOProcess` -> section 2 + `43232` -> VA `0x18000d8e0` (RVA `0xd8e0`).

Exact disassembly shows no filter state or coefficient math. The routine:

1. scans float samples against constants `+128.0f` and `-128.0f`;
2. for normal in-range PCM, preserves/copies the connection (`memcpy` when buffers differ);
3. for an out-of-range buffer, zeroes the destination (`memset`) and marks the output accordingly.

The two direct helper thunks resolve via the public PDB to `memcpy` and `memset`. `CAudioCleanup` is therefore a sanity/cleanup guard, not the missing LF pole/zero.

## Microsoft interstitial path remains transparent in the captured stereo contract

Existing state-pinned full-memory evidence already proves valid-fill byte identity from Dolby VR output to VLLDP input and, separately, exact ASAR unity on frozen stereo blocks. Thus the intervening AudioMeter / CAudioVolume / AudioConstrictor / mixers / ASAR path is not supported as the source of the W02 LF arc for this captured contract.

## One-LSB profile sensitivity did not identify a mistuned scalar

A detached offline 66-case sensitivity matrix perturbed evidence-backed Movie scalar fields, VLLDP group payload integers, and VLLDP postgain by one integer LSB. No production file was modified.

Results:

- IEQ amount, surround boost, virtualizer angles, effective output-mode perturbations: byte-identical under the matching-stereo gates;
- dialog-amount +/-1: much worse (`~58.74 / 57.54 dB`) and its delta is essentially orthogonal to W02;
- VolMax +/-1: much worse (`~48.6--48.7 dB`) and likewise does not project meaningfully onto W02;
- postgain +/-1: byte-identical for this state;
- one-LSB perturbations of the four VLLDP compressor-group payloads: byte-identical in this replay, consistent with internal quantization/no effective state change at that granularity.

No tested one-LSB profile parameter explains the Windows residual.

## Current boundary / next exact test

W02 is now narrowed to a tiny state/processing difference within the original Dolby VR/VLLDP reproduction or its exact Windows initialization/history, not a missing Microsoft system APO or PCM16 recorder artifact.

The highest-value next experiment is a fresh state-pinned Windows Movie run using the original `System.Media.SoundPlayer` path and exact source at 10% endpoint, with simultaneous WASAPI loopback and a full-memory `audiodg` dump during the loud 75-Hz section. That will permit direct Windows-vs-Linux comparison at VR output, VLLDP input, and VLLDP output on the same current oracle rather than inferring stage ownership from final loopback only.
