# SP11 full native audio-chain audit and tackle checklist

Date: **2026-08-26**
Audited repository commit: **`5165c7efd09da97447e3fefc7011868e2f646bdd`**
Audited boot: **Native Audio v18** (`sp11_entry=7.1.5-sp11-dmic-broker-div4-v18`)
Kernel: **`7.1.5-sp11-render-parity-v4+`**

## Verdict

The built-in SP11 audio chain is **functionally complete and usable as a daily driver**:

- built-in stereo speaker playback works through UbiG;
- WSA8845 VI/CPS speaker protection comes up during playback and remains fault-free;
- the two-channel internal MicArray works through the accepted TX/EP16 route;
- simultaneous playback + capture works;
- UbiG built-in profiles, Custom EQ, postgain control, persistence and reboot restore work;
- mute/unmute and volume are safe;
- all audio MMIO blocks and SoundWire return to runtime suspend after use;
- the v18 DTB/topology/UCM identity and Golden-v33 rollback artifacts verify exactly;
- the current public/native test suites pass when invoked with the repository root on `PYTHONPATH`.

There are **no P0 “audio is broken” findings**.

However, this audit reopens one important parity boundary: **Native Audio v18 currently does not execute the exact Windows endpoint volume/mute + GainStep transaction path.** The ALSA controls exist, but the combined graph returns `-ENODEV`; the production synchronizer safely falls back to host attenuation/hardware mute. As a result, active WSA RX remains at `81` instead of the Windows-parity `84`, and the volume-dependent MSIIR/GainStep update is not currently applied by the standalone service. The chain is safe and audible, but exact volume-dependent Windows speaker parity should not be claimed on v18 until this is closed and a fresh matched A/B is run.

---

## Status legend

- [x] **PASS** — directly revalidated in this audit or covered by a current pinned acceptance artifact.
- [~] **ACCEPTED DEBT / VERIFY** — understood and non-blocking, but worth closing.
- [ ] **TODO / GAP** — actionable work remaining.
- **P1** — highest-value closure work.
- **P2** — production/reproducibility hardening.
- **P3** — cleanup, integration or optional expansion.

---

# 1. Release identity, boot and rollback

- [x] Native Audio v18 is the saved GRUB default: `saved_entry=sp11-audio-dmic-broker-div4-v18`.
- [x] Running command line contains `sp11_entry=7.1.5-sp11-dmic-broker-div4-v18`.
- [x] v18 initrd SHA-256 matches the accepted identity: `ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d`.
- [x] v18 DTB SHA-256 matches: `09dcf2832487b1523ab2cdecba4ef9f2335d4e95e1bcd87a2dad41208d20ae0a`.
- [x] Active DT model is `X1E80100-Microsoft-Surface-Pro-11-VA-TX-AB-v16`.
- [x] Accepted combined topology is installed with SHA-256 `4e00057b8e316c217347bcdee0af0c6d4ff40e8e0f1870d7efeaddc2669ff54e`.
- [x] `verify-native-audio-v18.sh --live` passes.
- [x] Golden v33 rollback kernel/initrd/DTB/topology/modules still pass `verify-golden-v33.sh`.
- [x] Git `main`, remote `main` and the promoted microphone branch point to the same current commit.
- [ ] **P1 — add a clean Native Audio v18 rebuild path.** There is no `repro/native-audio-v18/build-and-verify.sh` equivalent to Golden v33. The current verifier proves identity, not rebuildability.
- [ ] **P1 — reproduce exact v18 kernel modules + initrd from a clean Golden-v33 source plus only production mic deltas 0072 + 0078, and make the resulting hashes part of the gate.**
- [ ] **P2 — create one complete install/deploy path for v18** (boot bundle + DTB + topology + UCM + GRUB entry + rollback) rather than relying on already-installed local state.
- [ ] **P2 — decide whether the GitHub Native Audio v18 release should carry a boot bundle/reproduction manifest in addition to `ubig-control_0.1.3_arm64.deb`.**

# 2. Kernel/module integrity and boot hardening

