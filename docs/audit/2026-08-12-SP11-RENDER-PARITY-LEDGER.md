# SP11 built-in-speaker Windows/Linux render parity ledger — 2026-08-12

> **2026-08-16 no-HD2 v3 result:** the one-variable HD2 correction passed
> active hardware gates (`RX CFG0=0x02`, SEC3=`0x00`) before and after real
> playback with all other Windows-proven WSA producer bytes intact and no new
> WSA/PA/SoundWire/XRUN fault.  Five-run external-mic median was nevertheless
> ~`0.527/0.571 dB` MAE/RMSE over 1--5 kHz versus RX84 generic
> ~`0.182/0.208`; one run approached baseline and one was an obvious capture
> outlier, so the structural correction is preserved but is not the final
> acoustic lever.  With the macro producer now Windows-like, the remaining
> high-value coupled difference is the WSA8845 consumer lifecycle: Windows
> keeps `DRE_CTL_1=0`/CSR fallback off while Linux unmute sets CSR_GAIN_EN.
> The rejected old forced-DRE0 experiment must not be repeated; any next test
> must change the amp lifecycle semantically and only after producer readiness.
> See `docs/findings/2026-08-16-WINDOWS-WSA-HD2-ISOLATION.md`.

> **2026-08-16 HD2 producer gap:** active read-only Linux regmap validation on
> `winproducer-init-v2` proves TOP_CFG1, RX84 volume, RX CFG1/CFG2, primary
> half-dB state, Surface curve, VBAT/BCL, softclip clocks and CB_DECODE now
> match the passive native Windows WSA corpus.  One direct RX mismatch remains:
> Linux primary-interpolator bring-up enables generic HD2 compensation
> (`CFG0=0x06`, SEC3=`0x11`) while Windows uses `CFG0=0x02` and its complete
> 330-transaction speaker lifecycle never accesses RX SEC3.  Mainline and the
> old generic Qualcomm driver both identify bit `0x04` as HD2, so SP11 Windows
> explicitly diverges from the generic Qualcomm policy.  The next one-variable
> candidate disables only HD2 and must verify `CFG0=0x02` before acoustic use.
> TX protection-path register timing is kept separate because independent live
> DP5/VI/SP evidence already validates the protection transport. See
> `docs/findings/2026-08-16-WINDOWS-WSA-HD2-GAP.md`.

> **2026-08-16 complete-producer v1 correction:** the first combined RX84 +
> Surface-curve + primary-half-dB-off + VBAT/BCL/CB_DECODE candidate was safe
> at the kernel/transport layer but acoustically worse and non-stationary; its
> five-run 1--5 kHz median was ~`0.496/0.570 dB` MAE/RMSE versus RX84 generic
> ~`0.182/0.208`.  Re-auditing the full 330 native Windows WSA transactions
> then exposed two producer init values v1 still lacked: both RX CFG1 bases need
> vendor/Windows bit `0x08` (Linux default `0x64`; active Windows `0x6d`) and
> TOP_CFG1 is Windows/vendor `0x03` while mainline defaults `0x00`.  These are
> independently corroborated by the old Qualcomm `wsa_macro_reg_init[]`; other
> vendor init entries remain unproven and will not be copied.  A narrowly scoped
> v2 must test only those two init corrections before any CSR/DRE revisit. See
> `docs/findings/2026-08-16-WINDOWS-PRODUCER-V1-INIT-GAP.md`.

> **2026-08-16 WSA VBAT/BCL isolation:** passive native Windows `qcaucd` tracing
> proved the legacy Qualcomm VBAT/BCL + v2.5 CB_DECODE producer stage is live
> on both internal-speaker paths and missing from current upstream Linux.  A
> one-shot RPV4/RX84 candidate restored only that lifecycle with CSR-assisted
> amps and no new WSA/PA/SoundWire/XRUN fault.  The synchronized three-run
> acoustic median nevertheless worsened from the RX84 generic baseline
> `~0.182/0.208 dB` to `~0.529/0.567 dB` MAE/RMSE over 1--5 kHz.  Standalone
> VBAT/BCL is therefore rejected as a parity improvement.  Because Windows
> directly proves VBAT/BCL, the Surface compander curve, and primary half-dB
> off simultaneously—and each partial transplant has been negative—the next
> bounded discriminator is the complete Windows-proven producer combination,
> still with safe CSR-assisted WSA8845 operation.  DRE/CSR-off remains rejected.
> See `docs/findings/2026-08-16-WINDOWS-WSA-VBAT-BCL-ISOLATION.md`.


