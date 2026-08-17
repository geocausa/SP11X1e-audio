# Golden v30 live validation — exact mute + DP1/DP3 transport — 2026-08-18

## Decision

**TECHNICAL GATES GREEN; USER LISTENING/PROMOTION VERDICT PENDING.**

Golden v30 was booted once through GRUB ID `sp11-audio-golden-v30-candidate`.
The persistent saved entry remained `sp11-audio-golden-v28` and the one-shot
`next_entry` cleared after boot. This validation therefore cannot silently
replace the known-good daily driver.

The candidate carries only three deltas over Golden v28:

1. exact Windows final `VOL_CTRL` endpoint mute, IID `0x4a63`, PID `0x08001039`;
2. WSA8845 DP1/DAC SIMPLE `BlockCtrl3` declaration; and
3. WSA8845 DP3/BOOST SIMPLE `OffsetCtrl2` declaration.

Golden v28's DP2/COMP `OffsetCtrl2=0x07`, Dolby chain, endpoint taper,
final-volume/GainStep path, WSA 63/10/6 lifecycle, PA policy, protection graph
and seek fixes remain unchanged.

## Boot identity

Live boot marker:

`sp11_entry=7.1.5-sp11-golden-v30-mute-dp1-dp3`

Kernel:

`7.1.5-sp11-render-parity-v4+`

Boot ID:

`01ae2095-f6c3-4dbb-9f35-396c82d15644`

Persistent GRUB state during and after the one-shot boot:

```text
saved_entry=sp11-audio-golden-v28
next_entry=
```

Loaded module srcversions:

- `snd_q6apm`: `F50BA24BDA6FAC8AE991A54`
- `snd_soc_wsa884x`: `A4F2E38C5C27D13E327887B`
- `snd_soc_lpass_wsa_macro`: `4AF6F542C17BA6DD46586DA` (unchanged Golden)
- `soundwire_qcom`: `406975A3ED60935B31491BF` (unchanged Golden)

Candidate boot hashes:

- kernel: `bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a`
- initrd: `51e17bd4952d785ef511b925f8835a415d7243ff95a7dcb97670e0879d5f0944`
- DTB: `3530e3426c500d664be6ed3ef066d1b548025ba8286a5810e8b98c591b6555ca`

The candidate initrd was created from the exact Golden v28 initrd and replaces
only signed `snd-q6apm` and `snd-soc-wsa884x`. Extract/repack verification was
zero-diff against its staged tree. WSA macro and `soundwire-qcom` compressed
module hashes remained byte-identical to Golden.

## Exact Windows endpoint mute is live

ALSA exposes:

- `SP11 Windows Volume Transaction`, numid 33, 288-byte control capacity; and
- `SP11 Windows Endpoint Mute`, numid 34, 12-byte control capacity.

The second control accepts exactly one u32 selector after the ALSA TLV header.
The kernel constructs the fixed 104-byte final-VOL_CTRL body. Static reverse
engineering already established qcadcm semantics: selector `1` means mute and
`0` means unmute. The generated Linux unmute body is byte-identical to the
retained Windows body, SHA-256
`441d3acf732158b63bea99b8581e172ec2385c0e5531ff7a4e7bf69cb46f4bea`.

With a protected graph RUNNING on digital zero, direct writes:

```text
01000000 -> rc=0
00000000 -> rc=0
01000000 -> rc=0
```

Kprobe on `audioreach_sp11_set_final_mute()` captured the matching `1/0/1`
selectors.

The production volume service automatically selected:

```text
volume_transaction_control_values=288 mode=windows-lr endpoint_mute=exact-dsp
```

After the running baseline was established, desktop mute-only, unmute-only and
final-mute gestures invoked `audioreach_sp11_set_final_mute()` with no
`audioreach_sp11_set_final_volume_q28()` calls in those stages. Thus mute is no
longer folded into endpoint gain/GainStep. The hidden hardware sink remains a
fail-safe backstop and is opened only after DSP unmute succeeds. Unit coverage
also proves a rejected DSP unmute fails closed.

The first edge of a later audible smoke-test was issued only ~50 ms after graph
wake and raced initial baseline establishment; it is not used as sequencing
evidence. The earlier established-baseline digital-zero trace is the accepted
mute-only discriminator.