- [x] Production audio modules report the expected v18/Golden vermagic and live srcversions.
- [x] Modules are PKCS#7-signed with a build-time generated key.
- [~] The running kernel does not trust that build key, so module verification logs a failure and the kernel is tainted. Secure Boot is disabled, so this is not currently preventing operation.
- [ ] **P2 — make module signing reproducible and trusted by the running kernel**, or explicitly document the intended no-Secure-Boot trust model.
- [ ] **P1 — remove and test without global `clk_ignore_unused`.** It can hide incomplete clock ownership.
- [ ] **P1 — remove and test without global `pd_ignore_unused`.** It can hide incomplete power-domain ownership.
- [ ] **P2 — once both flags are removable, rerun full playback/capture/protection/runtime-PM/suspend gates.**

# 3. AudioReach / ASoC / topology

- [x] AudioReach card registers successfully as the SP11 X1E80100 card.
- [x] Accepted topology recompiles byte-identically with `alsatplg`.
- [x] `MultiMedia3 Capture` is present and is the production MicArray FE.
- [x] `TX_CODEC_DMA_TX_3` is the accepted MicArray backend.
- [x] `MultiMedia4 Capture` is intentionally present in the combined topology, not a stray kernel PCM.
- [x] UCM/WirePlumber expose only the intended `MultiMedia3` path as the desktop internal microphone; no desktop `MultiMedia4` source is published.
- [~] Each recent boot logs one `qcom-apm ... CMD timeout for [1001021]`. This is `APM_CMD_GET_SPF_STATE (0x01001021)`, a long-standing optional early SPF-state probe; it occurs before normal DAI registration and did not recur during this audit's active stress.
- [ ] **P3 — eliminate or downgrade the known optional `GET_SPF_STATE` boot timeout** so a clean boot has no misleading APM timeout marker.
- [ ] **P2 — add a topology/UCM lint asserting that only the intended MicArray FE is exported to desktop policy even if extra topology PCMs exist.**

# 4. Speaker hardware path and protection

- [x] Hardware UCM speaker sink exists.
- [x] Left/right WSA digital volume boot state is `81`.
- [x] Left/right PA volume is pinned at the accepted protected value `24`.
- [x] Left/right VISENSE controls are enabled.
- [x] Both `WSA_SPKR_VI_1` and `WSA_SPKR_VI_2` mixer paths are enabled.
- [x] During live playback, both left/right CPS switches were observed **on**.
- [x] During live playback, both VISENSE switches and both VI mixer paths remained **on**.
- [x] Golden v33 promotion evidence includes source-identical Windows/Linux acoustic A/B within roughly 0.1 dB at 10% and 50% and a 20/20 true-cold 50% protection soak with 0 PA faults, 0 `err0=0x20` and 0 XRUNs.
- [x] This audit's live duplex stress produced no PA fault, `err0=0x20`, XRUN, SoundWire error, GLINK timeout or recurring APM timeout.
- [ ] **P1 — fix exact volume-transaction target availability on Native Audio v18.** `SP11 Windows Volume Transaction`, `SP11 Windows Volume Only` and `SP11 Windows Endpoint Mute` exist, but writes on the combined v18 graph return `-ENODEV`.
- [ ] **P1 — restore active WSA RX producer state `84` (0 dB) when the Windows-parity transaction path is active.** Current v18 active playback remains at `81` because the exact transaction fails.
- [ ] **P1 — restore volume-dependent MSIIR/GainStep application on v18.** `sp11-msiir-volume-sync.service` currently exits successfully because the combined transaction control exists, but that combined transaction then falls back on `ENODEV`; therefore the standalone MSIIR updater does not take ownership either.
- [ ] **P1 — after fixing transaction/MSIIR ownership, repeat the fresh matched Windows ↔ Linux speaker A/B at several endpoint volumes** (at minimum 10%, 25%, 50%; include FR and time-frequency metrics).
- [ ] **P1 — until the exact transaction is fixed, run a fresh matched Windows ↔ current-v18-fallback speaker matrix** so the actual current daily-driver loudness/FR difference is explicitly quantified instead of inferred from Golden-v33 evidence.
- [ ] **P2 — add a regression that fails if “transaction control exists” but the real graph target returns `ENODEV`.** Presence alone is not sufficient capability detection.

# 5. Speaker mute, volume and fallback safety

