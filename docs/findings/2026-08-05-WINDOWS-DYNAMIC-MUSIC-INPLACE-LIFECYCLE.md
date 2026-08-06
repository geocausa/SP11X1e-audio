# Windows Dynamic -> Music uses in-place Dolby retuning — 2026-08-05

## Executive result

The preserved June-12 active-tone capture plus fresh decompilation now close the
Windows main-profile lifecycle strongly enough to guide Linux implementation.

Windows does **not** represent a Dynamic -> Music change as a lower audio-graph
rebuild or as a fresh Dolby algorithm construction. The public profile call
updates RuntimeManager state, enumerates the already-active Dolby endpoints,
builds before/after DAP parameter maps, and sends only changed parameters through
`SetDapVariantParam`. The original VR Volume-Leveler amount/enable/DRC setters
only change configuration fields and an apply-dirty flag; they do not clear the
adaptive long-memory state.

The active ETL independently proves that, after the Dynamic -> Music action and
while the 997-Hz stream is still running, `audiodg.exe` is executing both the
main VLLDP processor and the main VR processor, including the exact long-memory
Volume-Leveler chain.

Therefore Linux profile changes should preserve the instantiated Dolby engines
and retune them in place. Reconstructing VLLDP/VR for a profile change destroys
state that Windows preserves.

## Capture timing correction

Capture:

```text
outputs/gate_traces/
  20260612_164740_dolby_access_dynamic_to_music_active_tone/
```

The gate harness starts a looping 997-Hz tone, starts WPR/QGPR, asks for only a
Dynamic -> Music change, waits for the action to be acknowledged, then waits a
further five seconds before stopping the traces.

A historical interpretation mistake came from the separately named `after`
VLLDP probe. Reading the harness proves that probe is taken only in the `finally`
path **after the loop tone has already been stopped**. Its later `Candidates=0`
therefore cannot be used as evidence that the profile change itself destroyed
the VLLDP object; ordinary stream teardown is sufficient to explain it.

The ETL is the correct evidence for the active post-switch interval because WPR
is stopped before the tone is stopped.

## Lower Qualcomm graph is unchanged

The same gate capture records:

```text
QGPR GRAPH_OPEN = 0
QGPR SET_CFG    = 0
```

So Dynamic -> Music does not rebuild or reconfigure the lower Qualcomm graph.
The `audiodg.exe` PID and Dolby module bases are also unchanged across the gate.

## Post-switch execution from ETL stacks

The WPR profile captured kernel sampled-profile stacks and
Microsoft-Windows-Audio stacks. Existing project Python environment
`.venv-etl` contains `dissect.etl`, allowing direct offline parsing of the
preserved ETL without installing new tooling.

Trace event range:

```text
2026-06-12 15:48:06.534498 UTC
through
2026-06-12 15:48:33.841021 UTC
```

Because the harness waits five seconds after the user confirms the profile
change and only then stops WPR, stack samples in the final <5 seconds are
unambiguously post-switch active-stream execution.

For `audiodg.exe` PID 10260, the final five seconds contain stacks in:

```text
DolbyDax3Apo.dll          35
DolbyApoVr.dll            24
DolbyAPOvlldp150.dll      12
audioeng.dll               9
```

The final three seconds still contain:

```text
DolbyDax3Apo.dll          17
DolbyApoVr.dll            13
DolbyAPOvlldp150.dll       5
audioeng.dll               5
```

### Exact VR long-memory chain after the switch

At `15:48:31.115284 UTC`, the VR stack contains RVAs including:

```text
0x3849C -> inside FUN_1800376B0  main VR processor
0x34D7C -> inside FUN_180034B78
0x58E3C -> inside FUN_180058990
```

This is the already-proved Volume-Leveler long-memory path:

```text
FUN_1800376B0
  -> FUN_180034B78
     -> FUN_180058990
```

Thus the adaptive leveler path is genuinely processing audio after Music is
selected; this is not merely a configuration/control-stack observation.

### Exact VLLDP processor after the switch