> **2026-08-14 late volume-path closeout:** fresh SP11 Windows KDNET and
> stationary WASAPI loopback corrected two lifecycle assumptions in the older
> notes below. Windows does not live-retune VLLDP postgain on endpoint slider
> changes or ordinary stream stop/start while the same Dolby APO survives; it
> does run qcadcm SetVolume -> final `0x4a63` -> GetGraphCkv/dependent MSIIR.
> Windows master volume is delivered left-channel first, then right-channel.
> The isolated Linux `sp11-audio-volume-channel-order` boot now reproduces that
> exact sequence with a 288-byte backward-compatible transaction control. A
> live kprobe proves the mixed and final L/R Q28 bodies inside the loaded
> module, the 272-byte maximum row is accepted, VLLDP remains generation-frozen,
> both WSA8845s stay fault-free, and no transport/DSP fault follows. V04/L03a/
> L03b are therefore structurally GREEN. The historical v2 physical verdict
> below must not be projected onto the corrected topology; W03 and the
> seek-specific L03 physical gate remain open. See
> `docs/findings/2026-08-14-WINDOWS-LIVE-VOLUME-LIFECYCLE-KDNET.md` and
> `docs/findings/2026-08-14-LINUX-WINDOWS-CHANNEL-VOLUME-ORDER-CLOSEOUT.md`.

> **2026-08-14 LPASS softclip correction:** live v3 register readback disproves
> the earlier H04 claim.  Although ALSA reported both softclip controls on,
> the v2.5 driver's hard-coded v2.1 control address left the real softclip
> enable bits off and changed `COMPANDER1_CTL9/17` from `0x00/0x24` to
> `0x01/0x25`.  Patch `0053` corrects the latent driver address bug, while the
> SP11 UCM now explicitly leaves the unproven Linux-only softclip path off.
> The v4 candidate must restore the two compander bytes and pass protected
> playback/listening gates before this correction becomes GREEN.

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

> **2026-08-14 codec-init correction:** the retained Windows
> `CODEX_QCAUCD_V2CMD` capture contains a second, complete 63-register WSA8845
> initialization transaction per amplifier which the earlier DP6-focused
> decoder ignored. It proves that Linux mis-encoded the two-bit 2S supply field
> (`0xdd` instead of Windows `0x9d`) and used several generic/downstream values
> where the actual SP11 Windows driver writes different BOP/UVLO, power-stage,
> DSM, DRE, class-H and OTP values. Patch `0052` corrects those fields and the
> exact PA start/stop ordering. The isolated `7.1.5-sp11-render-parity-v3+`
> candidate passes build, module, initramfs and GRUB preboot gates; H08 remains
> AMBER until it boots and passes register, safety and listening gates. See
> `docs/findings/2026-08-14-WINDOWS-WSA8845-INIT-PARITY-CORRECTION.md`.

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

**AMBER — the major Windows render/control state machines are now structurally reproduced, including the exact WSA8845 63/10/6 lifecycle and v27 resident clock-stop retention, but fresh synchronized muted-zero measurements confirm a large physical static remains. A mid-stream WSA-macro RX digital-mute A/B leaves that static unchanged, placing it downstream of or independent from ordinary PCM sample data. W03 therefore remains open as a confirmed hardware/transport-side physical failure; do not call playback complete until that boundary is closed and L03 receives its final physical verdict.**

## 1. Base Linux render path