- [x] UbiG visible sink mute correctly mirrors to the hidden hardware sink in fallback mode.
- [x] Unmute restores the hidden sink safely.
- [x] Live audit: visible volume `0.10 -> 0.12` moved hidden attenuation `0.27 -> 0.30`; restoring visible `0.10` restored hidden `0.27`.
- [x] Unknown exact-DSP-mute failures remain fail-closed.
- [x] Proven `ENODEV` switches to the downstream safe fallback rather than permanently muting the machine.
- [x] Fallback is sticky for a graph generation and no longer hammers unavailable DSP controls continuously.
- [x] Idle postgain updates are queued rather than waking the graph; the next UbiG callback acknowledged the latest queued generation (`5/5` in this audit).
- [ ] **P1 — close the exact Windows endpoint mute path on v18** so physical hardware mute is a fallback, not the normal actuator.
- [ ] **P2 — rerun mute/unmute, 1%-50% slider sweeps, pause/resume and seek transients after exact transaction repair.**

# 6. Microphone physical path

- [x] UCM publishes `Internal microphone array` as stereo capture on `hw:0,2`.
- [x] Production route is `MultiMedia3 -> TX_CODEC_DMA_TX_3`.
- [x] Windows lane mapping is applied: DEC0 <- DMIC1, DEC1 <- DMIC0 through `MSM_DMIC`.
- [x] TX DEC0/DEC1 capture mixers are enabled when the Mic device is active.
- [x] Patch 0072 supplies the accepted VA DMIC DIV4 behavior from the native 19.2 MHz basis.
- [x] Patch 0078 models TX capture's dependency on the VA-owned shared DMIC clock.
- [x] `vdd-micb` and VA/TX resources are stream/DAPM-owned rather than permanently pinned.
- [x] Accepted Windows RAW ↔ Linux v18 microphone parity index is **98.27%**.
- [x] Accepted Linux capture is 48 kHz, stereo, S16_LE.
- [x] This audit captured a fresh 12.20-second stereo MicArray file during simultaneous speaker playback; both channels were nonzero and the 997-Hz speaker stimulus appeared at **54.11 dB / 54.82 dB prominence**.
- [x] No capture reset/crash occurred.
- [ ] **P2 — add a one-command microphone smoke test to the v18 verifier** (short capture, nonzero/RMS/channel sanity, no fault scan, runtime-PM return).
- [ ] **P3 — optionally add a quiet-room self-noise/channel-balance baseline** to catch future degraded-but-nonzero microphone failures.

# 7. Duplex / simultaneous input-output

- [x] Speaker playback and MicArray capture ran simultaneously in this audit.
- [x] `pcm0p` and `pcm2c` were both `RUNNING` concurrently.
- [x] WSA protection and MicArray TX/VA resources were active concurrently.
- [x] No PA/XRUN/SoundWire/APM runtime fault was logged during duplex operation.
- [x] UbiG profile changes were applied in place while duplex audio was running; filter-chain PID remained unchanged.
- [ ] **P2 — add duplex to the automated acceptance gate** instead of relying on manual/reviewed evidence.
- [ ] **P3 — test real communications workloads** (browser/WebRTC, PipeWire/Pulse app, conferencing app) for format negotiation and echo/duplex behavior.

# 8. Runtime PM and low-power audio lifecycle

- [x] During duplex operation, WSA/TX/VA/SoundWire devices transitioned active as required.
- [x] After streams closed and the normal ~3-second autosuspend interval elapsed, WSA macro(s), RX, TX, VA and SoundWire all returned to `runtime_status=suspended`, `runtime_usage=0`.
- [x] All ALSA PCMs returned to `closed`.
- [x] No persistent MicArray power reference remained after capture.
- [ ] **P1 — system suspend/resume is still unvalidated and explicitly not claimed by the current release.** Test s2idle with idle audio, active/recent playback, active/recent capture and repeated cycles.
- [ ] **P1 — run at least 20 suspend/resume cycles with post-resume speaker + mic smoke and PA/XRUN/SoundWire/APM fault scan.**
- [ ] **P2 — verify UbiG/filter-chain/control-page state, saved Custom profile and volume/mute state survive system resume without manual restart.**
- [ ] **P2 — compare Windows SoundWire clock-stop timing (`SwrClockStopTimerMS=500`) with Linux 3000-ms autosuspend after the production path is stable without global ignore flags.**

# 9. UCM / WirePlumber / PipeWire desktop policy

