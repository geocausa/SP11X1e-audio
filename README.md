# SP11 X1E audio

Reproducible Linux audio bring-up for the Microsoft Surface Pro 11 using the
Qualcomm X1E80100 AudioReach, SoundWire, and WSA884x stack.

The project starts from the upstream Linux implementation.  It treats the
kernel/DT, AudioReach topology, ALSA UCM policy, and PipeWire policy as separate
layers so that a result in one layer is not mistaken for a driver fix in
another.

## Current status

### Validated audio-clean baseline — 2026-08-01

Kernel `7.1.5-sp11-audio-clean+` is installed, booted and accepted as the
pre-Dolby baseline. It came from a full `mrproper` rebuild at source commit
`f102e3fa8c7e860f3a9ac3ba2043a5fd55242e44`; the installed audio, Wi-Fi,
touch, GPI and SPI modules all match the new ABI.

The clean build removes the temporary upstream PA-volume ceiling and uses the
validated UCM operating point: PA 24 (+27 dB), WSA digital 81 (-3 dB), both
channels matched. It also removes the abandoned forced PA_AUX 18 dB experiment.
Both live amplifiers report `0xdd`, the normal variant-selected PA_AUX 0 dB
state, rather than experimental `0xe9`.

Both 8 kHz VI feedback paths run; SP/SPVI queries and the Windows-order
protection sequence succeed; the graph starts; and controlled plus sustained
playback showed no PA fault, recovery loop, SoundWire error, XRUN or channel
dropout. Playback remains routed through `sp11_dolby_bypass`. The operator
accepts the resulting ceiling as usable and materially improved over the
upstream-capped state, although it remains below Windows.

The one unsupported aggregate calibration frame remains a known diagnostic
fact and is continued exactly as Qualcomm GSL does. The rejected SPv5 event
subscription experiment is not in this build; passive event observation is
retained. Dolby dynamics and coefficient parity are the next separate phase.

Deployment identity, hashes and the completed acceptance gate are in
[`deploy/audio-clean/README.md`](deploy/audio-clean/README.md). The structured
first-boot record is
[`linux-audio-clean-first-boot-20260801.json`](artifacts/reviewed/linux-audio-clean-first-boot-20260801.json).

The sections below are chronology. Any statement that the diagnostic kernel is
current, that PA_AUX 18 dB is required, or that first boot is pending is
superseded by this baseline.

### 2026-08-02 control correction — Clean2 and MapDiag rejected

The accepted `audio-clean` entry was rebooted as an A/B control after Clean2
and MapDiag reproduced a silent physical channel and MapDiag developed a long
stream-start stall. During the control, both physical speakers remained
audible, both SoundWire amplifiers remained attached, seven protected graph
starts completed, and no SoundWire, regcache-sync, XRUN or WSA884x error was
observed. A representative graph started about 26 ms after its scoped
calibration warning. The operator confirmed that YouTube no longer stalled or
dragged as it had under MapDiag.

Fresh Ghidra verification of the hash-bound Windows `qcadcm8380.sys` corrected
the premise behind Clean2. Windows requests and sends the full selected
10,464-byte/107-frame ACDB graph calibration. In
`gsl_graph_set_sg_cal` at RVA `0x58c60`, status `3` follows a literal warning
path and graph construction continues. Windows does not remove the 48-byte
`PARAM_ID_SPR_SESSION_TIME` frame. The original `audio-clean` policy therefore
matches Qualcomm GSL more closely than Clean2's 10,416-byte filtered
aggregate. Clean2 and every candidate inheriting that topology remain rejected
for promotion.

MapDiag also exposed a measurement defect. Reading the complete WSA884x
debugfs register files populates the regmap cache; a later SoundWire detach
marks that cache dirty and can turn reattachment into a long, failure-prone
sync. Piping the generated register file through `grep` is equally invasive
because the entire file must still be read. Normal diagnostic collection now
performs no WSA884x debugfs register walk; full dumps are explicit opt-in only.

The corrected evidence and decision are recorded in
[`2026-08-02-windows-graph-calibration-warning-policy.md`](docs/findings/2026-08-02-windows-graph-calibration-warning-policy.md).
No new kernel should combine topology filtering, amplifier polling, GPIO
ownership changes and register-map collection. Remaining non-Dolby work is an
offline evidence audit of Windows' dual-reset lifecycle, post-start protection
telemetry, CPS use and any real left/right hardware asymmetry.

