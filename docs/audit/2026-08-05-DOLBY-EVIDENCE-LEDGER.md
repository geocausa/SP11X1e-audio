# Dolby completion evidence ledger — 2026-08-05

This file is a preservation index for high-value reverse-engineering results.
The rule is: important conclusions must not exist only in chat, `/tmp`, or an
uncommitted local experiment. Evidence-backed discoveries are checkpointed on
`agent/dolby-completion-2026-08-05` and pushed to origin. Claims are deliberately
separated into **proven**, **strong candidate**, and **open** so later work does
not accidentally convert a useful hypothesis into folklore.

## Proven / reproduced

- Original Windows ARM64 VLLDP150 DSP executes on Ubuntu from its deterministic
  constructor state.
- Exact VLLDP outer 432 scheduler, inner 256 accumulator and core orchestration
  execute on Ubuntu; arbitrary Linux chunk sizes produce bit-identical output.
- Original Windows DolbyApoVr processing also executes on Ubuntu and combines
  with VLLDP in the live Windows order.
- The live DAX service uses separate content/device tuning maps and dynamically
  rebuilds them at runtime.
- The recovered live Music content map has named Virtual Bass, Bass Enhancer,
  Bass Extraction and Volume Modeler disabled while Leveler, DRC, Regulator and
  Volume Maximizer tuning are active.
- Forcing the omitted disabled/zero VR scalar values through original Dolby
  setters leaves output bit-identical; dormant constructor defaults are not the
  missing effect.
- DAX volume feedback is real: endpoint-volume dB is converted to runtime
  `postgain`; a separate `VlldpSystemGain` is injected into the device map.
- SP11 endpoint volume range recovered from live DAX memory: about -75 dB to
  0 dB in 0.5-dB steps. Therefore normal DAX postgain is never positive.
- VLLDP/VR inner APO notification handlers ignore type-1 endpoint-volume
  notifications and act on type 2; the inner notification route is not a
  secret master-volume control path.
- The recovered Windows 75-Hz staircase has a large loud-level odd-harmonic
  onset (H3/H5) that the current VLLDP->VR Linux chain does not reproduce.
- Warm Dolby history changes the oracle match materially (Movie reaches about
  0.9674 correlation after two prior full passes) but does not by itself explain
  the missing loud-level nonlinear signature.
- The live-vs-replay VR `output-mode` core-state difference was traced to the
  output-mode setter; omitting that setter is bit-transparent for Music, so it
  is not the missing Music residual.
- Transplanting the two large evolving history arrays from the live Windows
  Music VR core into the Linux Music core makes the known-input candidate
  quieter and slightly worsens correlation. The recovered June VR history is
  active state but does not restore the missing pre-limiter drive.
- The same June `audiodg.exe` dumps contain the unique live VLLDP wrapper,
  staging buffers and full main state. Its 18/18 stable profile discriminators
  identify the Movie/Music VLLDP family; the sibling live VR core resolves the
  complete chain to Music.
- After pointer relocation and Windows lock bookkeeping are excluded, the only
  stable VLLDP tuning mismatch is a disabled sliding-bass block. Windows uses
  enable=0, boundary=6, attack=712 ms, release=500 ms, max=52, min=0 and a zero
  curve. Applying these exact values through original setters is bit-identical
  to baseline in Dynamic/Movie/Music and across the full Music known-input.
- The complete June captured VLLDP state/aux can be replayed at the original
  Windows heap/module VAs with constructor geometry MATCH. Its huge first block
  is program-history ring-out (also present with zero input). After three
  repeated 997-Hz blocks, captured Windows-warm and fresh Music states converge
  to the same RMS/peak output to displayed precision.
- The live AudioEng limiter state was recovered from its exact initialization
  signature. At both June snapshots current gain is 1.0 and attack-left is 0;
  it is instantiated but not actively limiting at those instants.

Primary detail record:

`docs/findings/2026-08-05-DOLBY-RUNTIME-GAIN-LIMITER-RECHECK.md`

## AudioEng limiter — live and decoded, but not the missing stage by itself

Windows `AudioEng.dll` contains `CAudioLimiter` and the AudioLimiter CLSID
`{d69e0717-dd4b-4b25-997a-da813833b8ac}` appears repeatedly in real
Microsoft-Windows-Audio `audiodg.exe` ETW Start/Stop events.

Evidence identities:

```text
AudioEng.dll SHA-256
1e2cc764cae6ebfb6985d8503bb83a36022852fbbf1841c377c5ad2fa2d6795b

silent_audio_provider_only_events.csv SHA-256
9622d267ea210ddaee9125bcc1f0bb4b887dd50974803729fde8fe23524e1e09
```

The real limiter state machine is now decoded: linked stereo peak detection,
0.985 ceiling, 64-frame look-ahead at 48 kHz, attack ramp and exponential
release. Exact offline replay engages on some stimulus regions but leaves the
critical loud 75-Hz steps untouched because the current Dolby candidate reaches
those steps below threshold. Therefore AudioLimiter is real and relevant, but
the missing gain/nonlinearity is upstream.

A second major source-of-truth recovery comes from the June full `audiodg.exe`
minidumps. They contain the unique live `LibWrapperVr` object at
`0x2453913C2F0` and core at `0x245391DD808`. Direct core reads give Bass Enhancer
and Bass Extraction OFF. Stable profile discrimination identifies Music and the
Linux Music replay matches 34/34 stable scalar discriminators. Large core
history regions evolve between the two Windows snapshots.

Detail:
`docs/findings/2026-08-05-AUDIOENG-LIMITER-AND-LIVE-VR-CORE.md`

