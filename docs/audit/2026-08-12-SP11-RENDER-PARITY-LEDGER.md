# SP11 built-in-speaker Windows/Linux render parity ledger — 2026-08-12

> **2026-08-14 physical-listening correction:** the user has now auditioned
> ordinary YouTube playback on the live `7.1.5-sp11-render-parity-v2+` build.
> The paused-media fragment on a notification wake remains fixed, and ordinary
> live volume tracking behaves correctly.  A sharp slider/transition spike is
> still audible, however, and overall Linux tonality is reported as much better
> than two weeks earlier but still **not close to Windows parity**.  W03 is
> therefore a confirmed physical failure, not a pending A/B.  Because the
> state-matched Windows/Linux Dolby waveform is already nearly identical, the
> next discriminator is the downstream AudioReach/WSA8845/protection state at a
> matched volume, not an invented userspace EQ or psychoacoustic block.  This
> begins with re-analysis of the already-preserved Windows QGPR corpus and the
> Aug-10/11 qcaucd captures (including 328 driver-submitted SoundWire FIFO
> writes), not an automatic request for another Windows capture.
> Suspend/resume is explicitly deferred to a separate investigation and is not
> part of the current built-in-speaker sound-quality gate.

> **2026-08-14 consolidated-candidate update:** isolated GRUB entry
> `sp11-audio-render-parity` now stages one coherent
> `7.1.5-sp11-render-parity+` build containing the already-live CPS/VI and
> volume-transaction work, Windows SOFT_PAUSE, the WSA ordinary-clock-stop
> correction and the exact four-link DEFAULT topology under a unique sound
> model.  All 7,886 modules have matching vermagic and signatures; Phase91
> touch, WCN7850 Wi-Fi, OLED and the complete audio closure are verified in the
> initramfs; 134 tests pass; GRUB syntax passes.  The saved entry remains
> `sp11-audio-cps-v3`, `next_entry` is empty, and no reboot occurred.  This
> closes build/deployment provenance only: L03b/L04/L07 remain AMBER until a
> deliberate one-shot boot and their listed runtime/physical gates.  See
> `docs/deployment/2026-08-14-render-parity-candidate.md`.

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
>
> **Second live gate passed at the transport layer:** corrected srcversion
> `ECFA21430839C02C9138786` accepted repeated 216-byte rows and all three
> 272-byte rows (GainSteps 3, 9, and 24) on a running graph. The combined
> service remained active, Wi-Fi remained connected, no runtime transaction
> failed, and the visible volume was returned to 15%. Physical slider/seek
> judgment remains the final gate; V04/L03b therefore remain AMBER.

This is the canonical playback/render ledger for the Surface Pro 11 (X1E80100) port as of 2026-08-12. It supersedes the earlier broad GREEN sign-off and the contradictory append-only SP7 ledger. The completion gate is **built-in-speaker rendering**. Microphone/input and Bluetooth are intentionally not completion blockers.

Status meanings: **GREEN** = deployed and evidence-backed Windows parity/on-par behavior; **AMBER** = present/working but exact parity or lifecycle proof incomplete; **RED** = known missing/mismatched Windows behavior; **N/A** = intentionally outside this completion gate.

## Overall gate

**RED — built-in-speaker rendering is functional and the steady Dolby core is highly matched, but there are still known audible Windows/Linux mismatches. Do not call playback complete until every RED item below is closed or explicitly reclassified by fresh evidence.**

## 1. Base Linux render path

