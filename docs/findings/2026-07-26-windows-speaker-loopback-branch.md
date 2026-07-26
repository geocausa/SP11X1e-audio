# Windows speaker secondary branch — loopback, not hardware output

## Finding

The second output of the `SPR` module in each recovered Windows speaker render
family feeds the Windows speaker loopback graph:

```text
DEFAULT family A:      SPR 412b:3 -> loopback SAL 4144:16
NOTIFICATION family B: SPR 4137:3 -> loopback SAL 4144:24
```

It is not an opposite-side speaker path and it does not lead to an amplifier.
The main hardware-render route continues from `SPR` output 1, through the
family's downstream processing, into the shared root, and ultimately to
`CODEC_DMA_SINK 4157`.

## Bound evidence

| Source | SHA-256 |
|---|---|
| `qcaudminiport8380.sys` | `79b26804d05332304c736c4e03e942db6a07ea886a2b07f3a4ff5947d1d05531` |
| `qcadcm8380.sys` | `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429` |
| `surface_audiominiext8380.inf` | `5acd5091f45da4232945046eeedc913bff75c57adc6e17954391264d7cec8134` |
| static GKV inventory | `eaaee9502eb355755406b9ed1b7b347e7446589d9e43d59069628a8c78c18d9a` |
| closure for POOL `0x46c78` | `d2d835f094624dde4952886d297fc3b7aef10192867fac61316dbcd01668fced` |

## INF and driver identity

The Surface extension binds the fourth `WaveSpeaker` format group explicitly:

```text
QCAUD\WaveSpeaker\FormatsAndModes3\type = "loopback"
```

The miniport function at `0x140089fc8` compares the configured type string and
maps `loopback` to streaming-type enum `3`. QCADCM function `0x1400937a0`
preserves streaming-type enum `3` as graph-key value `3`.

The static ACDB row at POOL offset `0x00046c78` selects:

```text
01000006=1  render endpoint = internal speaker
01000008=3  capture stream type = loopback
01000009=1  capture stream processing
0100000a=1  capture stream instance
0100000b=3  capture mix type = loopback
0100000c=1  capture mix processing
```

This row opens subgraphs `0xb0000045` and `0xb0000046`.

## Exact loopback path

Combining the POOL graph with its SCLU bridge gives:

```text
render-family SPR output 3
 -> SAL 4144
 -> DATA_LOGGING 402d
 -> RATE_ADAPTED_TIMER 40df
 -> VOL_CTRL 40e0
 -> MFC 40e1
 -> SOFT_PAUSE 40e2
 -> SWR_SINK 40e8
 -> VOL_CTRL 40e6
 -> PCM_CNV 40e3
 -> DATA_LOGGING 40e4
 -> SH_MEM_PUSH_MODE 40e5
```

The recovered AudioReach header identifies module `0x07001007` at IID `40e5`
as `MODULE_ID_SH_MEM_PUSH_MODE`. That terminal endpoint returns samples to the
host. It cannot be a speaker-amplifier endpoint.

The shared root's exact timer-drift control link also terminates on this graph:

```text
CODEC_DMA_SINK 4157 <-> RATE_ADAPTED_TIMER 40df
INTENT_ID_TIMER_DRIFT_INFO (080010c2)
```

That clock relationship is appropriate for a loopback capture of the rendered
speaker stream and independently corroborates the graph role.

## Implementation consequence

The first safe Linux playback baseline does not need the loopback graph to
drive the speakers. It does need:

- `SPR` declared with both real output ports so the topology remains capable of
  representing the Windows design;
- output 1 connected through the selected render family into the shared root;
- the root render chain ending at `CODEC_DMA_SINK 4157`;
- output 3 either connected to a separately implemented loopback graph or kept
  disabled without being misrouted to hardware.

Physical multi-speaker routing must now be recovered from the codec-DMA,
SoundWire, WSA macro, and amplifier endpoint configuration. Family A/B and the
SPR loopback tap are no longer candidates for left/right assignment.
