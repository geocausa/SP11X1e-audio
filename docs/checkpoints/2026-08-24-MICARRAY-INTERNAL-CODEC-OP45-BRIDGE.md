# SP11 MicArray1 internal-codec op4/op5 bridge — 2026-08-24

Branch: `agent/microphone-re-20260824`
Golden baseline: `release/golden-v33` remains untouched.
Evidence source: Surface qcasd INF + qcasd/qcaucd static RE. No runtime mutation.

## Surface MicArray1 policy

`surface_asdext8380.inf` explicitly configures:

```text
QCASD\MicArray1\InstallEndpoint = 1
QCASD\MicArray1\UseInternalCodec = 1
QCASD\MicArray1\NumEndpointFormats = 1
```

Endpoint format is 48 kHz / 16-bit / 2ch / mask 0x3.

MicArray1 exposes two capture pins:

- Pin0 `hostCapture`
  - DEFAULT, RAW, COMMUNICATIONS, SPEECH
  - all 48 kHz / 16-bit / 2ch / mask 0x3
- Pin1 `BufferedCapture`
  - SPEECH
  - 16 kHz / 16-bit / 2ch / mask 0x3

The base qcasd INF maps `KSNAME_MicArray1 = "MicrophoneArray0"` and describes it as the internal/front microphone-array capture endpoint.

## `UseInternalCodec` changes the MicArray object class

qcasd `FUN_140043e30` is the MicArray subobject factory.

- `UseInternalCodec == 0`: allocate ordinary `0xF0` object and keep the base class.
- `UseInternalCodec == 1`: allocate `0xF8` object and install vtable `PTR_FUN_140011a18`.

Therefore the Surface configuration deliberately selects an internal-codec subclass for MicArray1.

## Internal-codec subclass vtable

Relevant slots in `PTR_FUN_140011a18`:

```text
+0x00 FUN_140006330  destructor
+0x08 FUN_14002dca0  subclass init
+0x50 FUN_14002eab0  endpoint/format init
+0x90 FUN_14002de30  physical-codec START bridge
+0x98 FUN_14002e010  physical-codec STOP bridge
```

### Subclass init — `FUN_14002dca0`

The function stores:

```text
object+0x48 = qcasd/device context
object+0x24 = endpoint category
```

It then allocates an internal-codec helper at `object+0xF0` through `FUN_14002d6a0` and initializes it with `FUN_14002d790`, which in turn acquires the qcaucd private-interface singleton (`FUN_140031c00` / `FUN_140031db8`).

### Start — `FUN_14002de30`

The start override:

1. reads the visible endpoint category from `object+0x24`;
2. maps it through `FUN_14002d1c8(category, flag, 0)` to the qcadcm endpoint type;
3. calls `FUN_14002d328(...)` to construct the physical-codec request structure;
4. calls `FUN_1400324a8(internalCodec, 4)`.

`FUN_1400324a8` dispatches operation index 4 through the private qcaucd interface.

### Stop — `FUN_14002e010`

The stop override follows the same request-building path and calls:

```text
FUN_1400324a8(internalCodec, 5)
```

which dispatches qcaucd private operation 5.

## qcaucd side

qcaucd private dispatcher `FUN_14004ef90`:

- op4 -> `FUN_14004cba0`, expects a 0x40-byte hardware/resource descriptor and performs the real physical-codec start/resource-owner path;
- op5 -> `FUN_14004d750`, the corresponding stop path.

This closes the architectural bridge between the user-visible MicArray endpoint and the lower hardware resource lane:

```text
visible MicArray1 / qcasd category 4 / qcadcm EpType 16
    -> UseInternalCodec subclass PTR_FUN_140011a18
    -> FUN_14002de30 / FUN_14002e010
    -> qcasd private-codec request builder
    -> qcaucd private op4 / op5
    -> ASL hardware/resource translation
    -> physical VA-DMIC lane
```

The visible EP16 graph therefore does not need to be the physical microphone ingress itself. It explicitly owns a second lower-level hardware-codec start/stop path.

## Request builder immediately above qcaucd

`FUN_14002d328(param_1, param_2, epType, out)` builds the request passed toward the physical-codec layer:

```c
*out = 1;
out[2] = param_2[0];
out[3] = param_2[1];
*(u16 *)(out + 4) = *(u16 *)(param_2 + 2);
FUN_14002d400(param_1, param_2[3], epType, (u8 *)out + 0x12);
```

The unresolved function `FUN_14002d400` fills the descriptor tail using the stream/interface selector plus `epType`. It is now the highest-value static target because it should expose the exact mapping from visible MicArray1/EP16 semantics into the 0x40-byte physical descriptor consumed by qcaucd op4.

## Next target

Decompile and annotate `FUN_14002d400` and its direct callees. Correlate its output against:

- the known qcaucd op4 0x40-byte descriptor schema;
- `acdb-driver-inventory-current.json`;
- the already recovered endpoint-2 resource tuples (`0x01020000` / `0x01020001`, VA DMIC ports 8/9);
- the previous live `qcaucd+0x51264` serialized `0x08000020` row.

Promotion criterion: prove the concrete MicArray1 internal-codec descriptor/resource selection that reaches the EP2 VA-DMIC hardware lane, without mutating Windows.

## Safety

- No Windows runtime selector/ACDB mutation.
- No reboot.
- No Linux deployment or topology mutation.
- Golden v33 remains untouched.
- Windows bootdebug remains enabled for the unfinished oracle phase.