- [x] Hardware `Speaker` and `Mic` UCM devices are present and tracked source files match the installed copies byte-for-byte.
- [x] WirePlumber exposes one intended hardware speaker sink and one intended internal microphone source.
- [x] UbiG is the persisted/default desktop output.
- [x] No raw MultiMedia4 desktop source is exposed.
- [x] `filter-chain.service`, UbiG volume sync and monitor-link are enabled and active after reboot.
- [x] `sp11-msiir-volume-sync.service` is intentionally inactive/success when the combined transaction control is present; this is currently part of the parity gap described above, not a service crash.
- [ ] **P2 — make the transparent UbiG bypass on-demand instead of permanently auto-loaded.** Today the bypass output and UbiG output are both permanently linked to the hardware speaker sink; the bypass is idle, but accidental selection already caused a real user-visible regression once.
- [ ] **P2 — production installer should create/enable diagnostic bypass only when explicitly requested, or provide a separate `sp11-ubig bypass-on/off` diagnostic lifecycle.**
- [ ] **P3 — add a policy regression asserting the persisted default is UbiG after login/reboot and that bypass cannot silently become default.**

# 10. UbiG DSP engine and profiles

- [x] Installed production plugin SHA-256: `07efd17acc2d342af0e6a8f0a0cc5bc2b8ee2b47006f118457b9a771d7bcf2ca`.
- [x] Fresh local build of the candidate plugin is byte-identical to the installed production plugin.
- [x] Runtime plugin dependencies are only libc/libm/platform loader; no Windows DLL or PE-loader dependency is present.
- [x] UbiG engine flags report live when processing is instantiated.
- [x] Dynamic, Movie, Music, Game, Voice, Course and Custom controls all acknowledge correctly.
- [x] Profile regression yields six distinct stereo outputs; Music/Game are intentionally bit-identical under the final SP11 Windows 2-channel stereo policy.
- [x] Custom 20-band EQ works and survives reboot/login restore.
- [x] Postgain is independently versioned and acknowledged.
- [x] Full native UbiG DSP unit/regression suite passes.
- [x] Candidate control/order/profile matrix passes.
- [ ] **P1/P2 — close public provenance/distribution of the private Stage-B owner-data pack.** Production currently requires `$HOME/.local/share/ubig-private/sp11-stageb-v4.pack`, SHA-256 `30b9b8ce8dace4a9f5dee2c2defa7da2d9b8431cf68fb323f8d2c3e4e3c942df`.
- [ ] **P2 — define a public, legal, reproducible way for an owner to generate the exact pack from their own source material, or replace the remaining private data with source-owned/publicly redistributable tuning.**
- [ ] **P3 — audit pack/schema naming in source/spec comments (`v3`/`v4`) so deployment version and schema version cannot be confused.**

# 11. UbiG controller package

- [x] Installed `ubig-control 0.1.3 arm64`.
- [x] `.deb` rebuild is reproducible with SHA-256 `b19997a28adbe7ad35927c3033b9d8cec297aba4bd66268179bf2d585b297224`.
- [x] Published `.deb` is attached to the Native Audio v18 GitHub release.
- [x] Profile dropdown applies immediately.
- [x] Engine-online/offline state is visible to the controller.
- [x] Saved profile/Custom EQ restores automatically at GNOME login.
- [x] Production installer un-masks/enables the real filter-chain and rejects bypass as the persisted default.
- [ ] **P3 — add an explicit UI warning when the volume transaction is in host-fallback mode** so “UbiG engine live” is not mistaken for “all Windows parity actuators live.”
- [ ] **P3 — surface current WSA/MSIIR actuator mode in `ubigctl status` or a dedicated `sp11-audio status`.**

# 12. Automated tests and CI hygiene

- [x] With `PYTHONPATH=.`, repository Python suite passes: **224 passed, 3 skipped, 6 subtests passed**.
- [x] Full UbiG native DSP suite passes.
- [x] Candidate control lifecycle, VR->VLLDP order and seven-profile matrix pass.
- [x] Native Audio v18 verifier passes.
- [x] Golden v33 artifact verifier passes.
- [ ] **P2 — fix the test harness so bare `pytest -q` works from a clean clone.** Current bare invocation fails collection because `tools/` and `deploy/` are not on the import path.
- [ ] **P2 — provide one canonical top-level command** (for example `make check` or `./tools/check-production-audio.sh`) that runs Python tests, UbiG tests, topology round-trip, v18 verifier, package reproducibility and static hygiene.
- [ ] **P2 — add CI for architecture-independent tests and artifact verification.**
- [ ] **P3 — add an ARM64/native hardware CI checklist or documented manual hardware gate for the tests that cannot run in generic CI.**