### Corrected diagnostic observation closure — 2026-07-31

The full-configuration observation kernel is now built, installed and proven by
boot. The machine is currently running
`7.1.5-sp11-audio-diag-observe+`; its configuration matches `audio-vi`
(4,061 built-in options, 7,651 module options, 15,516 lines) and its installed
module tree contains 7,886 files. The ALSA card, touchscreen and audible speaker
path are working. The `sp11-audio-vi` entry remains the clean rollback.

The one-shot frame diagnostic closed the calibration-support question:

```text
frames=107 accepted=106 rejected=1 first-rejected=63
IID 0x412b, parameter 0x0800113d, 28 bytes: -EOPNOTSUPP
```

The calibration corpus is not broadly invalid or wholly unsupported. One
record explains the repeated aggregate `AR_EUNSUPPORTED` warning. The
individual retry strongly supports Qualcomm GSL's continue policy, while not
claiming formal proof of aggregate transaction atomicity.

Patch `0023` captured complete GET_CFG responses and selected SoundWire ports.
The preserved success capture contains 10 complete response bodies and 55 hex
dump lines; the earlier phrase "65 bodies" conflated those two counts. The two
stable payloads are 92 and 68 bytes. Both amplifiers select render ports 1/2/3
at 48 kHz and VI port 5 at 8 kHz.

Authoritative record:
[`2026-07-31-diagnostic-observation-success.md`](docs/findings/2026-07-31-diagnostic-observation-success.md).
Reviewed evidence:
[`linux-audio-diag-observe-success-20260731.json`](artifacts/reviewed/linux-audio-diag-observe-success-20260731.json).

The next bounded candidate is runtime DSP volume control. The existing ALSA
callback only changes a cached value and sends no `VOL_CTRL` packet. First prove
a manual DSP control with PipeWire held fixed; then integrate endpoint volume
and volume-step MSIIR selection. PBR/CPS transport and the larger per-speaker
Windows tuning branches remain separate later work.

### Historical offline audit and observation candidate — 2026-07-29

The following chronology describes the state before the corrected 2026-07-31
build and boot. Statements that the diagnostic candidate is staged, uninstalled
or blocked are superseded by the closure above.

The exhaustive pre-reboot audit is complete. A deduplicated 22.87 GB payload
scan, all unique compressed archives, the buried Windows resource binaries,
Qualcomm ACDB source and fresh Ghidra headless analysis found no safe hidden
PBR/CPS implementation to deploy. The same work disproved the earlier SP11
endpoint-component/regulator interpretation: the exact Surface
`0x08000041` payload has component count zero and `ADCMResources.bin` contains
no concrete PMIC, GPIO, clock or bus operation.

A separate observation-only candidate is fully built and staged as
`7.1.5-sp11-audio-diag-observe`. Patch `0023` logs complete bounded SP/SPVI
GET_CFG responses and the actual WSA enabled/selected SoundWire port masks. It
changes no gain, mixer state, port enablement, protection policy or codec
register. The complete module catalogue and exact Phase91 GPI/SPI/touch source
versions are included and signed. The installer has passed root-level
verification but has **not** been run in install mode; no boot file or GRUB
entry has changed.

The authoritative continuation record is
[`NEXT-BOOT-HANDOFF.md`](../NEXT-BOOT-HANDOFF.md). The exhaustive evidence
closure is in
[`2026-07-29-offline-exhaustive-closure.md`](docs/findings/2026-07-29-offline-exhaustive-closure.md).

The known-working rollback is kernel `7.1.5-sp11+` on Ubuntu 26.04 LTS
aarch64. Its basic MM1 speaker route works through both WSA884x amplifiers, but
it does not implement the recovered Windows protection graph.

The active validation entry is `7.1.5-sp11-audio-vi`, GRUB ID
`sp11-audio-vi`. It keeps the complete Ubuntu module catalog and the Phase91
touch/DMA/SPI overrides. Its device tree is the built audio-VI OLED tree with
the validated Phase91 touchscreen overlay applied; both older bootable entries
remain untouched.

`audio-vi` closes two boundaries proven after V3 transport validation:

- Linux had no physical path from both WSA884x `VISENSE` sources through
  SoundWire ports 10/11, WSA macro VI and `WSA_CODEC_DMA_TX_0` backend 106;
  and
