# SP11 built-in-speaker Windows/Linux render parity ledger — 2026-08-12

> **2026-08-13 staging update:** V04 and L03b are now **AMBER** rather than
> RED implementation gaps. Combined patch `0048` is built, signed and isolated
> in one-shot GRUB entry `sp11-audio-volume-transaction`; it implements final
> `VOL_CTRL` followed by the exact four-frame OOB GainStep delta. Static,
> exact-release, Wi-Fi bake, initramfs-isolation and userspace rollback checks
> pass. This is not a physical success claim: post-boot transport evidence and
> slider/seek listening still gate GREEN. Persistent fallback remains
> `sp11-audio-cps-v3`. See
> `docs/deployment/2026-08-13-windows-volume-transaction-candidate.md`.
>
> **First live-gate correction:** candidate v1 booted with Wi-Fi and protected
> audio intact. Normal 216-byte transactions succeeded; extended 272-byte rows
> exposed an 8-byte ALSA TLV capacity error and failed quiet with host
> attenuation restored. Patch `0049` corrects the control maximum from 276 to
> 284 bytes and is staged in a replacement one-shot initramfs. V04/L03b remain
> AMBER pending the second-boot large-row and physical tests.

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

- [x] **GREEN Q09 — Windows volume-dependent `MSIIR 0x489e` coefficient selection.** Endpoint-gain -> GainStep selector semantics and all 30 exact coefficient payloads are recovered/deployed and evidence-backed. This GREEN is deliberately limited to **selection/value parity**; the complete four-frame OOB runtime calibration transaction is tracked separately as RED L03b.
  - Current qcadcm `GetGainTableStepFrmQ28Gain` performs nearest-neighbour selection over the ACDB Q28 endpoint gain table.
  - `GetGraphCkv` maps internal index 0..29 to CKV step 1..30.
  - Internal-speaker endpoint key `0x01000006=1` has 30 gain anchors: mute, then -21 dB through 0 dB in 0.75 dB steps.
  - REV_0D contains 30 distinct `0x489e / 0x08001022` coefficient payloads.
  - `0x08001020`, `0x08001021`, and `0x08001026` are byte-identical across all 30 rows; only `0x08001022` changes.
  - Exact Windows step 30 and exact low-volume step 2 payloads were both accepted live by the deployed DSP with `rc=0` using the existing `SP11 MSIIR Inject` TLV path.
  - Step 2 has a strong Windows loudness contour: approximately +10.8 dB at 60 Hz, +8.7 dB at 100 Hz, while ~1 kHz is about -6 dB relative to step-30 unity. This is a concrete mechanism for the reported missing low-volume bass/fullness.
