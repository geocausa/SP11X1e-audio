# Golden v31 deterministic program/seek smoke

Date: 2026-08-18
Status: **objective program/seek smoke GREEN; operator normal-listening verdict pending**

## Source / playback

The exact v28 physical seek source was reused:

`/home/geoca/Documents/The White Stripes - Seven Nation Army (Official Music Video).mp3`

SHA-256:
`951A65CC63FEE17622485C1D94708614005524C7E20F86D3D815327F6BD0E8B3`.

The visible Windows-Dolby endpoint was set to 25%, unmuted, then restored to
6%.  GStreamer `playbin` targeted `effect_input.sp11_windows_dolby`, preroll
seeked to 19 s, and issued `FLUSH|ACCURATE` seeks at the retained v28 targets:

- `25.752 -> 55 s`;
- `58.920 -> 12 s`;
- `16.000 -> 90 s`.

Exact action log SHA-256:
`C8E2881E4E53F81A339A38A9B30F849CD78387E27CD34E433948DB5A5D622EE8`.

## Physical capture

All acoustic evidence is the SP7 microphone externally recording SP11; the
SP11 capture path is not used.

SP7 WAV:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\acoustic-reference-keyboard-length-20260818\linux-v31-seven-nation-seek\external-mic-20260818-110619.wav`

SHA-256:
`7BF735A7E53659940E174B5E52634E819B6B120CC7950F287365258D21C7790E`.

The same v28 physical discriminator was used: maximum absolute first-sample
difference within +/-20 ms of each seek compared with first-difference
percentiles from surrounding +/-0.75 s music, excluding central +/-60 ms.

| Seek | ch0 / p99.9 | ch0 / p99.99 | ch1 / p99.9 | ch1 / p99.99 |
|---|---:|---:|---:|---:|
| 1 | 0.0966 | 0.0717 | 0.0871 | 0.0710 |
| 2 | 1.1614 | 0.9746 | 1.0117 | 0.7211 |
| 3 | 0.0323 | 0.0155 | 0.0355 | 0.0183 |

Seek 2 slightly exceeds the local p99.9 derivative in channel 0 but remains
below local p99.99 on both channels.  No seek therefore creates a unique
needle/pop beyond the program material's own extreme transient scale.

## Control / fault boundary

The volume service logged only:

- the initial 25% graph handover (`windows-lr:init`, GainStep 3); and
- the final 25% -> 6% restore (`windows-ckv:vol->cal:3->1`).

No volume/GainStep transaction occurred during any seek.  The only qcom-apm
error is timestamped at graph start before the measured seek sequence and
matches the known unsupported startup-calibration record.  No runtime XRUN,
SoundWire, WSA or PA fault was found.

## Conclusion

v31 preserves the previously closed seek/discontinuity behavior while fixing
the 40 Hz volume-transition defect.  Objective promotion gates now consist of
repeat-GREEN 40 Hz control stress plus this GREEN deterministic program/seek
smoke.  Remaining promotion gate is operator normal listening / mute / volume
judgment.