- the captured front-channel DSP gain parameter was still Q28 `0x00077f1c`,
  approximately -54.7 dB, even when PipeWire and ALSA were set to unity.

The new candidate provides the complete 8 kHz/S32_LE/stereo feedback backend,
enables it through UCM, gates SP/SPVI enable on VI readiness, and explicitly
bypasses protection if that backend cannot prepare. It uses Q28 unity in the
DSP gain stage and the X1E driver's protected -3 dB WSA ceiling. Windows'
-12 dB `DefaultDeviceVolume` is an initial endpoint position, not a fixed
hardware gain.
Dolby remains present only as an identity boundary and is not part of this
milestone.

The first `audio-vi` boot reached the desktop with the complete platform and
proved the protected kernel path using a silent zero-data probe: both 8 kHz
VISENSE sources, backend 106 readiness, SP/SPVI enable, all calibration stages,
and graph start succeeded. The second boot isolated a stale-module packaging
error, and the third boot proved normal desktop probing, the physical speaker
sink, the Dolby identity boundary and sustained audible playback with the full
protected graph active.

The current boot started at `2026-07-29 19:29:03 BST` with the exact SP11
2S/4-ohm/18-dB WSA884x profile from patch `0022`. The loaded signed module has
srcversion `203517BBF9C87B3E6B2210C` and compressed SHA-256
`56f70402882b4c48bed4411a0350b8e05b5da599766e048e49e5df01e0ff23eb`.
At `2026-07-29 21:09:25 BST`, the boot had completed 16 protected
`GRAPH_START` transactions, exposed both 8 kHz VI sources repeatedly, and
logged zero PA faults, recovery actions, XRUN-like events or SoundWire IRQ
storms. Read-only live register capture at `2026-07-29 22:33:04 BST` then
confirmed both codecs contain the exact advertised `0022` V/I gain, OCP,
current-limit and 15-step PBR values. Controlled full-volume stress, nonzero
feedback proof, PBR/CPS transport closure and Windows sound-quality parity
remain pending. See the
[first-boot evidence](artifacts/reviewed/linux-audio-vi-first-boot-20260729.json),
[second-boot binary audit](artifacts/reviewed/linux-audio-vi-second-boot-20260729.json),
[third-boot evidence](artifacts/reviewed/linux-audio-vi-third-boot-20260729.json)
[0022 reboot observation](artifacts/reviewed/linux-audio-vi-0022-reboot-observation-20260729.json)
and [0022 register readback](artifacts/reviewed/linux-audio-vi-0022-register-readback-20260729.json).

The first protected candidate was retired after its boot proved that it had
been built from an incomplete 156-module configuration and used the wrong
device tree. No ALSA card registered. Its later apparent bus-clash symptom was
separately traced to the experimental global UCM activating VI/CPS on the
fallback kernel. See the
[failure classification](artifacts/reviewed/linux-protected-boot-failure-20260728.json).

The V3 transport baseline was a clean, isolated rebuild from the verified
official Linux 7.1.5 tarball:

- kernel `7.1.5-sp11-audio-v3`;
- boot entry ID `sp11-audio-v3`;
- full known-working SP11 configuration and Phase91 platform DTB/modules;
- one strict 48 kHz, S16_LE, stereo MM1 frontend;
- one integrated AudioReach graph with 29 recovered Windows modules, 26 data
  edges, three internal control links and seven containers;
- the canonical Windows `SH_MEM_PULL_MODE` source with a 3,840-byte ring,
  separate position page and registered watermark events;
- exact render, VI and CPS `CODEC_DMA` endpoints;
- ten ordered topology stages plus the exact captured pull/format/event and
  SP/SPVI GET/SET sequence;
- VI mixer, VISENSE and CPS support present but parked for first boot;
- a real userspace Dolby boundary instantiated as two identity-copy channels,
  with no invented Dolby processing or coefficients;
- no userspace equalizer;
- 7,886 installed modules, zero unresolved dependency diagnostics, and a
  validated initramfs containing the Phase91 overrides.

The recovered endpoint contracts are:

| Role | Endpoint | Interface | Format |
|---|---|---|---|
| render | IID `0x4157` | WSA type 1, index 1 | 48 kHz, S16_LE, stereo |
| voltage/current | IID `0x4026` | WSA type 1, index 1 | 8 kHz, S32_LE, stereo |
| CPS | IID `0x402b` | type 2, index 3, mask 3 | 24 kHz, S32_LE, stereo |