# 13. Public reproducibility / distribution

- [x] Golden v33 has a clean reproduction path.
- [x] Native Audio v18 pins exact DTB, topology, UCM and parity evidence.
- [x] UbiG controller package is reproducible.
- [~] Native Audio v18 currently proves identity but not full clean-source reproduction.
- [~] UbiG executable is source-owned, but exact production Stage-B data still depends on a private owner pack.
- [ ] **P1 — make a clean clone capable of reproducing every non-private v18 runtime artifact and detecting any local-only dependency.**
- [ ] **P2 — document prerequisites, kernel source baseline, exact patch order, module list, initrd construction, topology install, UCM install and GRUB rollback as one end-to-end procedure.**
- [ ] **P2 — add a “fresh machine” deployment rehearsal using only repo + explicitly declared owner-supplied data.**

# 14. Local machine cleanup / provenance hygiene

- [x] Active DT sound model makes the selected `VA-TX-AB-v16` topology unambiguous.
- [x] Current installed production UCM files match tracked sources.
- [ ] **P2 — remove or archive historical topology binaries from `/lib/firmware/qcom/x1e80100/` after copying hashes/evidence to the repo.** Current directory still contains Render-Parity, CPS-Parity, CPS-Headroom, Mic-EP16, VA-Diagnostic v14/v15 and backup files.
- [ ] **P2 — remove old UCM `.bak-*` files from the live UCM directory after confirming they are preserved in project evidence/history.**
- [ ] **P2 — retire the permanently loaded `98-sp11-ubig-bypass.conf` from normal production startup once on-demand bypass exists.**
- [ ] **P3 — audit remaining active user-systemd symlinks and generated drop-ins after each future promotion so no candidate/masked unit can silently survive into production.**
- [ ] **P3 — keep historical Dolby/Windows bridge code as provenance only; ensure no production install path can select it.**

# 15. System suspend / resume — not yet accepted

- [ ] **P1 — idle suspend/resume: 20 cycles.** Verify audio card, UbiG sink, mic source and runtime PM after every resume.
- [ ] **P1 — playback-before-suspend / playback-after-resume.** Verify no PA/CPS/VI/SoundWire fault and no stale mute.
- [ ] **P1 — capture-before-suspend / capture-after-resume.** Verify MicArray route, stereo data and vdd-micb/VA/TX return to idle afterwards.
- [ ] **P1 — suspend with recent duplex activity.** Verify both sides recover without reboot.
- [ ] **P2 — profile/Custom EQ/postgain/visible volume persistence across resume.**
- [ ] **P2 — long-idle after resume and repeated wake/sleep power-state audit.**

# 16. External / non-built-in audio integration

These are **not blockers for the accepted built-in speaker + MicArray chain**, but are the next integration layer if the goal becomes “all audio on the device,” not only native built-in audio.

- [ ] **P3 — Bluetooth A2DP playback.**
- [ ] **P3 — Bluetooth HFP/HSP microphone/duplex.**
- [ ] **P3 — USB audio playback/capture and hotplug.**
- [ ] **P3 — DisplayPort/HDMI audio through external display.**
- [ ] **P3 — verify the chassis's inert local headset/mic-jack controls remain policy-hidden and cannot create bogus endpoints.**
- [ ] **P3 — browser/WebRTC and conferencing-app integration.**

# 17. Upstreamability / kernel-quality closure

- [ ] **P2 — prove v18 without `clk_ignore_unused` and `pd_ignore_unused`.**
- [ ] **P2 — reduce SP11-specific command-line feature switches to normal DT/driver behavior where practical.**
- [ ] **P2 — separate production patches from diagnostic/rejected experiment history in a clean ordered series.**
- [ ] **P2 — validate bindings/DT changes against current upstream schemas.**
- [ ] **P2 — run `checkpatch`, relevant kernel build targets and clean module modpost from a fresh source baseline.**
- [ ] **P3 — document which changes are board quirks versus generally useful Qualcomm LPASS/SoundWire fixes before upstream submission.**