- Full live VR outer replay at the original Windows heap/module VAs reveals a
  persistent steady Music difference after identical continuous input: fresh
  replay ~0.15354 RMS versus captured Windows ~0.12005 RMS. Exact-address hybrid
  tests localize most of the difference to the live core plus one dependent
  1-KiB arena block at `outer+0x1F1430` (`core+0x23C28`). This is now the
  highest-value unresolved VR lifecycle/state target.

## Open high-value questions

- Which upstream Music runtime/history state drives the Windows 75-Hz signal
  hard enough to reach the decoded AudioEng limiter.
- DAX `vlldp-limiter-gain` is proved bidirectional Get/Set (`0x850` ->
  internal `0x2A` -> low-level `mb_compressor_limiter_gain`). Remaining
  question: live value semantics (configured vs evolving state) and correlation
  to VLLDP limiter state.
- Exact trigger behind the July Firefox/YouTube Movie/Music-family VLLDP state:
  profile switching, graph/content state, or another runtime policy.
- Remaining lifecycle/history state needed for exact cold/warm Windows parity.

## Preservation checkpoints

- `5563376` — runtime Dolby gain/limiter recheck, pushed to origin.
- `21d5638` — evidence ledger + AudioEng/AudioLimiter live-ETW evidence, pushed to origin.
- `c73e282` — canonical topology/index/production-manifest consolidation.
- This live-VR-core + decoded-AudioLimiter result is the next evidence checkpoint.

## AudioLimiter ETW graph-correlation increment

A deeper parse of the saved Microsoft-Windows-Audio provider trace confirms the
AudioLimiter CLSID is not a one-off registration artifact. In the same
`audiodg.exe` process (`PID 0x3460`) that is constructing the SP11 render graph,
the AudioLimiter CLSID

```text
{d69e0717-dd4b-4b25-997a-da813833b8ac}
```

appears repeatedly in paired Start/Stop engine events with stable object
addresses across graph/state transitions. One parsed trace contains 144 limiter
rows. This strengthens the claim from “class present” to “real audio-engine
object repeatedly instantiated/operated in audiodg.”

This still does **not** establish the exact ordering relative to VLLDP/VR or
prove that the May known-input speaker stream passes through this object. Those
remain the next discriminator; do not promote CAudioLimiter to proven missing
waveform stage until ordering/stream association and replay close the oracle.

### 2026-08-05 — May-19 deterministic capture-pack recheck

- The endpoint Dolby registry families `{5510c7ab...}` and `{f112024a...}` do
  **not** track active profile: a controlled Dynamic->Music switch leaves both
  families byte-identical. Historical endpoint registry values must not be used
  alone to label a live capture Dynamic/Movie/Music.
- Current original-code Dynamic/Movie/Music replay of the twelve May-19 paired
  Windows stimuli gives about 1.05/0.90/0.96 dB mean absolute RMS error
  respectively; the exact capture profile remains unproven.
- Adding decoded `AudioEng!CAudioLimiter` slightly worsens pack-wide RMS error,
  reinforcing its role as a safety ceiling rather than the missing loudness
  generator.
- A steady -18 dBFS 75-Hz Windows capture preserves the source H3/H5 at roughly
  -83/-90 dBc. The very large odd-harmonic signature appears only near the loud
  ceiling, not as a continuously enabled Virtual Bass process.
- `VR core+0x5E0` is a dormant/lazy output-mode cache flag, not missing gain.

### 2026-08-05 — VR persistent gap localized to Volume-Leveler adaptive history

- Exact-address hybrid replay reduced the dominant persistent June Windows
  Music VR difference to one active arena float at `outer+0x1F1768`:
  Windows `0.814902425`, fresh Music `0.801979303`.
- An ARM64 hardware data watchpoint caught shipped Dolby `FUN_18006A2D0`
  updating it with a hysteretic attack/release smoothing recurrence.
- Caller chain terminates in the VR Volume-Leveler/DRC family, not Virtual
  Bass. The state remains materially different after ~22 s of identical tone.
- `core+0x93C=2` is a processing-geometry cache key that prevents the internal
  Leveler aggregate from being rebuilt; it is not gain.
- `core+0x4100..0x41FF` is a 64-float short-history/analysis vector written by
  transform `FUN_180042590` from the VR-specific core. It refreshes rapidly
  with current audio and influences the Leveler trajectory, but is not itself
  the persistent long-memory state.
- Correct parity work should reproduce Windows lifecycle/history semantics,
  not hard-code the captured Leveler float.

### 2026-08-05 — PipeWire idle reset was erasing Dolby adaptive history

- PipeWire 1.6.2 (`95da54a482b68475958bbc3fa572a9c20df0df74`)
  handles filter-chain playback `PAUSED` by calling graph `reset()`; filter-graph
  reset calls LADSPA `deactivate()`/`activate()` on the existing instance.
- The SP11 Dolby node is passive and was observed live at idle as capture
  `suspended` / playback `idle`.
- Pre-fix `chain_activate()` rebuilt both VLLDP and VR, while original Windows
  VLLDP/VR `CApoBase::Reset` methods are no-op success returns and DAX3 forwards
  Reset to the inner APO.
- Read-only live-process inspection found the installed idle Linux VR Leveler
  state exactly at its constructor value `0.8019793033599854`, confirming the
  reset occurred in production rather than only in source theory.
- Fresh original Music can naturally reach `0.819313228` under dense music and
  `0.825634718` under medium pink noise; after two minutes of silence the dense
  Music state remains `0.816630960`.
- The lifecycle-fixed candidate preserves an already-ready instance on repeated
  LADSPA `activate()`. Dedicated 70-second warm/reset regression: pre-fix all
  288000 probe samples differ; fixed candidate is bit-identical (`diff=0`).
- All seven cold-start profile hashes and Dynamic/Music chunk-invariance remain
  unchanged. This is an evidence-backed lifecycle parity fix, not new tuning.
