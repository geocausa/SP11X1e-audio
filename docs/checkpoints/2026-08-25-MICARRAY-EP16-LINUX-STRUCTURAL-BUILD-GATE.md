# SP11 MicArray EP16 Linux structural build gate — 2026-08-25

Branch: `agent/microphone-re-20260824` (SP11 local worktree branch pushes to this remote branch).
Golden baseline: `release/golden-v33` and the running Golden filesystem remain untouched.

## Result

The Windows-normal MicArray EP16 graph can now be encoded losslessly in the
Linux AudioReach topology format at the structural layer.  The candidate is
**not yet a boot candidate**: two runtime values remain intentionally
unasserted rather than borrowed from another Qualcomm machine.

The exact Windows path represented is:

```text
TX DMIC1 -> TX DEC0
TX DMIC0 -> TX DEC1
          -> TX_AIF1 / TX_CODEC_DMA_TX_3
          -> 0x40c8 CODEC_DMA_SOURCE
          -> SG41
          -> SG40
          -> SG44
          -> 0x40dc SH_MEM_PUSH_MODE
          -> host
```

The generated structural topology contains all 15 Windows runtime modules and
all 14 Windows DSP data edges.  The compiled DAPM graph contains one additional
external backend route from `TX_CODEC_DMA_TX_3 Capture` into IID `0x40c8`.

Automated comparison against the reconstructed 1,496-byte Windows GraphOpen
oracle passed:

```text
EP16_STRUCTURAL_ASSERT=PASS
modules=15
dsp_edges=14
dapm_edges=15
backend=TX_CODEC_DMA_TX_3
pcm_cnv_interleave=UNRESOLVED/OMITTED
```

The Windows runtime GraphOpen oracle remains hash-locked at:

```text
f5c112b8b45aea730e06650995574ff2f88c747685a080a2118a3a1aadd2d4bb
```

## Exact Linux backend mapping

IID `0x40c8` is encoded as Windows requires:

```text
MID             0x07001024 CODEC_DMA_SOURCE
LPAIF type      1 = LPAIF_RXTX
interface index 4
fixed-point     1
integrated BE   120 = TX_CODEC_DMA_TX_3
```

This is the normal EP16 TX-macro path.  `VA_CODEC_DMA_TX_0` remains the EP2
sibling/alternate path and is not substituted underneath EP16.

## Runtime GraphOpen metadata retained exactly

All three subgraphs encode Windows runtime values `perf=2`, `direction=1`,
`scenario=2`.

```text
SG44 0xb0000044 / container 0xe000004a:
  capability 0x0b001001, stack 0x1000, graph-pos 0xffffffff,
  proc-domain 2, parent 0xffffffff, heap 1

SG40 0xb0000040 / container 0xe000003e:
  capability 0x0b001001, stack 0x1000, graph-pos 4,
  proc-domain 2, parent 0xffffffff, heap 1

SG41 0xb0000041 / container 0xe0000040:
  capability 0x0b001001, stack 0x1000, graph-pos 4,
  proc-domain 2, parent 0xffffffff, heap 1
```

The two runtime SCLU bridges are also encoded exactly:

```text
0x40c7:7 -> 0x40bf:2
0x40c4:1 -> 0x40db:2
```

## Kernel boundary 1: SH_MEM_PUSH_MODE topology recognition

Windows EP16 terminates at:

```text
MID 0x07001007 / IID 0x40dc = SH_MEM_PUSH_MODE
```

The Golden q6apm topology loader recognized WR/RD shared-memory endpoints and
`SH_MEM_PULL_MODE`, but rejected `SH_MEM_PUSH_MODE` as an AIF/buffer widget.
Patch `0074-ASoC-audioreach-accept-SH-MEM-PUSH-MODE-topology.patch` adds the
authoritative module ID and permits it through the same topology-loader class.
It deliberately adds **no runtime ring configuration**.

## Kernel boundary 2: TX stereo decimator lifecycle

The generic TX macro driver keeps both an active-decimator bitmap and a legacy
single `active_decimator` index.  With Windows's stereo path (DEC0 + DEC1), the
single index caused DAI mute to touch only one lane and could become `-1` while
a second lane remained active.

Patch `0075-ASoC-lpass-tx-macro-fix-multichannel-capture-state.patch` makes the
bitmap authoritative for mixer enable/disable and applies DAI mute/unmute to
every active decimator.  This matches the observed Windows lifecycle, which
explicitly toggles both TX0 (`0x400`) and TX1 (`0x480`) PGA-mute bits.

## Golden-v33 build gate

Both kernel patches were applied temporarily to the exact retained Golden-v33
replay kitchen and built successfully.  Candidate modules have vermagic:

```text
7.1.5-sp11-render-parity-v4+ SMP preempt mod_unload modversions aarch64
```

After copying candidates out, both affected source trees and output trees were
restored and independently hashed:

```text
src-qdsp6 PASS
src-codecs PASS
out-qdsp6 PASS
out-codecs PASS
```

No candidate module was loaded into the running system.

## Topology tooling correction

`tools/ar_topology_inventory.py` was corrected to preserve negative values in
`alsatplg` decoded word tuples (`-1` is the decoded form of `0xffffffff`) and
to parse normalized `SectionGraph.setN { ... }` blocks.  This allows exact
verification of Windows graph-position/parent values and compiled DAPM edges.
Unit tests cover both cases.

## Deliberate evidence gates

Two values remain open and are intentionally absent from the structural
candidate:

1. **PCM_CNV IID `0x40de`, PID `0x08001008`:** the tag mapping is closed
   (`0x04000005 -> 0x40de`), but the EP16 runtime payload/interleave value has
   not yet been observed.  Do not borrow Romulus's capture token.
2. **SH_MEM_PUSH_MODE IID `0x40dc`, PID `0x0800100a`:** Linux has a useful
   shared-ring implementation for render pull mode, but the exact Windows EP16
   capture ring size, period, position-buffer setup and event ordering have not
   yet been captured.  Do not substitute `RD_SHARED_MEM_EP` or invent these
   constants.

Therefore the saved topology is a structural/lint oracle only and is marked
`bootable=false`.

## Evidence

`artifacts/microphone-re-20260824/linux-candidate/2026-08-25-ep16-structural-linux-gate/`
contains the generated topology source, decoded inventory, structural assertion,
hashes and text build evidence.  Candidate `.ko` binaries remain outside Git.

## Safety

- No reboot.
- No live module unload/load.
- No running DT/topology mutation.
- No ACDB or DSP write.
- Golden branch untouched.
- Golden replay kitchen restored byte-for-byte in all affected subtrees.