- [x] **GREEN R01 — Kernel / ASoC platform stack.** Current live candidate uses the accepted `7.1.5-sp11-render-parity-v4+` kernel/DTB with an isolated signed `snd-q6apm` initramfs override for channel-ordered endpoint-volume parity; X1E80100, q6apm/q6prm/APR, SoundWire and WSA884x are live. Persistent fallback remains CPS-v3.
- [x] **GREEN R02 — Internal speaker card/PCM.** Surface Pro 11 ALSA card exposes the expected `MultiMedia1 Playback` PCM.
- [x] **GREEN R03 — UCM speaker route.** SP11-specific UCM is deployed; built-in speaker is the intended active render endpoint.
- [x] **GREEN R04 — PipeWire/WirePlumber integration.** PipeWire 1.6.2 + WirePlumber 0.5.13 healthy; default configured sink is `effect_input.sp11_windows_dolby`.
- [x] **GREEN R05 — Stereo render contract.** Built-in path is two-channel / 48 kHz in the reconstructed Windows-equivalent path.

## 2. Codec / SoundWire / speaker hardware

- [x] **GREEN H01 — Both WSA884x amplifiers active on SoundWire.** Left/right speaker routes are live.
- [x] **GREEN H02 — LPASS WSA-macro digital gain is Windows-proven at 0 dB.** Passive qcaucd KD tracing through the native Qualcomm register helper directly captured Windows `RX0/RX1 RX_VOL_CTL=0x00`, i.e. 0 dB. The isolated Linux RX84/0 dB candidate is protected and safe and materially improves the synchronized acoustic match versus the old temporary Linux RX81/-3 dB cap. RX84 is therefore the correct structural policy, not an acoustic guess. See `docs/findings/2026-08-16-WINDOWS-WSA-NATIVE-PROGRAMMING.md` and `docs/findings/2026-08-16-SYNCHRONIZED-ACOUSTIC-RX-GAIN-SWEEP.md`.
- [x] **GREEN H03 — exact shipping Windows WSA8845 three-state lifecycle plus ordinary resident clock-stop retention are live-proven on Linux.** v26 implements the recovered 63-write cold init plus exact 10-write START/6-write STOP and suppresses Linux-only ordinary DAPM mutations. Follow-up tracing proved ordinary SP11 SoundWire clock-stop may report UNATTACHED; the old generic callback then incorrectly cleared `hw_init`, dirtied the whole cache and replayed cold init. v27 retains initialized state across that ordinary detach/attach pair. After 20 s idle it reached RUNNING in 150 ms, kept the boot init count at two, retained raw `DRE_CTL_1=0x0e`, and emitted exactly 32 WSA writes = 2 amps × (10 START + 6 STOP), with no cold replay. System suspend/resume remains excluded. See `docs/findings/2026-08-17-LINUX-WSA8845-WINDOWS-THREE-STATE-V26.md`, `docs/findings/2026-08-17-WSA8845-CLOCKSTOP-RETENTION-V27-AND-STATIC-BOUNDARY.md`, reviewed artifacts, and patches `0063`/`0064`.
- [x] **GREEN H04 — LPASS WSA macro v2.5 softclip/compander addressing.** Patch `0053` is live in v4. Both macro regmaps read `COMPANDER0_CTL9/17 = 0x00/0x24`, `COMPANDER1_CTL9/17 = 0x00/0x24`, and the true v2.5 softclip controls `0x644/0x664 = 0x38` with enable bit 0 clear. This closes the old v2.1-address corruption without enabling the unobserved softclip block; see `docs/findings/2026-08-14-LPASS-WSA-V2.5-SOFTCLIP-ADDRESS-CORRECTION.md`.
- [x] **GREEN H05 — VISENSE / VI mixers reproduce the Windows slave width.** The corrected `sp11-audio-visense-parity` boot loaded WSA884x srcversion `782FC79EBBA505E52A2AE88`; both amplifiers requested DP5 ChannelEnable `0x03` at 8 kHz, matching the qcaucd slave FIFO, master-port-10/11 runtime tuples and static templates. VI feedback, SP/SPVI enable and bounded playback succeeded without PA fault, XRUN or port conflict, and both amplifiers returned to runtime suspend. Overall physical tonal parity remains W03, not part of this transport result.
- [x] **GREEN H06 — PBR/protected high-output policy.** Current protected path includes the recovered PBR/current-limit state used for the high-output deployment.
- [x] **GREEN H07 — physical L/R identity and per-speaker calibration attribution.** During the 2026-08-14 standard spoken stereo test the operator confirmed that `Front Left` came from physical chassis left and `Front Right` from physical chassis right. This closes the acoustic end of the already-consistent ordering chain: Windows R0/T0 records 0/1, SPVI V/I pairs 1/2 then 3/4, and Linux `left_spkr` then `right_spkr`. Record 0 remains physical left (R0 4.955847740 ohm, T0 38.65625 C); record 1 remains physical right (R0 5.370454669 ohm, T0 37.0 C). No calibration swap is required.
- [x] **GREEN H08 — Windows WSA8845 board-value subset / PA ordering and SoundWire transport.** The v3 candidate booted with both amplifiers carrying the patch-0052 SP11 board-value corrections and PA ordering recovered from `CODEX_QCAUCD_V2CMD`; protected playback and live telemetry ran with zero PA/protection errors. The retained `CODEX_DP6BRIDGE` cycles prove DP1/DAC, DP2/COMP, DP3/BOOST, DP5/VISENSE, DP6/CPS and no ordinary DP4 schedule, with DP5 `0x03` live on both amplifiers. This closes the proven board-value subset, PA transaction and SoundWire transport, not the complete 63-write codec initialization history or W03 acoustic parity; H03 now tracks that initialization-history gap and H04 separately tracks the LPASS-macro defect.

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
- [x] **GREEN Q10 — Windows live CKV re-selection on endpoint-volume changes is directly KDNET-proven.** Fresh SP11 Windows KDNET resolves qcadcm SetVolume at RVA `0x6e038`: each live endpoint update sends final `0x4a63/0x08001038`, calls GetGraphCkv, then applies the dependent MSIIR calibration. The controlled 8%→17%→8% run selected internal index 0/1/0 (CKV1/2/1, with the expected intermediate CKV2 during the first channel of the downward transition). Linux keeps the exact 30-row selector/payload implementation and combined final-VOL_CTRL-before-GainStep transport. See `docs/findings/2026-08-14-WINDOWS-LIVE-VOLUME-LIFECYCLE-KDNET.md`.
- [x] **GREEN Q11 — WaveSpeaker EQ / Bass Boost / DRC INF declarations are not a missing ordinary-speaker stage.** The Surface INF does advertise EQ/Bass Boost/DRC MFX GUIDs across WaveSpeaker host/offload modes, but the reviewed Windows DEFAULT and NOTIFICATION speaker graphs contain neither `DRC 0x07001066` nor `IIR_MBDRC 0x07001017`; their only equalizer-family AudioReach block is the already-deployed POPLESS EQ. The separate Surface Render APO EQ is independently disabled/identity in REV_0D, and the fresh Windows/Linux Movie oracle is already near sample-identical. The exact Qualcomm miniport also contains a concrete `Equalizer` descriptor but not even the INF-advertised DRC GUID, reinforcing that the INF list is capability metadata rather than a literal active-module inventory. Do not guess-enable these effects for ordinary browser/media playback. Re-open only for a specifically proven offload/mode-specific runtime graph. See `docs/findings/2026-08-13-WAVESPEAKER-MFX-NOT-A-MISSING-DEFAULT-STAGE.md`.

