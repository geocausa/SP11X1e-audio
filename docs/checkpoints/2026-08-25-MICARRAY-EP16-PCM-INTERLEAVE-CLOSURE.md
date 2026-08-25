# SP11 MicArray EP16 PCM_CNV interleave closure — 2026-08-25

## Result

The remaining PCM layout field for the normal Windows MicArray EP16 graph is
closed from the hash-locked qcadcm binary without borrowing a Qualcomm reference
topology:

```text
IID 0x40de
MID 0x07001003 PCM_CNV
PID 0x08001008 PCM output format
interleaved = 1 = PCM_INTERLEAVED
```

This promotes the Linux EP16 topology candidate from structural-only to
PCM-format-complete.  The candidate is still **not bootable** because the
`SH_MEM_PUSH_MODE` circular-buffer transaction remains open.

## Static proof

The analyzed driver is the handoff-locked binary:

```text
qcadcm8380.sys SHA-256
37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429
```

`FUN_14006f5c8` is the `SetStreamDataFormat` routine already identified by its
runtime/log strings.  It reads GraphType from graph offset `+0x30`; its caller
independently labels the same graph field as `GraphType`.

For both the decoder/encoder and PCM-converter tag paths it calls
`FUN_14006b410` (`CreatePcmOutputFormatPayload`) with a boolean argument:

```text
(graph_type == 1)
```

Inside `FUN_14006b410` the PCM output `interleaved` field is authored exactly as:

```text
1 by default
3 only when graph_type == 1
```

Thus there are only two possible outputs at this builder boundary:

```text
GraphType 1 -> interleaved 3 = PCM_DEINTERLEAVED_UNPACKED
otherwise   -> interleaved 1 = PCM_INTERLEAVED
```

This independently explains the already observed protected-render PCM_CNV
value 3.

## Capture-family proof

qcadcm `FUN_140098d98` classifies graph types into the two key families consumed
by `GetRenderCaptureGkv`:

```text
family 1: GraphType 1, 7
family 2: GraphType 2, 3, 4, 5, 8
```

The live EP16 MicArray GKV was already captured with the capture-family key set
(`0x01000008` through capture endpoint key `0x0100000d`).  Therefore its graph
is in family 2 and cannot be GraphType 1.

Consequently `SetStreamDataFormat` calls `CreatePcmOutputFormatPayload` with
false for `(GraphType == 1)`, and the exact Windows PCM_CNV output is
`interleaved=1`.

## Linux candidate

`tools/build_sp11_mic_ep16_topology.py` now emits token:

```text
token252 1
```

for IID `0x40de` unconditionally from this evidence closure.

The rebuilt topology validated with:

```text
EP16_PCM_CLOSED_TOPOLOGY_ASSERT=PASS
40de token252=1
modules=15
DSP edges=14
DAPM edges=15 (14 DSP + external TX backend route)
```

The remaining runtime blocker is now only the exact Windows EP16
`SH_MEM_PUSH_MODE` ring/position configuration and lifecycle.

## Safety

- Static/read-only Windows binary analysis on SP11 Linux.
- Ghidra analysis performed on a copied project.
- No Windows boot or KD mutation.
- No Linux module load/unload.
- No running topology/DT mutation.
- Golden v33 untouched.