- [x] **GREEN R01 — Kernel / ASoC platform stack.** Current live kernel is `7.1.5-sp11-render-parity-v2+`; X1E80100, q6apm/q6prm/APR, SoundWire and WSA884x are live. The consolidated render-parity work is booted and live-gated.
- [x] **GREEN R02 — Internal speaker card/PCM.** Surface Pro 11 ALSA card exposes the expected `MultiMedia1 Playback` PCM.
- [x] **GREEN R03 — UCM speaker route.** SP11-specific UCM is deployed; built-in speaker is the intended active render endpoint.
- [x] **GREEN R04 — PipeWire/WirePlumber integration.** PipeWire 1.6.2 + WirePlumber 0.5.13 healthy; default configured sink is `effect_input.sp11_windows_dolby`.
- [x] **GREEN R05 — Stereo render contract.** Built-in path is two-channel / 48 kHz in the reconstructed Windows-equivalent path.

## 2. Codec / SoundWire / speaker hardware

- [x] **GREEN H01 — Both WSA884x amplifiers active on SoundWire.** Left/right speaker routes are live.
- [x] **GREEN H02 — Digital mute/gain state.** RX0/RX1 mute off and digital volume 81 as deployed.
- [x] **GREEN H03 — PA operating point.** Both PA controls are 24 in the current high-output protected configuration.
- [x] **GREEN H04 — WSA softclip.** Softclip0/1 enabled.
- [~] **AMBER H05 — VISENSE / VI mixers are active, but the slave lane width differs from Windows.** Both Linux amplifiers and VI mixers are active at 8 kHz, but the recovered full qcaucd FIFO log proves Windows programs DP5 ChannelEnable `0x03` on each WSA8845 while the running Linux driver requests `0x01`. The static Windows master-port-10/11 templates and runtime master writes independently confirm `0x03`. See `docs/findings/2026-08-14-full-qcaucd-fifo-visense-gap.md`.
- [x] **GREEN H06 — PBR/protected high-output policy.** Current protected path includes the recovered PBR/current-limit state used for the high-output deployment.
- [~] **AMBER H07 — exact physical L/R identity and per-speaker calibration attribution.** Functional stereo is proven, but final physical-channel naming/calibration attribution is now part of the physical-parity investigation rather than merely cosmetic exactness work.
- [~] **AMBER H08 — the retained 328-write Windows transaction is fully decoded and one Linux mismatch is proven.** The exact raw logs were already copied locally under `01-audio/11.08.2026`; their hashes match the reviewed evidence. All three 109-command Windows cycles are payload-identical. Linux matches Windows DP1/DAC, DP2/COMP, DP3/BOOST, DP6/CPS, PA/class-H, 4-ohm/18-dB sensing, OCP, 2-cell current limit and PBR thresholds. The remaining proven transport mismatch is DP5/VISENSE: Windows mask `0x03` on both WSA slaves versus Linux `0x01`. A board-scoped candidate and live validation remain before H08 can turn GREEN. No new Windows capture is needed for that candidate.

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
- [~] **AMBER P06 — dual 8 kHz VI feedback starts, but Windows DP5 width is not reproduced.** Both WSA amplifiers feed the Linux graph, yet qcaucd runtime evidence proves Windows enables DP5 mask `0x03` per amplifier while Linux requests `0x01`. Until a mask-`0x03` candidate starts cleanly, accepted SP/SPVI setup is not proof that both native V/I subchannels are present.
- [x] **GREEN P07 — CPS feedback coupling.** SP/SPVI enable with VI+CPS feedback accepted; CPS runtime transport is closed as a deployment blocker.
- [x] **GREEN P08 — protected high-output gate.** High-output PA state is only used with protection active.
- [~] **AMBER P09 — passive/protection limiter telemetry equivalence.** Protected operation is proven; exact Windows telemetry/observer semantics remain incomplete.
- [~] **AMBER P10 — exact HLOS CPS payload semantics.** Transport/effect are present; every private field has not been named semantically.
- [x] **GREEN P11 — ordinary-playback PBR DP4 scheduling exactness.** Full decoding of the retained 328-write qcaucd FIFO log shows three identical Windows playback cycles. DP1/2/3/5/6 are positively programmed on both amplifiers; DP4 has no positive programming and appears only as right-slave bank-1 ChannelEnable `0x00` teardown once per cycle. The selector-5 DP4 template therefore remains supported capability, not a request in this captured playback scenario. Linux correctly keeps slave DP4 unscheduled while retaining the exact internal WSA8845 2-cell PBR/current-limit policy. Do not enable DP4.

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
- [ ] **RED W03 — real-browser physical tonal parity failed after the volume fixes.** On the live Render-Parity-v2 build the user reports that ordinary YouTube sound is much better than two weeks earlier but still not close to Windows parity. This is fresh evidence after Q09/Q10/V01/V02 and the exact final volume/GainStep transaction. It must not be papered over with guessed EQ, Bass Boost, Virtual Bass, ASAR/HRTF or DRC: the active native Dolby path and state-matched digital transfer are already strongly matched, while the remaining audible difference is heard at the physical speakers. Promote matched AudioReach/WSA/protection-state comparison (H08/P09-P11) before changing sample processing.