- [x] **GREEN Q12 - exact Windows QCADCM graph-open/lifetime state machine.** Fresh final KDNET plus hash-matched qcadcm Ghidra pins the ordinary protected-render open order from ADD_GRAPH through watermark and SOFT_PAUSE event registration, stream/mix media formats, speaker-protection RX/VI payloads, VI/source-sink endpoint HW tags, orientation, START and the PAUSE->STOP->resume-clear transition. It also corrects the old broad tag attribution: `0x04010003` is the category-1 source/sink endpoint HW-interface tag; the speaker-protection VI endpoint configuration uses category-3 `0x04010005`. No REMOVE_GRAPH was observed on the ordinary application release window, so DSP-graph lifetime remains distinct from the independently proven qcaucd SoundWire/WSA idle teardown. See `docs/findings/2026-08-17-WINDOWS-QCADCM-PROTECTED-RENDER-STATE-MACHINE.md` and `artifacts/reviewed/2026-08-17-windows-qcadcm-protected-render-state-machine.json`.

## 4. Speaker protection / CPS

- [x] **GREEN P01 — SP configuration/query.** Runtime accepted.
- [x] **GREEN P02 — SPVI configuration/query.** Runtime accepted.
- [x] **GREEN P03 — SPVI R0/T0 calibration.** Recovered calibration accepted.
- [x] **GREEN P04 — SPVI channel/processing mode.** Accepted.
- [x] **GREEN P05 — SP/SPVI tag calibration.** Accepted.
- [x] **GREEN P06 — dual 8 kHz native-width VI feedback.** Both WSA amplifiers request Windows DP5 mask `0x03`, both VI feedback paths report ready, SP/SPVI with VI+CPS feedback and `GRAPH_START` are accepted, bounded playback is fault/XRUN-free, and both amplifiers suspend cleanly afterward.
- [x] **GREEN P07 — CPS feedback coupling.** SP/SPVI enable with VI+CPS feedback accepted; CPS runtime transport is closed as a deployment blocker.
- [x] **GREEN P08 — protected high-output gate.** High-output PA state is only used with protection active.
- [~] **AMBER P09 — passive/protection limiter telemetry equivalence.** A bounded live run on `7.1.5-sp11-render-parity-v2+` captured 12 successful samples from each WSA884x while protected stereo playback ran: both PAs stayed enabled, both devices returned independent changing raw ADC/temperature words, current-limit register `0x44` remained selected, and every failed-read, error and interrupt field stayed zero. The temporary observer was reset to disabled afterward. The old Windows TMax/XMax GET and newer public telemetry IDs have already been tried and rejected by the Linux DSP. A complete hash-bound qcadcm/qcaudminiport causal trace proves Windows uses the TMax/XMax response only for WPP/ETW logging, so this AMBER observability gap is excluded as a cause of W03 and does not justify another Windows reboot. See `docs/findings/2026-08-14-LINUX-BOUNDED-WSA-PROTECTION-OBSERVATION.md` and `docs/findings/2026-08-14-WINDOWS-TMAX-XMAX-READBACK-IS-LOGGING-ONLY.md`.
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
- [x] **GREEN D09 — VLLDP endpoint-volume postgain relation and lifecycle.** The recovered relation `postgain = round(master_volume_dB * 16)` remains valid for Dolby/APO generation configuration, but fresh Windows KDNET proves it is **not** a per-slider live actuator: neither recovered VLLDP postgain setter fires during 8%→17%→8%, stationary loopback is unchanged, and idle→new-stream 17% versus 8% is also unchanged while the same APO instance survives. Linux now queues postgain once per Dolby/filter-chain generation and freezes it until that engine is recreated.
- [x] **GREEN D10 — adaptive history preservation.** Linux does not rebuild/reset the Dolby adaptive state on ordinary PipeWire PAUSED transitions where Windows APO Reset is effectively a no-op.
- [x] **GREEN D11 — exact AudioEngine final limiter.** Windows final limiter behavior is reproduced, including the recovered ~-0.13 dBFS ceiling.