- [x] **GREEN Q10 — automatic CKV re-selection on endpoint-volume changes / graph restart.** Enabled `sp11-msiir-volume-sync.service` follows the shared DAX postgain state, applies the nearest Windows CKV whenever the graph enters RUNNING or the step changes, and re-applies after the kernel's step-30 startup calibration. Live 25%→50%→25% exercised CKV1→CKV6→CKV1, and all 30 exact rows were accepted by the DSP.
- [x] **GREEN Q11 — WaveSpeaker EQ / Bass Boost / DRC INF declarations are not a missing ordinary-speaker stage.** The Surface INF does advertise EQ/Bass Boost/DRC MFX GUIDs across WaveSpeaker host/offload modes, but the reviewed Windows DEFAULT and NOTIFICATION speaker graphs contain neither `DRC 0x07001066` nor `IIR_MBDRC 0x07001017`; their only equalizer-family AudioReach block is the already-deployed POPLESS EQ. The separate Surface Render APO EQ is independently disabled/identity in REV_0D, and the fresh Windows/Linux Movie oracle is already near sample-identical. The exact Qualcomm miniport also contains a concrete `Equalizer` descriptor but not even the INF-advertised DRC GUID, reinforcing that the INF list is capability metadata rather than a literal active-module inventory. Do not guess-enable these effects for ordinary browser/media playback. Re-open only for a specifically proven offload/mode-specific runtime graph. See `docs/findings/2026-08-13-WAVESPEAKER-MFX-NOT-A-MISSING-DEFAULT-STAGE.md`.

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
- [~] **AMBER P11 — PBR DP4 scheduling exactness; Windows capability is concrete but runtime request is still unproven.** Linux carries the WSA8845 PBR current-limit policy but does not select slave DP4 during normal playback. Static review of the exact Windows `qcaucd8380.sys` selector-5 speaker table now identifies state slot 7 as a concrete DP4/PBR-shaped schedule: local slave port `4`, channel mask `0x03`, SampleCtrl1/2 `0xc8/0x00`, HCtrl `0x88`, BlockCtrl1 `0x08`, with the board mapping tying slave DP4 to shared master port 7. The same table contains the already-runtime-proven CPS DP6 slots 13/14. However, the explicit static speaker-control request descriptors recovered so far request slots 1/2/3 (render DAC/COMP/BOOST) plus an unrelated slot-2 path; none directly requests slot 7. The remaining candidate callers obtain descriptors from runtime-populated BSS arrays whose writers are not yet closed. Therefore the DP4 template proves Windows support/capability, **not** that ordinary Windows speaker playback schedules PBR DP4. Do not enable Linux DP4 from template existence alone.

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
- [~] **AMBER V04 — exact final endpoint-gain actuator is staged; physical transition validation is pending.** The Windows evidence and ramp policy described below remain the basis. Superseding combined patch `0048` generates the exact fixed-target `0x4a63/0x08001038` body and orders it before the complete GainStep delta. The signed exact-release module is isolated in one-shot GRUB entry `sp11-audio-volume-transaction`; the accepted kernel, DTB, and persistent CPS-v3 fallback are unchanged. Userspace fails quiet by retaining/restoring host attenuation until the DSP transaction succeeds. This closes the implementation/build gap, not the behavior gate: post-boot transport evidence and physical slider/seek listening are still required. See `docs/deployment/2026-08-13-windows-volume-transaction-candidate.md`.

## 7. Steady-state waveform parity

- [x] **GREEN W01 — fresh deterministic Windows-vs-Linux Movie oracle.** Same-source/state comparison reached correlation ~0.99999947 with fitted gain ~1.00016 and ~59.8 dB residual SNR; cold-state comparison was even closer (~84.7 dB residual SNR).
- [~] **AMBER W02 — strict sample identity.** The remaining ~60 dB full-file residual is tiny and concentrated around transient/state behavior but is not literal bit identity.
- [ ] **RED W03 — real-browser tonal parity requires fresh A/B after the volume fixes.** The previously reported Windows YouTube fullness/bass advantage was measured before Q09/Q10/V01/V02 existed. Linux now has the exact Windows volume-dependent MSIIR contour and endpoint taper; ordinary browser playback must be re-captured/re-auditioned before escalating to Q11.

## 8. Seek / discontinuity / lifecycle behavior

