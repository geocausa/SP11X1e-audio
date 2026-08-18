# SP7 WASAPI RAW acoustic calibration reference

Date: 2026-08-18  
Status: **RAW capture method validated on SP7; earlier shared-mode cross-capture absolute L/R values demoted to provisional**

## Why this was needed

The fixed SP7 microphone geometry materially improves repeatability, but the original
`Record-ExternalMic.ps1` opened the default microphone-array endpoint through WASAPI
**shared** capture.  That path is allowed to traverse Windows capture-engine/APO
processing.  Two back-to-back Linux-v31 speaker-calibration runs made the limitation
obvious: absolute isolated-channel levels moved by several dB even though the SP7 and
SP11 were not moved.

This does **not** invalidate synchronized within-one-WAV transient tests such as the
40-Hz Volume-Up gate.  Those tests compare UP/DOWN or event/control windows inside one
continuous recorder instance and additionally require event-locked repetition.  It does
mean that separate Windows and Linux WAVs from the shared capture path must not be used
as an absolute dB calibration oracle.

## RAW recorder

A new tracked SP7 helper is:

`tools/windows/Record-ExternalMic-Raw.ps1`

The helper activates the default capture endpoint through `IAudioClient2`, sets
`AUDCLNT_STREAMOPTIONS_RAW`, then opens the shared stream.  The C# COM declaration
flattens the inherited `IAudioClient` methods before the `IAudioClient2` methods; using
C# interface inheritance shifted the derived-method vtable and caused the initial
prototype to reject `SetClientProperties` with `0x88890001`.

The working SP7 endpoint accepts RAW mode and reports:

- WAVE extensible source (`tag 65534`);
- 2 capture channels;
- 48 kHz;
- 32-bit float engine format.

The helper writes deterministic PCM16 WAV output for compatibility with the existing
SP7 analysis scripts.  It changes no persistent Windows audio setting.

Tracked helper SHA-256:

`A8EFD78AC4EE00BA1700D76C86136053050E440695127D07EAB4D6158404E366`

## Fixed-geometry v31 RAW repeatability

Fixture:

- SP7 microphone centred and square-on to SP11;
- separation = one attached SP11 keyboard length;
- neither machine moved between passes.

Source:

- `sp11-acoustic-cal-v1.wav`;
- SHA-256 `F790AFB06E57DD8D2B1E33C1A5DC329B028A10BBEF208F09D0C69115BB0D02E2`;
- 48 kHz stereo PCM16, 32 s;
- 2 s silence, 8 s left-only, 2 s silence, 8 s right-only,
  2 s silence, 8 s stereo, 2 s silence;
- endpoint target 25%.

Three independent SP7 RAW WAVs:

1. `3B36A84D6FA62C4931E72FC84EA2A00E85B4B7BFAD046987369C7E9BEBC8AFA6`
2. `25B7B06E56BC79C55FD4D46C3CC834E9E0CF0D7D140F7798FD3E9BF458AC642C`
3. `16B61F98022633B3C5AC110EF4306857F23A606F514DA85B8B268DD8D44B21E6`

The analysis measures each of the 21 known calibration tones in six one-second blocks
inside each active segment and takes the median.  This makes an isolated room impulse
or neighbour noise affect at most one block instead of moving the whole segment.

Across `630 Hz .. 6.3 kHz`, the full three-pass peak-to-peak spread has:

- left-only median `0.1277 dB`, p90 `0.1423 dB`, max `0.2474 dB`;
- right-only median `0.1164 dB`, p90 `0.1664 dB`, max `0.2065 dB`;
- stereo median `0.0878 dB`, p90 `0.1707 dB`, max `0.1870 dB`.

Across the wider `100 Hz .. 6.3 kHz` range, a few isolated low-frequency bins move by
roughly `0.8 .. 1.9 dB`.  Those bins are explicitly treated as room-sensitive and must
be median-voted across repeated captures.  They are not accepted as one-shot speaker
changes.