At `15:48:31.515333 UTC`, the VLLDP stack includes RVA `0x1F950`, which lies
inside `FUN_18001F7A8`, the known main VLLDP audio processor. Additional frames
are in the VLLDP processing/helper chain.

## Public SetActiveProfile -> in-place parameter update

Fresh Ghidra analysis of the exact SP11 `DAX3API.exe` gives the public chain:

```text
RpcServer::SetActiveProfile
  -> RpcServer::DAXRPC::SetActiveProfile
     -> DAXAPIImplement::SetActiveMainProfile   FUN_14010EC28
```

`DAXAPIImplement::SetActiveMainProfile` resolves the requested profile and the
active endpoint set. When the profile differs, it calls the RuntimeManager
profile setter:

```text
FUN_140085BF0
  -> RuntimeManager::SetActiveMainProfile       FUN_140136740
```

On success it explicitly walks the already-existing endpoint objects and calls:

```text
for each existing endpoint:
    DolbyEndpointControl::UpdateDapParameters   FUN_140088EA8
```

This is direct decompiled control flow in `FUN_14010EC28`, not a naming
inference.

`DolbyEndpointControl::UpdateDapParameters` obtains the current and requested
DAP parameter maps, adjusts runtime fields such as postgain/system gain, and
hands the maps to:

```text
DAPControl::Update                             FUN_1400E2440
```

`DAPControl::Update` compares the stored and requested parameter values. For a
changed value it calls:

```text
SetDapVariantParam(existing_dap_control, parameter_id, ...new value...)
```

and then updates its cached map entry.

This is an in-place diff-and-set path. There is no Dolby VLLDP/VR constructor in
this public profile-change chain.

## VR setters preserve adaptive history

Fresh original `DolbyApoVr.dll` decompilation:

```text
FUN_18003D110  volume-leveler enable
  core+0x6DC = bool(value)
  core+0x1278 = 1   // apply dirty

FUN_18003D170  volume-leveler DRC enable
  core+0x6E4 = bool(value)
  core+0x1278 = 1

FUN_18003D1D0  volume-leveler amount
  clamp 0..10
  core+0x6D4 = value
  core+0x1278 = 1
```

All three are protected by the existing critical section. None clears or
reinitializes the Volume-Leveler core, its dependent 1-KiB region, or the
long-memory scalar previously isolated from the June Windows state.

## What this proves and what it does not

Proved:

- Dynamic -> Music leaves the lower Qualcomm graph unchanged;
- the same `audiodg` process/modules remain active;
- main VLLDP and main VR processing execute after the switch;
- the exact VR long-memory leveler chain executes after the switch;
- public SetActiveProfile applies the new profile by diffing parameters on
  existing endpoints and calling `SetDapVariantParam` for changes;
- the relevant VR leveler setters preserve adaptive-history storage.

The ETL does not expose heap-object pointers, so it cannot by itself prove that
no internal object was ever momentarily replaced. But the public implementation
contains an explicit existing-endpoint diff-and-set path, and the original
setters are state-preserving. Together with post-switch live processing, this is
sufficient evidence for the lifecycle requirement relevant to the Linux port:
**do not reconstruct the Dolby algorithms merely because the main profile
changed.**

## Linux parity consequence

The Linux production chain must distinguish:

```text
stream/graph lifetime event      != profile/tuning update
```

Profile/tuning changes should call the original setters/apply logic on the
existing VLLDP and VR state and preserve adaptive history. Full reconstruction
should be reserved for real instance creation/destruction or an explicit hard
reset whose Windows analogue also resets DSP state.

## Linux implementation and offline regression

The production bridge now exposes a sixth LADSPA port:

```text
Profile   integer 0..7
0         preserve SP11_DOLBY_PROFILE startup selection
1..7      Dynamic, Movie, Music, Game, Voice, OnlineCourse, Personalize
```

At a control change the existing instance compares old/new profile structures.
Only changed VR scalar controls are sent to the recovered original setters; IEQ
curve/output-mode work is performed only when those profile fields differ.
VLLDP compressor tuning is updated only when crossing the Dynamic-family versus
Movie/Music-family boundary, followed by the original VLLDP apply function.
Neither VLLDP construction/scheduler initialization nor VR deinit/construction is
called.

