# Windows KDNET structural-gap capture

This is a read-only capture for the three facts that the saved 2026-06-18 KDNET
sessions missed:

1. the six 32-bit ACDB selector values passed to query `0xacdb0017`; and
2. every client SET_CFG payload, including the two 64-byte graph-manager
   requests adjacent to each captured music GRAPH_OPEN; and
3. the mapped out-of-band request bodies used to construct large GPR commands,
   including GRAPH_OPEN.

Do not use this runbook with a different `qcadcm8380.sys`. The RVAs are tied to:

```text
SHA-256 37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429
PE timestamp 2025-04-17 22:12:37 UTC
```

## Why these breakpoints

The saved boot log proves that `qcadcm8380+0x307a8` receives selector query
`0xacdb0017` with this input layout:

```text
+0x00  uint32 count        (6)
+0x08  uint32 *values      (the pointer the old script did not dereference)
```

Independent disassembly of the matching driver and the saved playback stack
identify `qcadcm8380+0x5aa34` as the point after both request segments are
copied and immediately before the GRAPH_OPEN wrapper is built and sent. At that
point:

```text
[sp+0x20]  uint32 first_segment_size
[sp+0x24]  uint32 total_size
[sp+0x28]  uint32 second_segment_size
[sp+0xd0]  void  *mapped_request_base
```

For the saved `0xb18` request the values are `0xac8`, `0xb18`, and `0x50`;
`0xac8 + 0x50 == 0xb18`. The old log recorded the mapping base and the
`base+0xac8` segment boundary but never displayed bytes at the base.

The matching driver's `gsl_set_custom_config` entry is
`qcadcm8380+0x60b78`. ARM64 arguments `x1` and `w2` are the original payload
pointer and total size. This is before its 256-byte in-band/OOB split, so one
read-only breakpoint captures both paths and, unlike the GRAPH_OPEN-specific
breakpoint, does not exclude the decisive 64-byte requests.

## Capture procedure

1. Connect the USB EEM/KDNET debugger and stop at the first kernel boot break,
   before Windows audio initializes.
2. Start a WinDbg text log. The byte dumps in that log are the primary output.
3. Execute `tools/kdnet/capture-structural-gaps.kd` with WinDbg's `$$><`
   command. The script arms both deferred breakpoints and continues Windows.
4. Let Windows reach the desktop, then start ordinary speaker playback once.
5. After playback has opened, break into the target, save/close the WinDbg log,
   and detach or shut down normally.

The selector breakpoint records every valid six-word query, up to 64 matches,
because repeated or changed vectors are needed to reconstruct selection order.
The SET_CFG breakpoint records valid payloads up to `0x4000` bytes and disables
itself after 256 matches. The GRAPH_OPEN OOB breakpoint records only requests
from `0x500` through `0x4000` bytes whose two segment sizes sum to the total,
and disables itself after 32 matches. No breakpoint modifies target memory or
driver state.

Expected log markers:

```text
===== CODEX_ACDB_SELECTOR_6X32_BEGIN =====
...
===== CODEX_ACDB_SELECTOR_6X32_END =====

===== CODEX_QCADCM_SET_CFG_BEGIN =====
...
===== CODEX_QCADCM_SET_CFG_END =====

===== CODEX_QCADCM_OOB_BEGIN =====
...
===== CODEX_QCADCM_OOB_END =====
```

Retain the complete log rather than copying only the apparent GRAPH_OPEN hit.
Ordering is needed to bind all co-selected GKV schemas and their cross-bundle
edges to the same Windows session. In particular, do not omit 64-byte SET_CFG
blocks: their count and timing currently match the two SCLU bridges but their
uncaptured contents are the evidence needed to accept or reject that mapping.