- [x] **GREEN L01 — Windows seek oracle captured.** Controlled Edge split-tone `currentTime` seeks were captured through Windows loopback.
- [x] **GREEN L02 — recovered Dolby core contains the Windows-like discontinuity dynamics.** Feeding the same abrupt split-tone splice through the deployed Linux Movie bridge reproduces the sharp post-transition attenuation followed by hundreds-ms recovery. The core algorithm itself is not missing this behavior.
- [ ] **RED L03 — real YouTube seek path localized through both OSes; restored fourth POPLESS/headroom control link is not sufficient.** Genuine Firefox/YouTube Linux captures around verified seeks show the transient already at `effect_output.sp11_windows_dolby`; the aligned ALSA hardware-sink monitor is a sample-for-sample copy (scale 1.000000, correlation 1.000000, zero residual). The all-zero APO `SILENT`-flag hypothesis was rejected offline and not deployed. A matched Windows real-YouTube run at the same 25% / -20.74741 dB endpoint state also reaches roughly full-scale (~0.987) in WASAPI loopback after seeks even though the physical Windows speakers sound smoothly capped. This localizes the missing physical smoothing below the Windows loopback / Linux PipeWire monitor boundary. Audit of the reviewed Windows DEFAULT graph found Linux had dropped the fourth module-control link `POPLESS_EQ 0x4664 <-> VOL_CTRL 0x4663`, intent `INTENT_ID_P_EQ_VOL_HEADROOM (0x08001118)`; the generator was fixed and the isolated four-link candidate booted cleanly. On 2026-08-13 the user physically auditioned the four-link candidate at the explicit Windows-equivalent 25% state during verified real-YouTube seeks `47 -> 77 s`, `79 -> 59 s`, and `61 -> 96 s`; the sharp Linux re-entry spike remained. Therefore the restored link is structurally correct but does not close the audible defect by itself and must not be promoted as the fix. The user additionally reports the same class of spike while moving the master-volume slider during ongoing YouTube playback, broadening the defect from seek-only behavior to a more general live control/transition path. Immediate discriminators now remain the pending runtime-MSIIR listening A/B and V04 final-`VOL_CTRL` actuator A/B. V04 is a proven Windows/Linux volume-boundary mismatch and is directly relevant to the slider spike, but its seek linkage remains unproven.
- [ ] **RED L03a — live volume-change transient is physically reproducible; runtime MSIIR transition listening discriminator remains pending.** With the protected graph already RUNNING, a controlled 20%->30%->20%->40%->20% sequence completed in ~0.01 s per `wpctl` change when `sp11-msiir-volume-sync.service` was temporarily stopped; with it restored, the same sequence additionally injected CKV `2 -> 6 -> 2 -> 12 -> 2`. Three manual re-injections of the already-active CKV2 payload were then issued at fixed 20% with no slider movement. The software/control-path experiment completed successfully and the service was restored active; the physical listening verdict for those same-CKV injections is explicitly **pending** while the user is away and must gate attribution. A further deployed-topology audit closed one tempting omission: the exact booted Headroom-Test topology (SHA-256 `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`) contains the second serial MSIIR `0x48a1` with `0x08001020`, `0x08001021`, its full 164-byte `0x08001022` real-biquad payload, and enable `0x08001026=1`. Thus `0x48a1` was not accidentally omitted; `0x489e` remains the Windows-evidence-backed volume-dependent MSIIR stage.
- [~] **AMBER L03b — exact Windows GainStep transaction is implemented and staged; live validation is pending.** The full REV_0D sweep and recovered GSL/ACDB semantics identify the four ordered `0x489e` records `0x08001020`, `0x08001021`, `0x08001022`, `0x08001026`, sent as one 216/272-byte OOB APM SET_CFG after final `VOL_CTRL`. Candidate patch `0048` structurally validates and sends that complete group through the protected graph's existing OOB mapping; arbitrary targets, malformed payloads, nonzero padding, and idle graphs are rejected. Static checks, exact-release build/provenance, signing, initramfs isolation, Wi-Fi bake checks and software regressions pass. Post-reboot DSP evidence and physical slider/seek listening still gate GREEN. See `docs/deployment/2026-08-13-windows-volume-transaction-candidate.md`.
- [~] **AMBER L04 — Windows SOFT_PAUSE lifecycle exact; isolated Linux host patch is clean and exact-release buildable, but remains uninstalled.** Windows DEFAULT uses iid `0x466b`. Driver disassembly recovers tag `0x04010008`, key `0x01000021`, PAUSE/state-3 -> value `1`, RUN/release/state-4 -> value `0`; STOP also releases any outstanding pause. REV_0D for render subgraph `0xb000007e` maps value `1` to pid `0x0800102e` with zero-length payload and value `0` to pid `0x0800102f` with zero-length payload. The Aug-10 common-GPR Windows capture independently observed both exact `APM_CMD_SET_CFG` headers three times each, validating the isolated implementation's APM SET_CFG route. PAUSE_COMPLETE `0x0800103f` and RESUME_COMPLETE `0x08001043` are registered separately. Current topology GET data gives rampdown 20 ms and downstream delay 25 ms; with Windows' +5 ms allowance the implementation now derives its 50 ms completion bound explicitly as `20+25+5` rather than a magic constant. The canonical patch `patches/0045-q6apm-sp11-windows-soft-pause-lifecycle.patch` has SHA-256 `9dc808bbbf4dbe5240bd4bec4282a0e6fd1b956c657258c2f0ba24489d0dd05e` and passes strict `checkpatch` with 0 errors / 0 warnings / 0 checks. After regenerating isolated build metadata with `LOCALVERSION=+`, candidate `snd-q6apm.ko` and `q6apm-dai.ko` both report exact live vermagic `7.1.5-sp11-cps-v3+`; no module was installed. They are unsigned while the live modules use the build-time autogenerated kernel key, so signing plus live lifecycle regression remain deployment gates.
- [x] **GREEN L05 — SOFT_PAUSE is not the direct in-stream seek mechanism.** Controlled Edge seeks did not emit additional `0x466b` transactions; implement it for lifecycle parity, not as a guessed YouTube-seek fix.
- [~] **AMBER L06 — suspend/resume render lifecycle.** Not re-audited against the current Aug-12 Dolby/CPS deployment; must be rechecked before final GREEN sign-off.
- [~] **AMBER L07 — cold first-playback latency root cause localized to duplicate WSA884x regcache replay.** Muted local playback reproduces the delay deterministically: 18.627 s from `pw-play` launch to ALSA `RUNNING` with the SoundWire manager and both WSA slaves runtime-suspended; a repeat under tracing measured 18.464 s. Temporarily pinning the manager+slaves runtime-active reduced the same test to 7.310 s, proving autosuspend contributes but is not the whole defect. Function-graph tracing then showed four WSA-scale `regcache_sync()` calls of roughly 4.28-4.60 s each. Stack tracing proves two are from `wsa884x_runtime_resume()` (one per amp) and two more are from the subsequent SoundWire attach IRQ -> `wsa884x_update_status()` path (one per amp): **two full dirty-cache replays per amplifier on one ordinary cold stream open**. Source matches the trace: runtime suspend marks the whole cache dirty; runtime resume syncs it; an UNATTACHED status again clears `hw_init`/marks dirty; ATTACHED syncs the cache again before `wsa884x_init()`. Supplies/reset remain resident across this runtime-PM path and the codec advertises simple clock-stop capability. Earlier attribution of the multi-second gaps to the VPHX read was imprecise: VPHX is logged only after the attach-side cache replay. Occasional left `VPHX=0x0` remains a separate wake-quality observation. This fully explains the reported slow first audio/YouTube start; it does not yet explain warm seek/volume spikes. See `docs/findings/2026-08-13-WSA884X-COLD-START-DOUBLE-REGCACHE-REPLAY.md`. A conservative isolated candidate now removes only the unconditional `regcache_mark_dirty()` from ordinary `wsa884x_runtime_suspend()`, preserving cache-only write tracking and leaving the UNATTACHED/ATTACHED full-restore fallback intact. Regmap itself documents `regcache_mark_dirty()` for hardware reset/power-loss cases; cache-only writes already dirty the cache for later sync. Canonical patch `patches/0046-ASoC-wsa884x-avoid-full-cache-dirty-on-clock-stop.patch` has SHA-256 `b653431eb855d7dccc709841f2104fb20a519f15a7756732dac2ae2e37ae4ba2`, passes strict `checkpatch` with 0 errors / 0 warnings / 0 checks, and its rebuilt `snd-soc-wsa884x.ko` reports exact live vermagic `7.1.5-sp11-cps-v3+`. It remains unsigned/uninstalled; muted cold-start timing on a signed boot candidate is the next live gate. See `docs/findings/2026-08-13-WSA884X-COLD-START-DOUBLE-REGCACHE-REPLAY.md`.

