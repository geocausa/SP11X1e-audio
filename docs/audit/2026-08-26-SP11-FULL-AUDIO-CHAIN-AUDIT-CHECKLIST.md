# SP11 full native audio-chain audit and tackle checklist

Date: **2026-08-26**
Original audit baseline commit: **`5165c7efd09da97447e3fefc7011868e2f646bdd`**; subsequently updated for FullIO v19c closure
Current accepted boot: **FullIO v19c** (`sp11_entry=7.1.5-sp11-fullio-v19c`)
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
- the v19c DTB/topology/UCM identity and v18/Golden rollback artifacts verify exactly;
- the current public/native test suites pass when invoked with the repository root on `PYTHONPATH`.

There are **no P0 “audio is broken” findings**.

The v18 exact-volume parity boundary found by this audit is now **closed by FullIO v19c**. v18's generic render topology did not contain the Golden protected graph, so exact endpoint transaction targeting returned `-ENODEV`. FullIO restores the Golden SP/SPVI/VI/CPS/MSIIR/VOL_CTRL graph and fixes the remaining capture coexistence collision by moving capture subgraph/container IDs out of Golden's global AudioReach object namespace. Live v19c uses DSP endpoint mute, active WSA RX84 and volume-dependent GainStep/MSIIR while retaining the accepted MicArray and duplex/runtime-PM behavior.

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

- [x] FullIO v19c is the accepted promoted boot: `sp11_entry=7.1.5-sp11-fullio-v19c`.
- [x] v19c kernel is byte-identical to v18: `bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a`.
- [x] v19c initrd is byte-identical to v18: `ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d`.
- [x] v19c DTB SHA-256: `2fcfa738c229b32764ff2722847cf4056b3153c64a12f8490429309f29df6d00`.
- [x] Active DT model is `X1E80100-Microsoft-Surface-Pro-11-FullIO-v19c0`.
- [x] FullIO topology SHA-256: `e7bb06a03e7bd9b869825a51775355a6743477d1579d78eb09fad5881cfb20f0`.
- [x] `verify-native-audio-v19c.sh --live` passes.
- [x] `repro/native-audio-v19c/build-and-verify.sh` recompiles the tracked topology source byte-identically and checks the collision-free builder namespace.
- [x] Native Audio v18 remains installed as first rollback and Golden v33 remains the protected-output rollback.
- [x] **P1 — reproduce the unchanged v18/v19c kernel modules + initrd from clean Golden-v33 source plus only production mic deltas 0072 + 0078, and pin the resulting hashes.** Closed: the end-to-end pristine 7.1.5 -> Golden v33 -> 0072+0078 heavy gate reproduces all three LPASS `.ko` files and the promoted v19c initrd byte-for-byte.
- [ ] **P2 — create one complete fresh-machine installer for FullIO v19c** (boot bundle + DTB + topology + UCM + GRUB + rollback + UbiG policy) rather than relying on already-installed local state.
- [ ] **P2 — decide whether the GitHub FullIO v19c release should carry a boot/reproduction manifest in addition to the controller package.**

# 2. Kernel/module integrity and boot hardening

- [x] Production audio modules report the expected v18/v19c/Golden vermagic and live srcversions.
- [x] Modules are PKCS#7-signed with a build-time generated key.
- [~] The running kernel does not trust that build key, so module verification logs a failure and the kernel is tainted. Secure Boot is disabled, so this is not currently preventing operation.
- [ ] **P2 — make module signing reproducible and trusted by the running kernel**, or explicitly document the intended no-Secure-Boot trust model.
- [ ] **P1 — remove and test without global `clk_ignore_unused`.** It can hide incomplete clock ownership.
- [ ] **P1 — remove and test without global `pd_ignore_unused`.** It can hide incomplete power-domain ownership.
- [ ] **P2 — once both flags are removable, rerun full playback/capture/protection/runtime-PM gates.** System suspend/resume stays outside this RE.

# 3. AudioReach / ASoC / topology

