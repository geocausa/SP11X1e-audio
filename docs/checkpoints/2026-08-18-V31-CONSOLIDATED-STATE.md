# SP11 built-in speaker audio — v31 consolidated state

Date: 2026-08-18
Branch: `agent/golden-v31-ckv-delta-20260818`

This is the authoritative short-form checkpoint after the Golden-v28
consolidation, v30 mute/transport completion, the 40-Hz volume-transient
investigation, and the v31 prior/new-CKV correction.

## Safety / live state at checkpoint

- Current running one-shot candidate: `7.1.5-sp11-golden-v31-ckv-delta`.
- Persistent GRUB saved entry: `sp11-audio-golden-v28`.
- `next_entry` is empty; an ordinary reboot returns to Golden v28.
- Visible speaker endpoint was restored to 6%.
- `filter-chain.service` and `sp11-dolby-volume-sync.service` are active.
- Physical PCM is closed at idle.
- No deliberate test playback/recorder/key-injector job remains alive.
- No promotion to `main` or saved-default change has been made.

## Proven playback stack

### Golden v28 baseline

Golden v28 remains the published/saved daily-driver reference. It closes the
CSR-off broadband static, carries the recovered Windows WSA8845 lifecycle,
protected graph, Movie Dolby path, endpoint taper/final-volume control,
volume-dependent GainStep/MSIIR calibration, soft-pause behavior and the
DP2/COMP `OffsetCtrl2=0x07` causal fix. Its deterministic physical seek gate
passed and the operator reported coherent music, excellent leveler behavior
and no prior forward/reverse loudness spike.

### v30 deltas inherited by v31

v30 added and live-proved:

- exact Windows final endpoint mute at VOL_CTRL `0x4a63`, PID `0x08001039`;
- successful exact-DSP mute/unmute without redundant physical-sink mute on the
  success path; hardware mute remains fail-closed fallback;
- DP1/DAC `BlockCtrl3=0x00`;
- retained Golden DP2/COMP `OffsetCtrl2=0x07`;
- DP3/BOOST `OffsetCtrl2=0x1f`;
- exact resident WSA 10-write START + 6-write STOP behavior after idle, without
  replaying the 63-write cold initialization.

### v31 prior/new GainStep CKV semantics

The 40-Hz torture test exposed a real Windows/Linux semantic mismatch rather
than an electrical or Dolby-PCM defect. v30 resent the selected GainStep/MSIIR
OOB calibration on both per-channel final-volume calls, including when the
GainStep CKV had not changed. The recovered Qualcomm GSL/ACDB path uses
prior-CKV -> new-CKV changed-key delta semantics instead.

v31 adds one fixed final-volume-only kernel control and applies:

- same GainStep: `vol -> vol`;
- upward GainStep boundary: `cal -> vol`;
- downward GainStep boundary: `vol -> cal`;
- unknown/new graph: one complete baseline calibration.

No Dolby coefficient, PA/WSA policy, SoundWire field, EQ or volume-ramp tuple
was changed for this correction.

Fixed-geometry 40-Hz physical results:

- v30 real-key UP HP500 p95: `2.7855e-3`;
- v31 pass 1 UP HP500 p95: `6.6466e-5`, UP/DOWN `0.975x`;
- v31 independent repeat: `6.4095e-5`, UP/DOWN `0.945x`;
- native Windows UP HP500 p95: `6.1937e-5`, UP/DOWN `1.006x`.

The pathological v30 Volume-Up transient is therefore objectively closed on
v31 to the native-Windows/room-floor class.

## Other objective v31 gates

- Exact q6apm control transition shapes (`vol->vol`, `cal->vol`, `vol->cal`)
  were observed live with no runtime transaction failure.
- The exact deterministic Seven Nation Army seek smoke retained the v28
  physical no-unique-needle result.
- Exact DSP mute remains inherited from v30.
- WSA8845 / WSA macro / SoundWire identities and transport behavior remain
  unchanged from the proven v30 stack.
- Windows media-key volume step is 2%; GNOME was changed from 6% to 2% and a
  matched 12<->14% physical stress comparison became Windows-class.
- GNOME `audio-volume-change.oga` is present at idle but suppressed while media
  is continuously RUNNING, matching the observed Windows policy.

## CPS / protection checklist status

