# MicArray row5 hardware-interface discovery closure

## Finding

The pre-open EP16 **row5** GKV is not a second graph and is not graph staging. It is the graph-key context qcadcm uses while discovering the endpoint hardware-interface category before opening the normal row20 MicArray graph.

The row5 vector is:

```text
0x01000008 = 2
0x01000009 = 2
0x0100000a = 1
0x0100000b = 2
0x0100000c = 2
0x0100000d = 0x10
```

## Exact qcadcm call chain

Fresh KD stack logging during an ordinary MicArray capture closed the chain:

```text
AcdbCmdGetGraphTagKeyVectors / 0xacdb0017
  qcadcm +0x5eee8
  -> GetGraphTagIdKvs +0x94d60
  -> GetHwIfType +0x95378
  -> GetEpHwIfCfg +0x98e28
```

qcadcm's own embedded strings name all three helpers. `GetHwIfType` scans returned tag KVs for graph key:

```text
0x01000012
```

That key is qcadcm's endpoint hardware-interface selector/category. It is not the physical `lpaif_type` field inside `PARAM_ID_CODEC_DMA_INTF_CFG`.

## Five row5 ACDB calls explained

The fresh row5-only stack log produced five hits, all through the same `GetGraphTagIdKvs` wrapper:

```text
hit 0  tag 0x04010003  size pass   return +0x98ecc
hit 1  tag 0x04010003  fill pass   return +0x98ecc
hit 2  tag 0x04010004  size pass   return +0x98ee0
hit 3  tag 0x04010004  fill pass   return +0x98ee0
hit 4  tag 0x04010005  size probe  return +0x98f3c
```

Static disassembly of `GetEpHwIfCfg` proves those return sites correspond to this sequence:

```text
try 0x04010003
if needed, try 0x04010004
then probe 0x04010005 for EpFxHwIfType
```

`GetGraphTagIdKvs` itself performs a size query followed by allocation/fill when the tag returns data, explaining the paired hits for tags 03 and 04. Tag 05 does not proceed to a fill pass on this MicArray path.

## Live EP16 result

A second non-blocking return logger at `qcadcm+0x99034` captured the final `GetEpHwIfCfg` result twice during an ordinary MicArray open:

```text
status=0
endpoint=0x10
EpHwIfType=2
EpFxHwIfType=0
```

The repeated result is therefore:

```text
EP16 -> endpoint-HW category 2 -> tag 0x04010004
```

This matches the independently decoded current graph:

```text
SG41 tag 0x04010004
  -> CODEC_DMA_SOURCE MID 0x07001024 / IID 0x40c8
```

## Important layer separation

Do **not** interpret `EpHwIfType=2` as physical LPAIF type 2. qcadcm first discovers category 2 with graph key `0x01000012`, which selects tag `0x04010004`. The tag-specific ACDB hardware payload is a lower layer and independently describes the actual CODEC_DMA interface.

The already decoded capture CODEC_DMA table demonstrates this separation directly: its selection rows carry `0x01000012 = 2` while `PARAM_ID_CODEC_DMA_INTF_CFG` carries physical `lpaif_type = 3`, interface index 1.

## Consequence for row5 / row20 lifecycle

The normal Windows MicArray sequence is now:

```text
row5 GKV
  -> qcadcm endpoint-HW discovery
  -> category 2 / tag 0x04010004
  -> SG41 CODEC_DMA_SOURCE hardware endpoint resolved

row20 GKV
  -> actual GraphOpen
  -> SG41 + SG40 + SG44
  -> physical capture -> common processing -> host push
```

So row5 is a preparatory **hardware-interface lookup context**; row20 is the topology actually opened for ordinary host capture.

## Evidence

Raw caller KD log:

`artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-row5-hwif-caller-kd.log`

SHA-256:

`195f7264b9b66329c117cd676570dafe8ad49e5d09fd9eaea8dc0c7dbbd58bcd`

Normalized oracle:

`artifacts/microphone-re-20260824/windows-oracle/runtime/2026-08-25-micarray-row5-hwif-discovery.json`

## State / safety

The probes were non-blocking loggers only. No reboot, PnP restart, service restart, registry mutation, or audio-setting mutation was performed. Temporary breakpoints were cleared and SP11 was resumed.

## Next target

The graph-selection layer is now sufficiently closed. Return to the lower qcaucd TX/DMIC programming layer and recover the exact `0x3000-0x30ff` register sequence emitted during one ordinary MicArray capture, starting from `qcaucd FUN_140020348 / RVA 0x20348` after re-confirming its ABI from the saved static decompilation.
