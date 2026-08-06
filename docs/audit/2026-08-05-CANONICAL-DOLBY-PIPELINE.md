# Canonical SP11 Dolby / speaker pipeline — 2026-08-05

**Status:** canonical engineering model for the active Dolby completion branch.

This document is the first place to start when resuming SP11 Dolby work. It is
not a historical lab notebook. Older findings remain in the repository for
provenance, but where they disagree with this page the newer direct evidence
listed here wins.

Canonical branch:

```text
agent/dolby-completion-2026-08-05
```

Evidence ledger:

```text
docs/audit/2026-08-05-DOLBY-EVIDENCE-LEDGER.md
```

Production manifest:

```text
docs/deployment/2026-08-05-DOLBY-PRODUCTION-MANIFEST.md
```

## Evidence grades

- **A — direct runtime:** hardware execution trap, process/kernel dump, ETW/QGPR
  event, exact live memory, or direct Windows RPC read from the identified
  binary/session.
- **B — controlled reproduction:** original Windows code replayed on Linux,
  deterministic state oracle, exact-stimulus comparison, chunk invariance or
  actor-ablation experiment.
- **C — static:** shipped binary disassembly/RTTI, INF/XML/JSON/ACDB or other
  configuration evidence without a matching live execution observation.
- **OPEN:** credible target whose exact runtime role/order is not yet proved.

A lower-grade source must not override contradictory higher-grade evidence.
Historical AI prose is never evidence by itself.

## Canonical ordinary stereo speaker model

The currently supported model for ordinary built-in-speaker media is:

```text
application / shared-mode stream
        |
        v
Windows Audio Engine graph
        |
        +-- DolbyDax3Apo wrapper instance 1
        |       -> DolbyAPOvlldp150
        |
        +-- DolbyDax3Apo wrapper instance 2
        |       -> DolbyApoVr
        |
        +-- CAudioLimiter / other AudioEng engine state [position/effect OPEN]
        |
        v
MMDEVAPI speaker endpoint
        |
        v
Qualcomm AudioReach render graph
  SAL / VOL_CTRL / MSIIR / channel mixer / SPv5
        |
        +<-- SP_VI + WSA884x voltage/current feedback
        |
        v
SoundWire / dual WSA884x amplifiers / speakers
```

The **proven Dolby sample-processing order** in the tested Windows media stream
is:

```text
VLLDP -> VR
```

The diagram deliberately does not place `CAudioLimiter` before or after a
specific APO callback. Its class is live in AudioEng ETW, but its exact graph
position and contribution to the recovered nonlinear waveform are still open.

### Conditional Windows spatial side graph

Preserved June shared-mode Music traces additionally prove conditional execution
of `DolbyHrtfEnc.dll`, `DolbyAudioProcessing.dll`, `VirtualSurroundApo.dll`, and
AdaptiveSpatialAudioRenderer. This does **not** mean normal 2-channel speaker
content is fully HRTF-virtualized: the exact DAX content-processing contract
supplies `stereo_cp_bypass_mode=2` and `stereo_bypass_dap_dll=1`; DAPVR writes
that to its active stereo-bypass byte, and HRTF's `MixChannelBed` takes its
early-return bypass branch for matching stereo.

The exact June `AudioEng.dll` generation and Microsoft PDB now close the
remaining ASAR wrapper question for matching 48-kHz stereo. Live StackWalks
resolve to `CAdaptiveSpatialAudioRenderer::APOProcess -> MainPluginRenderer ->
AsarEncoderWrapper<IAsarEncoder2> -> DolbyHrtfEnc`; the DAX stereo-bypass
branch stages the original bed, the 48-kHz -> 48-kHz rate ratio leaves its
scale at 1.0, and HRTF `Process` copies that bed directly to output. The
matching `VirtualSurroundApo` hot child calls are standard AudioMeter and
AudioVolume rather than a hidden widening kernel.

The remaining original-VR stereo policy is now closed as well. The SP11
`msft_atmos` operator sets `bypass_stereo_virtualizer=true` for every profile;
DAX3API writes it to endpoint PROPERTYKEY
`{dc827e12-807b-4fbb-8e3c-6c62981dd3c9},1`, and the original DolbyAPOVR
`LibWrapperDap2::UpdatePropertyKeys` forces `speaker_virtualizer_enable=0` for
a 2-channel stream. Its final mode calculator therefore returns VR processing
mode 1 even when raw profile XML requests mode 11. The direct-core Linux host
now reproduces that wrapper policy.

