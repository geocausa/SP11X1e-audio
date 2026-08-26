# CURRENT SP11 AUDIO HANDOFF

**Read this first when resuming the project.**

Date: 2026-08-26
Current promoted full-audio boot: **FullIO v19c**
Speaker/protection base: **Golden v33**
Production userspace output engine: **UbiG**
Production input policy: **UCM/WirePlumber internal MicArray**
Repository: `geocausa/SP11X1e-audio`
Canonical branch: `main`

System suspend/resume is **externally deferred** to its own dedicated RE. It is
not an audio release gate here. Runtime PM/autosuspend remains part of this
release and is accepted.

## Machine / promoted boot identity

- kernel `7.1.5-sp11-render-parity-v4+`
- marker `sp11_entry=7.1.5-sp11-fullio-v19c`
- saved GRUB entry `sp11-audio-fullio-v19c`
- first rollback `sp11-audio-dmic-broker-div4-v18`
- second rollback `sp11-audio-golden-v33-topcfg1-physical-vi`
- kernel SHA-256 `bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a`
- initrd SHA-256 `ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d`
- DTB SHA-256 `2fcfa738c229b32764ff2722847cf4056b3153c64a12f8490429309f29df6d00`
- FullIO topology SHA-256 `e7bb06a03e7bd9b869825a51775355a6743477d1579d78eb09fad5881cfb20f0`
- UCM SHA-256 `9d36df8570b85f1dcecc385a8f85fa2d1e1058ef8efedee6ae2ce49dc259a06a`

Loaded microphone-related module identities remain the accepted v18 ones:

- LPASS common `2EA7312A851E75A7C860F82`
- VA macro `DC4373218C279E16F550900`
- TX macro `835AF5272E94DB266E85D55`

The kernel/initrd and production mic code are unchanged from Native Audio v18.
Production microphone patches remain **0072 + 0078 only**.

## Why FullIO v19c exists

Native Audio v18 closed microphone parity but used the generic combined
VA/TX topology for playback. That graph did not contain the complete Golden
protected render chain, so the exact Windows endpoint volume/mute + GainStep
transaction controls existed but their protected graph target returned
`-ENODEV`. The safe userspace fallback kept playback usable, but active WSA RX
stayed at 81 and exact volume-dependent MSIIR/GainStep parity was not active.

FullIO restores the exact Golden protected render graph and adds only the proven
MicArray capture closure. The first merged candidates exposed the remaining
coexistence bug: the v18 capture FE used subgraph/container `0x4003`, while the
resident Golden render graph owns module IID `0x4003`. SPF accepts the capture
graph alone but rejects it when the Golden graph is already resident.

v19c keeps the proven capture module IIDs and moves only its graph objects to
non-colliding native high namespaces:

- capture FE: subgraph `0xb0000203`, container `0xe0000203`;
- capture backend: subgraph `0xb0000209`, container `0xe0000209`.

`tools/build_sp11_native_audio_topology.py` now rejects cross-class AudioReach
object-ID collisions so this failure mode cannot silently return.

## Accepted microphone path

The Windows oracle uses the VA macro's shared physical DMIC clock even when the
visible MicArray data path is TX/EP16:

1. patch 0072 derives the Denali DMIC divider from the native 19.2 MHz VA MCLK,
   giving Windows-parity `VA 0x3084=0x05` (DIV4 + enable);
2. patch 0078 gives TX DAPM acquire/release ownership of the VA-owned shared
   DMIC clock.

The production route is DEC0←DMIC1 and DEC1←DMIC0 through `MSM_DMIC`, feeding
`TX_CODEC_DMA_TX_3` / `MultiMedia3 Capture`. UCM publishes it as the two-channel
**Built-in Audio Internal microphone array** at 48 kHz, stereo, S16_LE.

The retained Windows RAW ↔ Linux v18 Seven Nation Army comparison remains the
microphone acoustic acceptance baseline: **98.27% overall parity**.

## Accepted protected output / exact volume path

FullIO v19c restores Golden SP/SPVI + VI/CPS + MSIIR + final VOL_CTRL. During
live playback the production volume synchronizer now uses the exact DSP path
rather than host fallback:

- endpoint DSP mute/unmute succeeds;
- active WSA RX0/RX1 is `84` (0 dB native-Windows producer state);
- idle returns to Golden baseline `81`;
- 10% selected GainStep 1 with a 216-byte delta;
- 35% selected GainStep 9 with a 272-byte delta;
- restore 35%→10% exercised `vol->cal` ordering successfully.

The known graph-calibration `APM_CMD_SET_CFG (0x01001006)`
`AR_EUNSUPPORTED` marker may appear once while opening the protected graph. The
driver explicitly continues because Qualcomm GSL treats that reviewed GET-only
calibration frame as non-fatal; subsequent SP/SPVI/MSIIR/VOL_CTRL and
`GRAPH_START` stages are accepted. Do not misclassify that isolated marker as a
runtime audio fault.

## Duplex and runtime PM acceptance

FullIO v19c passes simultaneous protected playback + MicArray capture:

