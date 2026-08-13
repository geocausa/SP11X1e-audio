# PipeWire Dolby pause-drain closure — 2026-08-13

## Result

The one-shot fragment of paused YouTube audio heard when GNOME's volume-preview
notification woke the speaker route was localized and fixed. It was not a
Firefox/Pulse replay, an AudioReach ring-buffer replay, or the 64-frame
AudioEng limiter alone. PipeWire froze the recovered VR -> VLLDP -> AudioEng
chain with 1,776 frames (37 ms at 48 kHz) of already-rendered media still in
its algorithmic delay. The next notification resumed that same filter and
therefore emitted the old media before the notification reached its output.

The deployed bridge now consumes exactly those 1,776 frames with zero input
when PipeWire calls LADSPA `activate()` at its PAUSED/reset boundary. The drain
output is discarded. The original Dolby cores remain instantiated, so their
minutes-long Leveler/regulator history and in-place profile state are retained.

No kernel change or reboot was required. The user repeated the exact physical
test after live deployment and reported that the fragment **is gone**.

## Pre-fix localization

Initial PipeWire and kernel lifecycle capture:

```text
/home/geoca/Documents/SP11-AUDIO-AUDIT/pause-slider-state-trace-20260813-221210
```

During the real GNOME volume-preview event:

- Firefox remained `pulse.corked=true`;
- its links to the Dolby sink remained PAUSED;
- Firefox did not transition back to RUNNING;
- the Mutter `audio-volume-change` stream alone activated;
- ALSA used STOP/START, not PAUSE_PUSH/PAUSE_RELEASE;
- the booted SOFT_PAUSE implementation was therefore not invoked.

The first input-only Dolby uprobe established that the notification wake began
with digital-zero input followed by the Yaru notification waveform. That ruled
out a fresh Firefox/PipeWire media replay at the plug-in input.

The decisive input/output uprobe then showed the first notification-wake input
block was all zero while the Dolby output was immediately nonzero old media.
The retained output occupied the chain's previously measured 1,776-frame
input-to-output latency. A 64-frame-only AudioEng limiter explanation was ruled
out because the entire first 256-frame output block, including samples beyond
index 64, contained retained signal.

The symptom's one-shot nature matched this result: the first wake consumed the
frozen delay, so a second slider movement had nothing left to replay until
media was played and paused again.

## Windows processing-mode nuance

The user's observation that Windows separates NOTIFICATION from ordinary media
is correct at the AudioReach boundary. Recovered Windows graphs select distinct
DEFAULT (`GKV 2`) and NOTIFICATION (`GKV 7`) render families before joining the
same root speaker/protection graph.

That distinction is not, by itself, the source of this particular userspace
replay. KD evidence also proves both stimuli execute through the persistent
Dolby VR and VLLDP cores in the same `audiodg.exe` lifetime, although identical
coefficients/state across modes are not claimed. The stale fragment was already
present at the Linux Dolby output, upstream of the downstream AudioReach family
selection.

Windows' APO contract exposes each APO's processing delay through
`IAudioProcessingObject::GetLatency`, and the host calculates total stream
latency by summing the chain. The shipped Dolby `Reset()` implementations are
no-ops, so cold-rebuilding those cores on every Linux pause would remain wrong.
The Linux correction therefore handles the missing latency-consumption boundary
without inventing a second Dolby instance or discarding adaptive state.

Reference:

- <https://learn.microsoft.com/en-us/windows/win32/api/audioenginebaseapo/nf-audioenginebaseapo-iaudioprocessingobject-getlatency>

## Implementation

Production source:

```text
dolby-port/sp11_dolby_windows_chain_ladspa.c
```

The bridge now tracks whether processing occurred since the last pause drain.
On a ready instance's repeated `activate()` callback it:

1. temporarily connects zero input and discard output buffers;
2. processes exactly `SP11_CHAIN_DELAY_FRAMES=1776` frames through the original
   VR -> VLLDP -> AudioEng chain;
3. restores PipeWire's connected port pointers;
4. leaves both Dolby objects allocated and all long-term state intact;
5. skips work for initial or repeated activation with no intervening run.

The implementation deliberately does not clear arbitrary PE-memory ranges,
reconstruct a Dolby core, map ALSA STOP to SOFT_PAUSE, or create an unproven
notification-only Dolby instance.

## Regression evidence

`sp11_dolby_windows_chain_lifecycle_test.c` warms two original-code instances
identically for 70 seconds. Reference A receives an explicit 1,776-frame zero
drain; B receives only the PipeWire-style `activate()` callback. Their wake and
three-second probe outputs are bit-identical:

```text
warm_frames=3360000 warm_seconds=70.000
reference_hash=d8af2eb3d50cddec
pause_callback_hash=d8af2eb3d50cddec
diff_samples=0 max_abs_diff=0
pause_drain_frames=1776
wake_diff_samples=0 wake_peak=2.15959644e-05
LIFECYCLE_RESULT PASS
```

The residual silent-wake peak is about -93.3 dBFS. Profile lifecycle, runtime
postgain control, all exact AudioEng limiter chunk patterns, and C/Python oracle
parity also pass.

Candidate SHA-256:

```text
ee02ff299146b0ed8387fda1da820a8ed7c9612fc4a5946ed921e5c0dca715d9
```

## Live deployment and verification

Installed plug-in:

```text
/home/geoca/.local/lib/sp11-dolby/sp11_dolby_windows_chain.so
```

Rollback copy:

```text
/home/geoca/.local/lib/sp11-dolby/sp11_dolby_windows_chain.so.pre-pause-drain-20260813
SHA-256 087740f6c4c3a2b2411b100eb0e005f7fe0cf59147fcb734ecfb0d5d54882f9c
```

The dedicated restart preserved Movie, GEQ-off, endpoint postgain/volume,
volume-sync and the Dolby default sink. `filter-chain.service` returned active
with `NRestarts=0`.

Post-fix physical/uProbe capture:

```text
/home/geoca/Documents/SP11-AUDIO-AUDIT/pause-drain-live-20260813-222836/input-output.trace
```

At lines 4648-4650, PipeWire's pause callback runs the new 1,776-frame zero
drain and discards the old-media output. At lines 4651-4654, the later
notification wake starts with zero/tiny-decay output rather than old music,
followed by the notification input. After the notification, lines 4683-4685
show the same drain boundary preparing the next wake.

The user's physical verdict after this captured run: **the old-media fragment
is gone**.