## 8. Seek / discontinuity / lifecycle behavior

- [x] **GREEN L01 — Windows seek oracle captured.** Controlled Edge split-tone `currentTime` seeks were captured through Windows loopback.
- [x] **GREEN L02 — recovered Dolby core contains the Windows-like discontinuity dynamics.** Feeding the same abrupt split-tone splice through the deployed Linux Movie bridge reproduces the sharp post-transition attenuation followed by hundreds-ms recovery. The core algorithm itself is not missing this behavior.
- [ ] **RED L03 — real YouTube seek path localized through both OSes; restored fourth POPLESS/headroom control link is not sufficient.** Genuine Firefox/YouTube Linux captures around verified seeks show the transient already at `effect_output.sp11_windows_dolby`; the aligned ALSA hardware-sink monitor is a sample-for-sample copy (scale 1.000000, correlation 1.000000, zero residual). The all-zero APO `SILENT`-flag hypothesis was rejected offline and not deployed. A matched Windows real-YouTube run at the same 25% / -20.74741 dB endpoint state also reaches roughly full-scale (~0.987) in WASAPI loopback after seeks even though the physical Windows speakers sound smoothly capped. This localizes the missing physical smoothing below the Windows loopback / Linux PipeWire monitor boundary. Audit of the reviewed Windows DEFAULT graph found Linux had dropped the fourth module-control link `POPLESS_EQ 0x4664 <-> VOL_CTRL 0x4663`, intent `INTENT_ID_P_EQ_VOL_HEADROOM (0x08001118)`; the generator was fixed and the isolated four-link candidate booted cleanly. On 2026-08-13 the user physically auditioned the four-link candidate at the explicit Windows-equivalent 25% state during verified real-YouTube seeks `47 -> 77 s`, `79 -> 59 s`, and `61 -> 96 s`; the sharp Linux re-entry spike remained. Therefore the restored link is structurally correct but does not close the audible defect by itself and must not be promoted as the fix. The user additionally reports the same class of spike while moving the master-volume slider during ongoing YouTube playback, broadening the defect from seek-only behavior to a more general live control/transition path. Immediate discriminators now remain the pending runtime-MSIIR listening A/B and V04 final-`VOL_CTRL` actuator A/B. V04 is a proven Windows/Linux volume-boundary mismatch and is directly relevant to the slider spike, but its seek linkage remains unproven.
- [ ] **RED L03a — live volume-change transient is physically reproducible; runtime MSIIR transition listening discriminator remains pending.** With the protected graph already RUNNING, a controlled 20%->30%->20%->40%->20% sequence completed in ~0.01 s per `wpctl` change when `sp11-msiir-volume-sync.service` was temporarily stopped; with it restored, the same sequence additionally injected CKV `2 -> 6 -> 2 -> 12 -> 2`. Three manual re-injections of the already-active CKV2 payload were then issued at fixed 20% with no slider movement. The software/control-path experiment completed successfully and the service was restored active; the physical listening verdict for those same-CKV injections is explicitly **pending** while the user is away and must gate attribution. A further deployed-topology audit closed one tempting omission: the exact booted Headroom-Test topology (SHA-256 `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`) contains the second serial MSIIR `0x48a1` with `0x08001020`, `0x08001021`, its full 164-byte `0x08001022` real-biquad payload, and enable `0x08001026=1`. Thus `0x48a1` was not accidentally omitted; `0x489e` remains the Windows-evidence-backed volume-dependent MSIIR stage.
- [x] **GREEN L03c — paused-media fragment on notification wake localized and closed.** A real GNOME volume-preview trace proved Firefox stayed corked/PAUSED while Mutter alone woke the sink. Simultaneous Dolby input/output uprobes then proved the wake entered the plug-in with digital zero while its output replayed old media from the recovered chain's measured 1,776-frame/37-ms algorithmic delay. PipeWire had frozen that delay at PAUSED. The bridge now consumes exactly 1,776 zero frames to discard delayed audio at the pause callback while retaining the original VR/VLLDP objects and their long-memory adaptive state. A 70-second two-instance lifecycle regression is bit-identical to an explicit reference drain; the post-drain silent-wake peak is about -93.3 dBFS. Live deployment preserved the default/profile/volume services, the post-fix uprobe shows the drain at the boundary, and the user repeated the physical test and reports the fragment is gone. This closes the pause->notification replay only; it does not by itself close the separate real-seek spike in L03. See `docs/findings/2026-08-13-PIPEWIRE-DOLBY-PAUSE-DRAIN.md`.
- [~] **AMBER L03b — exact Windows GainStep transaction is live transport-GREEN in Render-Parity; physical validation remains.** The full REV_0D sweep and recovered GSL/ACDB semantics identify the four ordered `0x489e` records `0x08001020`, `0x08001021`, `0x08001022`, `0x08001026`, sent as one 216/272-byte OOB APM SET_CFG after final `VOL_CTRL`. Patch `0048` plus capacity correction `0049` structurally validates and sends that complete group through the protected graph's existing OOB mapping; arbitrary targets, malformed payloads, nonzero padding, and idle graphs are rejected. On the consolidated live boot a zero-valued 15->25->40->25->15->31% sweep accepted GainSteps 1/3/12/3/1/7, including both 272-byte rows, without a following runtime DSP error. Physical slider/seek listening still gates overall GREEN. See `docs/deployment/2026-08-14-render-parity-candidate.md`.
- [x] **GREEN L04 — Windows SOFT_PAUSE lifecycle is recovered and live-validated.** Windows DEFAULT uses iid `0x466b`, PAUSE/state-3 -> zero-length pid `0x0800102e`, RUN/release/state-4 -> `0x0800102f`, with completion events `0x0800103f`/`0x08001043`; STOP releases any outstanding pause. The first live candidate proved those DSP identities but exposed Linux callback ordering: a running pull watermark could block completion behind ALSA's held PCM stream lock. Patch `0051` enters STOPPED/state 3 before sending PAUSE. On `7.1.5-sp11-render-parity-v2+`, zero-valued direct ALSA PAUSE_PUSH entered `PAUSED` and received `0x0800103f`, PAUSE_RELEASE returned `RUNNING` with `0x08001043`, and STOP-while-paused received resume-complete before clean close. There were no timeouts; services remained active and all SoundWire devices returned to runtime suspend. See `docs/deployment/2026-08-14-render-parity-candidate.md`.
- [x] **GREEN L05 — SOFT_PAUSE is not the direct in-stream seek mechanism.** Controlled Edge seeks did not emit additional `0x466b` transactions; implement it for lifecycle parity, not as a guessed YouTube-seek fix.
- [ ] **N/A L06 — system suspend/resume lifecycle.** Explicitly deferred by the user to a separate investigation; it is not part of the present built-in-speaker sound-quality completion gate.
- [x] **GREEN L07 — ordinary cold first-playback latency correction.** The baseline reproduced 18.627/18.464 s launches and four 4.28-4.60 s WSA-scale `regcache_sync()` calls. Render-Parity removes only unconditional `regcache_mark_dirty()` from ordinary resident clock-stop suspend while retaining cache-only write tracking and UNATTACHED/ATTACHED full restore. Two live zero-valued starts from all three SoundWire nodes suspended reached ALSA RUNNING in 208 ms and 83 ms, with no multi-second replay, and returned to automatic suspend after stop. Forced attach and system-suspend recovery are deferred with L06 and do not reopen the ordinary cold-start result. See `docs/findings/2026-08-13-WSA884X-COLD-START-DOUBLE-REGCACHE-REPLAY.md` and `docs/deployment/2026-08-14-render-parity-candidate.md`.

