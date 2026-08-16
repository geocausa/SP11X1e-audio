# Synchronized Windows/Linux WSA macro RX gain acoustic sweep

Date: 2026-08-16  
Status: GREEN directional discriminator for the WSA-macro RX gain variable; not an anechoic SPL calibration

## Purpose

The current H03 investigation had isolated one still-unproven producer-side difference: Linux exposes the LPASS WSA macro RX digital controls only through generic Qualcomm policy, with the normal SP11 path parked at control value 81 (`-3 dB`) even though the hardware field supports value 84 (`0 dB`). Earlier short acoustic captures suggested that 84 sounded/measured closer to Windows, but recorder/playback start times were not sufficiently controlled.

This run repeats the comparison with the exact same deterministic chirp, the Windows and Linux endpoint both pinned to 12%, fixed physical geometry, and SP7 continuously recording the SP11 speakers around each playback.

## Stimulus

Exact source on both operating systems:

```text
chirp-40-16000Hz-24dBFS.wav
48 kHz / stereo / PCM16
2 s silence + 15 s exponential 40 Hz -> 16 kHz + 2 s silence
SHA-256 C8782C7741B3ECE628362E785D2EC91B990EF453BEFA4A3E7D6C3E1CD1F8A208
```

Windows used the existing `Run-AcousticChirp-Windows.ps1` path and `IAudioEndpointVolume=0.12`. Linux used `effect_input.sp11_windows_dolby`, the same 0.12 visible endpoint value and the live `7.1.5-sp11-render-parity-v4+` `sp11_wsa_macro0db_oracle=1` candidate. That candidate changes only the four generic RX-control maxima from 81 to 84; it does not change the control value by itself.

The Linux tests changed only both `WSA_RX0/RX1 Digital Volume` controls among 81, 82, 83 and 84. The controls were restored to 81 after the sweep. No DRE/CSR change was made.

## Safety/runtime result

All tested Linux chirp runs completed through the Windows-Dolby sink with no new WSA/PA/DSP/SoundWire/XRUN faults. Endpoint stayed at 12%. RX84 was explicitly restored to RX81 after testing.

## Analysis method

Because the SP7 default capture endpoint is not certified as a flat/AGC-free measurement microphone, **absolute SPL is not promoted as a parity metric**. Instead the sweep is frequency-indexed and each capture is normalized by its stable 1--5 kHz median before comparing response *shape* against the synchronized Windows capture.

The useful discriminator is therefore relative acoustic spectral shape under identical geometry, not laboratory sensitivity.

Low-frequency room modes and recorder noise dominate much of the region below roughly 500--630 Hz, so that region remains unsuitable for fine ranking in this setup.

## Result

Normalized shape error versus the synchronized Windows reference:

| Linux WSA RX value | 1--5 kHz MAE | 1--5 kHz RMSE | 630 Hz--6.3 kHz MAE | 630 Hz--6.3 kHz RMSE |
|---:|---:|---:|---:|---:|
| 81 (`-3 dB`) | ~0.47 dB | ~0.61 dB | ~0.51 dB | ~0.67 dB |
| 82 (`-2 dB`) | **~0.37 dB** | ~0.39 dB | ~1.41 dB | ~2.61 dB |
| 83 (`-1 dB`) | ~0.64 dB | ~0.77 dB | ~0.87 dB | ~1.05 dB |
| 84 (`0 dB`) | **~0.22 dB** | **~0.26 dB** | **~0.27 dB** | **~0.40 dB** |

RX84 is the strongest result in both stable comparison bands. In the 1--5 kHz band its frequency-by-frequency residual versus Windows is approximately:

```text
1.0 kHz  +0.04 dB
1.25 kHz -0.44 dB
1.6 kHz  -0.37 dB
2.0 kHz  +0.17 dB
2.5 kHz  -0.16 dB
3.15 kHz -0.17 dB
4.0 kHz  +0.07 dB
5.0 kHz  -0.34 dB
```

The result does not establish that every Windows WSA-macro register is now known, but it rejects the generic Linux `-3 dB` ceiling as the best acoustic match and makes `0 dB` the current evidence-backed RX producer setting for the next isolated candidate.

## Important caveat on absolute level

The SP7 capture path shows signs consistent with level adaptation/AGC between takes: increasing the Linux RX control does not produce a stable +3 dB change in recorded absolute level. Absolute microphone dBFS is therefore intentionally excluded from the decision. This finding is about **response shape** only.

## Next discriminator

The other already-isolated generic Linux producer policy is `WSA_MACRO_GAIN_OFFSET_M1P5_DB`, which toggles four half-dB PGA-policy bits at WSA RX offsets `0x0428`, `0x0444`, `0x04a8` and `0x04c4`. It has been proven safe in isolation on the prior protected stack, but not yet combined with render-parity-v4/RX84.

The next controlled candidate should therefore preserve DRE/CSR and all protection state, keep RX84, and change only that four-bit half-dB policy. If its synchronized response improves the Windows residual without faults, it becomes the next producer-parity advance; otherwise RX84 with the existing half-dB policy remains preferred.

## Capture provenance

SP7 retained captures:

```text
Windows 12%:
  DB648A04908B56C23B329E8CD1528B8456D9CBCDCEFDC78EF44FD94413D4F23F
RX81:
  C51A8D84EFAB7B8EBBA1179A00496B66752B289A82C5D7CCE0708642204ADE14
RX82:
  73A1A2B5D3EF0ECF4B6A4CDB8DFCD23D59F657BC0AF08C0A5CBF5C005B9CC0A1
RX83:
  C1F1FE677FCEB2B1001A652106F2369F7A6253E05203BC6BDAA2104C7092C409
RX84:
  773A3A33A98D4409478768D10052109CA664AC6CE19F7F649C1165848A34ABFD
```

Analysis summary on SP7:

```text
C:\Users\SurfacePro7\Documents\KDNET\Codex\sync-chirp-win-vs-rpv4-rx81-84-ranked-20260816.json
SHA-256 B7789CEC80F5F6C6A6F5B02BA549DBE91A7FB6BCBE0F6996CB7ADE72D1E460FC
```
