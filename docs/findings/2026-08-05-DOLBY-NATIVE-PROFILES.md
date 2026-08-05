# SP11 native Windows Dolby profiles — 2026-08-05

## Status

The original-code Linux bridge now supports the seven useful SP11 speaker
profile families selected at plugin activation with `SP11_DOLBY_PROFILE`:

- dynamic
- movie
- music
- game
- voice
- onlinecourse
- personalize (covers the three static Personalize slots; user GEQ is separate)

Profile values come from the archived SP11 OEM tuning file
`DAX3_SPEAKER_TUNING_MSHW0486_REV0D.xml`; processing is still performed by the
original ARM64 Windows `DolbyAPOvlldp150.dll` and `DolbyApoVr.dll` code.

## VLLDP split

Game, Voice, Online Course and Personalize use the same VLLDP base tuning as
Dynamic. Movie and Music share the alternate four-group compressor layout:

```text
group0 = 2,-256,12980,3,20,64
group1 = 7,-160,16366,10,20,64
group2 = 16,0,32767,10,20,0
group3 = 20,0,32767,10,20,0
channel deviation = 96
slow gain enable = 1
band0 slow gain mix = 103
```

The exact original setters are:

```text
0x18001CB90  compressor channel deviation
0x18001CC50  compressor slow-gain enable
0x18001CB20  compressor slow-gain mix level
```

Dynamic-family values are explicitly restored to `0`, `0`, and `256` on each
cold rebuild rather than relying on constructor residue.

The XML also says `speaker-peq-enable=0` for Movie/Music versus `1` for the
other profiles. No proven VLLDP callable setter has yet been bound to that key.
The corresponding `speaker-peq-filters` payload is empty on this SP11 tuning,
so no unproven function address is used for it.

## VR/DAP profile controls

The already recovered original scalar/structured handlers now receive each
profile's OEM values for leveler, dialog enhancer, IEQ target/amount, MI
steering, surround decoder/boost, virtualizer angles, VolMax boost, output
processing mode, 20-band IEQ target and regulator table.

Important profile distinctions include:

- Dynamic: mode 11, leveler amount 5, dialog 5, IEQ Balanced amount 10,
  surround boost 96, all MI steering enabled.
- Movie: mode 11, leveler amount 0, dialog 2, IEQ disabled/Warm target,
  surround boost 72, front/surround angles 16, VolMax 104.
- Music: mode 1, leveler amount 0, dialog off, IEQ disabled/Warm target,
  surround decoder off, surround boost 24.
- Game: mode 11, dialog off, IEQ off, surround boost 0.
- Voice: mode 1, leveler off, dialog amount 8, surround decoder off.
- Online Course: mode 1, leveler amount 0, dialog 5, VolMax 64.
- Personalize: mode 11, leveler amount 3, dialog 10, surround boost 48.

The two XML `output-mode-partial-*-virtualizer-enable` booleans are not exposed
through a proven direct handler in the recovered VR dispatch work. Dynamic had
already been validated without separately writing them, and mode selection is
applied through the original output-mode function. They remain a documented
profile-parity detail rather than a guessed write.

## Offline validation

A 50,000-frame deterministic test was run for all seven profiles. Every profile
was bit-identical across host chunk patterns 1, 64, 480, 1024, 127/353 and a
mixed irregular pattern. Reference hashes were distinct:

```text
dynamic       9d4b1534fa1c02b6
movie         fe84c0bdb078650d
music         de07320c2411d1bd
game          3ed5531aebf3b3ba
voice         9350232215c4d780
onlinecourse  d8ae8062d25f7877
personalize   e178bdf5e8cd8157
```

The complete 29.45-second known-input stimulus (1,413,600 frames) was also run
through every profile at 480-frame host chunks. All seven completed with zero
NaN/Inf samples.

Regression check against the previously installed Dynamic-only plugin at
200,000 frames produced the same bit-exact reference hash for old and new:

```text
01ec0adc40a8905b
```

Therefore adding profile selection does not alter the established Dynamic path.
## Live rollout validation

The profile-capable plugin was installed into the existing isolated
`filter-chain.service` after first selecting the transparent bypass and saving
a byte-for-byte rollback bundle under:

```text
~/.local/state/sp11-dolby/backups/20260805-091855-pre-profiles/
```

The complete live switch sequence was exercised:

```text
Dynamic -> Movie -> Music -> Game -> Voice -> Online Course -> Personalize -> Dynamic
```

Every profile created a live sink with the requested
`SP11_DOLBY_PROFILE` environment, `filter-chain.service` stayed active, and the
Dolby sink volume remained at the pre-switch `0.10` value. The machine was
returned to Dynamic after the test.

Two deployment-only edge cases were found and fixed during this test:

1. PipeWire initializes a newly recreated filter sink at 1.00 volume unless the
   previous sink volume is explicitly restored. The helper now snapshots and
   restores both volume and mute state around deliberate restarts, with 0.10 as
   the safe fallback if no prior live sink exists.
2. Rapid deliberate profile changes can hit systemd's user-service start-rate
   limit. The helper clears only the filter-chain service's failed/rate-limit
   state before a requested restart. The observed rate-limit event was not a
   Dolby crash; the preceding service processes all exited successfully.

The live-node lookup also excludes WirePlumber's configured-default pseudo row
(`0. Audio/Sink`) so an absent filter sink cannot be mistaken for a real node.


## Self-contained production build

The live profile-capable host was rebuilt with its default DLL paths pointing
only to the private local bundle:

```text
~/.local/lib/sp11-dolby/DolbyAPOvlldp150.dll
~/.local/lib/sp11-dolby/DolbyAPOVR.dll
```

The pre-GEQ self-contained profile host SHA-256 was:

