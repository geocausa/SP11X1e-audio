# Windows preserves the full graph calibration and tolerates status 3

## Result

The accepted `audio-clean` behavior is closer to the Windows Qualcomm GSL
implementation than the later 10,416-byte Clean2 promotion.

Windows requests the selected non-persistent graph calibration from ACDB and
sends the complete result as one out-of-band `APM_CMD_SET_CFG`. For the
reviewed full-volume speaker selection this body is 10,464 bytes and contains
107 aligned parameter frames, including frame 63, IID `0x412b`,
`PARAM_ID_SPR_SESSION_TIME` (`0x0800113d`). The Windows host does not remove
that 48-byte frame.

When SPF returns status `3` for this graph-calibration transaction, GSL logs a
warning and continues graph construction. It does not treat this scoped
non-persistent calibration result as a fatal graph-open error.

## Direct Windows-driver verification

The preserved driver is:

- file: `Gemini/dumps/WINDOWS_KERNEL_DUMP/qcadcm8380.sys`
- SHA-256: `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429`
- architecture: Windows ARM64

Ghidra decompilation of `gsl_graph_set_sg_cal` at RVA `0x58c60` independently
confirms the return policy. After the non-persistent calibration send, return
value `3` branches to the literal log message:

```text
graph send non-persist cal warning %d
```

and execution continues into persistent calibration. The same function
downgrades return value `3` to a warning for persistent and global-persistent
calibration. Other nonzero values follow the corresponding `failed %d` path
and are returned to the caller.

This verifies the policy in the hash-bound Windows executable rather than
inferring it from Linux or relying only on an earlier written analysis.

## Bound dynamic and static evidence

The reviewed Windows QGPR startup capture contains seven invariant protection
initializations. The graph/subgraph calibration transaction is 10,464 bytes
in five complete active selections and 9,872 bytes in two other complete graph
selections. `gsl_graph_set_sg_cal` sends this transaction immediately after
`GRAPH_OPEN`.

The OOB descriptor capture proves the live size, mapping, graph handle and
ordering. The selected 10,464-byte contents are reconstructed independently
from the recovered REV_0D ACDB using the captured speaker CKV. Qualcomm's
recovered resolver and the repository resolver produce the same SHA-256:

```text
2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1
```

## Linux A/B result

On 2026-08-02 the preserved `7.1.5-sp11-audio-clean+` entry was rebooted as a
control after MapDiag developed delayed stream start and a silent channel.
The control uses the 10,464-byte aggregate and the narrowly scoped
GSL-compatible warning policy.

During the live control:

- both physical speakers remained audible;
- both SoundWire amplifiers remained attached;
- repeated graph construction produced no SoundWire, cache-sync or XRUN
  errors;
- at uptime `183.049529`, graph calibration returned `AR_EUNSUPPORTED`;
- at uptime `183.075501`, `GRAPH_START` was accepted, about 26 ms later;
- the operator reported that YouTube no longer stalled or dragged as it had on
  MapDiag.

Clean2 changed the topology aggregate from 10,464 bytes/107 frames to 10,416
bytes/106 frames. Its topology inventory is otherwise structurally identical.
Clean2 was rejected after a reproducible physical right-only failure, and
MapDiag inherited the Clean2 topology before adding its own hardware
diagnostics.

This A/B does not by itself prove that deleting frame 63 causes the silent
channel. It does prove that deletion is not Windows parity, did not improve
the accepted acoustic baseline, and must not be promoted while the regression
is isolated.

## Kernel-dump boundary

`sp11_kernel_mcp_windbg.dmp` is a 667,664,704-byte Windows ARM64 kernel-only
dump captured while Firefox/YouTube was actively rendering. It preserves the
kernel audio drivers and kernel-side state, but its reviewed byte audit found
that the live AudioReach graph bodies were resident in ADSP shared/DMA memory
not included in this dump. The dump remains useful for driver objects, device
stacks, mappings, power/GPIO state and retained GSL objects; it is not the
source of the OOB calibration bytes.

## Decision

1. Preserve `audio-clean` as the accepted pre-Dolby control.
2. Preserve the full 10,464-byte Windows-selected aggregate.
3. Tolerate `AR_EUNSUPPORTED` only at the graph-calibration boundary, exactly
   as GSL does; other command errors remain fatal.
4. Keep the 10,416-byte Clean2 topology and its promotion patch as rejected
   diagnostic evidence.
5. Do not combine topology changes, amplifier polling, GPIO ownership changes
   and invasive regmap collection in one validation candidate.
