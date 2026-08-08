# Dolby findings index and supersession map — 2026-08-05

This index prevents historical reverse-engineering hypotheses from competing
with newer runtime evidence. Do not delete old findings: they preserve the
experiment trail. Read them according to the status below.

## Start here

1. `2026-08-05-CANONICAL-DOLBY-PIPELINE.md` — current topology, confidence and
   open questions.
2. `2026-08-05-DOLBY-EVIDENCE-LEDGER.md` — high-value evidence identities and
   preservation checkpoints.
3. `../deployment/2026-08-05-DOLBY-PRODUCTION-MANIFEST.md` — what source is
   actually production and how it maps to the installed host.
4. `../findings/2026-08-05-PIPELINE-COMPLETENESS-RECHECK.md` — detailed
   actor-by-actor completeness audit.
5. `../findings/2026-08-05-DOLBY-RUNTIME-GAIN-LIMITER-RECHECK.md` — current
   nonlinear/limiter investigation.

## Current evidence-bearing findings

| Document | Status / use |
|---|---|
| `../audit/2026-08-08-DOLBY-INTEGRATION-STATUS.md` | **Current integration entry point**: separates proved behavior, hypotheses, publication policy and the isolated original-ASAR execution gate. |
| `2026-08-08-ASAR-DUAL-LANE-PRE-VLLDP-CLOSURE.md` | **Current normal-shared pre-VLLDP closure**: proves unity channel bed + VirtualSurround object lane, code-proves the HRTF encoder merge, records the level-dependent 75-Hz/997-Hz transfer oracle, and preserves successful offline activation of the original speaker encoder + DAP module. |
| `2026-08-07-STATE-PINNED-DYNAMIC-VLLDP-PRELIMITER-DRIVE.md` | **Current pre-limiter localization**: state-pinned Dynamic Windows run reproduces May-like H3/H5 with `peak=0`, `system-gain=0`, negative postgain; live normal final limiter attenuates and inner VLLDP staging is already ~3 dB hotter than source. |
| `2026-08-04-DOLBY-LIVE-KDNET-HOT-PATH.md` | **Canonical primary runtime evidence** for hot DAX/VLLDP/VR callbacks and object addresses. |
| `2026-08-05-DOLBY-DYNAMIC-STATE-ORACLE.md` | **Current oracle** for deterministic VLLDP state construction/profile validation. |
| `2026-08-05-DOLBY-NATIVE-PROFILES.md` | **Current** recovered profile controls and Personalize/GEQ behavior. |
| `2026-08-05-DOLBY-NATIVE-LIVE-COMPLETION.md` | **Current deployment history**, but final parity remains explicitly open. |
| `2026-08-05-PIPELINE-COMPLETENESS-RECHECK.md` | **Current detailed completeness matrix.** |
| `2026-08-05-DOLBY-RUNTIME-GAIN-LIMITER-RECHECK.md` | **Current investigation** of live DAX gain feedback, odd-harmonic residual and AudioEng limiter lead. |
| `2026-08-06-MAY18-KNOWN-INPUT-ALIGNMENT-CORRECTION.md` | **Current correction** of the May-18 one-second periodic alignment error; recovers the second LFS loopback and restores the real loud `-6/-3 dBFS` nonlinear oracle. |
| `2026-08-06-MAY18-NONLINEAR-RESIDUAL-LOCALIZATION.md` | **Current residual localization**: realistic VLLDP endpoint postgain closes most of the seven-step amplitude curve; AudioEng is only the final 0.985 ceiling; the loud odd-harmonic residual remains upstream. Its earlier `peak-level` interpretation is refined by the dedicated ceiling finding below. |
| `2026-08-06-VLLDP-PEAK-LEVEL-LIMITER-CEILING.md` | **Peak-level semantic/provenance closure**: `peak-level` is a 1/16-dB final VLLDP limiter ceiling (DAX `0x842 -> 0x21`; VLLDP `core+0xDD4 -> DD8`); exact A/B makes the May-like effect entirely the lowered DD8 ceiling, while shipped/live state and 93 preserved DAX setter calls provide no evidence of a nonzero runtime write. |
| `2026-08-06-MAY-RUNTIME-STATE-AND-VLLDP-TELEMETRY-CLOSURE.md` | **Post-peak residual boundary**: May profile/volume provenance is absent from surviving artifacts; all exact profiles remain too linear; the normal VLLDP final limiter stays unity with ~2.47 dB headroom; stale `C60` scalar provenance is corrected; DAX `0x850`/software multiband gain telemetry is separated from the scalar final limiter by a 2,945-block exact A/B. |
| `2026-08-06-VLLDP-MB-COMPRESSOR-INPUT-AND-WARM-STATE-CLOSURE.md` | **MB-compressor input/lifecycle closure**: exact `FUN_180021E80` scalar/control provenance is mapped; the nested compressor object at `core+0x4F30` was outside the old warm-core overlay, but both authentic June objects are bit-transparent under conservative and full non-pointer transplants. Remaining work moves to sample-domain localization inside VLLDP, not more public-parameter sweeps. |
| `2026-08-06-VLLDP-FINAL-LIMITER-SAMPLE-DOMAIN-LOCALIZATION.md` | **Direct nonlinear-source localization**: debugger-captured pre-limiter PCM is ~-98 dBc H3 and bit-identical for peak 0/-48; the original final VLLDP limiter creates ~-35.65 dBc H3 when forced/overdriven. Exact profiles and the full meaningful nonpositive endpoint-postgain range never engage the real peak=0 limiter, leaving ~2.47 dB headroom. Remaining gap is historical Windows state provenance, not a missing Linux public control. |
| `2026-08-05-VLLDP-POSTGAIN-AND-LIMITER-STATE-CORRECTION.md` | **Current correction** closing the false wrapper `+0x7C0` history lead and recovering live nested VLLDP limiter gain state. |
| `2026-08-05-WINDOWS-DYNAMIC-MUSIC-INPLACE-LIFECYCLE.md` | **Lifecycle proof** that Windows main-profile changes retune existing Dolby endpoints via parameter diffs while active VLLDP/VR processing continues. |
| `2026-08-06-DAX3-WRAPPER-DIRECT-PROCESSING-PROOF.md` | **Hot-path elimination**: 639 live ETL stacks hit the equal-rate direct inner-call return site; DAX3 adds no sample-domain SRC/gain around VLLDP/VR on the speaker path. |
| `2026-08-06-AUDIOENG-ASAR-STEREO-IDENTITY-PATH.md` | **Bed-only closure with Aug-8 correction**: its proof that the matching stereo channel bed is a 48k->48k unity bypass remains valid, but its whole-ASAR identity conclusion assumed no competing encoded/object output. Use the Aug-8 dual-lane finding for the current normal-shared model. |
| `2026-08-06-SP11-STEREO-VIRTUALIZER-BYPASS-PARITY.md` | **VR stereo-policy closure**: DAX per-profile bypass -> endpoint PID1 -> original DolbyAPOVR wrapper forces effective mode 1 on 2ch; Linux direct-core bridge corrected and independently validated against Windows side/mid geometry. |
| `2026-08-05-AUGUST-BASS-RUNTIME-RECHECK.md` | **Current correction** separating named Bass Enhancer from other loudness/bass actors. |
| `2026-08-05-FAKE-BASS-STATUS.md` | **Current terminology boundary** between old hand-written fake bass, decoded Dolby harmonic synth and actual production path. |
| `2026-08-05-ASAR-STEADY-SPEAKER-BOUNDARY.md` | **Historical boundary** from pre-exact-AudioEng evidence; use the Aug-6 ASAR identity finding for matching stereo, while retaining this for experiment history. |
| `2026-08-05-WINDOWS-KNOWN-INPUT-CURRENT-CHAIN.md` | **Current historical waveform-oracle record.** |
| `2026-08-05-CUSTOM1-GEQ-FORENSIC-RECOVERY.md` | **Closed forensic result** for missing historical Custom1 values. |
| `2026-08-05-MINIMAL-WINDOWS-RUNTIME-CLOSEOUT.md` | **Future Windows runtime plan**, to be updated when targets are resolved. |
| `2026-08-04-DOLBY-ETW-KERNEL-CORRELATION.md` | **Historical evidence with explicit corrections**; useful for mode/profile correlation. |
| `2026-07-23-kdnet-windows-live-graph.md` | **Primary lower AudioReach topology evidence.** |
| `2026-08-02-windows-graph-calibration-warning-policy.md` | **Current lower calibration policy evidence.** |
| `2026-08-01-protection-event-return-path.md` | **Current lower protection telemetry boundary**, subject to later kernel changes. |

