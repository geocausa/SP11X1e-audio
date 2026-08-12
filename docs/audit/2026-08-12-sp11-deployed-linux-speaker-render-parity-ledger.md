# SP11 deployed Linux built-in speaker render parity ledger â€” 2026-08-12

Scope: built-in speaker rendering only. Microphone/capture and Bluetooth are explicitly deferred until render parity is complete.

Reference endpoint: Windows internal `Speakers (Qualcomm(R) Aqstic(TM) Audio Adapter Device)`, MSHW0486 REV_0D.

Audited Linux boot: `7.1.5-sp11-cps-v3+`, saved GRUB entry `sp11-audio-cps-v3`. The Windows one-shot was consumed and `next_entry` is empty after returning to Linux.

Legend: PASS = evidence-backed and deployed; PARTIAL = works but exact parity/certification incomplete; MISSING = known Windows behavior/content not deployed; OPEN = evidence not yet sufficient; DEFERRED = outside current render-first gate.

## Executive gate

**Overall built-in speaker render parity: NOT COMPLETE.**

The default render path is substantially functional: protected AudioReach graph, dual WSA playback, VI+CPS feedback, Dolby VR/VLLDP bridge, endpoint-volume feedback, final limiter model, and repeated playback are all live. The biggest concrete functional gap is Windows processing-mode/lifetime policy: the running Linux kernel hard-codes the DEFAULT render subgraphs (`0xb000007f`, `0xb000007e`) and cannot select the recovered NOTIFICATION family (`0x83`, `0x82`). Final Dolby waveform/state certification and physical speaker/protection closure also remain.

## Deployed Linux render path

- [PASS] Running intended CPS V3 kernel: `7.1.5-sp11-cps-v3+` with `sp11_cps_parity_v2=1 sp11_cps_v3=1`.
- [PASS] ALSA exposes the X1E80100 Microsoft Surface Pro card and `MultiMedia1` stereo playback PCM.
- [PASS] Qualcomm ASoC / AudioReach / SoundWire / dual WSA884x driver stack is loaded.
- [PASS] PipeWire, pipewire-pulse and WirePlumber are active.
- [PASS] Default user render sink is `effect_input.sp11_windows_dolby`; the Dolby filter is not merely installed, it is the default route.
- [PASS] Current Linux output volume was kept at 0.25 during audit/stress.
- [PASS] Controlled 10x sequential playback through the Dolby sink completed 10/10 with zero application failures.
- [PASS] New-message strict scan after the 10x test found no XRUN, underrun, overrun, ADSP crash, WSA fault, SoundWire error, kernel BUG or Oops.
- [PASS] No filter-chain or Dolby volume-sync service errors were logged during the stress window.
- [PASS] Persistent pull graph reuse works: repeated prepare reuses the running pull graph rather than reopening it incorrectly.

## Windows binary / Dolby parity

Fresh Windows `audiodg.exe` on the audited boot loaded:

- `DolbyApoVr.dll` 3.30704.742.0 â€” SHA-256 `1D74477EA0DAE66961A21BF6BC3CE0D8062836FC4DD96B59C14DE11257F5EECC`.
- `DolbyAPOvlldp150.dll` 3.30704.742.0 â€” SHA-256 `A2553FF7B013B5A248E50BDCAE46D08405E393C0085073975214D035CEDF02C1`.
- `DolbyDax3Apo.dll` 3.30704.742.0 â€” SHA-256 `6EA1702C0F86766E45C2E248E169022E3D71EAA3C655B3FCA159B4DD59F18D87`.
- `SurfaceAPO.dll` 1.216.42.0 â€” SHA-256 `AA3A97E2CC7740CE3BD6B80B154354A023170D3EF29992978E36C179550A5206`.
- `audioeng.dll` 10.0.26100.8972 â€” SHA-256 `843430C1516A2867FE716E89BCC35399E59E5040D992BFAFF7468EAB1CB63A93`.

Linux deployment:

- [PASS] Deployed `DolbyAPOVR.dll` is byte-identical to the Windows-loaded `DolbyApoVr.dll` above.
- [PASS] Deployed `DolbyAPOvlldp150.dll` is byte-identical to the Windows-loaded VLLDP binary above.
- [PASS] Deployed native bridge `sp11_dolby_windows_chain.so` SHA-256 is `EF4D995216B3BA5AE55189A7D5032A402968E308F18E2F780959788E21179D31`, matching the recorded 2026-08-10 production deployment.
- [PASS] Correct recovered sample dependency is VR -> VLLDP; the deployed bridge follows that order.
- [PASS] Endpoint-volume feedback is active and drives VLLDP postgain from actual endpoint attenuation. At Linux volume 0.25 the live postgain is `-578` Q4 dB = `-36.125 dB`.
- [PASS] Final AudioEng limiter model is the exact recovered `CAudioLimiter` model used by the production bridge; offline C/Python parity and chunk determinism were previously closed.
- [PASS] SurfaceAPO REV_0D render-EQ nodes were previously proven disabled for the relevant Windows speaker modes; Linux is not missing an active SurfaceAPO EQ stage on that evidence.
- [PARTIAL] Full DAX3 wrapper/profile/lifecycle/state behavior is not yet certified end-to-end. Linux executes the original VR/VLLDP processors but does not simply host the whole Windows `DolbyDax3Apo.dll` stack.
- [PARTIAL] Fresh same-input Windows-vs-Linux waveform certification across arbitrary content, profile history and all relevant endpoint lifecycles is still required before calling Dolby identical.
- [PARTIAL] Existing state-pinned oracle work is very close but is evidence for the modeled bridge, not a substitute for a final current-deployment A/B certification.

## Qualcomm lower render graph / topology

- [PASS] Linux render is fixed 48 kHz stereo at the protected speaker graph boundary.
- [PASS] Both WSA884x devices expose playback SoundWire ports 1/2/3 during render.
- [PASS] VI feedback is live at 8 kHz on SoundWire port 5 for both amplifiers.
- [PASS] CPS feedback is live at 24 kHz on SoundWire port 6 with channel mask `0x03` on both amplifiers.
- [PASS] SP and SPVI protection modules are found, configured and enabled when both VI and CPS feedback are ready.
- [PASS] Live graph setup accepts pull-ring configuration, watermarks/soft-pause, pull media format, PCM_CNV, MFC, SP operating mode, SP/SPVI calibration/query stages, render endpoint calibration, VI endpoint calibration, VOL_CTRL gain/mute, full-volume MSIIR, channel mixer and GRAPH_START.
- [PASS] Current live mixer state is symmetric: PA volume 24/24, WSA digital 81/81, RX mute off/off, VISENSE on/on, VI mixers on/on, softclip on/on.
- [PASS] CPS V3 transport has prior moderate-volume runtime evidence with no XRUN, bus clash, PA fault or protection-transport fault.
- [PARTIAL] One graph-calibration command returns `AR_EUNSUPPORTED`; Linux intentionally continues because Qualcomm GSL does the same. It is not causing playback failure, but exact transaction-by-transaction Windows equivalence should remain documented rather than calling the return code literally identical.
- [OPEN] Nonzero real protection/limiter intervention has not yet been deliberately provoked and measured against Windows.
- [OPEN] Exact Windows HLOS CPS payload/threshold semantics remain incompletely proven.
- [OPEN] Ordinary render SoundWire last-mile slave slot/offset binding and protected-speaker-index -> physical L/R mapping are less closed than CPS DP6 geometry.
- [OPEN] Any true Windows per-speaker asymmetry/calibration has not yet been proven or reproduced.

## Processing-mode / render-family policy

Windows reference already recovered:

| Windows mode | mode flag | QCADCM/GKV | lower family status |
|---|---:|---:|---|
| DEFAULT | `0x01` | 2 | exact content recovered; deployed on Linux as `0x7f/0x7e` |
| RAW | `0x02` | 1 | static family mapped; live-policy relevance not yet closed |
| COMMUNICATIONS | `0x04` | 6 | static family mapped; live-policy relevance not yet closed |
| SPEECH | `0x08` | 5 | static family mapped; live-policy relevance not yet closed |
| NOTIFICATION | `0x0A` | 7 | exact `0x83/0x82` content and calibration recovered, including distinct MSIIR coefficients |
| MEDIA | `0x14` | 4 | static family mapped; controlled WinRT Media and real Edge/YouTube selected DEFAULT |
| MOVIE | `0x28` | 3 | static family mapped; controlled WinRT Movie selected DEFAULT |

