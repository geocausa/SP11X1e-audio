# Dolby ETW / kernel / profile correlation checkpoint — 2026-08-04

> **2026-08-05 forensic correction:** Section 8's statement that the July `.dump /k` contains no user-mode Dolby memory "by construction" is too strong and is superseded by `2026-08-05-PIPELINE-COMPLETENESS-RECHECK.md`. The ARM64 RDMP does not retain the `audiodg.exe` process page-table root or complete user address space, but selected runtime pages do survive; one genuine live VLLDP state page was recovered and validated by its internal pointer geometry. The Firefox→speaker session provenance is also present directly in the dump. Preserve the rest of this note as the historical ETW/profile correlation record.

This note preserves the archive-correlation work performed after the native-chain
checkpoint. It exists so a future session does not have to rediscover which ETL,
RPC-state, and kernel captures correspond to which application/profile state.

## Executive conclusion

The evidence now supports a two-layer switching model rather than one global
"Dolby pipe":

```text
application / stream category
        -> Windows signal-processing mode + graph instance/config
             (DEFAULT / MEDIA / MOVIE / NOTIFICATION / ...)
        -> Dolby DAX3 wrapper / native Dolby modules
        -> live Dolby profile + feature state can change in-place
             (Dynamic / Music / IEQ selection / etc.)
        -> downstream endpoint / Qualcomm path
```

The important distinction is:

1. Windows can create streams under genuinely different signal-processing-mode
   GUIDs. In particular, system sounds can use NOTIFICATION while ordinary media
   uses DEFAULT/MEDIA/MOVIE-class paths.
2. That does **not** imply the notification graph contains no Dolby components.
   Historical ETW shows the Dolby DAX3 APO instantiated in both DEFAULT and
   NOTIFICATION-mode events.
3. Once a media graph is running, at least some Dolby profile changes are live
   reconfiguration of the existing graph rather than graph destruction/rebuild.
   The Dynamic -> Music capture proves this directly.

This reconciles the user's recollection that notification sounds behaved as if
on a different path with the newer finding that the same Dolby wrapper family
can still be resident/active in both paths.

---

## 1. Critical correction: `{9cf2a70b-...}` is NOTIFICATION

Older outputs 56 and 58 called this mode GUID "unknown":

```text
{9CF2A70B-F377-403B-BD6B-360863E0355C}
```

That interpretation is stale.

Later static work (`outputs/59_three_topics_documented.md`) and the Surface /
DAX INF files identify it unambiguously as:

```text
AUDIO_SIGNALPROCESSINGMODE_NOTIFICATION
```

Relevant source evidence includes:

```text
SOURCE/SurfaceAPO/surfaceapoextension.inf
  AUDIO_SIGNALPROCESSINGMODE_NOTIFICATION =
  "{9CF2A70B-F377-403B-BD6B-360863E0355C}"
```

and the Qualcomm/DAX extension INF contains the same mapping.

Therefore all historical ETW rows tagged `{9cf2a70b...}` must be re-read as
**Notification processing-mode activity**, not an OEM-private/unknown mode.

---

## 2. Historical render ETW already captured media vs notification behaviour

### YouTube/browser playback

`outputs/55_surfaceapo_etw_live_verification.md`

Capture condition:

```text
156-second ETW trace during YouTube/browser music playback
```

Observed live render path included:

```text
Dolby DAX3 APO {0ebd8605}
SurfaceAPO MFX {34d30cd8}
```

Dolby DAX3 was explicitly initialized in DEFAULT mode and processed throughout
browser playback.

### Speech/system-sounds/WAV mixed-mode run

`outputs/56_tracermi_attempt_and_mode_gated_etw.md`

Capture condition:

```text
speech synthesis + Windows SystemSounds + WAV playback
```

Event-50 initialization/topology rows showed the same core APO set in both:

```text
DEFAULT      {c18e2f7e-933d-4965-b7d1-1eef228d2af3}
NOTIFICATION {9cf2a70b-f377-403b-bd6b-360863e0355c}
```

For both modes the ETW contained:

```text
Dolby DAX3 {0ebd8605}
Dolby second wrapper instance {0ebd8606}
SurfaceAPO MFX {34d30cd8}
other Windows/APO graph objects
```

Thus Notification is a distinct Windows processing mode, but historical ETW
**does not support** the stronger statement "notifications bypass Dolby
entirely". The correct statement is that they can run under a different
processing-mode graph/configuration while still using Dolby wrapper instances.

