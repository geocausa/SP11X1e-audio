# Windows WSA start descriptor + macro-state closure — 2026-08-20

Branch: `agent/psycho-bass-20260818`

This checkpoint supersedes the open TX-macro lead at the end of `2026-08-20-STM-HWD4-RUNTIME-PROOF-AND-TX-MACRO-GAP.md`.

## Native-Windows runtime descriptor proof

A one-shot native-Windows boot was performed with persistent GRUB still pointing to Golden v31.  SP7 KDNET was attached before speaker playback and a breakpoint was placed at the already-recovered qcaucd WSA start-owner boundary.

The descriptor passed to `WSA_START_OWNER` is a type-`0x14` object.  Its resource count is byte `+0x88`; resource pointers begin at `+0x10,+0x20,...`; the first dword of each referenced resource is its type.

Repeated real speaker starts all produced the same descriptor:

```text
mode=2
count=2
resource[0] type=7
resource[1] type=8
```

Representative descriptor bytes:

```text
...+0x00  14 00 00 00 00 00 00 00 14 00 00 00 00 00 00 00
...+0x10  <ptr resource type 7> 14 00 00 00 00 00 00 00
...+0x20  <ptr resource type 8> 00 00 00 00 00 00 00 00
...
...+0x88  02 00 00 00 00 00 00 00
```

Therefore the generic qcaucd branches previously decoded for resource types 9/10 (TX0/TX1 macro writes at `0x06aa0400/0x06aa0480`) are **not used by the Surface Pro 11 speaker start path**.  Do not build a Linux candidate around those branches.

## WSA RX macro runtime parity

During active native-Windows speaker playback KD read:

```text
0x06b00400: 00000024 00000002 000000ef 0000008f
0x06b00480: 00000024 00000002 000000ef 0000008f
```

These are the same active WSA RX0/RX1 macro values previously measured on Golden Linux.  The WSA RX macro producer-gating hypothesis is therefore closed.

## Port-13 revalidation

The same live Windows session revalidated the known CPS master-port state, including:

```text
0x06b11d24 = 0x0000001f
0x06b11d2c = 0x00000018
0x06b11d34 = 0x000000ff
0x06b11d3c = 0x00000003
0x06b11d54 = 0x00000003
0x06b11d64 = 0x0300001f   (active sample)
```

`0x06b11d54=3` and controller-global `0x105c=0x0005000f` remain genuine Windows/Linux differences, but the already-tested combined Linux candidate produced 273 correctly formatted tap3 frames with **0/273 nonzero** payloads.  They remain closed as sufficient fixes.

## ADSP-owned LPAIF read boundary

KD physical reads of the ADSP-owned WSA LPAIF windows failed exactly as APSS `/dev/mem` did:

```text
Physical memory read at 6b9a000 failed
Physical memory read at 6b89000 failed
```

Thus neither Windows APSS KD nor Linux APSS `/dev/mem` can directly inspect the HWD4 WSA WRDMA/IRQ windows.  Do not repeat this route.

## DSP query surface

Static qcadsp review reconfirmed that CODEC_DMA `get_param` accepts only `FWK_EXTN_PARAM_ID_LATEST_TRIGGER_TIMESTAMP_PTR (0x0A001050)`.  There is no second public DMA-position/interrupt-status GET_CFG selector to reuse from HLOS.

## Updated root-cause box

Closed/accounted:

- GRAPH_OPEN/GRAPH_START structure and source SET_CFG;
- STM control and CODEC_DMA_SOURCE start;
- HWD4/LPAIF_WSA object open;
- WSA8845 DP5/VISENSE and DP6/CPS transport geometry;
- later Windows WSA8845 codec-init parity lineage;
- DP14 shadow;
- `0x105c + 0x1d54` as sufficient source-data fix;
- WSA RX macro active state;
- qcaucd resource types 9/10 / TX0/TX1 branches for this speaker path;
- qcadcm/qcasd post-RUN side effect;
- direct APSS reads of WSA LPAIF IRQ/WRDMA windows.

Remaining boundary:

```text
WSA8845 CPS/VISENSE producer
 -> SoundWire feedback dataports / WSA master
 -> [remaining WSA-master -> LPAIF WSA WRDMA handoff/data issue]
 -> HWD4 CODEC_DMA_SOURCE (opens/runs)
 -> zero CPS/VI samples
```

The next discriminator is to formalize whether the 273 tap3 frames prove real HWD4 signal-trigger/WRDMA period activity.  AudioReach generic-container source must be used to exclude alternate data/command triggers before promoting that conclusion.

## Safety closeout

KD was cleared/detached cleanly with `qd`; SP7 had no `kd.exe` left.  The SP11 was rebooted normally and returned to protected Golden v31:

- kernel `7.1.5-sp11-render-parity-v4+`;
- cmdline `sp11_entry=7.1.5-sp11-golden-v31-ckv-delta`;
- loaded `snd_q6apm` srcversion `687B16CF9C43B43E90C0746`;
- topology SHA256 `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`;
- `saved_entry=sp11-audio-golden-v31`;
- `next_entry=` empty;
- `pislave.service` enabled.
