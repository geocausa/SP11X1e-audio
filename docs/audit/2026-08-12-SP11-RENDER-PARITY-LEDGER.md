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

- [ ] **RED V01 — Windows endpoint UI taper is not reproduced.** Fresh Windows COM measurement gives endpoint scalar 0.25 -> about -20.747 dB. Current Linux PipeWire/WirePlumber 0.25 exposes linear channel gain 0.015625 -> -36.124 dB. The Linux UI therefore attenuates much more strongly at the same nominal percentage.
- [~] **AMBER V02 — volume law and Dolby/MSIIR share one endpoint-gain source, but that source still uses the Linux taper.** Dolby postgain and the new MSIIR selector now follow the same recovered endpoint-dB state; final parity requires V01 to make the actual attenuation itself Windows-equivalent.
- [x] **GREEN V03 — no accidental double attenuation inside the virtual Dolby sink.** Raw PipeWire props show the virtual Dolby sink `softVolumes` at unity; actual soft attenuation is applied at the hardware sink. The problem is taper/mapping, not a second hidden volume multiplier.

## 7. Steady-state waveform parity

- [x] **GREEN W01 — fresh deterministic Windows-vs-Linux Movie oracle.** Same-source/state comparison reached correlation ~0.99999947 with fitted gain ~1.00016 and ~59.8 dB residual SNR; cold-state comparison was even closer (~84.7 dB residual SNR).
- [~] **AMBER W02 — strict sample identity.** The remaining ~60 dB full-file residual is tiny and concentrated around transient/state behavior but is not literal bit identity.
- [ ] **RED W03 — real-browser tonal parity.** User-observed Windows YouTube still has more fullness/bass than Linux. Q09/Q10 and V01/V02 are now concrete causes/candidates that the deterministic oracle did not exercise.

## 8. Seek / discontinuity / lifecycle behavior

- [x] **GREEN L01 — Windows seek oracle captured.** Controlled Edge split-tone `currentTime` seeks were captured through Windows loopback.
- [x] **GREEN L02 — recovered Dolby core contains the Windows-like discontinuity dynamics.** Feeding the same abrupt split-tone splice through the deployed Linux Movie bridge reproduces the sharp post-transition attenuation followed by hundreds-ms recovery. The core algorithm itself is not missing this behavior.
- [ ] **RED L03 — live Linux browser seek path.** Real Firefox/YouTube still audibly spikes on seek. The first automated Firefox post-Dolby capture created no PipeWire audio stream and is invalid evidence; a genuine live browser capture is still required to localize the difference.
- [ ] **RED L04 — Windows SOFT_PAUSE start/stop lifecycle.** Windows DEFAULT graph uses iid `0x466b`; fresh KD captured `0x0800102d` (24 bytes) followed by `0x0800103e` (4-byte zero payload) on stream state transitions. Linux does not yet drive this lifecycle.
- [x] **GREEN L05 — SOFT_PAUSE is not the direct in-stream seek mechanism.** Controlled Edge seeks did not emit additional `0x466b` transactions; implement it for lifecycle parity, not as a guessed YouTube-seek fix.
- [~] **AMBER L06 — suspend/resume render lifecycle.** Not re-audited against the current Aug-12 Dolby/CPS deployment; must be rechecked before final GREEN sign-off.

## 9. Source/provenance/reproducibility

- [x] **GREEN S01 — clean integration source of truth identified.** Current work is on `/home/geoca/Documents/SP11-PROJECT/01-audio-cps-review`, branch `agent/render-parity-20260812`.
- [x] **GREEN S02 — Movie correction and fresh waveform evidence committed.** Local commit `994f41c` is the current Aug-12 render integration baseline.
- [~] **AMBER S03 — remote publication.** Current clean branch/commit has not been pushed because the Linux GitHub HTTPS credential/token is invalid for noninteractive push. Preserve patches/evidence on SP7 until remote publication is repaired.
- [~] **AMBER S04 — older everyday audio working tree.** `/home/geoca/Documents/SP11-PROJECT/01-audio` is older and heavily dirty/untracked; it must not be treated as deployed production provenance.

## 10. Explicitly outside the current completion gate

- [ ] **N/A O01 — microphone/capture.** Deferred by project priority; current playback gate does not require it.
- [ ] **N/A O02 — Bluetooth audio.** Deferred by project priority.
- [ ] **N/A O03 — external display / USB / other external playback endpoints.** Not part of the current built-in-speaker parity gate unless promoted later.

## Ordered closure queue

1. **V01/V02 — complete Windows volume behavior:** automatic `0x489e` CKV selection is now deployed; reproduce the Windows endpoint taper so the already shared Dolby/MSIIR gain state is driven by Windows-equivalent actual attenuation.
2. **W03/Q11 — re-test real YouTube tonal parity:** only if fullness remains after the exact volume-dependent MSIIR + taper fix, trace WaveSpeaker EQ/Bass Boost/DRC runtime controls.
3. **L03 — capture and fix genuine live Firefox/YouTube seek path:** compare browser input, post-Dolby output and hardware-bound stream around a seek.
4. **L04 — implement Windows SOFT_PAUSE start/stop transaction:** exact `0x466b` payloads already captured; prove lifecycle parity without conflating it with in-stream seek.
5. **L06 — cold boot + suspend/resume regression.**
6. **P09-P11/H07/W02 — lower-level exactness closure.** These are final exactness items after the audible REDs are gone.
7. Publish/merge the clean integration branch and archive obsolete contradictory docs.

## Completion rule

Built-in-speaker rendering becomes GREEN only when: (a) no known audible Windows/Linux mismatch remains in ordinary browser/media playback, (b) volume and seek/start-stop lifecycle behavior are evidence-backed, (c) protection remains live, and (d) the deployed binaries/config/kernel are reproducible from the clean source branch.