V2 proved the complete platform, both amplifier resets, ALSA card
registration, clash-free idle and loading of the integrated topology. Its
successive named failures also established the OOB DMA/SID policy and the
GSL-compatible handling of unsupported graph-calibration records.

The decisive V2 failure was `AR_ENOTEXIST` at source-endpoint setup. The full
QGPR trace and recovered Qualcomm source proved why: the Linux generator had
rewritten Windows IID `0x4660`, MID `0x07001006`, to an unrelated legacy
write-command endpoint. V3 retires that translation and implements the actual
pull endpoint contract. It also includes the recovered gain, volume-step
MSIIR, mute and root channel-mixer tail before graph start.

The complete transaction comparison is in the
[Windows/Linux start ledger](docs/audit/2026-07-28-windows-linux-start-transaction-ledger.md).
The installed V3 identities and rollback policy are in the
[audio-v3 deployment record](docs/deployment/2026-07-28-audio-v3-pull-pipeline.md).
The first V3 boot isolated and patch `0016` corrected a Linux DPCM ordering
error. The second boot identified and corrected the missing Windows PCM_CNV
layout token. The third boot then completed every recovered pre-start
transaction. Its final `GRAPH_START` success reply was discarded by Linux
because graph-client response dispatch omitted lifecycle opcodes. Patch
`0017` fixed that dispatch, and the fourth boot confirmed repeated
`GRAPH_START accepted` completions with no timeout. PipeWire then exposed one
later host-state bug: ALSA calls `prepare` twice on the same open PCM, while
the persistent pull endpoint accepts its ring configuration only once. Patch
`0018` made repeated prepare idempotent, as confirmed by the fifth boot.

That boot then reached ALSA `RUNNING`, but its DSP-owned hardware position
remained zero after PipeWire filled the complete ring. Recovered AudioReach
source and the Windows map packet identify the exact mismatch: the position
page must be uncached (`0x2`), while Linux mapped it cached (`0x0`). Patch
`0019` corrects only that mapping.

The sixth boot revealed that the first `0019` deployment had accidentally
relinked a pre-`0017` core object. Live tracing proved the DSP returned
successful `GRAPH_START` status zero after 7.193 ms, while the stale callback
discarded it. A forced cumulative rebuild now contains both lifecycle reply
handling and the uncached position map in machine code.

The seventh boot validates that cumulative build. The DSP position now
advances, a direct five-second zero stream remained `RUNNING` for 240,960
frames, and a five-second PipeWire stream crossed the Dolby bypass boundary
with 510 pull watermarks and no transport error. A heavily attenuated
left/right audible probe was then completed and both sinks were returned to
zero and muted. Perceptual quality and nonzero protection feedback are not yet
claimed. See the
[position-cache finding](docs/findings/2026-07-29-pull-position-cache-contract.md)
and the
[cumulative-build finding](docs/findings/2026-07-29-cumulative-core-build-regression.md),
followed by the
[runtime validation](docs/findings/2026-07-29-pull-transport-runtime-validation.md).

Dolby dynamic processing is deliberately outside this phase. Its userspace
boundary is present in identity/bypass mode; no Dolby coefficients, EQ or
synthetic processing have been invented.

## SoundWire protection boundary

The earlier local guard kept PBR, VISENSE and CPS out of the render-direction
SoundWire stream, but the silent V3 test proved that exclusion alone was
insufficient. `audio-vi` gives VISENSE a real source port and routes the two
amplifiers through SoundWire master ports 10/11 into one render-coupled VI
backend. The controller selects physical direction from its DAI identity,
because the ASoC link intentionally uses pseudo-playback semantics to keep the
feedback backend active with render.

The DSP data-edge tuples remain the recovered Windows tuples. The additional
Linux topology bridge is DAPM-only and exists solely for backend lifecycle and
power connectivity.

**Do not add low-frequency gain until that proof is captured.**

## Deployment

`deploy/` holds the live runtime configuration, and is the source of truth:

- `deploy/ucm2/Qualcomm/x1e80100/` -- the SP11 UCM profile
- `deploy/firmware/` -- local generated protected topology manifest; opaque
  binaries and recovered vendor calibration remain untracked
