# SP11 Audio Golden v31 CKV-delta candidate

v31 is an isolated follow-up to validated v30.  It changes only the runtime
semantics of final endpoint volume when the Windows GainStep CKV does not
change.

Recovered Qualcomm GSL runtime calibration carries both prior and new CKVs.
`AcdbComputeDeltaCKV()` emits only changed keys, so the GainStep-dependent
`0x489e` calibration group is absent when GainStep key `0x01000011` remains
unchanged.  v30 over-applies the selected GainStep group on every channel call.

v31 keeps the existing 288-byte `SP11 Windows Volume Transaction` unchanged and
adds fixed-target `SP11 Windows Volume Only`, carrying exactly left/right Q28.
Userspace uses prior/new semantics:

- same GainStep: volume-only -> volume-only;
- UP row change: combined calibration -> volume-only;
- DOWN row change: volume-only -> combined calibration;
- unknown/initial graph handover: one combined baseline row.

No Dolby, WSA8845, SoundWire, PA, ramp-policy, endpoint taper, mute, DP1/DP2/DP3,
or protection calibration is changed.  Kernel and DTB are byte-identical to
v30; only q6apm differs in the candidate initrd.

The installer only registers `sp11-audio-golden-v31-ckv-delta-candidate` and
runs `update-grub`.  It never changes `saved_entry` and never reboots.