P10 is now GREEN at the effective HLOS boundary for this Windows driver build.
The reviewed qcadcm build never transported public
`PARAM_ID_CPS_LPASS_HW_INTF_CFG (0x08001259)` through the searched SET_CFG/OOB
paths. Instead qcaucd classifies the WSA8845 SoundWire identity, selects its
image-backed selector-5 templates, and applies the complete effective CPS DP6
geometry: shared physical master port 13, both slave DP6 masks `0x03`, 24 kHz /
800 clocks, left/right OffsetCtrl1 `0/25`, SampleCtrl `0x1f/0x03`, HCtrl
`0xff`, BlockCtrl1 `0x18`, BlockCtrl3 `0x00`. Linux CPS-v3 implements those
effective values through normal SoundWire/ASoC paths.

P09 remains AMBER only for dynamic/calibrated limiter telemetry observability.
It is proven non-blocking for speaker rendering and is not a known audible gap.

## Acoustic calibration methodology correction

The SP7 microphone is the external acoustic oracle; the SP11 microphone path is
not part of this investigation.

The older SP7 recorder used the Windows microphone-array endpoint in WASAPI
shared mode. Separate shared-mode captures can move by several dB due to the
capture engine/APO path, so old absolute Windows-vs-Linux L/R dB conclusions
are now **provisional** and must not drive speaker tuning.

A tracked WASAPI-RAW recorder is now available at:

`tools/windows/Record-ExternalMic-Raw.ps1`

The exact tracked script was fetched from Git on SP7 and successfully produced
2-channel / 48-kHz / float32 RAW recordings. Repeated RAW Linux captures are
far more stable. Within-one-capture transient tests such as the 40-Hz UP/DOWN
gate remain valid because they compare events under one microphone state.

### Important calibration caveats discovered after RAW conversion

Absolute L/R/bass calibration is **not yet closed**. Several exploratory
calibration passes intentionally remain excluded from parity conclusions:

1. Fresh Dolby generations and long-lived generations can have very different
   VLLDP adaptive state. Movie profile and VLLDP bootstrap state must therefore
   be standardized before cross-OS comparison.
2. Some attempted 25%/50% calibration passes did not actually deliver the
   requested target into q6apm before playback; their equal physical levels are
   a harness/control-state result, not a speaker response result.
3. A same-WAV 997-Hz RAW linearity check proved the SP7 RAW recorder does
   respond to endpoint level, so RAW capture is not secretly normalizing
   amplitude.
4. Low-frequency single-microphone bins remain room/noise sensitive and require
   repetition/robust statistics. Isolated one-off impulses are rejected because
   neighbours/environment can contaminate a capture.

The correct next physical parity campaign therefore needs both **RAW SP7
capture** and a **defined fresh Movie/VLLDP start state**, with explicit live
readback that the intended q6apm endpoint level has landed before playback.

## What remains before v31 promotion

### Required operator gate

The operator still needs to manually listen to v31 when back at the devices:

- repeat the 40-Hz Volume-Up/Down torture;
- normal music/YouTube;
- ordinary and hammered 2% volume keys;
- exact mute/unmute;
- forward/back seek;
- overall Golden-v28 coherence, leveler behavior and bass impression.

No automated measurement should substitute for that final daily-driver verdict.

### Physical parity campaign after the listening gate

Once v31 is accepted subjectively, obtain a matched **Windows RAW vs Linux RAW**
reference from the fixed geometry (SP7 centred/square-on, one attached SP11
keyboard length from the SP11), with standardized Dolby/APO start state. Split
this into two questions rather than forcing one stimulus to do everything:

- high-SNR left/right transfer/channel fingerprint;
- low-volume low-bass / psychoacoustic-bass parity.

Do not reuse the superseded shared-mode absolute L/R numbers as tuning targets.

## Explicitly non-blocking / separate

- `W02`: dedicated Windows WASAPI-loopback branch residual, not a physical
  speaker gate.
- `P09`: protection telemetry observability, not a known actuator gap.
- `S04`: old dirty everyday audio tree; never use as production provenance.
- clean pristine-upstream kernel packaging: still separate from sound-quality
  closure because the historical Phase91 platform baseline is not normalized
  into a clean public patch series.
- suspend/resume, microphone/input and Bluetooth are outside the current
  built-in-speaker completion gate.

## Promotion policy

Until the operator passes the listening gate:

- keep Golden v28 as `saved_entry`;
- keep CPS-v3 as rescue;
- keep v31 isolated on `agent/golden-v31-ckv-delta-20260818`;
- do not fast-forward `main` to v31;
- do not delete the v28 rollback assets.

If the operator verdict is GREEN, the next controlled step is to promote v31 as
the named Golden/default, update `main`/README/manifests, and reduce active audio
boot choices to the promoted Golden plus v28 comparison/rollback and CPS-v3
rescue.