Therefore generic HRTF/PHRTF, an invented ASAR matrix, a standalone
VirtualSurround widening stage, or raw mode-11 speaker virtualization must not
be inserted into the canonical ordinary-stereo chain. Remaining profile
changes are the proven non-virtualizer DAX/VR retunes plus any independently
proved mode-specific Surface APO behavior. See
`docs/findings/2026-08-06-WINDOWS-SPATIAL-STEREO-HRTF-BYPASS.md`,
`docs/findings/2026-08-06-AUDIOENG-ASAR-STEREO-IDENTITY-PATH.md`, and
`docs/findings/2026-08-06-SP11-STEREO-VIRTUALIZER-BYPASS-PARITY.md`.

## Actor confidence matrix

| Actor / boundary | Evidence | Linux reproduction | Status |
|---|---|---|---|
| `DolbyDax3Apo` wrappers | A: August hardware-hot wrapper callbacks | Wrapper plumbing is not executed wholesale; inner native processors are hosted directly | Wrapper sample-rate/notification side paths substantially audited; no missing 48-kHz SRC effect found |
| `DolbyAPOvlldp150` | A: hardware-hot scheduler/core; July live state page | Original ARM64 PE code, original 432 scheduler + 256 accumulator/core | **Proven hot / reproduced** |
| `DolbyApoVr` | A: second persistent wrapper/core hot | Original ARM64 PE wrapper/core | **Proven hot / reproduced** |
| DAX profile/config maps | A/C: RPC + DAX3API + exact DolbyAPOVR endpoint-policy reader | Profile setters reproduce recovered controls; 2ch effective output mode now honors per-profile stereo-virtualizer bypass | **High confidence; stereo wrapper policy closed** |
| DAX runtime volume feedback | A/C: decompiled live service path + endpoint object | Not yet continuously coupled to Linux endpoint volume | **Real runtime layer; parity work remains** |
| Named Bass Enhancer | A: 33/33 direct reads = 0; live DAX map = `0` | Off | **Proven off in recovered states** |
| Named Virtual Bass / extraction / modeler | A/C: live Music map values `0`; native setter probes | Off; exact off-block probes bit-identical | **Off in recovered state; not explanation for current residual** |
| Leveler / DRC / regulator / VolMax | A: live DAX/core state; B: output/ablation + lifecycle regression | Original VR/VLLDP processing; repeated LADSPA activate now preserves state | **Active, acoustically important, long-memory lifecycle reproduced** |
| SurfaceAPO media EQ | C: REV_0D MEDIA/MOVIE/default nodes disabled/identity | Not separately emulated | **Likely no-op for captured media mode** |
| Modern ASAR/AIDE DAPVR | A: tested ordinary-stereo cores hardware-cold | Not in production chain | **Cold for tested mode; mode-dependent open elsewhere** |
| Windows spatial `DolbyHrtfEnc` / `DolbyAudioProcessing` | A: June active Music ETL has real HRTF + spatial-DAP stack frames; C: exact stereo-bypass policy decoded | Not in production chain | **Conditional and genuinely hot when spatial graph is active; DAX speaker matching-stereo HRTF bed virtualization explicitly bypassed** |
| `VirtualSurroundApo` / AdaptiveSpatialAudioRenderer | A/C: exact June binaries/PDB + render-thread StackWalks + HRTF bypass code | Not separately reproduced because matching-stereo boundary is identity | **Matching 48-kHz stereo closed to unity-copy transport; spatial/object modes remain conditional** |
| `AudioEng!CAudioLimiter` | A: real audiodg ETW; C: exact ARM64 state machine decoded | Offline exact oracle decoded; not deployed | **Proven live final limiter; unity gain / inactive attack in both June snapshots; not source of normal loudness** |
| Qualcomm lower graph | A: KD/QGPR live graph | Reconstructed protected graph | **High topology confidence** |
| VOL_CTRL gain/mute | A: parameter IDs and live writes | Present | **Known control, not mystery limiter** |
| MSIIR stages | A/C: live graph + ACDB | Loaded in protected graph | **Static-stage confidence high** |
| SPv5 / SP_VI / VI feedback | A: Windows graph; live Linux logs | Active with both VI channels | **Active / substantially reproduced** |
| Graph calibration | A: 107 Windows frames | 106 accepted, one unsupported filtered | **Not literal 107/107 parity** |
| Protection telemetry/result callbacks | A: Windows evidence | Partial on Linux | **Observability gap** |

