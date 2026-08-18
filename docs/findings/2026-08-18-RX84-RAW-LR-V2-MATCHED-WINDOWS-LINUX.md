# Active-RX84 RAW L/R v2 — matched native Windows / Linux result

Date: 2026-08-18
Status: **GREEN discriminator; downstream content-dependent residual remains**

## Matched acquisition

The same versioned 38 s source was independently generated on Linux ARM64 and
Windows amd64/Python 3.14 and produced the exact same SHA-256 on both:

`0395BB466047B1C55F8E7D9594DC360A85C371BBA806B8ED61F325AE0933A0C0`.

Both operating systems used:

- endpoint 25%, unmuted;
- fixed keyboard-length SP7 fixture, untouched across the OS switch;
- hardened SP7 WASAPI RAW recorder with `-ExpectedEndpointDb 0`;
- three complete source repetitions;
- native Windows MediaPlayer / Windows APO path on Windows;
- Golden v31 + active RX84 producer policy / Movie path on Linux.

SP7 physical RAW:

- Linux: `4F5B818278B51B3811B2F9F82C5960AF58E2C8852158C6214BCA82EAC8BACF14`;
- Windows: `C1ED4461F7A4B756874BE8921565F7FC75FA601C599CB0BD3DFF842B71971D21`.

Windows endpoint was 10%/unmuted before the test, set to 25%, and restored to
10%/unmuted before the intentional reboot back to Golden v31.

Windows continuous loopback:

`135CAAA4A4A5B02239805E58BF9187FB868DE503E1B070BA7D8CDB67383D5C2F`.

Windows loopback analysis:

`BD5E5A931E2801132FAC7212B78C97A7C3F7CF740BFD2FA4836D6782F187DAB2`.

The Windows evidence was later read through a clean `ntfs-3g.probe --readonly`
gate and a read-only NTFS mount; the volume was unmounted immediately after the
analysis JSON was copied. No NTFS write occurred.

## Correct Linux digital normalization

An earlier Linux `pw-record --target effect_output.sp11_windows_dolby` file was
all zero and is rejected. The correct post-Dolby tap is the **hidden ALSA sink
monitor**:

`pw-record --target alsa_output.platform-sound.HiFi__Speaker__sink`
with `stream.capture.sink=true`.

The valid fresh-25% Linux digital triple is:

- WAV SHA `7E9DFB777C11D9A38E8FA1F66CA01139142FD8DC12434B2921704EA0C711F62B`;
- analysis SHA `AB6E59C284452D1CBA16EE9E30B977E6BB98A71451A95C59363AE17EC5803693`.

## Digital parity is extremely tight

The Windows loopback and Linux hidden-sink-monitor R-minus-L fingerprints agree
to roughly **0.004..0.013 dB** across the 19 test bins. Thus the physical
residual cannot be assigned to a missing Dolby/APO channel-EQ stage.

Absolute digital drive is also close. In the stable 630 Hz..6.3 kHz band,
Windows-minus-Linux median drive is approximately:

- left: `-0.049 dB`;
- right: `-0.171 dB`.

Both OSes independently show nearly the same repetition-history drift (~1.9 dB
left / ~1.7 dB right), proving the larger physical comparison is not simply a
mismatched Windows volume setting.

## Downstream R-L residual

Define for each OS:

`downstream_RL = physical_RL - digital_RL`.

Then compare `Windows downstream_RL - Linux downstream_RL`:

| Hz | residual dB |
|---:|---:|
| 100 | +3.858 |
| 125 | +1.925 |
| 160 | +1.325 |
| 200 | +1.789 |
| 250 | +1.704 |
| 315 | +1.786 |
| 400 | +3.044 |
| 500 | -0.080 |
| 630 | +2.494 |
| 800 | +1.075 |
| 1000 | -0.349 |
| 1250 | -2.954 |
| 1600 | -2.649 |
| 2000 | +2.095 |
| 2500 | +0.025 |
| 3150 | +0.542 |
| 4000 | +1.152 |
| 5000 | +0.320 |
| 6300 | +1.271 |

For 630 Hz..6.3 kHz:

- mean absolute residual ~`1.357 dB`;
- median absolute residual ~`1.152 dB`;
- max absolute residual ~`2.954 dB`.

This is larger than the digital mismatch by roughly two orders of magnitude.
It therefore lives downstream of the measured Dolby/APO output boundary.

## Critical interpretation boundary

Do **not** read the table above as a linear speaker-EQ correction curve.
The v2 stimulus is a simultaneous 19-tone bed, and several frequencies are
harmonically related (for example 100/200/400/800/1600 Hz). The SP11 physical
speaker path is nonlinear by design: WSA compander/DRE/protection and the tiny
speakers generate harmonic/intermodulation energy. That energy can land exactly
on another v2 test bin.

The matched result therefore proves a **content-dependent downstream semantic
difference** remains after RX84, consistent with the still-open WSA8845
consumer/DRE/CSR lifecycle boundary. It does *not* yet prove a fixed linear gain
or EQ error.

This distinction also reconciles the result with the earlier single-tone RX84
probe, where 75/100 Hz fundamentals at useful source levels came within tenths
of a dB of native Windows.

## Next discriminator

Use a versioned **sequential single-tone matrix**, with only one frequency and
one source channel active at a time and multiple source levels. Capture Windows
and Linux RAW plus their digital normalization. If the large residual persists
on isolated fundamentals, it is level/frequency-dependent consumer gain. If it
collapses while v2 remains different, the missing semantic is primarily
nonlinear harmonic/intermodulation behavior.

Do not change DRE/CSR registers until that discriminator is complete; prior
blind `DRE_CTL_1=0` experiments were unsafe.
