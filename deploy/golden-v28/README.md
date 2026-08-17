# SP11 Audio GOLDEN v28

This directory defines the current recommended built-in-speaker baseline for the
Microsoft Surface Pro 11 (X1E80100). It is the state that closed the confirmed
broadband-static defect, passed the objective external-microphone seek gate and
received the user's best-to-date Linux listening verdict on 2026-08-17.

## What Golden v28 contains

- Linux `7.1.5-sp11-render-parity-v4+`.
- Recovered Windows AudioReach protected-render topology and control links.
- Windows endpoint taper, final `VOL_CTRL` Q28 and volume-dependent GainStep/MSIIR.
- Windows SOFT_PAUSE lifecycle and pause-drain handling.
- LPASS WSA producer at the Windows-proven 0 dB operating point.
- Exact recovered WSA8845 cold/START/STOP lifecycle with resident clock-stop retention.
- WSA8845 DP2/COMP `OffsetCtrl2=0x07`, which closes the CSR-off broadband static.
- Original matching SP11 Dolby VR/VLLDP code hosted by the Linux bridge, Movie profile.
- Downstream fail-safe propagation of the desktop mute state. The exact Windows
  runtime DSP mute parameter `0x4a63/0x08001039` remains a separate parity task.

The exact boot/runtime identities are in `manifest.json`; verify a deployed
machine with `./verify-golden-v28.sh`.

## Reproduction boundary

The repository contains the reviewed Linux integration, patch history, topology
builders, UCM/PipeWire policy, Dolby host source, tests and immutable hashes.
It intentionally does **not** redistribute Microsoft/Qualcomm calibration dumps,
private firmware or Dolby DLLs. `deploy/dolby/build-production.sh` requires the
user's own matching DLLs and refuses hash mismatches.

The kernel lineage starts from official Linux 7.1.5 plus the SP11 Phase91
platform port. The current historical working source directory is not a Git
checkout, so a pristine-upstream one-command kernel rebuild is **not yet claimed**.
Until that platform base is normalized into a clean public patch series,
`manifest.json` is the bit-identical deployment oracle and `patches/` is the
reviewed integration/audit trail.

That limitation is deliberate: this project prefers an honest reproducibility
boundary over claiming a build recipe that has not been independently replayed.

## Registering an already-built Golden image

After placing boot artifacts at the exact directory/path in `manifest.json`:

```bash
./deploy/golden-v28/verify-golden-v28.sh
sudo ./deploy/golden-v28/install-grub-entry.sh
```

The installer verifies hashes, creates GRUB ID `sp11-audio-golden-v28`, runs
`update-grub`, and selects Golden v28 as the saved default. It **does not reboot**.

Keep `sp11-audio-cps-v3` as the rescue entry. v29 DP3-OffsetCtrl2 is a structural
comparison candidate only and is not promoted over Golden v28.