- `deploy/grub/47_sp11_audio_vi` -- isolated, rollback-safe AUDIO VI entry
- `deploy/initramfs/sp11-audio-vi-phase91` -- AUDIO-VI-only early Phase91 module
  inclusion, ahead of the generic GPI/SPI copies
- `deploy/first-boot/` -- read-only automatic first-boot evidence capture
- `deploy/pipewire/98-sp11-dolby-bypass.conf` -- required two-channel identity
  stage reserving the separate Dolby project's userspace boundary
- `deploy/pipewire/99-sp11-speaker-eq.conf` -- archived optional experiment,
  disabled by default
- `deploy/install-audio-config.sh` -- idempotent installer, `--dry-run` and
  `--uninstall` supported, preflights on DT model and amp count

```sh
sudo ./deploy/install-audio-config.sh --dry-run
sudo ./deploy/install-audio-config.sh
```

`x1e80100.conf` is package-owned by `alsa-ucm-conf`; the installer protects the
SP11 branch with a `dpkg-divert` so upgrades do not clobber it. The other two
UCM files are locally added and unowned.

The failed-candidate diagnosis and clean source lineage remain in the
[audio-v2 rebuild record](docs/deployment/2026-07-28-audio-v2-rebuild.md).
The current hashes, rollback rule and first-boot gates are in the
[audio-vi deployment record](docs/deployment/2026-07-29-audio-vi-protected-pipeline.md).

### UCM speaker-count note

This machine has two WSA884x amps and two drivers. The card conf must include
`/codecs/wsa884x/two-speakers/init.conf`, not the four-speaker variant, or the
`Speakers Volume` remap resolves to nonexistent `Woofer*`/`Tweeter*` controls.

Neither upstream `wsa-macro` init file is usable: the plain one uses the
unprefixed `'WSA_RX0 Digital Volume'` namespace while this card uses the
prefixed `'WSA WSA_RX0 Digital Volume'`, and the four-speaker one additionally
csets a nonexistent `WSA2` macro. There is no `wsa-macro/two-speakers/`
directory. The boot state is therefore declared inline in the card conf.

`Wsa1SpeakerEnableSeq.conf` means WSA macro instance 1, **not** one speaker. It
is correct for this machine; do not "fix" it.

## Volume policy

All user-facing volume is in PipeWire, deliberately. `SpkrLeft/SpkrRight PA
Volume` is a 0..10 control spanning only -9..+6 dB in 1.5 dB steps, so exposing
it as the endpoint volume would leave "0%" audible at -9 dB. PA is pinned at 6
(= 0 dB). The WSA digital channels are currently pinned at 81 (= -3 dB), the
protected X1E ceiling used during validation. Windows REV_0D
`DefaultDeviceVolume=0xFFF40000` is the initial -12 dB endpoint position, not a
fixed amplifier gain; the same INF declares a 0 dB endpoint maximum.

If hardware volume is ever wanted, the correct control is `WSA WSA_RX0/RX1
Digital Volume` (0..81 in the current protected driver, 1 dB steps), which
needs its own stereo ctl-remap. Moving the protected ceiling from -3 dB to 0 dB
remains a separate, protection-gated decision.

## Known cosmetic defect

`qcom-apm gprsvc:service:2:1: CMD timeout for [1001021] opcode` at boot. The
ADSP does not answer `APM_CMD_GET_SPF_STATE`, so `audioreach_send_cmd_sync()`
waits the full `5 * HZ`. This is generic x1e80100 behaviour, not SP11 specific.

Nothing consumes the result: `apm_probe()` discards it, `q6apm_get_apm_state()`
discards the send result, and `apm->state` is read only by
`q6apm_is_adsp_ready()`, which re-queries anyway and has no caller here. The
only cost is a 5 second stall in `apm_probe()` that delays card registration to
~9.8s. `02-kernel/recipe/patches/0006-*` removes the redundant call; it is not
yet in a built kernel.

## Safety rules

- Never overwrite the currently bootable kernel, DTB, topology, or UCM files.
- Capture exact hashes before every experiment (`tools/capture-live-state.sh`).
- Build a separate boot entry with an explicit rollback path.
- Do not treat a successful PCM open as proof of speaker protection.
- Keep raw observations separate from hypotheses derived from Windows traces.
- Do not publish firmware or vendor binaries without confirming redistribution
  rights.

