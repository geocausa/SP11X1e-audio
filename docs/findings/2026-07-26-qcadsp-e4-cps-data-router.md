# Windows DSP module `0x070010e4` — CPS Data Router

## Finding

The module instantiated by Windows as module ID `0x070010e4`, IID `0x4028`,
is the Speaker Protection v5 CPS Data Router.

This identification is based on the module implementation in the recovered
SP11 Windows ADSP firmware, not on the module's position in the graph alone.

## Bound source

| Item | Value |
|---|---|
| File | `qcadsp8380.mbn` |
| SHA-256 | `921870a839ee2aba647b04598d62ed96f3d2d5dfbb2499fc842f9a6ff0e0da13` |
| Format | ELF32 little-endian Qualcomm DSP6, 58 program headers |
| Recovered path | `00-RE-archive/recovered-adata/ubi/Documents/SP11/repo/firmware/qcom/x1e80100/microsoft/denali/qcadsp8380.mbn` |

## Firmware registration record

The exact little-endian module ID occurs at file offset `0x12c6014`, inside a
16-byte static module registration record:

```text
file 0x12c6014 / VA 0xb0611014

e4100007 e0d73eb0 20d83eb0 0a000000
```

Interpreted as little-endian words:

```text
module_id              = 0x070010e4
get_static_properties  = 0xb03ed7e0
init                   = 0xb03ed820
registration class     = 0x0000000a
```

The neighboring records register the known SP_VI and SPv5 modules:

```text
0x070010e3  0xb03e9748  0xb03e9788  0x0000000a
0x070010e2  0xb03e4e3c  0xb03e4e7c  0x00000004
```

The `0x07001180` byte sequences elsewhere in this firmware are not equivalent
registration records: they occur in high-entropy payload regions. They must
not be used to claim that the v5 and v7 IDs are aliases.

## Behavioral identity

The init function installs the CAPI vtable at `0xb0782a88`. Its set-parameter
handler is `0xb03edd2c`. Direct Hexagon disassembly shows that handler compares
the incoming parameter ID against:

| ID | Recovered Qualcomm API identity |
|---|---|
| `0x0a001018` | `INTF_EXTN_PARAM_ID_IMCL_PORT_OPERATION` |
| `0x080013cb` | `PARAM_ID_CPS_CHANNEL_MAP_V5` |
| `0x08001026` | `PARAM_ID_MODULE_ENABLE` |

The same implementation supplies `0x08001537` while opening its IMCL control
port. The recovered API names this `INTENT_ID_CPS`.

The recovered Qualcomm header
`audioreach-graphservices/spf/api/modules/cps_data_router.h` describes
"Speaker Protection v5 CPS Data Router", declares the v5 channel-map parameter
`0x080013cb`, and specifies:

- one input data port;
- zero output data ports;
- a dynamic control port carrying `INTENT_ID_CPS`;
- module-enable support.

## Independent live-graph corroboration

The reviewed Windows `GRAPH_OPEN` body instantiates `0x070010e4` as IID
`0x4028` with one input and zero outputs. It receives data from MUX_DEMUX IID
`0x4029` and has this exact control relationship:

```text
4028:80000000 <-> 4027:80000001  INTENT_ID_CPS (08001537)
```

Firmware behavior, recovered API semantics, and live graph structure therefore
agree. The old labels `unknown`, `adjunct`, and `proprietary E4` are retired.

## Implementation consequence

Linux parity requires a CPS Data Router between the auxiliary MUX_DEMUX path
and the SPv5 module's CPS control port. It is not an optional equalizer or a
Dolby component. The Windows firmware uses the legacy v5 module ID
`0x070010e4`; the currently recovered public header exposes the newer v7 ID
`0x07001180`. That version distinction must be preserved when selecting what
the installed DSP firmware actually supports.