## Superseded or historical Dolby models

The following files are preserved because they contain useful disassembly,
measurements or intermediate discoveries, but their architecture/executive
conclusions must not be used as the current model.

| Document | Why historical | Use instead |
|---|---|---|
| `../findings/2026-08-01-dolby-integration-map.md` | Good binary/import inventory, but predates Aug-4 live callback recovery and later runtime-map work. | Canonical pipeline + Aug-4 hot-path finding. |
| `../findings/2026-08-04-DOLBY-PORT-STATE-OF-PLAY.md` | Describes the modern `DolbyAudioProcessing.dll` reconstruction as the main port target. Later hardware evidence made the persistent DAX/VLLDP150/VR path primary for tested stereo media. | Canonical pipeline + native live completion. |
| `../findings/2026-08-04-DOLBY-NATIVE-CHAIN-PROGRESS.md` | Its `DAPVR -> VLLDP -> limiter` architecture and “missing modern DAPVR” target are superseded by live `VLLDP -> VR`. | Aug-4 hot path + pipeline completeness recheck. |
| Older fitted/hand-written leveler/regulator/fake-bass notes | Useful algorithm archaeology but no longer production parity implementation. | Original-code VLLDP/VR bridge and fake-bass status. |
| Any note claiming VLLDP only drives Qualcomm MSIIR | Superseded by hardware-hot VLLDP sample-processing callback and successful original-code replay. | Aug-4 hot path + canonical pipeline. |
| Any note calling July `.dump /k` entirely devoid of user-runtime pages | Too strong; one genuine VLLDP runtime page was recovered. | Pipeline completeness recheck. |

## Source-code status classes

The repository intentionally keeps old implementations for reproducibility. Use
these labels when reading `dolby-port/`:

- **PRODUCTION:** part of `sp11_dolby_windows_chain_ladspa.c` build or deployment.
- **ORACLE/TEST:** proves ABI, state, scheduling, chunk invariance or waveform
  behavior; not installed.
- **RE PROBE:** exploratory program that executes/inspects original code; not an
  audio product path.
- **HISTORICAL REIMPLEMENTATION:** hand-decoded or hand-written DSP retained for
  archaeology/ablation; not authoritative when original Windows code is
  available.
- **GENERATED BINARY:** local test output; should not be used as provenance.

The exact file-level production list is maintained in the production manifest.

## Rule for new findings

Do not create a new “final architecture” document for every hypothesis. New
experiments normally update:

1. the specific detailed finding;
2. the evidence ledger if the result is high value;
3. the canonical pipeline if the system model changes.

Create a separate finding only when the experiment has enough independent
context/evidence to be reusable later.