## Tools

Capture the current machine state without opening an audio stream:

```sh
./tools/capture-live-state.sh
```

Captures raw control values with dB scales, the decoded topology, copies of the
actual config payloads, and package ownership. Output lands in `artifacts/live/`
and is gitignored until deliberately promoted.

Check an `alsatplg` decoded configuration or binary topology for duplicate
AudioReach module instance IDs:

```sh
./tools/ar_topology_lint.py topology.conf
./tools/ar_topology_lint.py topology.bin
```

Note: a result of "checked 0 module definitions" means the linter could not
parse the file, not that the file is clean. The pre-baseline
`*-tplg.bin.bak` reports 0 and must be inspected with the inventory tool.

Create a full structural inventory, including hand-injected raw-byte modules
that `alsatplg` does not render as normal tuples:

```sh
./tools/ar_topology_inventory.py topology.bin --json inventory.json --markdown inventory.md
```

Decode containers, module lists, port declarations, and connections from a
raw Windows ACDB `POOL` GRAPH_OPEN bundle:

```sh
./tools/ar_graph_open_inventory.py 01e842_POOL.bin --offset 0x35d84
```

Decode raw GKV schemas and bind their rows to `POOL` bundles:

```sh
./tools/acdb_gkv_inventory.py GKVT.bin GKVL.bin --pool 01e842_POOL.bin --json windows-gkv.json
```

Decode the ACDB cross-subgraph lookup and resolve its SCDO/POOL bridge objects,
while preserving raw words for object forms whose semantics are still unknown:

```sh
./tools/acdb_sclu_inventory.py 00ea12_SCLU.bin \
  --scde 00f4be_SCDE.bin --scdo 00f4f2_SCDO.bin \
  --pool 01e842_POOL.bin \
  --json windows-sclu.json
```

Compose one decoded GKV/POOL bundle with only the SCLU relationships that are
explicitly typed as `APM_PARAM_ID_MODULE_CONN`:

```sh
./tools/windows_graph_closure.py windows-bundle.json windows-sclu.json \
  --json windows-closure.json
```

Generate the current evidence-backed Linux structural baseline from a decoded
installed topology:

```sh
alsatplg -d /lib/firmware/qcom/x1e80100/X1E80100-Microsoft-Surface-Pro-11-tplg.bin \
  -o installed.conf
./tools/make_structural_baseline.py installed.conf structural-baseline.conf
alsatplg -c structural-baseline.conf -o structural-baseline.tplg
./tools/ar_topology_lint.py structural-baseline.tplg
```

Recover the in-band subgraph activation lists from a decoded QGPR capture and
resolve each list against that GKV inventory:

```sh
./tools/qgpr_activation_inventory.py qgpr.decoded.csv windows-gkv.json --json activations.json
```

The remaining Windows selector and GRAPH_OPEN-body gap has a version-locked,
read-only [KDNET capture runbook](docs/runbooks/windows-kdnet-structural-gap-capture.md).

The loudness-event collector is retained for a later phase. Do not use it as a
substitute for closing the topology ledger:

```sh
./tools/capture-loudness-event.sh --duration 120 --record-sink
```

Press Enter whenever the jump is heard. The tool records PipeWire volume,
PipeWire graph events, ALSA control events, before/after control state, kernel
messages, and optionally the digital signal at the default-sink monitor. It
does not change the volume or any audio control.

## Archived userspace EQ

`deploy/pipewire/99-sp11-speaker-eq.conf` inserts a stereo biquad chain in front
of the ALSA sink when explicitly installed with `--with-pipewire-eq`. It is
disabled by default and was removed from the protected baseline. Chain: -4 dB
preamp, 140 Hz LR4 high-pass, +3.5 dB peak @220 (body), -3 dB @900 (de-box),
+2.5 dB @3.5k (presence), +2 dB shelf @9k (air).

Revised 2026-07-25. The previous revision used a +5 dB low **shelf** at 130 Hz
behind a 2nd-order 60 Hz high-pass, which lifted the entire 60-130 Hz octave --
the region where this driver has the least excursion headroom and least useful
output -- with no closed-loop protection anywhere in the path.

This is a userspace layer, **not** the DSP path. Disable it before any
Windows-parity work, or the A/B measures these biquads instead of `graph_105`.