## 9. Source/provenance/reproducibility

- [x] **GREEN S01 — clean integration source of truth identified.** Current work is on `/home/geoca/Documents/SP11-PROJECT/01-audio-cps-review`, branch `agent/render-parity-20260812`.
- [x] **GREEN S02 — clean Aug-12 render integration history.** Movie correction (`994f41c`), Windows volume-dependent MSIIR (`f73ad55`), and the Windows endpoint-taper implementation are committed on the clean render-parity branch.
- [x] **GREEN S03 — remote publication and candidate preservation.** GitHub authentication is repaired. The clean `agent/render-parity-20260812` branch is the publication target, and isolated soft-pause, WSA regcache and final-VOL_CTRL candidates plus the safe actuator A/B tooling are preserved under tracked `patches/`, `tools/` and `tests/` paths. Generated unsigned modules remain excluded and uninstalled.
- [~] **AMBER S04 — older everyday audio working tree.** `/home/geoca/Documents/SP11-PROJECT/01-audio` is older and heavily dirty/untracked; it must not be treated as deployed production provenance.

## 10. Explicitly outside the current completion gate

- [ ] **N/A O01 — microphone/capture.** Deferred by project priority; current playback gate does not require it.
- [ ] **N/A O02 — Bluetooth audio.** Deferred by project priority.
- [ ] **N/A O03 — external display / USB / other external playback endpoints.** Not part of the current built-in-speaker parity gate unless promoted later.

