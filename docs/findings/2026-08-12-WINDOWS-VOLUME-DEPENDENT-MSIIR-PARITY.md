# Windows volume-dependent MSIIR parity — 2026-08-12

Fresh qcadcm reverse engineering and REV_0D ACDB decode closed the missing
low-volume speaker-calibration policy.

- qcadcm `GetGainTableStepFrmQ28Gain` selects the nearest Q28 endpoint-gain row.
- `GetGraphCkv` maps internal index 0..29 to GainStep 1..30.
- internal speaker endpoint `0x01000006=1` has mute then -21..0 dB anchors in
  0.75-dB steps.
- all 30 `0x489e/0x08001022` rows are distinct; the other three parameters in
  the 216-byte volume group are constant.
- CKV 3, 9 and 24 carry 152-byte coefficient payloads; the other rows carry 96.
- every one of the 30 exact reviewed payloads was accepted live by the DSP.
- graph close/reopen proved the kernel startup still loads CKV30, after which
  the enabled userspace service immediately reapplies the selected row.
- live silent 25% -> 50% -> 25% exercise produced CKV1 -> CKV6 -> CKV1 with
  Dolby postgain and MSIIR following the same endpoint-dB source.

The low-volume rows are emphatically not flat. Using the raw Windows
coefficient convention, CKV2 is approximately +10.8 dB at 60 Hz and +8.7 dB at
100 Hz while the ~1 kHz region is about -6 dB relative to CKV30 unity. This is
an evidence-backed Windows loudness contour and a direct explanation for why
fixed CKV30 Linux playback sounded thin at ordinary volume.

Remaining volume blocker: the Linux PipeWire/WirePlumber UI taper itself does
not match Windows. At nominal 25%, Linux currently yields about -36.124 dB
while fresh Windows COM measurement yields about -20.747 dB. That is tracked
separately as V01 in the render parity ledger.
