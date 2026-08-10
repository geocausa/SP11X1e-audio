# qcadcm query/event/hardware-resource runtime follow-up

Date: 2026-08-10 (Europe/London)

## Result

After the common SET_CFG capture failed to expose literal `PARAM_ID_CPS_LPASS_HW_INTF_CFG` (`0x08001259`), the runtime experiment moved to three additional qcadcm boundaries: GPR receive/query responses, graph events, and the qcadcm hardware-resource command/response path.

These additional paths were exercised during boot and audio activity. No literal `0x08001259` was observed in the bounded packet/payload searches performed at these boundaries. This further supports the driver-version-specific conclusion that the public CPS LPASS hardware-interface structure is not materialized as a literal parameter in the ordinary qcadcm traffic observed on this Surface Pro 11 stack.

This does not weaken the transport closure: the separately captured qcaucd runtime evidence already preserves the complete equivalent per-slave WSA8845 DP6 configuration required by the handoff.

## Capture boundaries

Hash gate remained unchanged:

- `qcadcm8380.sys` SHA-256: `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429`

The rebooted qcadcm image loaded at `0xfffff80329a10000` for this run. Read-only auto-continue breakpoints were placed at:

- qcadcm RVA `0x53958`: common GPR transmit/SET_CFG submit path;
- qcadcm RVA `0x572d0`: GPR receive path;
- qcadcm RVA `0x53644`: GET_CFG response completion path;
- qcadcm RVA `0x5c5d8`: hardware-resource command submission;
- qcadcm RVA `0x5c470`: hardware-resource response path;
- qcadcm RVA `0x83d00`: graph-event delivery.

All payload inspection used ordinary mapped kernel virtual memory with bounded sizes. No debugger physical MMIO read/write and no DSP write was used.

## Hardware-resource path

Early boot produced a repeatable private command/response exchange at the dedicated qcadcm hardware-resource path.

Observed command opcodes included:

- `0x0100100f`
- `0x01001010`

Observed response opcodes included:

- `0x02001002`
- `0x02001003`

Command payload sizes observed in the trace included `0x14`, `0x18`, `0x24`, and `0x48` bytes. Complete bounded searches at the command and response boundaries emitted no `0x08001259` or `0x08001537` search hit.

This excludes the obvious qcadcm hardware-resource request/response path as the source of the missing literal CPS LPASS parameter for this driver build.

## GPR receive / GET_CFG responses

The GPR receive hook observed normal response traffic including opcode `0x02001000` (GET_CFG response). The GET_CFG completion hook reported successful status (`status=0`). Actual responses captured in the inspected log included the following packet sizes and target instance IDs:

| Packet size | Target IID | Observed |
|---:|---:|---:|
| `0x34` | `0x0000466b` | 2 |
| `0x44` | `0x0000466b` | 2 |
| `0x5c` | `0x00004024` | 1 |
| `0x74` | `0x00004027` | 2 |

The common GPR receive breakpoint searched complete bounded packets for `0x08001259` and `0x08001537`; no search-result address was emitted for either value in the GET_CFG responses captured during this run.

The CPS source IID `0x402b` did not appear as a target IID in the observed GET_CFG responses.

## Graph-event path

The graph-event boundary was highly active. In the first inspected interval the persistent log contained:

- 2,144 events for IID `0x00004660`, event ID `0x0800101c`, payload size `0x4`;
- one event for IID `0x0000466b`, event ID `0x08001043`, payload size `0x0`.

The repetitive `0x4660/0x0800101c` event was then filtered from debugger logging because its four-byte payload cannot contain a four-byte parameter header plus any CPS structure and it was dominating the trace. Other graph events remain instrumented with bounded searches.

No literal `0x08001259` or `0x08001537` was observed in graph-event payloads during the inspected interval.

## Evidence versus interpretation

### Directly observed

- Successful early-boot qcadcm hardware-resource command/response traffic at RVAs `0x5c5d8` and `0x5c470`.
- Successful GPR receive and GET_CFG response traffic, including response opcode `0x02001000` and status `0`.
- GET_CFG responses for IIDs `0x466b`, `0x4024`, and `0x4027`, but not CPS source IID `0x402b`.
- 2,145 graph events in the first counted interval, dominated by IID `0x4660` / event `0x0800101c`.
- No debugger search result for literal `0x08001259` at the instrumented query/response, graph-event, or hardware-resource boundaries.

### Interpretation

Taken together with the earlier wrapper-level and common-GPR SET_CFG negatives, the remaining plausible explanation is that this Windows driver version either synthesizes the CPS SoundWire hardware-interface semantics below qcadcm or uses a private representation that never contains the public `0x08001259` identifier in the observed host-side buffers.

The qcaucd runtime capture remains the authoritative Windows-equivalent transport evidence: both WSA8845 slaves are individually identified and receive the complete DP6 programming required by the original handoff acceptance rule.

## Raw evidence

The active persistent KD log for this follow-up is:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\CPS_EVT_20260810_2249BST_167c_2026-08-10_22-49-44-527.log`

The log remains outside Git pending final closure/hash and the normal secret/privacy review.