- [PASS] Windows DEFAULT-first overlap policy is evidence-backed: an Alerts client is classified as NOTIFICATION while DEFAULT is already alive, but the existing lower DEFAULT graph remains owner; Windows does not open a second lower graph for that overlap.
- [PASS] Cold isolated Windows Alerts selects the NOTIFICATION GKV 7 family.
- [PASS] A fresh 2026-08-12 cold Alerts-first probe again hit mode flag `0x0A` and began a fresh AudioReach lifecycle (`0x01001000`, `0x01001002`) before the diagnostic run was interrupted.
- [OPEN] Reciprocal NOTIFICATION-first -> DEFAULT overlap ownership is still not closed. The 2026-08-12 audit attempt is intentionally marked inconclusive because the outbound-GPR hardware breakpoint held the target and the target-side script ended after `ALERT_START`, before Media overlap.
- [MISSING] The running Linux CPS V3 kernel has no render-family selector. Source for the loaded module hard-codes subgraph order `{0xb0000001, 0xb000007f, 0xb000007e}`.
- [MISSING] Recovered NOTIFICATION family `0x83/0x82`, including its differing MSIIR coefficient payload, is not deployed/selectable on Linux.
- [PARTIAL] Remaining Windows processing families are statically mapped, but Linux policy should only implement them when live Windows behavior proves they can own this endpoint; do not infer policy merely from static GKV existence.

## End-to-end quality / certification

- [OPEN] Matched-attenuation physical Windows-vs-Linux A/B for loudness, tonal balance, channel balance, limiter behavior and latency remains to be performed.
- [OPEN] Suspend/resume and repeated power-management lifecycle parity has not been included in this audit's 10x stream-open/close stress test.
- [PARTIAL] Audio is stable and the known signal-processing chain is substantially ported, but the missing mode-family policy means output cannot yet be called globally Windows-equivalent for all system sounds/use cases.

## Repository / provenance state

- [PASS] SP7 engineering repo `C:\Users\SurfacePro7\Documents\SP11X1e-audio-engineering` was clean and exactly synchronized with its upstream before this audit document was added.
- [PARTIAL] Evidence is split across sibling branches after common commit `fae2c7c`: render/mode work is on `7003b3d` (`agent/cps-dp6-runtime-closure-20260810`), while latest CPS review closure is on `a77a982` (`origin/agent/cps-parity-review-20260811`). This should be integrated deliberately, not silently merged during an audit.
- [PARTIAL] Linux `/home/geoca/Documents/SP11-PROJECT/01-audio` is not a clean canonical worktree; deployed Dolby/runtime state is newer/different than its checked-out branch. Treat SP7 + Git history and deployment hashes as authority until the Linux worktree is reconciled.

## Deferred by current priority

- [DEFERRED] Microphone / capture.
- [DEFERRED] Bluetooth audio.

## Render-first closure order

1. Close reciprocal NOTIFICATION-first lifetime policy with a safer non-stalling Windows trace.
2. Implement Linux endpoint-lifetime render-family selection and deploy exact DEFAULT + NOTIFICATION content without inventing Windows policy.
3. Close any other real processing-family ownership cases that live Windows applications actually select.
4. Run fresh state-pinned same-input Windows/Linux Dolby waveform certification across relevant profiles/lifecycles.
5. Validate protection intervention/threshold behavior and remaining physical L/R / ordinary-render SoundWire last-mile details.
6. Perform matched physical A/B (loudness, tone, balance, limiter behavior, latency) and power-management lifecycle stress.
7. Only after render passes the above gate, move to microphone/input; Bluetooth remains non-priority unless requested.

## Audit artifacts on SP7

- `C:\Users\SurfacePro7\Documents\SP11-Audio-Audit-20260812\windows-live-audio-modules.json` â€” live Windows `audiodg.exe` module/hash manifest.
- `C:\Users\SurfacePro7\Documents\SP11-Audio-Audit-20260812\SP11_REVERSE_OWNER_TEST_20260812.log` â€” preserved reciprocal attempt, SHA-256 `18EDE020AF60684F5E559EA8AFD46B6700268652A77640B703AD3BCC871CD692`; explicitly inconclusive for overlap ownership.
- `C:\Users\SurfacePro7\Documents\SP11-Audio-Audit-20260812\ledger.md` â€” durable working copy of this ledger.