## 6. Volume law / user-facing loudness

- [x] **GREEN V01 — Windows endpoint UI taper is reproduced.** Fresh SP11 Windows `IAudioEndpointVolume` capture pinned the nonlinear scalar->dB curve (201 points, 0.005 spacing, endpoint range -75..0 dB). Linux keeps the visible virtual-sink scalar but retapers the hidden downstream ALSA sink to the Windows endpoint gain. At 25%, Linux now measures -20.7474 dB exactly while the visible slider remains 25%. Full reboot and filter-node recreation gates passed.
- [x] **GREEN V02 — Windows endpoint state is reproduced with the correct split lifecycle.** One pinned scalar→dB law drives three related states, but not at one lifecycle boundary: VLLDP postgain is initialized once per Dolby-engine generation; live qcadcm SetVolume drives final `0x4a63` and GetGraphCkv/GainStep; the hidden host sink remains unity after the DSP handover and is retained only for fail-quiet fallback. Fresh Linux 12%→8%→17%→8%→12% playback kept VLLDP request/ack frozen at `-503/-503` while final Q28/GainStep moved exactly through `0x0039db88/1`, `0x00c7763f/2`, `0x0039db88/1`, `0x00702a69/1`.
- [x] **GREEN V03 — no endpoint attenuation before Dolby; visible volume is now control metadata only.** The earlier `softVolumes == unity` inference was disproved: at 8% the old single-node sink had `channelVolumes = 0.000512` (`0.08^3`) and attenuated PCM before VLLDP/VR. Production now uses `effect_input.sp11_windows_dolby` only as the visible control sink, routes its unity monitor ports into hidden `effect_input.sp11_windows_dolby_engine`, and leaves endpoint attenuation to the recovered final AudioReach `VOL_CTRL`/fail-quiet host handover. A fresh production run matches the already-proven Movie oracle at `0.999952 / 0.999902` correlation with only `+0.0069 dB` fitted gain error; both WSA8845s stayed active with current-limit code 17 and zero PA errors. The node-recreation guard also prevents PipeWire's transient default-unity Props from ever becoming a DSP unity transaction. See `docs/findings/2026-08-14-PIPEWIRE-PRE-DOLBY-VOLUME-BOUNDARY-CORRECTION.md`.
- [x] **GREEN V04 — final endpoint-gain actuator and channel ordering match Windows live.** Fresh Windows KDNET proves qcadcm SetVolume sends final `0x4a63/0x08001038`, then GetGraphCkv/dependent MSIIR, and delivers one stereo master gesture as left=new/right=old followed by left=new/right=new. The isolated Linux candidate exposes the 288-byte two-Q28 transaction form and reproduces that exact ordering. A live kprobe on `audioreach_sp11_set_final_volume_q28()` captured all eight expected mixed/final bodies through 12%→8%→17%→8%→12%; the 272-byte GainStep-3 maximum row also passed. VLLDP stayed frozen and fail-quiet host ordering remained intact. Physical tonality/seek verdicts are tracked separately in W03/L03, not as an actuator implementation gap. See `docs/findings/2026-08-14-LINUX-WINDOWS-CHANNEL-VOLUME-ORDER-CLOSEOUT.md`.