- `pcm0p` and `pcm2c` both reached `RUNNING` concurrently;
- microphone data remained nonzero on both channels;
- exact Windows volume transaction stayed active during duplex;
- no GRAPH_OPEN/ASoC/XRUN/SoundWire runtime error occurred;
- after close, PCMs returned to `closed`;
- WSA, TX and VA returned to runtime suspend/usage 0;
- `vdd-micb` consumer use returned to 0.

This is a runtime-idle acceptance only, not a system suspend/resume test.

## Production desktop endpoint policy

Normal desktop output selection exposes **SP11 UbiG**. The physical ALSA
`alsa_output.platform-sound.HiFi__Speaker__sink` remains present only as UbiG's
explicit backend and is marked `node.hidden=true`, `priority.session=0` by the
tracked WirePlumber rule, and the `pipewire-pulse` bridge receives no read
permission to that node. Native UbiG/WirePlumber access is unchanged. The transparent `effect_input.sp11_ubig_bypass` is no
longer autoloaded or present in the production graph. Its old config is retained
in the repository solely as an explicit historical/debug utility.

The production installer enforces this policy and the live v19c verifier rejects
a visible raw backend or an active bypass. A real post-install playback smoke
confirmed `SP11 UbiG -> hidden ALSA speaker` links, `pcm0p` RUNNING and no new
runtime audio fault beyond the already accepted one-shot `0x01001006` marker.

## UbiG production control

The production control ABI is `/run/user/1000/ubig-control-v2`. The old
`sp11-ubig` compatibility helper had two stale behaviors: substring node lookup
could select `effect_input.sp11_ubig_bypass`, and it wrote the obsolete two-byte
`sp11-ubig-profile.control` file. Both are fixed.

The helper now uses exact node-name matching and `ubigctl` v2 for profile,
postgain and GEQ requests. Live `Game -> Music -> Voice -> Course -> Dynamic ->
Movie -> Custom` requests all reached desired==active without restarting the
filter process, UbiG stayed the default sink, and the saved non-flat 20-band
Custom curve was preserved.

## Verification

From a clean clone, the fast topology gate is:

```bash
./repro/native-audio-v19c/build-and-verify.sh
```

The heavy kernel/initrd gate reconstructs pristine Linux 7.1.5 -> Golden v33 -> production 0072+0078 modules and requires raw byte identity for all three LPASS modules and the promoted initrd:

```bash
JOBS=12 ./repro/native-audio-v19c/build-kernel-initrd-and-verify.sh
```

Accepted heavy-gate output ends with `FULLIO v19c KERNEL + INITRD EXACT REPRODUCTION PASS`; the reproduced initrd SHA-256 is `ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d`.

The deployed artifact verifier remains:

```bash
./deploy/native-audio-v19c/verify-native-audio-v19c.sh
```

On SP11:

```bash
./deploy/native-audio-v19c/verify-native-audio-v19c.sh --live
```

Golden rollback remains independently verifiable with
`deploy/golden-v33/verify-golden-v33.sh` and `repro/golden-v33/build-and-verify.sh`.

## Maintenance rules

- Keep v18 and Golden v33 installed as rollback entries.
- Keep mic production code at 0072 + 0078 unless new evidence requires a
  parity rerun; 0079-0086 remain diagnostic history.
- Preserve the exact v19c DTB/topology/UCM hashes.
- Keep capture AudioReach graph objects disjoint from all Golden module,
  subgraph and container IDs.
- Keep DMIC power/clock ownership DAPM/runtime-PM driven; do not pin mic power.
- Do not reintroduce the rejected SP_VI reorder on the output side.
- Treat Windows as the behavioral oracle where board-specific measured behavior
  conflicts with generic assumptions.
- Do not add system suspend/resume back to this checklist; it belongs to the
  dedicated suspend/resume RE.

## Canonical pointers

- `README.md`
- `deploy/native-audio-v19c/`
- `repro/native-audio-v19c/`
- `deploy/native-audio-v18/` — first rollback / microphone parity provenance
- `deploy/golden-v33/` — protected-output base / second rollback
- `deploy/ucm2/Qualcomm/x1e80100/SP11-HiFi.conf`
- `deploy/ubig/`
- `docs/checkpoints/2026-08-26-FULLIO-V19C-GOLDEN-MIC-COLLISION-FIX-ACCEPTANCE.md`
- `docs/checkpoints/2026-08-26-FULLIO-V19C-EXACT-KERNEL-INITRD-REPRODUCTION.md`
- `docs/audit/2026-08-26-SP11-NATIVE-AUDIO-FULLIO-V19C-AUDIT.md`
- `docs/audit/2026-08-26-SP11-FULL-AUDIO-CHAIN-AUDIT-CHECKLIST.md`
- `artifacts/2026-08-26-native-mic-v18-parity/`

## Next target

The tested non-suspend built-in speaker + protection + MicArray + UbiG chain is
closed. Exact clean kernel/initrd reproduction for the unchanged v18/v19c 0072+0078 module delta is also closed. Remaining work is production hardening: remove global clock/power ignore flags if safe, close public owner-pack provenance, clean stale firmware/UCM artifacts, improve top-level test/CI hygiene, and then external Bluetooth/USB/DP audio integration/upstreaming.
