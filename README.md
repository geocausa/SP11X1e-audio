# SP11 X1E audio

Reproducible Linux audio bring-up for the Microsoft Surface Pro 11 using the
Qualcomm X1E80100 AudioReach, SoundWire, and WSA884x stack.

The project starts from the upstream Linux implementation.  It treats the
kernel/DT, AudioReach topology, ALSA UCM policy, and PipeWire policy as separate
layers so that a result in one layer is not mistaken for a driver fix in
another.

## Current status

The known-working rollback is kernel `7.1.5-sp11+` on Ubuntu 26.04 LTS
aarch64. Its basic MM1 speaker route works through both WSA884x amplifiers, but
it does not implement the recovered Windows protection graph.

The first protected candidate was retired after its boot proved that it had
been built from an incomplete 156-module configuration and used the wrong
device tree. No ALSA card registered. Its later apparent bus-clash symptom was
separately traced to the experimental global UCM activating VI/CPS on the
fallback kernel. See the
[failure classification](artifacts/reviewed/linux-protected-boot-failure-20260728.json).

The current candidate is a clean, isolated rebuild from the verified official
Linux 7.1.5 tarball:

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
`0018` makes repeated prepare idempotent for this fixed-format pull graph; its
signed frontend override is installed for the next one-shot V3 boot. Physical
playback and nonzero protection feedback are not yet claimed.

Dolby dynamic processing is deliberately outside this phase. Its userspace
boundary is present in identity/bypass mode; no Dolby coefficients, EQ or
synthetic processing have been invented.

## SoundWire protection boundary

Mainline Linux 7.1 adds every enabled WSA884x sink port to the playback
SoundWire stream. On this board that includes PBR, VISENSE and CPS even though
the recovered Windows graph represents VI/CPS as internal
`CODEC_DMA_SOURCE` endpoints.

Patch `0005` therefore carries a **local candidate guard** which excludes those
three sink ports from the playback-direction stream while leaving their codec
enable controls active. This is not an upstream fix and is not yet proof of a
complete feedback path. It addresses the previously observed playback bus
collision; only the protected boot can establish whether the integrated DSP
graph receives nonzero VI/CPS data.

**Do not add low-frequency gain until that proof is captured.**

## Deployment

`deploy/` holds the live runtime configuration, and is the source of truth:

- `deploy/ucm2/Qualcomm/x1e80100/` -- the SP11 UCM profile
- `deploy/firmware/` -- local generated protected topology manifest; opaque
  binaries and recovered vendor calibration remain untracked
- `deploy/grub/46_sp11_audio_v3` -- isolated, rollback-safe V3 boot entry
- `deploy/initramfs/sp11-audio-v3-phase91` -- V3-only early Phase91 module
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
The current hashes, rollback rule and V3 acceptance gates are in the
[audio-v3 deployment record](docs/deployment/2026-07-28-audio-v3-pull-pipeline.md).

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
(= 0 dB). The WSA digital volume sits at 72 (= -12 dB), matching the Windows
REV_0D default `DefaultDeviceVolume=0xFFF40000`.

If hardware volume is ever wanted, the correct control is `WSA WSA_RX0/RX1
Digital Volume` (0..84, 1 dB steps), which needs its own stereo ctl-remap.

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
