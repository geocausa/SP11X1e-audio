# qcadcm ADIE ATS/Diag transport recovery

Date: 2026-08-16
Status: AMBER — packet/service identities are statically pinned; no live request has yet been issued

## Purpose

Recover a safe Windows path for observing LPASS/WSA macro register state after direct APPS/KD MMIO reads of the WSA aperture caused a fatal WHEA 0x124. Only read-side ADIE commands are in scope.

## Exact ATS ADIE command IDs

The shipped SP11 `qcadcm8380.sys` stores the ADIE realtime-calibration command IDs as little-endian 32-bit constants:

- `0x41520001` — ADIE codec-info GET
- `0x41520002` — ADIE single-register GET
- `0x41520003` — ADIE multi-register GET
- `0x41520004` — single-register SET (forbidden)
- `0x41520005` — multi-register SET (forbidden)

`0x4152` is the registered ADIE realtime-calibration ATS service category (`AR`).

The ATS dispatcher validates an 8-byte outer request prefix:

```text
u32 command_id
u32 payload_size
u8  payload[payload_size]
```

and requires `packet_size - 8 == payload_size` before dispatch.

For command `0x41520002`, the handler requires exactly the first 12 bytes of payload as:

```text
u32 handle
u32 register_id
u32 mask
```

and returns exactly one 32-bit register value on success. Its internal `CodecRtcIoctl` command is `2`.

## DiagRouter transport

qcadcm opens the kernel target name:

```text
\\Device\\DiagRouter
```

The exact qcadcm DiagRouter IOCTL constants recovered from the binary are:

```text
0x80082400  IOCTL_DIAGROUTER_CMD_INIT
0x80082404  IOCTL_DIAGROUTER_CMD_REG
```

The registration payload is 16 bytes and registers:

```text
subsystem = 0x11
command range = 0x0803 .. 0x0834
```

Incoming Diag subsystem packets in that range are reassembled by qcadcm and forwarded to `ats_execute_command`.

## Codec-info GET

ADIE command `0x41520001` invokes `CodecRtcIoctl` command `8` first and falls back to command `1` for the older codec-properties format. The v2 response format is:

```text
u32 version          // expected 2 on the v2 path
u32 codec_count
codec_count * 0x14 bytes
```

The v1 fallback is:

```text
u32 codec_count
codec_count * 0x10 bytes
```

This read-only command is the preferred way to discover the valid runtime codec handle before attempting any register GET.

## Safety rule

Do not issue commands `0x41520004` or `0x41520005`. Do not direct-read the WSA MMIO aperture with KD/APPS physical access. A live request is allowed only after the local Diag framing/path is proven and the request is constrained to command `0x41520001` or `0x41520002`.

## Immediate next step

Boot Windows one-shot with KDNET available, inspect the live DiagRouter exposure and qcadcm ATS state, and determine the lowest-risk way to invoke codec-info GET. If local user-mode DiagRouter injection is not proven, prefer KD observation around qcadcm's own read-only ATS/CodecRtc path rather than guessing a packet path.
