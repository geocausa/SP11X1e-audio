# Golden v31 psychoacoustic-bass boundary — Linux side

Date: 2026-08-18
Status: Linux-side mechanism boundary established; matched native-Windows RAW oracle pending.

## Summary

The ordinary internal-speaker path must not be "fixed" by blindly enabling a
virtual-bass or Bass Enhancer block. Existing Windows evidence strongly rules
out the modern ASAR harmonic-synthesis speaker path in the tested condition,
and the persistent DolbyApoVr Bass Enhancer is strongly supported as OFF by
OEM/profile/default/live evidence.

The evidence-backed low-volume fullness mechanisms already present in Golden
v31 are instead:

1. the exact Qualcomm GainStep-dependent MSIIR loudness contour;
2. the original Movie VLLDP/VR dynamics with long-lived generation state;
3. downstream physical speaker/amp harmonic generation.

## Exact GainStep loudness contour

The 30 reviewed REV_0D GainStep rows were decoded using the AudioReach MSIIR
coefficient convention. The important quantity for psychoacoustic balance is
the LF-vs-1-kHz *shape* (independent of numerator-shift absolute scaling).
Representative left-channel values are:

| UI range | GainStep | 60 Hz relative to 1 kHz | 100 Hz relative to 1 kHz |
|---|---:|---:|---:|
| 1-16% | 1 | +17.44 dB | +15.02 dB |
| 17-24% | 2 | +16.83 dB | +14.72 dB |
| 25-26% | 3 | +16.60 dB | +14.76 dB |
| 34-35% | 9 | +13.44 dB | +12.75 dB |
| 40-41% | 12 | +11.68 dB | +11.28 dB |
| 49-50% | 16 | +9.31 dB | +9.13 dB |
| 59-61% | 20 | +6.92 dB | +6.84 dB |
| 72-75% | 24 | +3.55 dB | +3.52 dB |
| 97-100% | 30 | 0 dB | 0 dB |

This is a classic level-dependent loudness compensation and is already driven
by Golden v31 using Windows prior/new CKV semantics.

Some Windows rows contain real L/R coefficient differences (notably rows 3, 9
and 24). Golden v31 carries the exact row payloads and therefore preserves
those differences.

## Dedicated psycho-bass probe

A deterministic 46-second stereo PCM16 probe was generated with two cycles of:

- pure 40, 60, 75, 100 and 150 Hz tones;
- missing-fundamental complexes for perceived 60, 75 and 100 Hz using 2x/3x/4x
  source components.

Source SHA-256:
`01558cac5ec08ef3ed7ad172b7c64c69482bdb494638165e01d929c5a77b759f`

The valid long-lived-v31 run proved the endpoint handover actually reached
25% / GainStep 3 before the probe. It captured both post-Dolby PCM and SP7
WASAPI RAW from the fixed one-keyboard-length geometry.

Long-lived run evidence:

- stage log SHA-256 `3ff0fc7c20f423197465990e64363368c16b64090c3d614371e27953c924c916`
- post-Dolby WAV SHA-256 `43f6472618355d227ffdd0dc749add10c1ec40b9d23a7d70bbcbecfcba73f4d6`
- SP7 RAW WAV SHA-256 `652dd70c42637fa8cb0e7bc7f344d15e1b1dda4928d9170b3e9949dfc8227dc4`
- SP7 analysis SHA-256 `b3a380decb765a0e44072b0e5628dc24e58028418ab62df8299e2588343384cb`

At the physical microphone, representative second-harmonic ratios were roughly:

- 60 Hz: -17.5 to -18.1 dBc;
- 75 Hz: -14.2 to -14.3 dBc;
- 100 Hz: -20.3 to -20.8 dBc;
- 150 Hz: -24.5 to -24.8 dBc.

The same post-Dolby PCM generally had those harmonics tens of dB farther down
(often near -90 to -120 dBc once settled). Therefore the strong psychoacoustic
harmonic content is not being synthesized as a normal Dolby PCM sideband; it
is predominantly downstream/physical in this condition.

For the missing-fundamental blocks, the actual 60/75/100-Hz component remained
at or near the RAW microphone noise floor. This argues against a hidden block
that synthesizes the missing fundamental itself.

## Fresh Movie / VLLDP-history probe

The Dolby filter generation was recreated at 10% UI, Movie profile, with
VLLDP postgain `-545` (-34.0625 dB), then the same probe was moved live to 25%
after graph wake.

Evidence:

- stage log SHA-256 `fa2c16b78cd96e5568f1d07ec54444b97d8be3e7d34dfc71e71fd7b975eef97c`
- post-Dolby SHA-256 `79719aca97ff3de454d2182b4116af305f6647fcbda2aa3c701d620c4443d894`
- post-Dolby analysis SHA-256 `c1ce6127b821322710be585dda685354acfe993834e70161f6e0a7b110ee958d`
- SP7 RAW SHA-256 `510f68764c87c55b86e550fba5475486639a9b28d41f0aaa5b6bc8389f14693d`
- SP7 analysis SHA-256 `de6c8a65e74660f9bdb4acb2f14ddc05d938014091c331933ac16910add11762`

The two cycles do not maintain one fixed LF amplitude: several fundamentals
rise materially during the sequence. This is consistent with the already
observed long-memory VLLDP/VR dynamics and means Windows/Linux bass parity must
standardize APO/Dolby generation history, not merely endpoint volume and a
single frequency response snapshot.

## Current mechanism model

For ordinary Golden v31 internal-speaker playback, the most evidence-consistent
model is:

`Dolby/VLLDP dynamics + Windows GainStep MSIIR loudness contour`
`-> strong LF-region drive into protected speaker graph`
`-> natural downstream speaker/amp harmonics`
`-> perceived bass/fullness beyond the tiny driver's clean fundamental reach`.

This model is a hypothesis at the final perceptual-combination level, but each
component above is independently evidenced. Do not enable synthetic virtual
bass as a parity change without new native-Windows runtime evidence.

## Next oracle

Run the same hash-identical probe on native Windows with SP7 WASAPI RAW and a
standardized Windows APO state. Compare fundamental transfer and 2x/3x harmonic
ratios at the same fixed geometry. Windows boot is required for that comparison.
