# SP11 Windows microphone endpoint-2 graph reconstruction

Date: 2026-08-24
Evidence: canonical REV_0D ACDB + qcadcm SCLU/SCDE/SCDO semantics

## Physical root

`IID 0x4158 CODEC_DMA_SOURCE -> 0x402f DATA_LOGGING -> 0x4030 SPLITTER`

`0x4158`: 48 kHz / 16-bit / 2ch; codec DMA `{lpaif_type=3, interface_index=1, channel_mask=3}`.

## Proven capture-mode split

| Mode | GKV POOL | Splitter edge | Downstream SG chain | Terminal |
|---|---:|---|---|---|
| DEFAULT | `0x00000a98` | `4030:out7 -> 0x00004751:in2` | `0xb0000006 -> 0xb000008e -> 0xb000008f` | `0x00004a42 SH_MEM_PUSH_MODE` |
| SPEECH | `0x0000127c` | `4030:out5 -> 0x00004739:in2` | `0xb0000006 -> 0xb000008c -> 0xb000008d` | `0x00004a41 SH_MEM_PUSH_MODE` |
| COMMUNICATIONS | `0x00001b10` | `4030:out3 -> 0x00004721:in2` | `0xb0000006 -> 0xb000008a -> 0xb000008b` | `0x00004a3f SH_MEM_PUSH_MODE` |
| RAW | `0x000023d4` | `4030:out1 -> 0x00004803:in2` | `0xb0000006 -> 0xb000000a -> 0xb000000b` | `0x00004a3d SH_MEM_PUSH_MODE` |

The exact port map is therefore: RAW=1, COMMUNICATIONS=3, SPEECH=5, DEFAULT=7.

## Playback-reference inputs

SPEECH and COMMUNICATIONS are not source-only chains. Static ACDB contains speaker-root-splitter reference inputs:
- SPEECH: `4002:out5 -> 0x00004747:in2`
- COMMUNICATIONS: `4002:out11 -> 0x00004730:in2`

These are exact static graph connections; their algorithmic role is consistent with reference/echo-processing but is not renamed beyond evidence.

## Remaining runtime evidence gate

- Observe which GKV family Windows starts for ordinary DEFAULT microphone capture and for RAW capture.
- Capture runtime SH_MEM_PUSH_MODE / frontend configuration and graph start/stop ordering.
- Preserve all Windows IIDs/SGs while implementing on `agent/microphone-re-20260824`; golden v33 remains untouched.
