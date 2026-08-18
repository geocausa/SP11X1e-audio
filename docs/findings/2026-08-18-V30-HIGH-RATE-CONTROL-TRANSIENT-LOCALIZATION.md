# Golden v30 high-rate volume/mute micro-transient localization — 2026-08-18

## Scope

After Golden v30 passed normal listening, exact DSP mute, SoundWire transport,
retention and physical-static gates, the user found a very small intermittent
crackle only by aggressively repeating volume/mute controls during non-zero
media playback. Ordinary mouse-slider use rarely reproduces it.

This finding intentionally does **not** reopen the closed broadband-static gate.
The artifact is a short high-rate control-transition edge and requires non-zero
program material.

All physical captures below use the **SP7 microphone externally recording the
SP11 speakers**. The SP11 capture/microphone path is not used.

## 1. GNOME notification-sound hypothesis

A real hardware-media-key path was generated with `ydotool`, not `wpctl`.

At idle, one Volume-Up key creates a real libcanberra notification stream:

- `event.id = audio-volume-change`
- `media.filename = /usr/share/sounds/Yaru/stereo/audio-volume-change.oga`
- `media.role = Notification`
- `target.object = 45` (`effect_input.sp11_windows_dolby`)
- the notification node transitions to RUNNING.

Idle PipeWire-monitor SHA-256:
`154725f5e64f037b90b9ad3e751f1a3bb01cd25b30a33c6c52dd24911c3aa13f`.

With continuous 997-Hz media already RUNNING, one real Volume-Up plus
Mute/Unmute produced **zero** `audio-volume-change` event nodes. The same remained
true during 24 rapid real Volume-Up/Down keys plus 20 rapid real Mute keys while
the media stream remained RUNNING.

Active monitor SHA-256:
`14aab462228fc47dcb76edf4d73d5049a18ba31f4e2f3c5b7458dd6db076ffc9`.

Active-hammer monitor SHA-256:
`adea05e100253c5fe0e4249c3725d8f90dbbf6ae7bc77e5a0efd266055a1e259`.

Thus GNOME already implements the same effective policy the user observes on
Windows: notification feedback is present at idle but suppressed while media is
actively playing. A notification leak is not the reproduced continuous-playback
micro-transient. An application that actually corks/pauses remains a separate
case and is already covered by the earlier pause-drain/notification-wake work.

## 2. Active digital-zero control

A 30-second stereo 48-kHz digital-zero stream kept the complete speaker graph
RUNNING while real Volume-Up/Down and Mute keys were hammered.

SP7 external capture SHA-256:
`2D2834B2DD43AD6EDFCB978F541FF4A4D08379D7AF8CB9FA8814553FEF0F6BE2`.

Representative 100-ms/0.5-s first-difference physical metrics stayed at the
room-floor class:

- pre-control median diff-RMS: `1.7689e-5`
- volume-key window median diff-RMS: `1.7785e-5`
- mute-key window median diff-RMS: `1.7814e-5`

No unique high-frequency impulse appeared in either control window. Therefore
this is not a naked PA/WSA/SoundWire/control-write click. The audible edge needs
non-zero program samples.

## 3. Exact DSP mute versus downstream hardware-mute backstop

The current v30 policy sends exact Windows final `VOL_CTRL` mute
`0x4a63/0x08001039` and, after a successful DSP transaction, also mirrors the
mute bit into the downstream PipeWire hardware sink as a safety backstop.

A synchronized same-tone A/B used 40 rapid mute edges:

### A — current v30, DSP mute + downstream hardware mirror

SP7 SHA-256:
`0A9FDDDC22A6E8C951626E42EE411173D63B6A41274BEB2E8175AF925A706F57`.

At detected 997-Hz mute edges:

- >6-kHz peak median: `4.054e-5`
- >6-kHz peak p95: `1.759e-4`
- >10-kHz peak p95: `1.351e-4`

### B — exact DSP mute only on successful transactions

The physical PipeWire sink stayed open at unity through all 40 mute edges.
Failure handling remained fail-closed.

SP7 SHA-256:
`56951A9DF6EF7D2E69F1C363B389D0FE6A2EC26BF82B575CFEFE98A4E54D6B4A`.

- >6-kHz peak median: `3.966e-5`
- >6-kHz peak p95: `1.051e-4`
- >10-kHz peak p95: `8.091e-5`

The median edge is essentially unchanged, but the rare sharp p95 tail falls by
about 40% when successful Windows DSP mute is no longer followed by a second
physical-sink mute actuator. This matches the user's description of one tiny
intermittent "slip" under pathological hammering.

**Disposition:** on a kernel with proven `0x08001039`, successful DSP mute should
be the sole normal actuator. Retain downstream hardware mute only for DSP failure
(fail-closed) and for rollback kernels that do not expose exact DSP mute.

## 4. GainStep/MSIIR row boundary is not the volume-specific trigger

A single SP7 recording compared rapid real media-key changes on a 997-Hz tone:

- `6% <-> 12%`: both endpoints select recovered GainStep row 1;
- `12% <-> 18%`: crosses GainStep row 1 <-> row 2.

SP7 SHA-256:
`5DF089508AC7A0199A91ED90231F1AC1224313CF55DFBA57C3F11FE0C2B8A4C4`.

Detected transition metrics:

| window | >6-kHz peak median | >6-kHz peak p95 | >10-kHz peak p95 |
|---|---:|---:|---:|
| same row 1 | `2.647e-5` | `3.601e-5` | `2.684e-5` |
| row 1 <-> 2 | `2.675e-5` | `2.947e-5` | `2.345e-5` |

Crossing the dependent MSIIR/GainStep row does not worsen the transient; the
rare tail is actually lower in this run. The remaining volume-side suspect is
the final endpoint gain transition/timing itself rather than the low-volume bass
calibration row change.

## Current localization

The reproduced evidence supports:

1. **not** the old broadband/static defect;
2. **not** a control-only amp/WSA/SoundWire click;
3. **not** GNOME `audio-volume-change.oga` leaking during continuously active media;
4. **not** specifically a GainStep/MSIIR-row change;
5. successful DSP mute plus the extra physical-sink safety mirror measurably
   worsens rare mute-edge transients;
6. the remaining volume artifact requires non-zero samples and is localized to
   high-rate endpoint-gain transition/scheduling semantics.

## Next discriminator

Use native Windows on the same SP11 as a physical stress oracle with the same
997-Hz source, real volume/mute media keys and SP7 external microphone. Compare
high-rate edge statistics and, if needed, capture qcadcm timing to determine
whether Windows coalesces/serializes rapid endpoint changes differently from the
current Linux userspace transaction dispatcher.