## 7. Steady-state waveform parity

- [x] **GREEN W01 — fresh deterministic Windows-vs-Linux Movie oracle.** Same-source/state comparison reached correlation ~0.99999947 with fitted gain ~1.00016 and ~59.8 dB residual SNR; cold-state comparison was even closer (~84.7 dB residual SNR).
- [~] **AMBER W02 — strict sample identity.** The remaining ~60 dB full-file residual is tiny and concentrated around transient/state behavior but is not literal bit identity.
- [~] **AMBER W03 — fresh corrected-topology measurement confirms a downstream physical static remains.** On v27, repeated 1%-muted digital-zero captures with the exact WSA8845 runtime lifecycle stable produced median steady microphone diff-RMS `0.002126` and `0.002350`, versus the Windows reference `0.00001825`. The failure is therefore current, not historical. A synchronized mid-stream A/B switched both WSA-macro RX Digital Mutes ON while the PA stayed active; noise changed only ~1% (`0.002325` pre, `0.002290` muted, `0.002304` post). The static is downstream of or independent from the ordinary WSA-macro RX sample path, so Dolby/q6apm/PCM processing is excluded as its direct source. Continue at WSA8845 analog/PA state and SoundWire clock/transport state; do not invent an upstream processing block. See `docs/findings/2026-08-17-WSA8845-CLOCKSTOP-RETENTION-V27-AND-STATIC-BOUNDARY.md`.

## 8. Seek / discontinuity / lifecycle behavior

