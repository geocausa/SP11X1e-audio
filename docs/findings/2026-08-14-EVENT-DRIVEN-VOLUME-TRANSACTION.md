# Event-driven Windows volume transaction and durable parity ledger

Date: 2026-08-14

## Outcome

The exact Windows endpoint-volume transaction remains unchanged, but its Linux
trigger is now event-driven.  The former implementation took a complete
`pw-dump` snapshot every 100 ms.  GNOME can begin its volume-preview sound as
soon as the visible slider changes, so the preview could run ahead of the DSP
transaction by up to one polling interval.  The new implementation subscribes
to `pw-dump -m` and consumes a complete volume Props event directly.

This is a timing/atomicity correction, not a newly invented acoustic fade.
Windows' recovered final-volume ramp is already present in the topology:

- PID `0x08001037`;
- ramp period `10 ms`;
- step `1000 us`;
- curve `3`;
- final speaker VOL_CTRL IID `0x4a63`;
- runtime gain PID `0x08001038`.

The transaction order remains:

1. Dolby VLLDP postgain request;
2. final DSP VOL_CTRL Q28 gain;
3. the exact four-frame Windows GainStep OOB delta in the same kernel control
   call, after VOL_CTRL;
4. hidden ALSA sink unity only after the DSP call succeeds.

When the graph is idle or the DSP transaction fails, the hidden sink retains
the recovered Windows endpoint attenuation.  This prevents a full-volume
handover interval.

## Why this is not the old pause ghost

The earlier pause ghost was physically localized to 1776 frames (37 ms) of
already-rendered media held in the Dolby chain's algorithmic delay.  It was
fixed by consuming exactly 1776 zero-input frames on the LADSPA activation /
pause boundary and discarding the output.  The user confirmed that old media
fragment disappeared.

The slider spike can feel similar because the GNOME preview exposes both
problems, but the new concrete mismatch was control latency: the preview could
start at the previous gain before the already-correct 10 ms DSP ramp began.

## Implementation

`deploy/dolby/sp11_windows_volume_transaction_sync.py` now uses the proven
monitor architecture from `sp11_dolby_volume_sync.py`:

- subscribe before the initial snapshot, preventing a startup attachment gap;
- bootstrap until the virtual and hidden hardware nodes exist;
- reject stale queued startup/unity events with a bounded fresh-snapshot guard;
- apply complete steady-state Props events directly, with no polling snapshot;
- reconcile partial target-node lifecycle events from a full snapshot;
- retain host attenuation while idle and on transaction failure;
- restore host attenuation on service shutdown.

The old `--interval-ms` argument is accepted only for compatibility and no
longer drives steady-state processing.

## Validation

- focused Python suite: `29 passed, 3 subtests passed`;
- source and installed executable SHA-256 matched after deployment:
  `4b1da1ff0f540949ad511df0f6322ef47282680fc4247d76882bc50516f7a72b`;
- `sp11-dolby-volume-sync.service` restarted successfully;
- service process selected `sp11-windows-volume-transaction-sync`;
- a live child `pw-dump -m` monitor is present;
- graph was idle at deployment, so no playback was interrupted and host
  attenuation remained the safe actuator;
- a subsequent zero-input real-graph test moved the visible endpoint
  `41% -> 42% -> 43% -> 42% -> 41%`; the service emitted all five accepted
  transactions in order, restored 41%, and remained active;
- the test graph closed normally; the kernel journal contained no XRUN,
  transport conflict, PA/protection failure, or timeout;
- that same live gate independently reconfirmed DP5/VISENSE `ch-mask=0x3` and
  DP6/CPS `ch-mask=0x3` on both amplifiers.

## Physical slider result and remaining ordering gap

The subsequent ordinary YouTube + GNOME slider test was **improved but not
closed**.  The user still intermittently hears a transition/spike whose sense
can appear forward or reversed depending on content.  This proves the removed
100 ms polling latency was a real contributor, not the complete cause.

The older standalone MSIIR synchronizer is not a competing writer: on this
kernel it detects `SP11 Windows Volume Transaction`, prints that the combined
transaction owns GainStep updates, and exits successfully.

One concrete cross-domain ordering gap remains.  The synchronizer currently:

1. writes the Dolby postgain **request**;
2. immediately sends the synchronous final VOL_CTRL + GainStep kernel call.

The Dolby plug-in does not apply and acknowledge that request until the next
audio callback/block.  Therefore “request written before DSP call” does not
guarantee “Dolby postgain applied before DSP call.”  Scheduler timing can make
the effective acoustic order vary, which is consistent with the reported
direction-dependent residual.  At the inspected settled state, request and
ack both reached `-214`; the issue is transition ordering, not a stuck value.

Next work must timestamp postgain request/ack and the kernel transaction during
one bounded real transition, then enforce the recovered Windows order if the
race is observed.  Do not add a guessed second fade or alter the proven
10 ms / 1 ms / curve-3 DSP ramp before closing this acknowledgement boundary.

## DP5/VISENSE `0x03` — closed; do not rediscover

The DP5 finding is already committed and pushed in commit `c4c6dde` on branch
`agent/render-parity-20260812`.  Current evidence and deployment are:

- Windows programs DP5/VISENSE `ChannelEnable = 0x03` on both WSA8845 amps;
- Linux candidate adds the Denali/SP11-scoped
  `qcom,visense-channel-mask = <3>` property to both codec nodes;
- loaded WSA884x candidate source version:
  `782FC79EBBA505E52A2AE88`;
- live boot logged both DP5/VISENSE requests at 8 kHz with `ch-mask=0x3`;
- DP6/CPS remained `ch-mask=0x3`, offsets left `0`, right `25`;
- bounded PipeWire/Dolby playback completed without XRUN, PA fault, transport
  conflict, or timeout, and both amps runtime-suspended afterward.

Canonical pointers:

- `artifacts/reviewed/2026-08-14-windows-qcaucd-full-fifo-vs-linux.json`;
- `deploy/visense-parity/README.md`;
- `docs/runbooks/2026-08-14-windows-physical-render-parity-handoff.md`;
- commit `c4c6dde` (`Record live Windows-width VISENSE validation`).

This transport question is GREEN.  Remaining subjective tonal/Dolby parity is
separate and must not reopen the proven DP5 mask unless new contradictory
hardware evidence is captured.
