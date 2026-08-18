# SP11 Audio GOLDEN v31 — Windows parity daily driver

Golden v31 is the promoted SP11 built-in-speaker daily driver as of 2026-08-18.
It inherits Golden v28, v30 exact endpoint mute and DP1/DP3 transport completion,
and adds the prior/new GainStep CKV semantics that close the pathological 40-Hz
Volume-Up transient.

## What v31 adds over v28

- exact final VOL_CTRL mute `0x4a63 / 0x08001039`;
- DP1/DAC `BlockCtrl3=0x00`;
- retained DP2/COMP `OffsetCtrl2=0x07`;
- DP3/BOOST `OffsetCtrl2=0x1f`;
- Windows prior-CKV -> new-CKV changed-key semantics for GainStep/MSIIR runtime calibration;
- fixed final-volume-only q6apm control used when GainStep does not change;
- Windows-style 2% desktop media-key volume step;
- hardware mute only as fail-closed fallback after successful exact DSP mute.

The fixed-geometry 40-Hz external-mic gate fell from v30 `2.7855e-3` HP500 p95
to `6.6466e-5` and `6.4095e-5` on two independent v31 runs. Native Windows
measured `6.1937e-5`. Deterministic seek closure also remains GREEN.

## Rollback

Keep:

- `sp11-audio-golden-v28` as the known Golden rollback/comparison entry;
- `sp11-audio-cps-v3` as the conservative rescue entry.

The installer verifies hashes, registers the canonical `sp11-audio-golden-v31`
entry and sets it as the saved default. It **does not reboot**.

```bash
sudo ./deploy/golden-v31/install-grub-entry.sh
./deploy/golden-v31/verify-golden-v31.sh
```

For physical Windows/Linux L/R or bass calibration use the tracked SP7
WASAPI-RAW recorder and the fixed keyboard-length geometry. Older shared-mode
absolute L/R figures are provisional and must not drive tuning.
