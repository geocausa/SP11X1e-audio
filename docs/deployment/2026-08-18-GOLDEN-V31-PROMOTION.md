# Golden v31 promotion

Date: 2026-08-18

## Decision

The operator explicitly authorized Golden v31 to become the normal SP11 Linux
built-in-speaker boot and to be promoted to repository `main`. Promotion does
not mean the research project is globally complete: matched SP7 WASAPI-RAW
Windows/Linux L/R and psychoacoustic-bass calibration remains open, and the
operator plans further subjective hammer/listening checks later.

## Promoted identity

- GRUB ID: `sp11-audio-golden-v31`
- menu label: `SP11 Audio GOLDEN v31 — Windows parity daily driver`
- boot marker: `sp11_entry=7.1.5-sp11-golden-v31-ckv-delta`
- kernel release: `7.1.5-sp11-render-parity-v4+`
- boot directory: `/boot/sp11-7.1.5-audio-golden-v31-ckv-delta`
- canonical deployment package: `deploy/golden-v31/`

Golden v28 remains the rollback/comparison entry and CPS-v3 remains rescue.

## Why v31 was promoted

v31 inherits the validated v30 exact endpoint mute and DP1/DP3 transport state,
and fixes the stateful 40-Hz Volume-Up transient by matching Qualcomm
prior-CKV -> new-CKV changed-key GainStep calibration semantics.

Fixed-geometry SP7 external-microphone results:

- v30 UP HP500 p95: `2.7855e-3`
- v31 pass 1: `6.6466e-5`
- v31 independent repeat: `6.4095e-5`
- native Windows: `6.1937e-5`

v31 also retained the deterministic physical seek closure, exact DSP mute, the
proven WSA lifecycle and DP1/DP2/DP3 transport state.

## Rootfs/module promotion

The running v31 initrd used q6apm srcversion
`687B16CF9C43B43E90C0746` while the root filesystem still contained the old
v28 q6apm. Before promotion the exact signed v31 module was installed at:

`/lib/modules/7.1.5-sp11-render-parity-v4+/kernel/sound/soc/qcom/qdsp6/snd-q6apm.ko.zst`

Promoted compressed SHA-256:

`acf8ba1e6ded43cfac86afa7d12dd30dabd972286e74b021fbb7bddf98955033`

Loaded and on-disk srcversions then matched. The previous rootfs module was
moved into the external promotion archive before cleanup.

## Boot-menu consolidation

Before pruning, superseded v29/v30 boot files and GRUB generators were
SHA-256 inventoried under the external archive:

`/home/geoca/Documents/SP11-PROJECT/02-kernel/archive/20260818-v31-promotion`

Reviewed copies of the inventories are tracked as:

- `artifacts/reviewed/2026-08-18-v31-promotion-superseded-boot-files.sha256`
- `artifacts/reviewed/2026-08-18-v31-promotion-superseded-boot-dirs.txt`
- `artifacts/reviewed/2026-08-18-v31-promotion-archived-files.sha256`

The active SP11 audio boot set is now deliberately only:

1. `sp11-audio-golden-v31` — normal/default;
2. `sp11-audio-golden-v28` — rollback/comparison;
3. `sp11-audio-cps-v3` — conservative rescue.

No reboot was performed during promotion.

## Acoustic-method correction carried forward

The SP7 external microphone remains the physical oracle. Cross-capture absolute
Windows/Linux response work must use the tracked WASAPI-RAW recorder rather
than the older shared-mode capture path. The fixed physical geometry is SP7
centred/square-on to SP11 at one attached-SP11-keyboard length.

Older shared-mode absolute L/R dB figures are provisional and must not be used
as speaker tuning targets. Within-one-recording event comparisons such as the
40-Hz UP/DOWN gate remain valid.

## Remaining work

- operator subjective hammer/listening verification on the promoted v31;
- matched Windows RAW vs Linux RAW L/R transfer with standardized APO/Dolby state;
- low-volume low-bass / psychoacoustic-bass parity;
- P09 protection telemetry observability (non-blocking);
- W02 Windows loopback-only residual research (non-blocking);
- clean public replay of the historical Phase91 kernel platform baseline.
