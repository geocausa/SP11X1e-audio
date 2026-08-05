# PipeWire / Windows Dolby lifecycle parity — 2026-08-05

## Finding

The Linux production bridge had a real state-lifecycle parity bug: every
ordinary PipeWire filter-chain pause/reset called the LADSPA `activate()`
callback, and the plugin's `activate()` rebuilt both original Dolby DSP states
from cold. Windows does not do that. The shipped Windows VLLDP150 and VR APO
`Reset()` methods are no-ops and preserve adaptive DSP history.

This matters because the original VR Volume Leveler contains minutes-long
content-dependent state. A Windows-like Music state around `0.8149` is naturally
reachable under ordinary music/noise and remains above `0.8166` after two
minutes of digital silence. Cold-resetting to `0.8019793` whenever the Linux
sink goes idle prevents the Linux graph from preserving the same history.

## PipeWire source proof

Installed PipeWire version:

```text
1.6.2
```

Matching upstream tag/source used for the audit:

```text
tag     1.6.2
commit  95da54a482b68475958bbc3fa572a9c20df0df74
```

`src/modules/module-filter-chain.c`:

```text
playback_state_changed(PW_STREAM_STATE_PAUSED)
 -> pw_stream_flush(...)
 -> reset_graph(impl)
 -> spa_filter_graph_reset(impl->graph)
```

`spa/plugins/filter-graph/filter-graph.c` implements graph reset as:

```text
for each instantiated graph handle:
    if (descriptor->deactivate) descriptor->deactivate(handle)
    if (descriptor->activate)   descriptor->activate(handle)
```

`spa/plugins/filter-graph/plugin_ladspa.c` maps those descriptor methods
straight to the LADSPA plugin's `activate` / `deactivate` callbacks.

The deployed SP11 Dolby filter-chain has no LADSPA `deactivate` callback, but it
has `activate=chain_activate`. Therefore a normal PipeWire PAUSED graph reset
calls `chain_activate()` again on the same plugin instance.

The live SP11 configuration uses a passive playback node:

```text
capture  effect_input.sp11_windows_dolby
playback effect_output.sp11_windows_dolby
node.passive = true
```

At idle, live `pw-dump` showed:

```text
effect_input.sp11_windows_dolby   suspended
effect_output.sp11_windows_dolby  idle
```

so this is not merely a theoretical PipeWire code path; the real production
node enters the pause/idle lifecycle.

## Pre-fix Linux behavior

Pre-fix production `chain_activate()` was:

```c
static void chain_activate(LADSPA_Handle h){
    ChainInst*p=h;
    p->ready=(vl_reset(p)==0 && vr_build(p)==0);
}
```

`vl_reset()` and `vr_build()` reconstruct the original VLLDP and VR states.
That was useful for deterministic cold test runs but wrong for a host callback
that PipeWire also uses as its graph-reset mechanism.

Read-only inspection of the **actual installed filter-chain process** while the
Dolby sink was idle found:

```text
filter-chain PID          92891
VR PE base                0xffffba2d9000
VLLDP PE base             0xffffba5f2000
live LibWrapperVr object  0xaaaacba154f0
live VR outer             0xaaaacb8e9200
live VR core              0xaaaacbab6a08
outer+0x1F1768            0.8019793033599854
```

That value is exactly the fresh constructor Leveler state, directly confirming
that the live idle Linux graph had lost accumulated adaptive history.

## Windows Reset semantics

Original shipped DLL decompilation:

```text
DolbyAPOVR.dll
  CApoBase::Reset          0x1800F1C70

DolbyAPOvlldp150.dll
  CApoBase::Reset          0x180030030
```

Both methods log `CApoBase::Reset` and immediately return success. They do not
clear or rebuild the DSP state.

The DAX3 wrapper reset:

```text
DolbyDax3Apo.dll
  CDolbyAPOWrapper::Reset  0x180037ED0
```

forwards Reset to the inner APO interface. Thus Windows preserves the VLLDP/VR
adaptive state across ordinary APO Reset.

A full Windows unlock/destruction, graph recreation, process restart, or profile
service reconstruction is a different lifecycle event and may create fresh
state. The parity target here is specifically ordinary Reset on an existing
instance.

## Natural Leveler-state reachability

Fresh original-code Music replays prove the captured Windows Leveler state is
ordinary content history, not a hidden initialization constant.

Representative final `outer+0x1F1768` states:

```text
30 s silence                       0.801309526
30 s 997-Hz tone -18 dBFS          0.757566571
~70 s repeated dense loud music    0.819313228
~60 s medium pink noise            0.825634718
June Windows Music capture         0.814902425
```

After the dense-loud Music state reaches about `0.819313228`, digital silence
only decays it to:

```text
+5 s silence     0.819199324
+30 s            0.818636060
+60 s            0.817974925
+120 s           0.816630960
```

So resetting to `0.801979303` whenever Linux playback idles materially destroys
state that Windows can preserve for minutes.

## Fix

The production source now treats LADSPA `activate()` on an already healthy
instance as Windows Reset semantics: preserve the state and return.

```c
static void chain_activate(LADSPA_Handle h){
    ChainInst *p=h;
    if(!p)return;
    if(p->ready)return;
    p->ready=(vl_reset(p)==0 && vr_build(p)==0);
}
```

Fresh construction is still performed during plugin instantiation. Service
restart / profile-helper restart still creates a fresh instance. The change
only prevents repeated `activate()` calls on the same healthy instance from
throwing away VLLDP/VR history.

## Offline regression

Pre-fix production build:

```text
230932e53734c0fc0749eb54c8b8db462c739d7a7bf32cd937be4cb635d9be2b
```

Lifecycle-fixed candidate:

```text
cad1c0f0d1cfd6abeedbb3ed4c59ac910f77dc501feb6c178cb4152e43f5006c
```

A dedicated regression test instantiates two identical Music chains, warms both
for 70 seconds, calls `activate()` again on only one instance (PipeWire reset
simulation), and feeds an identical 3-second probe.

Pre-fix control:

```text
continue_hash    82392ef5d27ee75c   RMS 0.185976812
reactivate_hash  85659a91b53b9e1e   RMS 0.160016826
diff_samples     288000
max_abs_diff     0.652559578
LIFECYCLE_RESULT FAIL
```

Fixed candidate:

```text
continue_hash    82392ef5d27ee75c   RMS 0.185976812
reactivate_hash  82392ef5d27ee75c   RMS 0.185976812
diff_samples     0
max_abs_diff     0
LIFECYCLE_RESULT PASS
```

The fixed build also preserves every established first-activation profile hash
for Dynamic, Movie, Music, Game, Voice, Online Course and Personalize, and
passes 200k-frame arbitrary-chunk invariance for Dynamic and Music.

## Consequence

This is a concrete parity correction, not a subjective tuning change. It does
not enable Virtual Bass, add a limiter, change profile tuning, or alter cold
startup output. It preserves the original Dolby adaptive history across the
same class of ordinary reset that Windows already preserves.

A separate question remains for profile switching: the current Linux helper
restarts the filter-chain service when changing profile, which intentionally
creates fresh DSP state. Windows may preserve or selectively reset more history
across an in-place profile switch. That should be investigated independently;
it does not block this PAUSED/reset fix.