## DP1 / DP2 / DP3 slave transport is exact

Kprobe on `sdw_write_no_pm()` captured both WSA8845 slave pointers and both
SoundWire banks during a muted-zero wake. Each target appeared three times per
amp over the observed lifecycle:

| field | B0 address/value | B1 address/value | both amps |
|---|---|---|---|
| DP1/DAC BlockCtrl3 | `0x127 = 0x00` | `0x137 = 0x00` | yes |
| DP2/COMP OffsetCtrl2 | `0x225 = 0x07` | `0x235 = 0x07` | yes |
| DP3/BOOST OffsetCtrl2 | `0x325 = 0x1f` | `0x335 = 0x1f` | yes |

The values are supplied by Qualcomm master transport parameters; v30 only
advertises which optional SIMPLE slave registers exist. No transport constant
is hard-coded in the WSA8845 codec driver.

This closes the three known DP1/DP2/DP3 slave-side structural differences in the
ordinary Windows speaker schedule. It does not alter the earlier causal result:
DP2 `OffsetCtrl2=0x07` remains the field that closed the broadband-static defect.

## WSA8845 resident lifecycle retained exactly

After PCM returned `closed` and both SoundWire devices runtime-suspended, v30
was left idle for a full 20 seconds. A subsequent five-second muted-zero wake
was traced at `_regmap_write`.

Total WSA8845 writes: **32**.

Each of the two amplifier regmaps emitted exactly 16 writes: the recovered
Windows 10-write START sequence followed by the 6-write STOP sequence:

```text
START:
3021=07 3020=67 3581=90 3582=00 34d0=67
3067=08 304d=52 3430=01 3067=0c 304d=5a

STOP:
3430=00 3581=ff 3582=03 3585=ff 3586=03 34d0=00
```

There was no 63-write cold-init replay. The boot lifecycle-enable marker count
remained exactly two total (one per physical amplifier). After settling, PCM
was `closed` and both SoundWire WSA devices were suspended. No WSA, SoundWire,
XRUN, PA or regcache fault appeared.

## Physical broadband-noise regression — SP7 external microphone

**Capture provenance:** the microphone is the Surface Pro 7 recording the SP11
speakers externally. The SP11 microphone/capture path is not used.

SP7 capture:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\acoustic-zero-linux-v30-mute-dp1-dp3-1pct-20260818\external-mic-20260818-004136.wav`

SHA-256:

`B791573E99386585E4ED938F862412055B483BABFB0394E45878F4FDA60728A6`

Format: 48 kHz, stereo, signed 16-bit PCM, 29.99 s.

The SP11 visible Dolby endpoint was set to 1% and muted. A 20-second stereo
48-kHz PCM16 digital-zero stream woke the physical speaker graph, then PCM
returned closed. The endpoint was restored to 9% muted afterward.

Using the same 0.5-second first-difference combined-RMS metric family used for
the v28 closure:

- v30 whole-capture median: `1.8148076164603558e-05`
- same recalculation on retained v28 repeat: `1.818919259777128e-05`
- v30 / v28 recalculation: `0.9977395130x`
- v30 / published v28 (`1.8179778081257844e-05`): `0.9982561989x`
- v30 / retained Windows (`1.8253227918889202e-05`): `0.9942392790x`

Using the known v30 active timing window 3.0--22.5 s gives essentially the same
conclusion: v30 `1.8531597502830225e-05` versus v28
`1.8510120695455838e-05`, ratio `1.0011602738x`.

Therefore DP1/DP3 structural completion does **not** reopen the v27 broadband
static. v30 remains at the v28/Windows/room-floor noise class.

## Audible smoke test

A local copy of *Seven Nation Army* was decoded offline to 48-kHz stereo PCM16
and played through the normal Movie/Golden chain at the pre-existing 9% visible
level. The test exercised mute/unmute transitions and ended muted. It produced
no XRUN, WSA, SoundWire or PA fault. This is a machine-health smoke test, not the
operator's promotion verdict.

## Promotion state

All objective v30 implementation/regression gates requested for this candidate
are GREEN. **v30 is not yet the saved default.** Golden v28 remains the saved
entry until the user manually auditions normal music/YouTube, volume, mute and
seek behavior and confirms that the Golden listening quality survived.