A dedicated source-level lifecycle regression warms Dynamic for 12 seconds,
records the exact VLLDP core, VR outer/inner/core pointers and the proved VR
long-memory float at `outer+0x1F1768`, writes the mapped runtime request for
Music and executes a zero-frame control cycle, and checks both the plugin
acknowledgement and state before any new audio is processed. Result:

```text
initial long memory    0.801979303  bits 3f4d4e84
warmed long memory    0.794327974  bits 3f4b5914
after Music retune    0.794327974  bits 3f4b5914

VR outer pointer      unchanged
VR core pointer       unchanged
VLLDP core pointer    unchanged
leveler amount        5 -> 0
state preserved       YES
PROFILE_LIFECYCLE_RESULT PASS
```

The same mapped request/ack path is swept across Movie, Music, Game, Voice,
OnlineCourse, Personalize, Dynamic and back to Music. Every transition preserves
the exact long-memory bits and all VLLDP/VR object pointers while the expected
profile-specific leveler amount changes. After one second of Music audio the
state evolves normally, and an in-place return to Dynamic again preserves the
exact instantaneous long-memory bits.

The candidate also passes the existing regressions:

```text
block-size/plugin test       PASS, reference hash 01ec0adc40a8905b
70-second activate/reset     PASS, 0 differing samples
old installed Dynamic hash  01ec0adc40a8905b
new candidate Dynamic hash  01ec0adc40a8905b
```

The first live rollout exposed an important PipeWire-host detail before any
profile was exercised. PipeWire 1.6.2 correctly publishes the custom LADSPA
controls and their ranges in `PropInfo`, but external `pw-cli set-param ... Props`
writes do not mutate the filter graph's control values; even a real Bypass
`false -> true` request reads back unchanged. This matches the PipeWire
Filter-Chain contract, which documents node `control = { ... }` values as the
**initial values** for control ports rather than a general writable runtime
interface.

The production runtime path therefore uses a two-byte user-runtime control map:

```text
$XDG_RUNTIME_DIR/sp11-dolby-profile.control
byte 0  requested code: 0 none/startup, 1..7 profiles
byte 1  plugin-applied acknowledgement
```

The file is opened/truncated/mapped once during plugin instantiation. The audio
callback performs only atomic byte loads/stores on the mapping; no open/read/
write/stat or allocation occurs in the real-time path. The helper writes byte 0
without truncating the file. If audio is active, byte 1 acknowledges the retune
immediately; if idle, the request remains queued and is applied before the first
future audio block. The LADSPA `Profile` port remains as a secondary/future
control surface, with non-negative `0 = startup` encoding so a host default
cannot override a non-Dynamic cold-start profile.

### Lazy-instantiation and control-plane corrections from live validation

The first shared-control live validation exposed three host-side details before
any non-Dynamic profile was actually applied:

1. PipeWire can publish the LADSPA descriptor/PropInfo before creating the DSP
   instance; the runtime control map therefore does not necessarily exist while
   the sink is completely idle. A short stream causes instantiation and map
   creation.
2. The initial shell octal writer was wrong and wrote ASCII `$` (`36`) instead
   of binary Music code `3`. The plugin correctly ignored code 36, so this did
   not change DSP profile. The helper now uses Python `os.open` + `os.pwrite` to
   write one exact binary byte without truncating a mapped file. It can also
   create the two-byte map before lazy instantiation.
3. `systemctl show ... Environment` reflects the reloaded unit configuration,
   not necessarily the environment of the already-running process. Live status
   now reads `/proc/$MainPID/environ` first and uses the unit environment only as
   fallback.

The plugin's mapping open path now preserves an already-queued request byte and
resets only the per-instance acknowledgement byte. Therefore an idle request can
be queued before LADSPA instantiation and is applied/acknowledged before the
first future audio block. A dedicated regression precreates `{Music,0}`, then
instantiates a Dynamic-start plugin and verifies the first zero-frame process
cycle becomes `{Music,Music}` without reconstruction.
