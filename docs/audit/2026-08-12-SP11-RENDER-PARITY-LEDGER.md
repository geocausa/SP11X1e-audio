# SP11 built-in-speaker Windows/Linux render parity ledger — 2026-08-12

This is the canonical playback/render ledger for the Surface Pro 11 (X1E80100) port as of 2026-08-12. It supersedes the earlier broad GREEN sign-off and the contradictory append-only SP7 ledger. The completion gate is **built-in-speaker rendering**. Microphone/input and Bluetooth are intentionally not completion blockers.

Status meanings: **GREEN** = deployed and evidence-backed Windows parity/on-par behavior; **AMBER** = present/working but exact parity or lifecycle proof incomplete; **RED** = known missing/mismatched Windows behavior; **N/A** = intentionally outside this completion gate.

## Overall gate

**RED — built-in-speaker rendering is functional and the steady Dolby core is highly matched, but there are still known audible Windows/Linux mismatches. Do not call playback complete until every RED item below is closed or explicitly reclassified by fresh evidence.**

## 1. Base Linux render path

- [x] **GREEN R01 — Kernel / ASoC platform stack.** Deployed `7.1.5-sp11-cps-v3+`; X1E80100, q6apm/q6prm/APR, SoundWire and WSA884x stack live.
- [x] **GREEN R02 — Internal speaker card/PCM.** Surface Pro 11 ALSA card exposes the expected `MultiMedia1 Playback` PCM.
- [x] **GREEN R03 — UCM speaker route.** SP11-specific UCM is deployed; built-in speaker is the intended active render endpoint.
- [x] **GREEN R04 — PipeWire/WirePlumber integration.** PipeWire 1.6.2 + WirePlumber 0.5.13 healthy; default configured sink is `effect_input.sp11_windows_dolby`.
- [x] **GREEN R05 — Stereo render contract.** Built-in path is two-channel / 48 kHz in the reconstructed Windows-equivalent path.

## 2. Codec / SoundWire / speaker hardware

- [x] **GREEN H01 — Both WSA884x amplifiers active on SoundWire.** Left/right speaker routes are live.
- [x] **GREEN H02 — Digital mute/gain state.** RX0/RX1 mute off and digital volume 81 as deployed.
- [x] **GREEN H03 — PA operating point.** Both PA controls are 24 in the current high-output protected configuration.
- [x] **GREEN H04 — WSA softclip.** Softclip0/1 enabled.
- [x] **GREEN H05 — VISENSE / VI mixers.** VISENSE enabled on both amplifiers and both VI mixers active.
- [x] **GREEN H06 — PBR/protected high-output policy.** Current protected path includes the recovered PBR/current-limit state used for the high-output deployment.
- [~] **AMBER H07 — exact physical L/R identity and per-speaker calibration attribution.** Functional stereo is proven; final physical-channel naming/calibration attribution remains lower-priority exactness work.

## 3. Qualcomm AudioReach graph and calibration

- [x] **GREEN Q01 — Windows-family protected render graph.** Reconstructed root/render/VI/protection topology is deployed and starts successfully.
- [x] **GREEN Q02 — Render endpoint calibration.** Accepted at runtime.
- [x] **GREEN Q03 — VI endpoint calibration.** Accepted at runtime.
- [x] **GREEN Q04 — VOL_CTRL gain/mute transaction path.** Accepted and present in the reconstructed startup sequence.
- [x] **GREEN Q05 — static/full-volume MSIIR calibration transport.** Reviewed REV_0D calibration is accepted at graph startup.
- [x] **GREEN Q06 — channel-mixer calibration.** Accepted.
- [x] **GREEN Q07 — GRAPH_START.** Accepted; protected render graph reaches running state.
- [x] **GREEN Q08 — Qualcomm GSL graph-calibration policy.** `AR_EUNSUPPORTED` graph-calibration result is handled with the same continue policy as Qualcomm GSL rather than treated as a fatal Linux error.