---

# Current live audit measurements

## Fresh duplex audit

A fresh deterministic 997-Hz stimulus was played through `effect_input.sp11_ubig` while the internal MicArray recorded concurrently.

- capture: 48 kHz / stereo / S16_LE;
- duration: 12.2027 s;
- recording SHA-256: `7f1f4bba2309e86eec5c4919706cdd67a1666f3b0bff5326333ae8e161d3e7dc`;
- channel 0: RMS `7.783`, peak `51`, 997-Hz prominence **54.11 dB**, nonzero 91.98%;
- channel 1: RMS `8.193`, peak `54`, 997-Hz prominence **54.82 dB**, nonzero 92.22%;
- both playback and capture PCMs were `RUNNING` simultaneously;
- both CPS switches, both VISENSE switches and both WSA VI mixer paths were on;
- Movie -> Voice -> Music -> Game -> saved Custom profile transitions all acknowledged in place;
- filter-chain PID remained stable;
- no PA/XRUN/SoundWire/APM runtime fault appeared during the test;
- all tested audio MMIO devices and SoundWire later returned to runtime suspend/usage 0.

## Current volume-path reality

At visible UbiG volume `0.10`:

- hidden hardware sink is safely attenuated to `0.27` in fallback mode;
- desired/active UbiG postgain after wake is `-545`;
- exact endpoint mute/combined transaction writes return `-ENODEV` on v18;
- active WSA RX0/RX1 remain `81`, not Windows-parity active `84`;
- standalone MSIIR service exits because the combined transaction control exists;
- therefore exact volume-dependent MSIIR/GainStep parity is **not currently active**.

This is the primary technical gap to close next.

---

# Recommended tackle order

1. **P1 — fix the v18 combined graph's exact endpoint volume/mute transaction target (`-ENODEV`).**
2. **P1 — restore WSA RX84 + volume-dependent MSIIR/GainStep ownership and prove it live.**
3. **P1 — rerun matched Windows ↔ v18 speaker A/B across several volumes; record a new acceptance artifact.**
4. **P1 — build a clean `repro/native-audio-v18` path that reproduces production modules/initrd from Golden v33 + 0072/0078.**
5. **P1 — run and close system suspend/resume stress for speaker, mic and duplex.**
6. **P1/P2 — remove `clk_ignore_unused` / `pd_ignore_unused` and revalidate power ownership.**
7. **P2 — close public owner-pack provenance/distribution for UbiG Stage B.**
8. **P2 — make bypass on-demand and clean stale live firmware/UCM artifacts.**
9. **P2 — fix bare `pytest`, add one top-level production check and CI.**
10. **P2/P3 — module-signing hardening, optional SPF timeout cleanup, then external Bluetooth/USB/DP integration.**

---

# Canonical recheck commands

```bash
# Native Audio v18 identity + live desktop path
./deploy/native-audio-v18/verify-native-audio-v18.sh --live

# Golden speaker/protection rollback artifacts
sudo ./deploy/golden-v33/verify-golden-v33.sh

# Repository Python tests (current required invocation)
PYTHONPATH=. pytest -q

# Native UbiG DSP tests
make -C ubig check

# Runtime profile/control/order matrix
UBIG_SP11_STAGEB_PACK="$HOME/.local/share/ubig-private/sp11-stageb-v4.pack" \
  make -C ubig candidate-control-check

# Controller package reproducibility
./packaging/debian/build-control-deb.sh 0.1.3
sha256sum dist/ubig-control_0.1.3_arm64.deb
# expected b19997a28adbe7ad35927c3033b9d8cec297aba4bd66268179bf2d585b297224
```

## Do not regress already-closed boundaries

When tackling this list, do **not** reopen accepted microphone divider/routing or Golden-v33 protection behavior without reproducible counter-evidence. In particular:

- keep MicArray production code at the accepted 0072 + 0078 delta unless a failing test proves a required change;
- keep `MultiMedia3 -> TX_CODEC_DMA_TX_3` and the accepted VA-TX-AB-v16 topology for microphone parity work;
- keep Golden v33 as the speaker/protection rollback baseline;
- treat Windows as the behavioral oracle where Linux/upstream assumptions conflict with measured SP11 behavior;
- preserve rollback-safe boot entries for any kernel/power/transaction experiment.