- [x] **GREEN L01 — Windows seek oracle captured.** Controlled Edge split-tone `currentTime` seeks were captured through Windows loopback.
- [x] **GREEN L02 — recovered Dolby core contains the Windows-like discontinuity dynamics.** Feeding the same abrupt split-tone splice through the deployed Linux Movie bridge reproduces the sharp post-transition attenuation followed by hundreds-ms recovery. The core algorithm itself is not missing this behavior.
- [~] **AMBER L03 — corrected seek path is structurally closed; fresh physical verdict remains.** Fresh SP7 KDNET now proves three steady-state Windows seeks cause zero runtime hits at qcad `SetVolume`, `GetGraphCkv`, or filtered SET_CFG for `0x4663/4664/4669/466b/4a63/489e/48a1/412b`; the same armed probes fire immediately on a 25%->17%->25% volume positive control. Thus Windows has no hidden host seek transaction to reproduce. Linux already carries the exact POPLESS calibration/headroom link and surrounding ramp policies, and a fresh corrected-topology 25% seek run produced no volume transaction, DSP/XRUN/SoundWire/WSA/PA fault while every obtained amp sample remained healthy. A synchronized local MediaPlayer Windows loopback shows a short near-silence transition, but prior real Edge/YouTube Windows loopback reaches ~0.987 full scale after seek, so that app-specific dip is not promoted into a guessed Linux fade. The old physical RED predates the pre-Dolby-volume, VLLDP-lifecycle and per-channel final-volume fixes and cannot be projected onto the current candidate. See `docs/findings/2026-08-15-WINDOWS-SEEK-KDNET-NO-HOST-CONTROL.md`. User physical listening is the remaining gate.
- [x] **GREEN L03a — ordinary live master-volume lifecycle and per-channel sequencing match Windows.** Fresh KDNET proves Windows qcadcm SetVolume→final VOL_CTRL→GetGraphCkv/dependent MSIIR while VLLDP remains frozen, with left-channel state applied before right-channel state. Linux reproduces the same mixed/final Q28 bodies and mixed/final GainStep selection; kprobe traces prove the actual kernel arguments, and a live MP3 transition sweep kept both WSA8845s healthy with zero runtime faults. Any remaining audible seek/re-entry issue is therefore seek-specific L03, not ordinary master-volume sequencing.
- [x] **GREEN L03c — paused-media fragment on notification wake localized and closed.** A real GNOME volume-preview trace proved Firefox stayed corked/PAUSED while Mutter alone woke the sink. Simultaneous Dolby input/output uprobes then proved the wake entered the plug-in with digital zero while its output replayed old media from the recovered chain's measured 1,776-frame/37-ms algorithmic delay. PipeWire had frozen that delay at PAUSED. The bridge now consumes exactly 1,776 zero frames to discard delayed audio at the pause callback while retaining the original VR/VLLDP objects and their long-memory adaptive state. A 70-second two-instance lifecycle regression is bit-identical to an explicit reference drain; the post-drain silent-wake peak is about -93.3 dBFS. Live deployment preserved the default/profile/volume services, the post-fix uprobe shows the drain at the boundary, and the user repeated the physical test and reports the fragment is gone. This closes the pause->notification replay only; it does not by itself close the separate real-seek spike in L03. See `docs/findings/2026-08-13-PIPEWIRE-DOLBY-PAUSE-DRAIN.md`.
- [x] **GREEN L03b — exact Windows GainStep transaction and maximum-row transport are live-proven.** The full REV_0D sweep identifies the four ordered `0x489e` records `0x08001020`, `0x08001021`, `0x08001022`, `0x08001026`, sent after final `VOL_CTRL`. The current channel-order candidate preserves the strict target/padding/four-frame validation and extends only the independent L/R Q28 header. Live 12%→25%→12% accepted the maximum 272-byte GainStep-3 row in the new two-Q28 form and returned through mixed step 3 to final step 1 without `-EINVAL`, DSP error or host fallback. Fresh Windows KDNET independently proves GetGraphCkv/dependent calibration is part of every live SetVolume call.
- [x] **GREEN L04 — Windows SOFT_PAUSE lifecycle is recovered and live-validated.** Windows DEFAULT uses iid `0x466b`, PAUSE/state-3 -> zero-length pid `0x0800102e`, RUN/release/state-4 -> `0x0800102f`, with completion events `0x0800103f`/`0x08001043`; STOP releases any outstanding pause. The first live candidate proved those DSP identities but exposed Linux callback ordering: a running pull watermark could block completion behind ALSA's held PCM stream lock. Patch `0051` enters STOPPED/state 3 before sending PAUSE. On `7.1.5-sp11-render-parity-v2+`, zero-valued direct ALSA PAUSE_PUSH entered `PAUSED` and received `0x0800103f`, PAUSE_RELEASE returned `RUNNING` with `0x08001043`, and STOP-while-paused received resume-complete before clean close. There were no timeouts; services remained active and all SoundWire devices returned to runtime suspend. See `docs/deployment/2026-08-14-render-parity-candidate.md`.
- [x] **GREEN L05 — SOFT_PAUSE is not the direct in-stream seek mechanism.** Controlled Edge seeks did not emit additional `0x466b` transactions; implement it for lifecycle parity, not as a guessed YouTube-seek fix.
- [ ] **N/A L06 — system suspend/resume lifecycle.** Explicitly deferred by the user to a separate investigation; it is not part of the present built-in-speaker sound-quality completion gate.
- [x] **GREEN L07 — ordinary cold first-playback latency correction, including transient SoundWire UNATTACHED handling.** The baseline reproduced 18.627/18.464 s launches and four 4.28-4.60 s WSA-scale `regcache_sync()` calls. Later v26 tracing exposed a second route back into the same failure: ordinary SP11 clock-stop may transiently report `SDW_SLAVE_UNATTACHED`, and the generic status callback treated that as complete context loss, causing full-cache replay plus cold codec initialization. v27 retains `hw_init` across this resident detach/attach pair and syncs only genuine cached writes. A 20 s idle wake reached RUNNING in 150 ms, did not increment the two boot-time codec initializations, retained raw DRE state and returned to suspend. True system suspend/full context-loss recovery remains deferred with L06. See `docs/findings/2026-08-13-WSA884X-COLD-START-DOUBLE-REGCACHE-REPLAY.md` and `docs/findings/2026-08-17-WSA8845-CLOCKSTOP-RETENTION-V27-AND-STATIC-BOUNDARY.md`.
- [x] **GREEN L08 — desktop-idle PA lifecycle now matches Windows demand behavior.** The August-14 visible-control/hidden-engine split accidentally left the hidden Dolby engine input non-passive, so its persistent monitor links held the entire Dolby→ALSA speaker graph RUNNING even with zero application streams. Safe CPS-v3 traced the PA staying unmuted for ~316.6 s until an explicit userspace teardown. Adding `node.passive=true` to the hidden engine capture stream makes the graph suspend naturally: the fixed MP3 wakes SoundWire/COMP/PA on demand, and after playback the PA mutes first, producer tears down, SoundWire deprepares and ALSA PCM returns `closed`. This also removes the non-Windows idle condition present during the rejected delayed-crackle CSR-off test. See `docs/findings/2026-08-16-DOLBY-IDLE-PA-LIFECYCLE-PARITY.md`.

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

1. **W03 — localize the confirmed downstream physical static:** v27 stabilizes the exact WSA8845 lifecycle and clock-stop retention, and mid-stream WSA-macro digital mute proves ordinary PCM data is not the noise source. Compare read-only WSA8845 analog/PA and SoundWire transport/clock state against Windows before another actuator experiment.
2. **L03 — isolate the seek/discontinuity-specific physical smoothing:** ordinary live volume sequencing (L03a/V04/L03b), SOFT_PAUSE and the pause-drain path are structurally closed. Re-capture a matched seek on the corrected topology and compare Windows qcad/AudioReach control activity around the discontinuity rather than reopening the ordinary slider path.
3. **W02 — strict digital identity:** revisit only after the current physical gates are resolved; the existing ~60 dB residual is too small to justify a large guessed coloration stage.
4. Publish/merge the clean integration branch and archive obsolete contradictory docs.

## Completion rule

Built-in-speaker rendering becomes GREEN only when: (a) no known audible Windows/Linux mismatch remains in ordinary browser/media playback, (b) volume and seek/start-stop lifecycle behavior are evidence-backed, (c) protection remains live, and (d) the deployed binaries/config/kernel are reproducible from the clean source branch.