### Volume-dependent MSIIR / loudness contour

- [x] **GREEN Q09 — Windows volume-dependent `MSIIR 0x489e` coefficient selection.** Root cause, exact payload set, qcadcm selector semantics, live injection and persistent runtime selection are now deployed and evidence-backed.
  - Current qcadcm `GetGainTableStepFrmQ28Gain` performs nearest-neighbour selection over the ACDB Q28 endpoint gain table.
  - `GetGraphCkv` maps internal index 0..29 to CKV step 1..30.
  - Internal-speaker endpoint key `0x01000006=1` has 30 gain anchors: mute, then -21 dB through 0 dB in 0.75 dB steps.
  - REV_0D contains 30 distinct `0x489e / 0x08001022` coefficient payloads.
  - `0x08001020`, `0x08001021`, and `0x08001026` are byte-identical across all 30 rows; only `0x08001022` changes.
  - Exact Windows step 30 and exact low-volume step 2 payloads were both accepted live by the deployed DSP with `rc=0` using the existing `SP11 MSIIR Inject` TLV path.
  - Step 2 has a strong Windows loudness contour: approximately +10.8 dB at 60 Hz, +8.7 dB at 100 Hz, while ~1 kHz is about -6 dB relative to step-30 unity. This is a concrete mechanism for the reported missing low-volume bass/fullness.
- [x] **GREEN Q10 — automatic CKV re-selection on endpoint-volume changes / graph restart.** Enabled `sp11-msiir-volume-sync.service` follows the shared DAX postgain state, applies the nearest Windows CKV whenever the graph enters RUNNING or the step changes, and re-applies after the kernel's step-30 startup calibration. Live 25%→50%→25% exercised CKV1→CKV6→CKV1, and all 30 exact rows were accepted by the DSP.
- [~] **AMBER Q11 — WaveSpeaker EQ / Bass Boost / DRC MFX runtime mapping.** Surface INF advertises these effects for DEFAULT/MEDIA/MOVIE/etc., but their active runtime module/parameter mapping is not yet proven. Do not guess-enable them.

## 4. Speaker protection / CPS

- [x] **GREEN P01 — SP configuration/query.** Runtime accepted.
- [x] **GREEN P02 — SPVI configuration/query.** Runtime accepted.
- [x] **GREEN P03 — SPVI R0/T0 calibration.** Recovered calibration accepted.
- [x] **GREEN P04 — SPVI channel/processing mode.** Accepted.
- [x] **GREEN P05 — SP/SPVI tag calibration.** Accepted.
- [x] **GREEN P06 — dual 8 kHz VI feedback.** Both WSA amplifiers expose live 8 kHz / S32 VI feedback into the protection graph.
- [x] **GREEN P07 — CPS feedback coupling.** SP/SPVI enable with VI+CPS feedback accepted; CPS runtime transport is closed as a deployment blocker.
- [x] **GREEN P08 — protected high-output gate.** High-output PA state is only used with protection active.
- [~] **AMBER P09 — passive/protection limiter telemetry equivalence.** Protected operation is proven; exact Windows telemetry/observer semantics remain incomplete.
- [~] **AMBER P10 — exact HLOS CPS payload semantics.** Transport/effect are present; every private field has not been named semantically.
- [~] **AMBER P11 — PBR DP4 scheduling exactness.** Current protected behavior is functional; whether Windows separately schedules every PBR data-port detail remains an exactness question.

## 5. Dolby / AudioEngine userspace processing