## 9. Source/provenance/reproducibility

- [x] **GREEN S01 — clean integration source of truth identified.** Current work is on `/home/geoca/Documents/SP11-PROJECT/01-audio-cps-review`, branch `agent/render-parity-20260812`.
- [x] **GREEN S02 — clean Aug-12 render integration history.** Movie correction (`994f41c`), Windows volume-dependent MSIIR (`f73ad55`), and the Windows endpoint-taper implementation are committed on the clean render-parity branch.
- [x] **GREEN S03 — remote publication and candidate preservation.** GitHub authentication is repaired. The clean `agent/render-parity-20260812` branch is the publication target, and isolated soft-pause, WSA regcache and final-VOL_CTRL candidates plus the safe actuator A/B tooling are preserved under tracked `patches/`, `tools/` and `tests/` paths. Generated unsigned modules remain excluded and uninstalled.
- [~] **AMBER S04 — older everyday audio working tree.** `/home/geoca/Documents/SP11-PROJECT/01-audio` is older and heavily dirty/untracked; it must not be treated as deployed production provenance.

## 10. Explicitly outside the current completion gate

- [ ] **N/A O01 — microphone/capture.** Deferred by project priority; current playback gate does not require it.
- [ ] **N/A O02 — Bluetooth audio.** Deferred by project priority.
- [ ] **N/A O03 — external display / USB / other external playback endpoints.** Not part of the current built-in-speaker parity gate unless promoted later.
- [ ] **N/A O04 — system suspend/resume.** Deferred to its own platform-lifecycle investigation at the user's request; sound quality and ordinary playback are the current priority.

