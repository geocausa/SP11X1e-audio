# SP11 Native Audio FullIO v19c

FullIO v19c is the accepted built-in SP11 audio topology that combines the
Golden-v33 protected speaker render graph with the proven native two-channel
MicArray capture path.

System suspend/resume is deliberately outside this release gate. It belongs to
the separate suspend/resume reverse-engineering effort. Runtime PM/autosuspend
of the audio devices is part of this release and is accepted.

## What changed from Native Audio v18

The v18 kernel, initrd, microphone driver patches, UCM and desktop policy remain
unchanged. The functional change is the AudioReach topology:

- restore the exact Golden protected speaker graph, including SP/SPVI, VI/CPS,
  MSIIR and final VOL_CTRL;
- retain the accepted `MultiMedia3 -> TX_CODEC_DMA_TX_3` capture modules;
- move capture graph objects out of the diagnostic low-ID namespace that
  collided with Golden module IID `0x4003` when both graphs were resident.

FullIO v19c capture graph-object IDs are:

- FE subgraph `0xb0000203`, container `0xe0000203`;
- backend subgraph `0xb0000209`, container `0xe0000209`.

The capture module IIDs remain `0x6020..0x6024` and `0x6090..0x6091`.

## Accepted identities

- kernel SHA-256: `bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a`
- initrd SHA-256: `ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d`
- DTB SHA-256: `2fcfa738c229b32764ff2722847cf4056b3153c64a12f8490429309f29df6d00`
- topology SHA-256: `e7bb06a03e7bd9b869825a51775355a6743477d1579d78eb09fad5881cfb20f0`
- topology source SHA-256: `241f32cd2278c7df745f17a6c70f3259109b68ffeea1d4984353de4afd99bc39`
- UCM SHA-256: `9d36df8570b85f1dcecc385a8f85fa2d1e1058ef8efedee6ae2ce49dc259a06a`

Kernel/microphone production delta remains patches **0072 + 0078 only**. Later
0079-0086 work is diagnostic history and is not present in this boot image.

## Reproduction

From a clean repository clone with `alsatplg` installed:

```bash
./repro/native-audio-v19c/build-and-verify.sh
```

That compiles the tracked FullIO source and requires byte identity with the
accepted topology. The source-side merger is
`tools/build_sp11_native_audio_topology.py`; it additionally refuses a wrong
Golden base and rejects cross-class AudioReach object-ID collisions.

For artifact-only checks:

```bash
./deploy/native-audio-v19c/verify-native-audio-v19c.sh
```

On the deployed SP11:

```bash
./deploy/native-audio-v19c/verify-native-audio-v19c.sh --live
```

## Rollback

Keep both of these installed:

- Native Audio v18: `sp11-audio-dmic-broker-div4-v18`;
- Golden v33: `sp11-audio-golden-v33-topcfg1-physical-vi`.

See
`docs/checkpoints/2026-08-26-FULLIO-V19C-GOLDEN-MIC-COLLISION-FIX-ACCEPTANCE.md`
for runtime evidence and
`docs/audit/2026-08-26-SP11-NATIVE-AUDIO-FULLIO-V19C-AUDIT.md` for the current
non-suspend built-in-audio audit.