- [x] AudioReach card registers successfully as the SP11 X1E80100 card.
- [x] Accepted topology recompiles byte-identically with `alsatplg`.
- [x] `MultiMedia3 Capture` is present and is the production MicArray FE.
- [x] `TX_CODEC_DMA_TX_3` is the accepted MicArray backend.
- [x] FullIO v19c contains the protected Golden render graph plus only the production `MultiMedia3` MicArray capture closure.
- [x] UCM/WirePlumber expose the intended `MultiMedia3` path as the desktop internal microphone.
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
- [x] FullIO v19c restores the protected graph target required by `SP11 Windows Volume Transaction` and `SP11 Windows Endpoint Mute`; live writes no longer fall back with `-ENODEV`.
- [x] Active WSA RX0/RX1 reaches `84` (0 dB native-Windows producer state) and returns to Golden idle `81` after close.
- [x] Volume-dependent MSIIR/GainStep applies through the exact combined transaction path; live 10% and 35% tests exercised GainStep 1/216-byte and GainStep 9/272-byte deltas.
- [x] DSP endpoint mute/unmute succeeds on the protected graph.
- [~] The prior Golden-v33 matched Windows/Linux speaker A/B remains the acoustic acceptance baseline because FullIO v19c restores that exact protected render graph. A fresh multi-volume v19c-vs-Windows matrix is optional strengthening evidence rather than a functional blocker.
- [ ] **P2 — add a regression that opens protected playback and asserts the real transaction target is writable, not merely present.**

# 5. Speaker mute, volume and fallback safety

- [x] UbiG visible sink mute correctly mirrors to the hidden hardware sink in fallback mode.
- [x] Unmute restores the hidden sink safely.
- [x] Live audit: visible volume `0.10 -> 0.12` moved hidden attenuation `0.27 -> 0.30`; restoring visible `0.10` restored hidden `0.27`.
- [x] Unknown exact-DSP-mute failures remain fail-closed.
- [x] Proven `ENODEV` switches to the downstream safe fallback rather than permanently muting the machine.
- [x] Fallback is sticky for a graph generation and no longer hammers unavailable DSP controls continuously.
- [x] Idle postgain updates are queued rather than waking the graph; the next UbiG callback acknowledged the latest queued generation (`5/5` in this audit).
- [x] FullIO v19c uses exact DSP endpoint mute/unmute during live protected playback.
- [ ] **P2 — extend the existing live mute/volume regression into a broader 1%-50% slider + seek/pause transient sweep on v19c.**

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
- [ ] **P2 — add a one-command microphone smoke test to the v19c verifier** (short capture, nonzero/RMS/channel sanity, no fault scan, runtime-PM return).
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
- [~] **DEFERRED — system suspend/resume belongs to a separate dedicated RE and is intentionally out of scope for this audio-chain checklist.** Do not treat it as audio debt here.
- [ ] **P2 — compare Windows SoundWire runtime-idle clock-stop timing (`SwrClockStopTimerMS=500`) with Linux 3000-ms autosuspend after the production path is stable without global ignore flags.** This is runtime-idle work, not system suspend/resume.

# 9. UCM / WirePlumber / PipeWire desktop policy

- [x] Hardware `Speaker` and `Mic` UCM devices are present and tracked source files match the installed copies byte-for-byte.
- [x] WirePlumber exposes one intended hardware speaker sink and one intended internal microphone source.
- [x] UbiG is the persisted/default desktop output.
- [x] No raw MultiMedia4 desktop source is exposed.
- [x] `filter-chain.service`, UbiG volume sync and monitor-link are enabled and active after reboot.
- [x] `sp11-msiir-volume-sync.service` is intentionally inactive/success when the combined transaction control is present; this is currently part of the parity gap described above, not a service crash.
- [x] **Production transparent bypass retired.** `effect_input.sp11_ubig_bypass` is no longer autoloaded or present in the normal graph; the config remains repository-only historical/debug material.
- [x] **Production installer enforces a single desktop speaker endpoint.** It removes any active bypass config, installs the WirePlumber hidden-backend policy and restarts the user audio graph.
- [x] **Endpoint policy regression added.** Tests and the live v19c verifier require UbiG as the production sink, no active bypass, and `-----` read permission for the raw ALSA speaker on the `pipewire-pulse` bridge while native UbiG access remains intact.

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