- [x] **GREEN D01 — original SP11 Dolby ARM64 code hosted on Linux.** Production bridge executes the recovered shipped Dolby implementation rather than an approximate EQ clone.
- [x] **GREEN D02 — production callback order.** Corrected current path is **VR -> VLLDP**; older VLLDP->VR comments/docs are stale.
- [x] **GREEN D03 — VLLDP adaptive leveler/regulator/compressor processing.** Deployed and exercised.
- [x] **GREEN D04 — VR profile processing.** Deployed.
- [x] **GREEN D05 — effective Windows speaker profile.** Fresh Windows evidence selected Movie for the ordinary internal-speaker reference state; Linux fallback was corrected to Movie and persisted.
- [x] **GREEN D06 — stereo virtualizer bypass.** Windows operator policy bypasses stereo virtualization for ordinary two-channel speaker playback; Linux reproduces that effective behavior.
- [x] **GREEN D07 — named Dolby Bass Enhancer / Virtual Bass.** **N/A as a missing-switch theory:** recovered ordinary Windows states have these named paths disabled too. They are not the current bass-parity fix.
- [x] **GREEN D08 — ASAR/HRTF ordinary-stereo finding.** Later provenance work shows the frozen ordinary stereo ASAR/HRTF path is effectively unity / no active object frames. Do not insert ASAR blindly as a bass fix.
- [x] **GREEN D09 — VLLDP endpoint-volume postgain relation.** Recovered relation `postgain = round(master_volume_dB * 16)` is implemented and runtime-controlled.
- [x] **GREEN D10 — adaptive history preservation.** Linux does not rebuild/reset the Dolby adaptive state on ordinary PipeWire PAUSED transitions where Windows APO Reset is effectively a no-op.
- [x] **GREEN D11 — exact AudioEngine final limiter.** Windows final limiter behavior is reproduced, including the recovered ~-0.13 dBFS ceiling.

## 6. Volume law / user-facing loudness

- [x] **GREEN V01 — Windows endpoint UI taper is reproduced.** Fresh SP11 Windows `IAudioEndpointVolume` capture pinned the nonlinear scalar->dB curve (201 points, 0.005 spacing, endpoint range -75..0 dB). Linux keeps the visible virtual-sink scalar but retapers the hidden downstream ALSA sink to the Windows endpoint gain. At 25%, Linux now measures -20.7474 dB exactly while the visible slider remains 25%. Full reboot and filter-node recreation gates passed.
- [x] **GREEN V02 — endpoint attenuation, Dolby postgain and MSIIR share one Windows-equivalent gain state.** The volume-sync daemon maps the visible scalar through the pinned Windows taper, writes VLLDP `postgain=round(dB*16)`, sets the downstream endpoint gain, and Q10 consumes the same postgain for CKV selection. Live 10/25/50/100% tests produced the expected Windows dB values and CKV1/2/16/30; mute, filter recreation, cold boot and first playback all passed.
- [x] **GREEN V03 — no accidental double attenuation inside the virtual Dolby sink.** Raw PipeWire props show the virtual Dolby sink `softVolumes` at unity; actual soft attenuation is applied at the hardware sink. The problem is taper/mapping, not a second hidden volume multiplier.

## 7. Steady-state waveform parity

- [x] **GREEN W01 — fresh deterministic Windows-vs-Linux Movie oracle.** Same-source/state comparison reached correlation ~0.99999947 with fitted gain ~1.00016 and ~59.8 dB residual SNR; cold-state comparison was even closer (~84.7 dB residual SNR).
- [~] **AMBER W02 — strict sample identity.** The remaining ~60 dB full-file residual is tiny and concentrated around transient/state behavior but is not literal bit identity.
- [ ] **RED W03 — real-browser tonal parity requires fresh A/B after the volume fixes.** The previously reported Windows YouTube fullness/bass advantage was measured before Q09/Q10/V01/V02 existed. Linux now has the exact Windows volume-dependent MSIIR contour and endpoint taper; ordinary browser playback must be re-captured/re-auditioned before escalating to Q11.

## 8. Seek / discontinuity / lifecycle behavior

