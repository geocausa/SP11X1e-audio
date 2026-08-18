# Golden v31 CKV-delta candidate — preboot staging

Date: 2026-08-18
Status: **built, signed, initrd-isolated, preboot verified; live gate pending**

## Causal reason

A fixed-geometry SP7 external-microphone campaign proved that native Windows is
floor-clean during the exact warm 40 Hz 2%-step volume torture while Linux v30
produces a large Volume-Up transient.  The decisive Linux direct-q6apm test held
the visible PipeWire endpoint fixed and reproduced an even larger transient
after four full 6->46->6 exact channel-ordered volume histories.  PipeWire,
GNOME event sounds and Dolby PCM are therefore not required for the core fault.

Recovered Qualcomm GSL/ACDB semantics use prior/new CKVs.  The non-persistent
calibration delta contains only changed keys; the GainStep-dependent `0x489e`
group is absent when key `0x01000011` did not change.  v30 instead re-sends the
selected GainStep group on every channel call.

## v31 delta

v31 adds one fixed q6apm control:

`SP11 Windows Volume Only`

It carries exactly two Q28 words (left/right, 16-byte ALSA TLV capacity) and can
only call the existing fixed final-VOL_CTRL helper on a running protected graph
under the existing volume transaction mutex.

The existing 288-byte combined transaction remains ABI-identical.  Userspace
selects:

- same GainStep: volume-only -> volume-only;
- UP row change: combined -> volume-only;
- DOWN row change: volume-only -> combined;
- initial unknown prior CKV: one combined baseline.

The capability is enabled only when the named control exists and reports
exactly 16 bytes.  v30 rollback was live-tested and reports
`ckv_delta=legacy-resend`.

## Build / package proof

- strict checkpatch: 0 errors, 0 warnings, 0 checks;
- focused local tests: 60 passed + 6 subtests;
- q6apm ABI: `7.1.5-sp11-render-parity-v4+`;
- q6apm srcversion: `687B16CF9C43B43E90C0746`;
- signed q6apm SHA-256:
  `965fb53b21feca3feed605cd172d6cb2680463311526478fbdb39a2b35cda84e`;
- module signer/key exactly matches v30;
- v31 initrd SHA-256:
  `7bf757419e4451fb0967ae535eae8d73416b0793c975a4505738a475ed66c608`;
- v31 initrd vs v30: zero non-q6apm file-hash diff lines, zero symlink diff lines;
- kernel and DTB are byte-identical to v30.

Persistent saved default must remain `sp11-audio-golden-v28` until the v31
physical gate and user listening gate pass.