## What the July Firefox/YouTube dump proves

Dump:

```text
SP11-PROJECT/Gemini/dumps/WINDOWS_KERNEL_DUMP/sp11_kernel_mcp_windbg.dmp
```

Directly recovered facts:

- Firefox is associated with the built-in speaker endpoint in retained audio
  session material.
- A genuine live `DolbyAPOvlldp150` runtime page survives.
- Its object geometry resolves the July VLLDP state near
  `0x0000018BD668C360` and DLL base near `0x00007FFBC9F00000`.
- Named VLLDP controls are `deviation=96`, `slow_enable=1`, `slow_mix=103`.
- Those values identify the **Movie/Music VLLDP family**, not Dynamic.
- The retained page also gives live `peak_level=0` and `target_power=-80`.

The `.dump /k` is **not** a complete user-mode oracle. The VR page needed to
read every control did not survive, and the dump alone cannot certify the
whole Windows audio engine or lower DSP pipeline.

## What the old exact-stimulus Windows oracle proves

Recovered pair:

```text
sp11-known-input-stimulus-48k.wav
known-input/windows-loopback-20260518-153312.wav
```

The May DLL hashes match the July/August VLLDP/VR/DAX3 binaries. With latency
alignment and one global gain fit, the current original-code chain reaches
roughly 0.96 whole-waveform correlation. Movie is narrowly best in the current
candidate set; warm history can raise Movie to about 0.967.

Actor ablation is informative:

```text
VLLDP only       ~0.941 correlation
VLLDP -> VR      ~0.963 correlation (cold Movie candidate)
```

Therefore VR is acoustically material and the recovered order is independently
supported by waveform behavior.

The oracle also exposes a **real unresolved residual**. At loud 75-Hz steps,
Windows reaches a repeatable near-full-scale ceiling and produces much stronger
odd harmonics (especially H3/H5) than the current Linux VLLDP->VR output. Warm
history explains part of the mismatch but not this nonlinear signature.

This residual is the current parity target. Do not hide it with guessed EQ or a
hand-written bass enhancer.


## June full audiodg dump: live VR source-of-truth

The June-8 full `audiodg.exe` process dumps retain the actual live
`DolbyApoVr` wrapper and core. A unique relocated `LibWrapperVr` vtable pointer
resolves the object at `0x2453913C2F0` and core at `0x245391DD808`; its fill
advances `96 -> 160` across the two captures.

Direct core reads prove Bass Enhancer and Bass Extraction are OFF in the actual
sample-processing core. Comparing all seven replayed profiles against stable
profile-discriminating fields identifies the live core as **Music**, with the
Linux Music replay matching all `34/34` stable scalar discriminators.

The two Windows core snapshots also expose large evolving history/state regions,
while a stable but different `core+0xB48..0xC38` tuning table was experimentally
copied into Linux and produced bit-identical audio. Subsequent A/B work traced
the `core+0x120` discrepancy to the output-mode setter; omitting it is
bit-transparent for Music. Transplanting the two large live Windows VR history
arrays makes the candidate quieter and slightly less Windows-like, so the
specific June VR history snapshot does not explain the missing pre-limiter
drive. The same June dumps now also yield the unique live VLLDP wrapper, real staging
audio and full main state. Stable VLLDP profile discriminators are 18/18 for
the Movie/Music family; the sibling VR core resolves the complete chain to
Music. After pointer relocation and Windows `CRITICAL_SECTION` bookkeeping are
excluded, the only stable VLLDP tuning mismatch is a disabled sliding-bass
block. Reproducing its exact Windows values through original setters is
bit-identical, closing that discrepancy as dormant. The complete captured
Windows VLLDP state was then replayed at its original heap/module addresses.
Its first-block excess is stored program-history ring-out; after three repeated
997-Hz blocks the Windows-warm state and a fresh Music state converge to the
same steady RMS/peak output to displayed precision. Detail:
`docs/findings/2026-08-05-LIVE-VLLDP-CORE-RECOVERY.md`.