## Ordered closure queue

1. **W03 — re-test real browser/YouTube tonal parity when listening is available:** the exact Windows MSIIR loudness contour and endpoint taper are deployed. Q11 is closed as a false generic missing-stage lead; do not add WaveSpeaker EQ/Bass Boost/DRC without new mode-specific runtime evidence.
2. **V04/L03/L03a/L03b — close the general physical transient:** the four-link POPLESS/headroom restoration failed the physical seek gate. The exact Windows endpoint-volume transaction is now structurally recovered: final `VOL_CTRL 0x4a63` with its 10 ms / 1000 us / curve-3 ramp, then the selected GainStep as one 216/272-byte OOB four-frame `0x489e` delta (`0x1020/0x1021/0x1022/0x1026`). Build/static-test that combined path while the user is away; install/listen only when they are physically present. Do not re-run already-closed digital localization or revive superseded limiter assumptions.
3. **L04 — live-gate the clean Windows SOFT_PAUSE host lifecycle candidate:** patch/style, evidence-derived timing, and exact `7.1.5-sp11-cps-v3+` build identity are now closed. Sign/stage only when an explicit live regression window is appropriate; preserve persistent STOP/reprepare behavior and verify pause/resume completion events before promotion.
4. **L07 — live-gate candidate A for the stack-proven duplicate WSA884x regcache replay:** the conservative no-runtime-`mark_dirty` patch is clean and exact-release buildable. Sign/stage only for a muted timing regression; expected result is removal of the runtime-resume full replay while retaining the attach-side context-loss restore. Then perform L06 cold boot + suspend/resume regression.
5. **P09-P11/H07/W02 — lower-level exactness closure.** These are final exactness items after the audible REDs are gone.
6. Publish/merge the clean integration branch and archive obsolete contradictory docs.

## Completion rule

Built-in-speaker rendering becomes GREEN only when: (a) no known audible Windows/Linux mismatch remains in ordinary browser/media playback, (b) volume and seek/start-stop lifecycle behavior are evidence-backed, (c) protection remains live, and (d) the deployed binaries/config/kernel are reproducible from the clean source branch.
