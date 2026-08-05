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

## Open high-value questions

- Which upstream Music runtime/history state drives the Windows 75-Hz signal
  hard enough to reach the decoded AudioEng limiter.
- Exact semantics of DAX `vlldp-limiter-gain` / low-level
  `mb_compressor_limiter_gain` (setter vs status/readback vs both).
- Exact trigger behind the July Firefox/YouTube Movie/Music-family VLLDP state:
  profile switching, graph/content state, or another runtime policy.
- Remaining lifecycle/history state needed for exact cold/warm Windows parity.

## Preservation checkpoints

- `5563376` — runtime Dolby gain/limiter recheck, pushed to origin.
- `21d5638` — evidence ledger + AudioEng/AudioLimiter live-ETW evidence, pushed to origin.
- `c73e282` — canonical topology/index/production-manifest consolidation.
- This live-VR-core + decoded-AudioLimiter result is the next evidence checkpoint.
