# Windows WSA VBAT/BCL isolation on RPV4/RX84

Date: 2026-08-16  
Status: **REJECTED as a standalone acoustic parity improvement; structurally validated and safe in the bounded test**

## Question

The passive native `qcaucd` trace proved that Windows runs the legacy Qualcomm WSA VBAT/BCL producer lifecycle, including the v2.5 CB_DECODE extension, while current upstream Linux removed that DAPM stage. Does restoring only that missing lifecycle improve the current best RPV4/RX84 Windows acoustic residual?

## Candidate

One-shot GRUB entry `sp11-audio-rpv4-macro84-vbatbcl-v1` retained the known-safe render-parity-v4 stack, RX84 capability, generic Linux compander curve, current M1P5 half-dB policy, speaker protection, and CSR-assisted WSA8845 lifecycle. It changed only the producer-side VBAT/BCL stage:

- VBAT path/config `0x0180/0x0184` lifecycle;
- RX0/RX1 `RX_PATH_CFG1` bits `0x80` and `0x02`;
- existing softclip CRC/mux clock dependency without enabling the softclip effect;
- BCL gain bytes `0x01dc..0x01fc = ff 03 00 ff 03 00 ff 03 00` on bring-up and zero on teardown;
- v2.5 CB_DECODE `0x0900/0x0904/0x0908` runtime writes, with teardown order `0908 -> 0904 -> 0900` before the legacy VBAT teardown.

The device-tree WSA resource is already 0x1000 bytes. The candidate only raised the driver regmap ceiling from `0x0760` to the minimum needed `0x0908`, whitelisted the proven registers, and marked all newly introduced producer registers volatile so regcache cannot replay lifecycle/pulse state. The DAPM graph inserts a separate VBAT event between each interpolator and speaker chain, preserving the old Qualcomm PRE_PMU / POST_PMD lifecycle ordering. Bounded `SP11VBAT` log markers contain no hardware reads.

The signed WSA module loaded as srcversion `670409B04CD7E0213A44E2D`. Its compressed SHA-256 is `B8880BF8F024C999C1083A45CD093AC3F37E61D6209F997EEFF7EECBEA0783C1`. The initramfs was unpacked before boot and byte-verified to contain this WSA module plus the exact previously proven RX84 X1E module. Persistent GRUB fallback remained `sp11-audio-cps-v3`; the one-shot cleared after boot.

## Runtime / safety result

Before user stimulus, the new DAPM callback exercised both physical speaker paths symmetrically during graph lifecycle. The protected AudioReach graph, VI and CPS feedback, and SoundWire DAC+COMP+BOOST transport remained healthy. The known Qualcomm `AR_EUNSUPPORTED` graph-calibration reply remained handled exactly as before and is not a candidate regression.

At endpoint `0.12` with both WSA RX controls explicitly pinned to `84 / 0 dB`, three deterministic chirps completed with playback return code zero. No new WSA/PA fault, SoundWire error, XRUN, underrun/overrun, kernel fault, or candidate-attributable DSP failure was observed. DRE/CSR state was not changed.

## Acoustic result

The exact synchronized oracle stimulus was reused:

`chirp-40-16000Hz-24dBFS.wav`, 48 kHz stereo PCM16, SHA-256 `C8782C7741B3ECE628362E785D2EC91B990EF453BEFA4A3E7D6C3E1CD1F8A208`.

The same SP7 external-mic geometry and ridge extractor were retained: channel 1, Hann STFT 8192, hop 1024, nearest exponential-chirp ridge, mean frames within +/-1/24 octave, normalize by the 1--5 kHz median.

The saved RX84 generic-producer baseline reproduces as:

- 1--5 kHz: **0.182 dB MAE / 0.208 dB RMSE**;
- 630 Hz--6.3 kHz: **0.184 / 0.214 dB**.

The three-run VBAT/BCL median measures:

- 1--5 kHz: **0.529 dB MAE / 0.567 dB RMSE**;
- 630 Hz--6.3 kHz: **0.482 / 0.535 dB**.

All eight 1--5 kHz bins and all eleven 630 Hz--6.3 kHz bins meet the existing <=2 dB run-spread criterion. Individual 1--5 kHz runs were approximately `0.426/0.513`, `0.608/0.689`, and `0.640/0.691` MAE/RMSE. This is a clear degradation, not a borderline ranking.

SP7 capture hashes are recorded in `artifacts/reviewed/2026-08-16-rpv4-macro84-vbatbcl-v1-result.json`; the large WAVs remain on SP7 rather than in Git.

## Decision and interpretation

**Do not promote standalone VBAT/BCL.** The experiment proves that the missing Windows VBAT/BCL lifecycle can be restored safely and reproducibly, but placing it under the generic Linux compander/M1P5 producer state makes the acoustic match materially worse.

This negative is consistent with the two earlier controlled negatives:

1. the directly recovered Surface compander curve alone was worse than RX84 generic baseline;
2. the Windows curve + primary half-dB-off pair recovered substantially but was still worse than the generic baseline.

Windows directly proves all three producer properties simultaneously: Surface curve, primary half-dB off, and VBAT/BCL/CB_DECODE. The next bounded candidate should therefore test that **complete Windows-proven producer combination** on RX84 while preserving the safe CSR-assisted WSA8845 lifecycle. It must not change DRE/CSR yet.

Candidate source delta is retained as `artifacts/reviewed/2026-08-16-rpv4-macro84-vbatbcl-v1.patch`.