Reviewed three-pass analysis JSON on SP7:

- SHA-256 `62A66441D8027D156C32D1DCD6A9858DCEF2C2BE927C25123EBFAAF56B0C103C`.

## Concurrent Linux digital stability

Each RAW acoustic pass also recorded `effect_output.sp11_windows_dolby`.  Across the
same three runs, the post-Dolby calibration-tone level changes only about
`0.09 .. 0.11 dB` over `100 Hz .. 6.3 kHz`, confirming that the several-dB movement
seen in the old shared-mode physical captures was not the SP11 Dolby chain wandering.

## Consequence for the earlier L/R finding

`docs/findings/2026-08-18-FIXED-GEOMETRY-WINDOWS-LINUX-LR-PHYSICAL-PARITY.md`
and its reviewed JSON remain preserved as historical shared-mode measurements, but the
absolute cross-OS physical differences (`-6.537 dB`, `-1.288 dB`, and the derived L/R
fingerprint error) are now **provisional and must not drive tuning**.

The digital Windows-loopback/Linux-post-Dolby channel evidence from that finding remains
useful.  The physical Windows-vs-Linux L/R/bass comparison must be repeated with this
RAW SP7 capture method on both operating systems before the downstream channel-parity
question is reopened as a tuning target.

## Next gate

Capture the same deterministic calibration and the dedicated bass/psychoacoustic probe
on native Windows using `Record-ExternalMic-Raw.ps1`, without moving the established
SP7/SP11 fixture.  Compare repeated RAW Windows passes against the three-pass Linux-v31
RAW baseline.  Do not use the old shared-mode absolute dB values as the target.

## 2026-08-18 measurement-gain correction and recorder hardening

A later RX84 40-Hz regression exposed one more measurement-state variable that
RAW mode itself does not bypass: the **hardware capture endpoint gain**.  The
SP7 default Microphone Array endpoint was found at `+20.000 dB` with hardware
volume support.  A reversible idle RAW A/B, with SP11 silent, measured:

```text
                    +20 dB             0 dB
HP500 RMS           1.9152e-4          2.0934e-5
HP2000 RMS          1.3433e-4          1.5188e-5
HP6000 RMS          9.3699e-5          1.1071e-5
raw peak             0.02466            0.001434
```

The retained quiet v31 reference raw peak is about `0.001526`, so the 0-dB
capture state returns to the expected measurement scale.  The +20-dB 40-Hz
take was therefore rejected as a recorder-side contamination, not interpreted
as an SP11 regression.

For subsequent fixed-fixture measurements the SP7 endpoint is pinned manually
to **0 dB** and the Windows Settings process is kept closed.  The recorder does
not silently change this setting.

`tools/windows/Record-ExternalMic-Raw.ps1` is now hardened to make this state
explicit and auditable:

- prints default capture endpoint ID, dB, scalar, mute and hardware-support mask;
- records exact UTC timestamps immediately after `IAudioClient.Start()` and
  immediately after `Stop()`;
- writes those values and endpoint state to `<wav>.metadata.json`;
- accepts optional `-ExpectedEndpointDb`, refusing **before capture** if the
  hardware endpoint dB does not match within 0.01 dB.

Exact SP7 smoke test on the 0-dB endpoint:

- `-ExpectedEndpointDb 0`: capture succeeds and emits WAV + metadata;
- `-ExpectedEndpointDb 20`: returns nonzero before `IAudioClient.Start()` and
  emits no WAV.

The tested candidate SHA-256 is
`BA622A89C68EE68E301B02A1BE80CBE8F1F1826611CD8CD0E95D0778E41FB1C9`.
It is deployed on SP7 as
`C:\Users\SurfacePro7\Documents\KDNET\Codex\Record-ExternalMic-Raw.ps1`.
The older hash-pinned `A8EFD78A...` helper is retained separately for provenance.