---

## 3. Current DAX profile-ID mapping recovered from live RPC captures

The old archive contained stale profile-ID notes. Current-build capture
`outputs/142_dax_rpc_state_dumper_and_dynamic_capture_20260612.md` establishes:

```text
active_profile = 5  -> visible Dolby Access Dynamic
```

Later Music captures establish:

```text
active_profile = 1  -> Music
```

Relevant state files include:

```text
outputs/vlldp_state_runs/20260612_090215_dolby_dynamic_clean_tone_only_dax_rpc_state/dax_rpc_state.json
outputs/vlldp_state_runs/20260612_151508_music_warm_ieq_rpc_check/dax_rpc_state.json
outputs/vlldp_state_runs/20260612_151812_music_off_ieq_rpc_check/dax_rpc_state.json
outputs/vlldp_state_runs/20260614_165537_dolby_youtube_live_165532/dax_rpc_state.json
```

Do not reuse much older notes that mapped Dynamic to profile 0.

---

## 4. Dynamic -> Music is live in-place reconfiguration

Purpose-labelled capture:

```text
outputs/gate_traces/
  20260612_164740_dolby_access_dynamic_to_music_active_tone/
```

Capture action:

```text
997-Hz tone active
Dolby Access: switch only Dynamic -> Music
wait 3-5 seconds
```

### audiodg/module evidence

Before and after snapshots used the same:

```text
audiodg.exe PID        10260
process start time     12/06/2026 16:47:56
```

The following modules remained mapped at the same base addresses before and
after the profile change:

```text
DolbyDax3Apo.dll
SurfaceAPO.dll
DolbyAPOvlldp150.dll
DolbyApoVr.dll
DolbyAudioProcessing.dll
DolbyHrtfEnc.dll
```

No relevant module reload occurred.

### Qualcomm/APM evidence

`qgpr_gate_raw.summary.md` for the same capture:

```text
records        1314
SET_CFG        0
GRAPH_OPEN     0
all 1314       APM_CMD_GET_CFG / PARAM_ID_HW_EP_GET_CFG
```

Therefore this specific Dynamic -> Music transition did **not** rebuild the
Qualcomm graph or send a captured low-level SET_CFG. It happened in the already
running user-mode audio graph through live Dolby policy/parameter state.

This is strong evidence that top-level Dolby profile changes should not be
modelled as separate physical render pipelines.

---

## 5. IEQ change demonstrates a different kind of transition

Purpose-labelled capture:

```text
outputs/gate_traces/
  20260612_165053_dolby_access_music_ieq_off_to_detailed_active_tone/
```

Capture action:

```text
997-Hz tone active
Music profile: switch IEQ Off -> Detailed
```

Its QGPR summary contains:

```text
2 x APM_CMD_SET_CFG
1 x APM_CMD_GRAPH_STOP
1 x APM_CMD_GRAPH_FLUSH
shared-memory map/unmap/deregister activity
```

The two SET_CFG rows were hardware-endpoint parameters (`PARAM_ID_HW_EP_V2` /
`PARAM_ID_HW_EP_TIMESTAMP_CFG`), not an obvious Dolby DSP parameter marker.

This means not every UI feature transition has the same boundary. Some feature
changes can coincide with lower graph/endpoint lifecycle work even while the
Dolby user-mode policy layer remains central.

Do not generalise the Dynamic -> Music "no SET_CFG" result to every Dolby UI
control.

---

## 6. Enhancements-off gate is endpoint-level, not just a Dolby profile switch

Capture:

```text
outputs/gate_traces/
  20260612_163412_enhancements_gate_dynamic_on_to_off/
```

The endpoint `DisableSysFx` changed from unset to `1` for the active Speakers
endpoint. The audiodg process and core Dolby/VLLDP/Surface modules remained
mapped across the snapshot.

This is another reminder that DLL residency is not equivalent to active DSP
processing. Presence/absence of a mapped module is weaker evidence than ETW
APO-process activity plus runtime state.

---

## 7. Browser auto-profile / application policy evidence

`SOURCE/Dolby/RPC_Reversal/analysis.md` documents DAX package policy with
per-application mappings. Relevant defaults include:

```text
Browsers (Edge/Chrome/etc.) -> Dynamic
Media applications          -> Dynamic / Music / Movie mappings
Communication applications  -> Voice
```

The same package schema exposes `capture_stream` monitoring for VoIP apps via
`CaptureStreamMonitor.dll`.