- [x] **GREEN L01 — Windows seek oracle captured.** Controlled Edge split-tone `currentTime` seeks were captured through Windows loopback.
- [x] **GREEN L02 — recovered Dolby core contains the Windows-like discontinuity dynamics.** Feeding the same abrupt split-tone splice through the deployed Linux Movie bridge reproduces the sharp post-transition attenuation followed by hundreds-ms recovery. The core algorithm itself is not missing this behavior.
- [ ] **RED L03 — real YouTube seek path localized through both OSes; restored fourth POPLESS/headroom control link is not sufficient.** Genuine Firefox/YouTube Linux captures around verified seeks show the transient already at `effect_output.sp11_windows_dolby`; the aligned ALSA hardware-sink monitor is a sample-for-sample copy (scale 1.000000, correlation 1.000000, zero residual). The all-zero APO `SILENT`-flag hypothesis was rejected offline and not deployed. A matched Windows real-YouTube run at the same 25% / -20.74741 dB endpoint state also reaches roughly full-scale (~0.987) in WASAPI loopback after seeks even though the physical Windows speakers sound smoothly capped. This localizes the missing physical smoothing below the Windows loopback / Linux PipeWire monitor boundary. Audit of the reviewed Windows DEFAULT graph found Linux had dropped the fourth module-control link `POPLESS_EQ 0x4664 <-> VOL_CTRL 0x4663`, intent `INTENT_ID_P_EQ_VOL_HEADROOM (0x08001118)`; the generator was fixed and the isolated four-link candidate booted cleanly. On 2026-08-13 the user physically auditioned the four-link candidate at the explicit Windows-equivalent 25% state during verified real-YouTube seeks `47 -> 77 s`, `79 -> 59 s`, and `61 -> 96 s`; the sharp Linux re-entry spike remained. Therefore the restored link is structurally correct but does not close the audible defect by itself and must not be promoted as the fix. The user additionally reports the same class of spike while moving the master-volume slider during ongoing YouTube playback, broadening the defect from seek-only behavior to a more general live control/transition path. Immediate discriminator: isolate runtime volume-dependent MSIIR writes versus ordinary Dolby-postgain/endpoint-taper changes, then continue to lower-DSP SAL_V2/dynamic-limiter or lifecycle work as evidence directs.
- [ ] **RED L03a — live volume-change transient is physically reproducible; runtime MSIIR transition is under direct isolation.** With the protected graph already RUNNING, a controlled 20%->30%->20%->40%->20% sequence completed in ~0.01 s per `wpctl` change when `sp11-msiir-volume-sync.service` was temporarily stopped; with it restored, the same sequence additionally injected CKV `2 -> 6 -> 2 -> 12 -> 2`. Three manual re-injections of the already-active CKV2 payload were then issued at fixed 20% with no slider movement. The software/control-path experiment completed successfully and the service was restored active; the physical listening verdict for those same-CKV injections is still pending and must gate attribution.
- [~] **AMBER L04 — Windows SOFT_PAUSE lifecycle and exact commands recovered; Linux host plumbing remains.** Windows DEFAULT uses iid `0x466b`. Driver disassembly recovers tag `0x04010008`, key `0x01000021`, PAUSE/state-3 -> value `1`, RUN/release/state-4 -> value `0`; STOP also releases any outstanding pause. REV_0D for render subgraph `0xb000007e` maps value `1` to pid `0x0800102e` with zero-length payload and value `0` to pid `0x0800102f` with zero-length payload. The Aug-10 common-GPR Windows capture independently observed both exact SET_CFG headers three times each. PAUSE_COMPLETE `0x0800103f` and RESUME_COMPLETE `0x08001043` are registered separately. The earlier zero-filled `0x0800102d`/`0x0800103e` buffers were GET/readback requests, not commands. Linux already instantiates SOFT_PAUSE/registers events, but q6apm drops non-watermark module events and q6apm-dai does not drive pause/release. Next gate: implement, compile and statically test the bounded SP11 pull-graph lifecycle without installing an unfinished module tonight.
- [x] **GREEN L05 — SOFT_PAUSE is not the direct in-stream seek mechanism.** Controlled Edge seeks did not emit additional `0x466b` transactions; implement it for lifecycle parity, not as a guessed YouTube-seek fix.
- [~] **AMBER L06 — suspend/resume render lifecycle.** Not re-audited against the current Aug-12 Dolby/CPS deployment; must be rechecked before final GREEN sign-off.
- [~] **AMBER L07 — cold first-playback / graph-start latency.** A controlled local playback open on 2026-08-13 did not reach `GRAPH_START` for about 18 seconds. Most of the delay occurred before AudioReach stage programming while the WSA884x devices woke/read VPHX state; one run logged the right amp as 2S at +5 s and the left amp four seconds later as `0x0`/unsupported, while a repeat later detected both as 2S but retained a similar multi-second serialized wake. Once those codec reads completed, the AudioReach startup stages reached `GRAPH_START` essentially immediately. This is a real Linux startup-latency defect and plausibly explains the user's observation that YouTube now takes longer to begin playback; exact Windows latency parity and the SoundWire/runtime-PM root cause remain open.

