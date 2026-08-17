# SP11 Audio Golden v30 candidate

This is the isolated post-Golden-v28 parity candidate for the Microsoft Surface
Pro 11 (X1E80100). It deliberately changes only three known Windows semantics:

1. final AudioReach `VOL_CTRL` endpoint mute (`0x4a63/0x08001039`);
2. WSA8845 DP1/DAC SIMPLE `BlockCtrl3` programming; and
3. WSA8845 DP3/BOOST SIMPLE `OffsetCtrl2` programming.

Golden v28's causal DP2/COMP `OffsetCtrl2=0x07` correction is retained unchanged.
No Dolby coefficient, endpoint taper, GainStep/MSIIR, WSA gain, PA state,
63/10/6 lifecycle, protection calibration, or seek policy is changed.

## Validation status — 2026-08-18

Machine-verifiable gates are GREEN on a one-shot v30 boot:

- exact candidate q6apm and WSA8845 modules loaded from the isolated initrd;
- `SP11 Windows Endpoint Mute` exposed as a dedicated 12-byte TLV control;
- direct DSP mute/unmute/mute (`1/0/1`) accepted with `tlv_write rc=0`;
- once the running baseline is established, desktop mute-only transitions invoke
  `audioreach_sp11_set_final_mute()` without re-sending final volume/GainStep;
- live SoundWire writes on both amplifiers and both banks show DP1 BlockCtrl3
  `0x00`, DP2 OffsetCtrl2 `0x07`, and DP3 OffsetCtrl2 `0x1f`;
- after 20 seconds resident idle, a muted-zero wake produced exactly 32 WSA8845
  writes = 2 amps × (10 START + 6 STOP), with no 63-write cold replay;
- SP7 external-microphone muted-zero capture remained at the v28/Windows room
  floor (`1.8148e-5` whole-capture combined diff-RMS).

The **promotion gate is still the user's listening verdict**. Until that verdict,
GRUB saved default must remain `sp11-audio-golden-v28`.

See the live validation finding and reviewed JSON for hashes and measurements.

## Register an already-built candidate

The repository does not ship the generated initrd or vendor/private bytes.
After placing the candidate boot assets at the paths in `manifest.json`:

```bash
./deploy/golden-v30-candidate/verify-v30-candidate.sh
sudo ./deploy/golden-v30-candidate/install-grub-entry.sh
```

The installer creates only `sp11-audio-golden-v30-candidate` and runs
`update-grub`. It **does not** set the saved default and **does not reboot**.
Use `grub-reboot sp11-audio-golden-v30-candidate` for a one-shot test.