- [x] With `PYTHONPATH=.`, repository Python suite passes: **227 passed, 3 skipped, 6 subtests passed**.
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
- [x] Native Audio v18/v19c kernel delta now has full clean-source exact reproduction through `repro/native-audio-v19c/build-kernel-initrd-and-verify.sh`.
- [~] UbiG executable is source-owned, but exact production Stage-B data still depends on a private owner pack.
- [x] **P1 — make a clean clone capable of reproducing the non-private v18/v19c kernel/initrd runtime artifact set and detecting local-only dependency.** The heavy gate pins source, patches, module bytes/srcversions, cpio metadata and final initrd SHA.
- [ ] **P2 — document prerequisites, kernel source baseline, exact patch order, module list, initrd construction, topology install, UCM install and GRUB rollback as one end-to-end procedure.**
- [ ] **P2 — add a “fresh machine” deployment rehearsal using only repo + explicitly declared owner-supplied data.**

# 14. Local machine cleanup / provenance hygiene

- [x] Active DT sound model makes the selected `VA-TX-AB-v16` topology unambiguous.
- [x] Current installed production UCM files match tracked sources.
- [ ] **P2 — remove or archive historical topology binaries from `/lib/firmware/qcom/x1e80100/` after copying hashes/evidence to the repo.** Current directory still contains Render-Parity, CPS-Parity, CPS-Headroom, Mic-EP16, VA-Diagnostic v14/v15 and backup files.
- [ ] **P2 — remove old UCM `.bak-*` files from the live UCM directory after confirming they are preserved in project evidence/history.**
- [x] **`98-sp11-ubig-bypass.conf` retired from normal production startup.** The live user config is absent; tracked copy is marked diagnostic/historical only.
- [ ] **P3 — audit remaining active user-systemd symlinks and generated drop-ins after each future promotion so no candidate/masked unit can silently survive into production.**
- [ ] **P3 — keep historical Dolby/Windows bridge code as provenance only; ensure no production install path can select it.**

# 15. System suspend / resume — externally deferred

- [~] **DEFERRED / OUT OF SCOPE.** System suspend/resume is a known separate platform issue with its own dedicated reverse-engineering effort. Do not spend audio-chain time on it and do not use it as a release gate for the built-in audio work tracked here.

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

On FullIO v19c the exact Windows path is live:

- protected playback opens the Golden SP/SPVI/VI/CPS/MSIIR/VOL_CTRL graph;
- endpoint DSP mute/unmute succeeds;
- active WSA RX0/RX1 is `84`, returning to idle `81` after close;
- 10% endpoint volume uses GainStep 1 / 216-byte delta;
- 35% uses GainStep 9 / 272-byte delta;
- exact volume control remained active during simultaneous MicArray capture;
- no host-attenuation `ENODEV` fallback was observed in the accepted v19c test.

The primary v18 volume-path gap is therefore closed. The next work is hardening/reproduction, not another render-path RE.

---

# Recommended tackle order

1. **P1/P2 — remove `clk_ignore_unused` / `pd_ignore_unused` and revalidate runtime power ownership.**
2. **P2 — close public owner-pack provenance/distribution for UbiG Stage B.**
3. **P2 — clean stale live firmware/UCM artifacts after preserving evidence hashes.**
4. **P2 — fix bare `pytest`, add one top-level production check and CI.**
5. **P2 — build a complete fresh-machine FullIO v19c installer/release manifest.**
6. **P2/P3 — module-signing hardening, optional SPF timeout cleanup, then external Bluetooth/USB/DP integration and upstreaming.**

System suspend/resume is intentionally absent from this order because it belongs to the separate dedicated RE.

---

# Canonical recheck commands

```bash
# FullIO v19c identity + live desktop path
./deploy/native-audio-v19c/verify-native-audio-v19c.sh --live

# Clean topology reproduction
./repro/native-audio-v19c/build-and-verify.sh

# Heavy exact kernel/module/initrd reproduction
JOBS=12 ./repro/native-audio-v19c/build-kernel-initrd-and-verify.sh

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
- keep `MultiMedia3 -> TX_CODEC_DMA_TX_3` and the accepted FullIO-v19c capture object namespace for production;
- keep Golden v33 as the speaker/protection rollback baseline;
- treat Windows as the behavioral oracle where Linux/upstream assumptions conflict with measured SP11 behavior;
- preserve rollback-safe boot entries for any kernel/power/transaction experiment.
