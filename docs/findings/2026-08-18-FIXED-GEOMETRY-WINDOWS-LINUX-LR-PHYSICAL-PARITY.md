# Fixed-geometry Windows/Linux L/R speaker physical parity

Date: 2026-08-18
Status: **digital L/R parity essentially exact; physical left-path parity gap exposed**

## Deterministic calibration source

`sp11-acoustic-cal-v1.wav` is generated independently on Windows and Linux and
is byte-identical:

- SHA-256 `F790AFB06E57DD8D2B1E33C1A5DC329B028A10BBEF208F09D0C69115BB0D02E2`;
- 48 kHz stereo PCM16, 32 s;
- deterministic multitone frequencies:
  `100,125,160,200,250,315,400,500,630,800,1000,1250,1600,2000,2500,3150,4000,5000,6300,8000,10000 Hz`;
- composite source peak scalar `0.08`;
- segments: 2 s silence, 8 s left-only, 2 s silence, 8 s right-only,
  2 s silence, 8 s stereo, 2 s silence;
- endpoint target: 25% on both OSes;
- SP7 microphone fixture unchanged between Windows and Linux.

## Windows references

SP7 external microphone:

- SHA-256 `3CAD56D2B15E2C9C49B2314F89FCC17902BE67669739638E58689866DD2B3CFC`.

Windows WASAPI loopback:

- SHA-256 `CE162BE88BD99B83CCCE2D83B628D89B1BE28C470D1C6E654B77F8F0B4964E2A`;
- left-only RMS: L `-27.0557 dBFS`, R `-96.5522 dBFS`;
- right-only RMS: L `-89.3710 dBFS`, R `-25.0461 dBFS`;
- stereo RMS: L `-25.6954 dBFS`, R `-24.7861 dBFS`.

The fixed microphone point is physically biased toward the right speaker at
many frequencies.  This is intentionally retained as a repeatable physical
fingerprint; it is **not** interpreted as an electrical speaker imbalance by
itself.

## Linux v30 references

SP7 external microphone:

- SHA-256 `E1C765B97DF7B7F83856BF3C9A82241C441B094EE495A822B29448B866E5E1E2`.

Linux post-Dolby digital capture:

- SHA-256 `A7734DE6011D3044E8F4D9C0B76A8826866A14929ED79E43D823A1567DC9769F`;
- left-only RMS: L `-27.0544 dBFS`, R `-115.3473 dBFS`;
- right-only RMS: L `-122.4729 dBFS`, R `-25.0453 dBFS`;
- stereo RMS: L `-25.6981 dBFS`, R `-24.7887 dBFS`.

The active-channel RMS therefore matches Windows loopback to about `0.0014 dB`
(left) and `0.0008 dB` (right); stereo differs by only about `0.003 dB` per
channel.  Upstream L/R channel routing and Dolby transfer are effectively
closed for this stimulus.

## Physical Windows/Linux result

Across the robust `100 Hz .. 6.3 kHz` range at the unchanged SP7 fixture:

- Linux left-only minus Windows left-only median: **`-6.537 dB`**;
- Linux right-only minus Windows right-only median: **`-1.288 dB`**;
- Linux-vs-Windows L/R fingerprint error median: **`-5.249 dB`**.

The full physical stereo segment is substantially closer than the left-only
segment because the fixed microphone position is dominated by the physically
stronger right-side contribution.

Selected frequency points, Linux minus Windows physical dB:

| Hz | Left | Right | L/R fingerprint error |
|---:|---:|---:|---:|
| 100 | -5.36 | -1.20 | -4.17 |
| 200 | -6.14 | -1.40 | -4.74 |
| 315 | -6.63 | -0.96 | -5.67 |
| 500 | -6.37 | -0.26 | -6.11 |
| 800 | -9.44 | -0.65 | -8.79 |
| 1250 | -3.41 | -1.20 | -2.20 |
| 2000 | -2.13 | -0.64 | -1.50 |
| 3150 | -12.12 | -1.32 | -10.80 |
| 4000 | -18.52 | -1.94 | -16.58 |
| 6300 | -20.59 | -6.09 | -14.51 |

8 kHz and 10 kHz physical bins approach the room/mic floor and are retained in
the reviewed JSON but should not drive tuning decisions.

## Boundary

Because Windows loopback and Linux post-Dolby are essentially identical while
the fixed-geometry physical left-only response diverges, this mismatch is
**downstream of the Dolby output boundary**.  Candidates include the protected
AudioReach speaker graph, channel-specific WSA-macro path, SoundWire mapping,
WSA8845 per-side operating state, or another downstream speaker-specific
configuration.  A simple Dolby-port channel attenuation is ruled out.

The next causal gate is a 40 Hz real-key Volume-Up sweep performed separately
with left-only and right-only source content.  If the click is strongly
left-specific, the L/R physical gap and the Volume-Up microtransient likely
share a downstream cause.