## 9. Source/provenance/reproducibility

- [x] **GREEN S01 — clean integration source of truth identified.** Current work is on `/home/geoca/Documents/SP11-PROJECT/01-audio-cps-review`, branch `agent/render-parity-20260812`.
- [x] **GREEN S02 — clean Aug-12 render integration history.** Movie correction (`994f41c`), Windows volume-dependent MSIIR (`f73ad55`), and the Windows endpoint-taper implementation are committed on the clean render-parity branch.
- [~] **AMBER S03 — remote publication.** Current clean branch/commit has not been pushed because the Linux GitHub HTTPS credential/token is invalid for noninteractive push. Preserve patches/evidence on SP7 until remote publication is repaired.
- [~] **AMBER S04 — older everyday audio working tree.** `/home/geoca/Documents/SP11-PROJECT/01-audio` is older and heavily dirty/untracked; it must not be treated as deployed production provenance.

## 10. Explicitly outside the current completion gate

- [ ] **N/A O01 — microphone/capture.** Deferred by project priority; current playback gate does not require it.
- [ ] **N/A O02 — Bluetooth audio.** Deferred by project priority.
- [ ] **N/A O03 — external display / USB / other external playback endpoints.** Not part of the current built-in-speaker parity gate unless promoted later.

## Ordered closure queue

1. **W03/Q11 — re-test real browser/YouTube tonal parity:** the exact Windows MSIIR loudness contour and endpoint taper are now deployed. Only if a fresh A/B still shows missing fullness should WaveSpeaker EQ/Bass Boost/DRC runtime controls be traced/deployed.
2. **L03/L03a — close the general physical transient:** the four-link POPLESS/headroom restoration failed the physical seek gate. First finish the direct runtime-MSIIR A/B (including the fixed-volume same-CKV injection listening verdict); if MSIIR injection is not causal, proceed to Windows SAL_V2/dynamic-limiter instances and other lower-DSP transition controls rather than re-running already-closed digital localization.
3. **L04 — implement Windows SOFT_PAUSE lifecycle:** exact state/TKV and generated commands are recovered: pause -> `0x466b/0x0800102e/size 0`, release -> `0x466b/0x0800102f/size 0`, with separate pause/resume completion events. Implement and compile the SP11 pull-graph host lifecycle, preserving persistent STOP/reprepare behavior; do not live-install the unfinished module tonight.
4. **L07 — remove the abnormal cold graph-start latency / WSA wake delay, then L06 cold boot + suspend/resume regression.**
5. **P09-P11/H07/W02 — lower-level exactness closure.** These are final exactness items after the audible REDs are gone.
6. Publish/merge the clean integration branch and archive obsolete contradictory docs.

## Completion rule

Built-in-speaker rendering becomes GREEN only when: (a) no known audible Windows/Linux mismatch remains in ordinary browser/media playback, (b) volume and seek/start-stop lifecycle behavior are evidence-backed, (c) protection remains live, and (d) the deployed binaries/config/kernel are reproducible from the clean source branch.