## Ordered closure queue

1. **W03/H05/H08/P06/H07 — validate the proven DP5/VISENSE mismatch:** the retained 328-write qcaucd FIFO log is fully decoded. Windows uses DP5 mask `0x03` on each amplifier; running Linux uses `0x01`. Implement a Denali-scoped normal-SoundWire override, build it separately, require clean master-port-10/11 allocation plus SP/SPVI startup, then perform a bounded listening comparison. DP4 is closed negative for ordinary playback and must remain unscheduled. No new Windows capture is needed for this candidate.
2. **L03/L03a/V04/L03b — close the remaining physical transition spike:** the exact final `VOL_CTRL` and four-record GainStep transaction are live and ordinary level tracking is correct, but the user still hears a sharp slider/transition spike. Preserve the already-fixed pause/notification drain and distinguish a control transition from steady-state tonality in the matched Windows capture.
3. **W02 — strict digital identity:** revisit only after the physical downstream mismatch is explained; the existing ~60 dB residual is too small to justify a large guessed coloration stage.
4. Publish/merge the clean integration branch and archive obsolete contradictory docs.

## Completion rule

Built-in-speaker rendering becomes GREEN only when: (a) no known audible Windows/Linux mismatch remains in ordinary browser/media playback, (b) volume and seek/start-stop lifecycle behavior are evidence-backed, (c) protection remains live, and (d) the deployed binaries/config/kernel are reproducible from the clean source branch.
