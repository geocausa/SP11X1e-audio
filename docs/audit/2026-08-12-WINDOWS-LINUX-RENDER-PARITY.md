# SP11 Windows/Linux built-in-speaker render parity — 2026-08-12

## Scope

Built-in speaker rendering only. Microphone/input and Bluetooth are intentionally excluded.

## Fresh Windows reference

SP11 was booted one-shot into Windows from the normal CPS V3 Linux default. The active endpoint was:

- `Speakers (Qualcomm(R) Aqstic(TM) Audio Adapter Device)`
- endpoint `{0.0.0.00000000}.{5bb689e6-2c6b-4357-b4c1-beb815638f88}`
- stereo 48 kHz / 16-bit PCM device format
- endpoint master attenuation `-34.04602 dB` (`0.100000016` scalar), unmuted

During the deterministic source stream, `audiodg` loaded AudioEng, SurfaceAPO, DolbyDax3Apo, DolbyApoVr, DolbyAPOvlldp150, DolbyAudioProcessing, DolbyHrtfEnc, and VirtualSurroundApo. DAX runtime XML contained no explicit `SelectedMainProfile` override. The shipped operator policy selects Movie for the internal speaker with spatial audio enabled and Music with spatial audio disabled; auto-profile is disabled.

Source SHA-256:

`FD5898DB52F2292C2D3F603CC0A9CE7C9A1128B5A6BEF89BA53AD52E184431CD`

Clean Windows loopback SHA-256:

`1D73D8FE3D45F6A8250160DC7061A74E09D7BDAA43EBDC65975A3D46B676A709`

## Linux candidate

The production Dolby host is the VR -> VLLDP -> AudioEng chain from commit `7c6ad09`, contained in the integrated `agent/cps-parity-review-20260811` history. A clean build of the pre-audit production source reproduced the deployed plugin byte-for-byte at SHA-256:

`ef4d995216b3ba5ae55189a7d5032a402968e308f18e2f780959788e21179d31`

The Windows endpoint attenuation maps through the recovered DAX rule:

`round(-34.04602 * 16) = -545`, i.e. the VLLDP Q4 postgain request is `-545` (`-34.0625 dB`).

All seven recovered profiles were rendered offline from the exact same source and state contract.

## Profile identification

Full 29.45-second aligned Windows-vs-Linux results:

| Profile | Correlation | Residual SNR | Fitted scale |
|---|---:|---:|---:|
| Movie | **0.999999473836** | **59.778 dB** | **1.000156535** |
| Music | 0.999426997222 | 29.409 dB | 1.034945566 |
| Game | 0.999426997222 | 29.409 dB | 1.034945566 |
| Personalize | 0.997828861107 | 23.628 dB | 0.950012729 |
| Online Course | 0.992052942258 | 18.005 dB | 1.168204040 |
| Dynamic | 0.989926935496 | 16.980 dB | 0.897671291 |
| Voice | 0.946635558153 | 9.835 dB | 2.109068378 |

Movie is unambiguous. A cold-state 3-second section reaches correlation `0.9999999983347901`, fitted scale `0.9999687419`, residual SNR `84.67 dB`, and maximum fitted residual below `9.2e-5`.

Windows loopback peak was `0.985870361`; Linux Movie peak was `0.985905945`.

## Audit finding and fix

The deployed Linux host was persisted to Dynamic before this audit. That was a real policy mismatch: the implementation contained the correct Movie tuning, but Linux selected the wrong default relative to the current Windows built-in-speaker configuration.

The live Linux selection was changed to Movie and acknowledged in-place by the running filter. It survived the subsequent Windows/Linux reboot cycle and reports `Profile: movie (live: movie)`.

For clean-install reproducibility, this audit changes the deployment/native no-selection fallback from Dynamic to Movie. Explicit user profile selections are unchanged. The profile lifecycle regression now verifies that an instance with no `SP11_DOLBY_PROFILE` starts as Movie.

## Interpretation

Built-in-speaker rendering meets the project goal of Windows-equivalent / on-par behavior by direct fresh waveform evidence. It is not claimed to be bit-for-bit identical at the final Windows loopback boundary: the full-file residual is about 60 dB below signal and is concentrated in small state/transient regions. The remaining lower-level amber items concern speaker-protection/control transaction parity, not a missing render stage.
