# Windows qcaucd RTC callback and live-handle map

Date: 2026-08-16
Status: GREEN static transport recovery / live table read pending

## Purpose

Recover the exact Windows path behind qcadcm ATS/ADIE register GET without repeating the rejected APPS-side direct WSA-macro MMIO experiment.

Direct KD physical reads of the `0x06b00000` LPASS WSA aperture remain forbidden: the preserved experiment caused WHEA `0x124`. Everything below is ordinary driver code/static analysis and the proposed live follow-up reads only normal qcaucd kernel virtual memory.

## Cross-driver interface identity

qcadcm publishes a KMDF query interface under GUID bytes:

`a2 19 2c cd dc d9 d6 47 92 ac 4b a3 32 74 8d 87`

GUID form:

`{CD2C19A2-D9DC-47D6-92AC-4BA332748D87}`

The exact 16-byte sequence occurs once in the matching `qcaucd8380.sys`, at qcaucd VA/RVA `0x140013830 / 0x13830`.

qcaucd registers PnP notification for that GUID, opens the qcadcm device interface, then calls its KMDF query-interface thunk with:

- target = opened qcadcm I/O target
- GUID = `0x140013830`
- output = qcaucd global `DAT_140017e00`
- size = `0x38`

The returned 0x38-byte interface is:

- `+0x20` qcadcm `AcdbIoctl`
- `+0x28` qcadcm `AcdbCodecRegister`
- `+0x30` qcadcm `AcdbCodecDeregister`

qcadcm stores the callback passed to `AcdbCodecRegister` in its device context at `+0x1ab8` and `CodecRtcIoctl` invokes that callback as `(command, buffer, size)`.

## Exact qcaucd callback

There is exactly one qcaucd reference to the returned `AcdbCodecRegister` pointer: `FUN_140053b90` (RVA `0x53b90`). It registers:

`FUN_140052860` (RVA `0x52860`)

as qcadcm's codec RTC backend callback.

The callback command switch confirms:

- command `1` = legacy codec information
- command `2` = single register GET
- command `3` = multiple register GET
- command `4` = single register SET
- command `5` = multiple register SET
- command `8` = V2 codec information

This independently agrees with qcadcm ATS opcodes `0x41520001..0x41520005`, while keeping GET and SET unambiguous.

## Single-register GET semantics

qcadcm's ATS single GET accepts the first 12 bytes:

`{ u32 handle, u32 register_id, u32 mask }`

and invokes `CodecRtcIoctl(command=2, buffer, size=0x10)`. The fourth dword is returned to ATS.

Inside qcaucd callback command 2, `FUN_14004de78` repacks the request into the internal codec request:

- `u16 register_id`
- `u8 mask`
- `u8 shift`
- `u8 value` output
- padding/internal fields
- `u32 handle`

For the ATS path, `shift = 0`. After successful read, qcaucd returns the low 8-bit value.

The helper submits internal command `0x302`. The adjacent single SET helper uses `0x301`; multi SET/GET use `0x304/0x305` respectively. Thus the recovered live follow-up can remain strictly read-only.

## `0x302` register engine

qcaucd dispatcher `FUN_1400266c8` routes `0x302` to:

`FUN_1400259e8(mode=2, request)`

The engine first validates the handle through `FUN_140022a10`.

A handle is accepted only if it occurs in both of two 8-entry runtime tables:

1. codec-properties table rooted at qcaucd RVA `0x17c14`, 0x30-byte stride;
2. live device-object table rooted at qcaucd RVA `0x17720`, 0x18-byte stride.

The properties table also returns the codec backend type from entry offset `+0x1c` (root RVA `0x17c30`). The live-object table returns the matching device pointer from entry offset `+0x08` (root RVA `0x17728`).

The generic read path then locks the register family and dispatches either through the generic register helper or the live device-object/SoundWire helper depending on backend type. No arbitrary handle is accepted.

## Handle population

`FUN_140021a40` populates both tables during codec/device discovery. Handles are synthesized from the discovered codec class/instance rather than being raw MMIO addresses. `FUN_140022cf8` also initializes the first platform codec entry from live platform data.

Therefore there is no reason to guess a handle or brute-force ATS. The live Windows tables are the authoritative source.

## Safe live next step

On the next Windows boot, use KDNET only to:

1. resolve live `qcaucd8380.sys` base;
2. read ordinary qcaucd kernel virtual memory at:
   - `base + 0x17c14` for eight 0x30-byte codec-property entries;
   - `base + 0x17720` for eight 0x18-byte live device entries;
3. decode the nonzero handles/backend types and correlate them to the already-known WSA devices;
4. only after the exact handle is known, consider the recovered read-only ATS GET path for specific register IDs.

This does **not** read the physical WSA macro aperture and must not be replaced with `dd/!dd` against `0x06b00000`.

## Relevant static RVAs

qcadcm:

- `CodecRtcIoctl`: `0x2d000`
- `AcdbCodecRegister`: `0x6a0d0`
- `AcdbCodecDeregister`: `0x6a1e0`
- query-interface GUID: `0x1c768`

qcaucd:

- matching qcadcm GUID: `0x13830`
- query-interface result: `0x17e00`
- AcdbIoctl slot: `0x17e20`
- AcdbCodecRegister slot: `0x17e28`
- AcdbCodecDeregister slot: `0x17e30`
- RTC callback: `0x52860`
- single GET helper: `0x4de78`
- internal register dispatcher: `0x266c8`
- register engine: `0x259e8`
- handle lookup: `0x22a10`
- codec-properties table: `0x17c14`
- live device table: `0x17720`