Important nuance: package defaults also say `auto_profile_enabled=false` in the
generic settings file, so static package policy alone is not proof that every
runtime capture used auto-profile. Use the live RPC state captured beside each
experiment whenever available.

---

## 8. July kernel/KDNET capture provides a known media-active anchor

Kernel dump:

```text
/home/geoca/Documents/SP11-PROJECT/Gemini/dumps/WINDOWS_KERNEL_DUMP/
  sp11_kernel_mcp_windbg.dmp
```

Manifest provenance (`manifest.txt`) records operator testimony for the exact
capture state:

```text
Windows desktop
Firefox open
YouTube music actively rendering through internal speakers
nothing else open
```

The dump is PAGEDU64 / kernel-only, so user-mode audiodg/Dolby memory is absent
from the dump bytes by construction.

However, the accompanying live KDNET analysis in `extra-capture.md` preserved
the audiodg module inventory from the active session. It included:

```text
DolbyAudioProcessing.dll
DolbyDax3Apo.dll
DolbyAPOvlldp150.dll
DolbyApoVr.dll
DolbyHrtfEnc.dll
SurfaceAPO.dll
```

This is valuable provenance: it is an independently captured **Firefox +
YouTube active-media state** confirming the modern Dolby stack was resident in
that live graph.

Do not byte-scan the kernel dump for user-mode Dolby state; the manifest already
records why that cannot work. The dump remains useful for kernel driver state,
ADCM device context, transport structures, and live device-stack provenance.

---

## 9. Best current switching model

The archive now supports the following working model:

```text
A. Stream/application class selects Windows stream category / processing mode
   (DEFAULT, MEDIA, MOVIE, NOTIFICATION, COMMUNICATION, ...).

B. Windows instantiates/configures the corresponding audio graph/APO instances.
   Notification therefore can legitimately sound different from media even if
   Dolby wrapper DLLs appear in both graphs.

C. DAX policy selects a Dolby profile/features inside the live graph.
   Example: Dynamic(active_profile=5) -> Music(active_profile=1) can change
   in-place with no audiodg restart, module reload, GRAPH_OPEN, or SET_CFG.

D. Individual controls can have different consequences.
   Example: Music IEQ Off -> Detailed coincided with graph stop/flush and two
   endpoint SET_CFG operations.
```

For the Linux port this means we should avoid inventing one monolithic global
state machine. The clean architecture should separate:

```text
stream/category routing policy
Dolby profile/feature state
actual DSP state/history
endpoint/speaker-protection path
```

The current exact-DSP work remains focused on reconstructing the media path and
its AIDE -> DAPVR -> VLLDP behaviour. Notification-mode parity can be treated as
a separate routing/configuration profile once media DSP parity is solved.

---

## 10. High-value evidence paths for continuation

```text
# Mode identity correction
outputs/59_three_topics_documented.md

# YouTube/browser ETW
outputs/55_surfaceapo_etw_live_verification.md

# System sounds / Notification-mode ETW
outputs/56_tracermi_attempt_and_mode_gated_etw.md
outputs/58_whole_system_render_chain_consolidated.md

# Current Dynamic RPC mapping
outputs/142_dax_rpc_state_dumper_and_dynamic_capture_20260612.md

# Purpose-labelled transition captures
outputs/gate_traces/20260612_164740_dolby_access_dynamic_to_music_active_tone/
outputs/gate_traces/20260612_165053_dolby_access_music_ieq_off_to_detailed_active_tone/
outputs/gate_traces/20260612_163412_enhancements_gate_dynamic_on_to_off/

# July known-media-active kernel/KDNET evidence
/home/geoca/Documents/SP11-PROJECT/Gemini/dumps/WINDOWS_KERNEL_DUMP/manifest.txt
/home/geoca/Documents/SP11-PROJECT/Gemini/dumps/WINDOWS_KERNEL_DUMP/extra-capture.md
```

## 11. Continuation priority

This correlation work does not replace the current primary DSP target from
`2026-08-04-DOLBY-NATIVE-CHAIN-PROGRESS.md`:

```text
AIDE adaptive EQ / steering
    -> DAPVR leveler/regulator
    -> VLLDP speaker optimisation/protection
```

It does, however, clarify which Windows capture should be compared to which
runtime state and prevents future confusion between:

```text
Windows processing mode
Dolby DAX profile
Dolby DSP stage enablement
module residency
Qualcomm graph lifecycle
```
