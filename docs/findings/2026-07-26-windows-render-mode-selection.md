# Windows speaker graph selection — DEFAULT versus NOTIFICATION

## Finding

The two recovered SP11 Windows speaker render families are alternatives selected
by the Windows signal-processing mode:

| Family | Subgraphs | Windows mode |
|---|---|---|
| A | `0xb0000001 + 0xb000007e + 0xb000007f` | `DEFAULT` |
| B | `0xb0000001 + 0xb0000082 + 0xb0000083` | `NOTIFICATION` |

They are not the physical left and right speaker halves. Both use the same
render endpoint graph-key value. Their only selector difference is the
processing-mode value applied to both the stream and mix keys.

The reviewed machine-readable evidence is
`artifacts/reviewed/windows-render-mode-gkv-mapping.json`.

## Bound sources

| Source | SHA-256 |
|---|---|
| `qcadcm8380.sys` | `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429` |
| `qcaudminiport8380.sys` | `79b26804d05332304c736c4e03e942db6a07ea886a2b07f3a4ff5947d1d05531` |
| `surface_audiominiext8380.inf` | `5acd5091f45da4232945046eeedc913bff75c57adc6e17954391264d7cec8134` |
| reviewed static GKV inventory | `eaaee9502eb355755406b9ed1b7b347e7446589d9e43d59069628a8c78c18d9a` |

## The six-key render selector

The recovered `qcadcm8380.sys` function at `0x140093c60` identifies itself in
its trace strings as `GetRenderCaptureGkv`. It constructs these six keys in
this exact order:

| Key | Meaning |
|---|---|
| `0x01000001` | render stream streaming type |
| `0x01000002` | render stream processing mode |
| `0x01000003` | render stream instance |
| `0x01000004` | render mix streaming type |
| `0x01000005` | render mix processing mode |
| `0x01000006` | render endpoint |

`AudioDspGraphOpen` at `0x140085270` calls this builder for the normal render
graph types. The processing enum is applied twice: once to the stream key and
once to the mix key.

The two exact static ACDB rows are:

| Key | Family A | Family B |
|---|---:|---:|
| stream type `01000001` | `2` | `2` |
| stream processing `01000002` | `2` | `7` |
| stream instance `01000003` | `1` | `1` |
| mix type `01000004` | `2` | `2` |
| mix processing `01000005` | `2` | `7` |
| endpoint `01000006` | `1` | `1` |

Family A is at POOL offset `0x0003d164`; family B is at `0x00042668`.
Consequently, endpoint identity cannot explain the two families: both select
endpoint value `1`. Only processing mode differs.

## Exact Windows mode translation

The miniport function at `0x1400448d0` recognizes the standard Windows
processing-mode GUIDs. The compact translator at `0x140094080` converts its
mode flag into the processing enum passed to QCADCM. Finally,
`qcadcm8380.sys:0x1400938e0`, whose trace string names `GetProcKeyvalue`,
converts that enum to the ACDB graph-key value.

The complete recovered mapping is:

| Windows mode | Miniport flag | QCADCM enum | GKV value |
|---|---:|---:|---:|
| DEFAULT | `1` | `2` | `2` |
| RAW | `2` | `1` | `1` |
| COMMUNICATIONS | `4` | `4` | `6` |
| SPEECH | `8` | `3` | `5` |
| NOTIFICATION | `10` | `7` | `7` |
| MEDIA | `20` | `6` | `4` |
| MOVIE | `40` | `5` | `3` |

The Surface extension INF independently defines the NOTIFICATION GUID as
`{9CF2A70B-F377-403B-BD6B-360863E0355C}` and advertises NOTIFICATION among the
speaker SFX/MFX processing modes.

This gives two complete, independent chains:

```text
DEFAULT GUID -> flag 1 -> QCADCM enum 2 -> GKV 2 -> family A
NOTIFICATION GUID -> flag 10 -> QCADCM enum 7 -> GKV 7 -> family B
```

## Consequences for Linux parity

1. The normal speaker baseline is family A, the DEFAULT-mode graph.
2. Family B is a separate NOTIFICATION-mode alternative. It must not be joined
   to family A as if it drove the opposite physical side.
3. The absence of simultaneous A/B lifetimes is expected and is no longer an
   unresolved parity requirement.
4. Each family is internally a complete 16-module speaker path connected to
   the shared root/protection graph. The actual multi-speaker hardware routing
   must be resolved inside those graphs and their external peers, not by
   assigning one family to each side.
5. A Linux baseline may initially implement DEFAULT mode only, but its selector
   model should retain a clean place for the NOTIFICATION alternative.