Detail: `docs/findings/2026-08-05-AUDIOENG-LIMITER-AND-LIVE-VR-CORE.md`.

## AudioEng limiter closure

The Windows `CAudioLimiter` is now decoded as a linked stereo look-ahead limiter
with a 0.985 ceiling, 64-frame look-ahead at 48 kHz, and exponential release.
An exact offline replay slightly improves whole-waveform correlation but does
not touch the loud 75-Hz oracle segments because the current Dolby output is
below threshold there. The missing gain/nonlinearity is therefore upstream of
this final limiter rather than created by it alone.


### Full VR state replay / localized lifecycle gap

The complete June VR outer allocation is retained and replays at its exact
Windows heap VA with the VR DLL loaded at its exact captured ASLR base. The
persistent Windows-vs-fresh Music difference has now been localized into the VR
**Volume Leveler / DRC adaptive-history path**, not a missing static profile.
The dominant long-memory arena float is `outer+0x1F1768`: captured Windows
`0.814902425`, fresh Music `0.801979303`. Hardware watchpoints caught original
Dolby `FUN_18006A2D0` updating it with a hysteretic attack/release recurrence.
Fresh original-code Music naturally reaches/exceeds that state under ordinary
music/noise and retains it for minutes through silence.

A Linux host-lifecycle mismatch was then proven: PipeWire filter-chain PAUSED
reset calls LADSPA `activate()` again, while pre-fix `activate()` rebuilt VLLDP
and VR from cold. Original Windows VLLDP/VR `CApoBase::Reset` methods are no-op
success returns. A fixed candidate preserves healthy state on repeated
activation; a 70-second warm/reset A/B changes all 288000 probe samples pre-fix
and zero samples after the fix. Cold-start profile hashes are unchanged.

## Runtime switching model

Windows has more than one kind of state change:

1. signal-processing mode / graph construction (`DEFAULT`, `MEDIA`, `MOVIE`,
   `NOTIFICATION`, etc.);
2. DAX profile and feature changes inside an existing graph;
3. runtime DAX parameter-map rebuilding, including endpoint-volume feedback;
4. state/history inside VLLDP/VR leveler/regulator/maximizer blocks;
5. lower endpoint/protection/gain controls.

A module being loaded is not proof it processes samples. A profile XML value is
not proof it is the final runtime value. Conversely, a runtime nonlinearity is
not proof that the feature whose marketing name sounds similar is enabled.

## Current production boundary

The Linux production host intentionally executes the **proven original Windows
VLLDP and VR processors** and does not add the old hand-written fake-bass DSP,
modern ASAR, or an inferred AudioEng limiter.

That is the correct engineering boundary while the residual is unresolved: do
not improve subjective sound by adding an unproved stage and then call it
parity.

## Current open questions, in priority order

1. Resolve the upstream Windows Music runtime/history state that drives loud
   75-Hz material several dB harder before the now-decoded AudioEng limiter.
2. DAX `vlldp-limiter-gain` public identity and generic Get/Set routing are
   proved: `0x850` -> internal `0x2A` -> `mb_compressor_limiter_gain`. Fresh
   backend analysis groups `0x2A` with telemetry/info parameters, while direct
   June Music limiter recovery finds its real nested current/previous/target
   gains all at `1.0` despite changing envelope history. Finish the exact `0x2A`
   exported-field/units linkage and backend Set semantics; do not treat generic
   front-end Set routing as proof of a writable production knob.
3. Resolve the July Movie-vs-Music ambiguity from an independent retained state
   source if possible.
4. Obtain or reconstruct a state-pinned same-stimulus Windows oracle for final
   waveform certification.
5. Close the one unsupported lower calibration record and protection telemetry
   only if they prove acoustically/materially relevant.

## Change-control rule

A new finding changes this canonical model only when it has a reproducible
A/B/C evidence record. When that happens:

- update this page;
- update the evidence ledger;
- mark the contradicted older note as superseded or historical;
- add a regression/oracle when executable behavior changed;
- commit and push the checkpoint before moving to the next major hypothesis.