```text
49eb13d0f6be940ee5759954082e809b6f56a22b9135df0c012f506fd10aed63
```

After adding the original Personalize GEQ path, the installed production host
SHA-256 is:

```text
230932e53734c0fc0749eb54c8b8db462c739d7a7bf32cd937be4cb635d9be2b
```

`deploy/dolby/build-production.sh` verifies the two known SP11 Dolby DLL hashes
before building and embeds only those private bundle paths. Two independent
builds produced the same host SHA-256 above. Dynamic retained its existing
200,000-frame reference hash `01ec0adc40a8905b`, and all seven profile smoke
tests passed before the self-contained host was installed.

## Resolved: partial virtualizer flags are not separate DSP controls here

The OEM XML contains two additional keys:

```text
output-mode-partial-surround-virtualizer-enable
output-mode-partial-height-virtualizer-enable
```

Across every SP11 profile they are perfectly redundant with the existing
`output-mode/processing_mode` setting:

```text
processing_mode 11 -> partial surround=1, partial height=1
processing_mode  1 -> partial surround=0, partial height=0
```

There is no profile in the OEM table where either boolean varies independently.
`DolbyAPOVR.dll` contains parameter strings for `output-mode:processing_mode`,
`output-mode:num_output_channels`, and `output-mode:mix_matrix`, but no
`partial-surround` or `partial-height` parameter string.

The three recovered callers of the real `dap_vr_output_mode_set` at
`0x180032320` likewise pass only processing mode, output-channel count, and the
matrix. The setter itself derives/normalizes mode and channel geometry, stores
the resulting output state and Q14-derived matrix, and marks the DSP state
dirty. No separate partial-virtualizer argument exists in that boundary.

For this SP11 tuning these XML booleans are therefore upstream policy metadata
that mirrors mode selection, not an omitted independent write in the native
bridge.

## Resolved for this endpoint: `speaker-peq-enable`

The SP11 XML changes `speaker-peq-enable` from 1 in Dynamic-family profiles to
0 in Movie/Music, but `speaker-peq-filters` is empty in every profile.

The live Windows Dynamic -> Music+IEQ-Off capture provides the stronger runtime
check. Between these two profiles:

- `child1+0x580..+0x778` VLLDP config dump: byte-for-byte identical;
- `child1+0x1080` tail dump: byte-for-byte identical;
- endpoint FX state dump: identical;
- the static/runtime changes in `child1+0xdf0..+0xef0` are the known
  four-group Movie/Music compressor layout now reproduced by the native bridge;
- the remaining `+0xc00..+0xc58` changes are live/history gain state.

`DolbyAPOvlldp150.dll` also has no independently registered
`speaker-peq-enable` scalar handler, while its active Render Speakers PID 5
`vlldp-filter-config` payload is empty. Consequently there is no missing PEQ
filter graph or separate VLLDP setter to reproduce for this endpoint/profile
set. If a future OEM tuning provides non-empty `speaker-peq-filters`/PID 5,
that would be a different configuration and should be decoded separately.

## Personalize / Custom graphic EQ

The remaining user-editable GEQ path has now been recovered from the verified
`DolbyAPOVR.dll` itself.

Ghidra string/xref analysis identifies the original scalar handler:

```text
graphic-equalizer-enable -> 0x180032780
```

It stores the normalized enable state at `dap_vr_state_s+0xA4C` and marks both
the GEQ block (`+0xAA0`) and global DSP state (`+0x1278`) dirty.

The original 20-band run-time path uses the same proven Dolby band-grid and
target-building functions already used for IEQ, but on the GEQ state block:

```text
band grid   FUN_18004C560: core+0xAA4
band target FUN_18004C8E8: core+0xAA4 -> core+0xA50
limits      -576 .. +576
```

DAX3's public Custom-EQ API exposes the narrower user range `-192..+192`, so the
Linux helper accepts exactly 20 integer values in that range and passes them
without speculative rescaling. The standard SP11 20 band centers are used.

The plugin reads `SP11_DOLBY_GEQ` only at state construction. GEQ is applied
only when the selected profile is `personalize`; other profiles explicitly keep
GEQ disabled even if a saved Custom curve exists. Missing/`off`/`flat` data
means GEQ disabled. Invalid environment data also fails safe to GEQ disabled
instead of aborting the audio host.

Regression tests with GEQ absent preserve every established profile hash. A
non-flat 20-band test curve changed only the Personalize output and remained
bit-identical across 1, 64, 480, 1024, 127/353 and mixed host chunks.

The deployment helper adds:

```text
sp11-dolby geq                 # show saved curve/off
sp11-dolby geq reset           # disable/reset
sp11-dolby geq set <20 ints>   # each -192..192
```

The saved curve persists in `~/.config/sp11-dolby/geq` and is carried through
profile switches. An isolated fake-home/fake-systemd test verified persistence,
drop-in generation and reset behavior before live deployment.

### Historical Custom1 data gap

The 2026-06-12 Custom1 capture proves a user-edited high-bass/high-treble curve
was visible in Dolby Access, but that early capture predates the corrected
20-value `GetGEQLevels` RPC recorder. The old probe mistakenly treated opnum 17
(`GetGEQLevels`) as a scalar, so no numeric 20-band array was preserved in the
obvious state JSON/log files. Do not infer the curve from VLLDP `+0xC0C`: GEQ is
applied later in the VR stage.

The implementation is therefore complete for arbitrary Custom GEQ data. The
exhaustive historical-recovery pass is documented in
`2026-08-05-CUSTOM1-GEQ-FORENSIC-RECOVERY.md`; the exact June Custom1 values
remain unavailable from currently accessible evidence, but this does not block
new or recreated Custom curves.
