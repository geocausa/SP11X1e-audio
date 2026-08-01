# Protection event return path — evidence and current state (2026-08-01)

## The gap

Windows registers ADSP callbacks and persists the results. Linux does not.
This is why speaker protection on this machine can be shown **configured** but
never shown **acting**.

From Ghidra of `qcadcm8380.sys` (prior session, `00_STATUS.md` R15):

> "ADSP callback handler mapped (EVENT_ID_VI_CALIBRATION,
> EVENT_ID_SPv5_SPEAKER_DIAGNOSTICS). Registry calibration mined:
> R0CalQ24=4.956Ω/5.371Ω, T0CalQ6=38.7°C/37.0°C per channel."

So the Windows loop is: DSP measures via V/I -> event fires -> driver updates
R0Cal/T0Cal -> persisted to registry -> fed back as the new reference. Your
R0/T0 values are *measured results Windows calibrated*, not factory constants.

Linux has the same inputs but no return path.

## Event IDs — verified against binaries, not trusted

Claims from the archive were checked directly against the shipped binaries
rather than accepted:

| ID | `qcadcm8380.sys` | `qcadsp8380.mbn` (22 MB ADSP FW) | Verdict |
|---|---|---|---|
| `0x08001511` VI_CALIBRATION | 2 hits `0x7901c`, `0x7cfb8` | **2 hits** `0x100f95`, `0xa3338d` | real, in both |
| `0x0800138C` SPEAKER_DIAGNOSTICS | 2 hits `0x79018`, `0x7cfa8` | **0 hits** | host-side only |
| `0x0800150A` | 0 hits | 0 hits | SPv5 config param, not an event constant |

In `qcadcm8380.sys` the two event IDs sit **adjacent in a data table**
(`0x79018`/`0x7901c`), which is what a pair of event constants looks like.

In the ADSP firmware `0x08001511` appears as an unaligned immediate mid
instruction (`...010000e7 11150008 83250000...` at `0x100F95`), i.e. a constant
loaded into a register — consistent with the archive's
"written into an output/event record in `TGT_b03e9ae4`".

Also confirmed present in ADSP firmware: `SP` `0x070010E2` at `0x12c6034` and
`SP_VI` `0x070010E3` at `0x12c6024`, adjacent. The protection modules really
are in the DSP image.

### Every checkable archive claim held

Including the specific one that the ADSP image contains **only**
`0x08001511` of the three. That is exactly what a fresh scan found.

### But the interpretive claims are NOT verified

These rest on Ghidra decompilation not reproduced here, and are the ones most
likely to be wrong or context-dependent:

* "`0x0800138C` belongs to instance `0x4ac1` in graph `0xb000008a`, an
  unrelated audio graph"
* "`0x08001511` (firmware-helper-constant): **do-not-init-send**"
* "not mapped to the requested SPv5 init path with enough confidence"

The pattern for this archive, learned the hard way today: **raw findings are
reliable, layered conclusions need checking.** Compare the fabricated MSIIR
payload (plausible output, byte-scanning tool) and the `G_18_DB` confusion
(correct fact, wrong inference attached).

## `0x0800150A` is already being sent and accepted

From a June 2026 runtime capture:

```text
SP11DBG SPv5 separate instance=0x00004027 module=0x070010e2 index=6
        param=0x0800150a size=20 rc=0
POOL+0x2013c, 20 bytes: 0200000000000008000000080000000000000000
u32: [2, 0x8000000, 0x8000000, 0, 0]
```

One document calls it "VI calibration event — PROVISIONAL", another
"firmware-set-dispatch-proven, size unknown". It is an SPv5-owned parameter,
accepted with `rc=0`, and it is in the current deployment.

**This matters:** the DSP may already be primed to emit events, and the only
reason nothing is seen is that the Linux callback discarded them.

## What was implemented

`sound/soc/qcom/qdsp6/q6apm.c`, in `graph_callback()`,
`case APM_EVENT_MODULE_TO_CLIENT`:

Upstream silently drops every module->client event except
`EVENT_ID_SH_MEM_PULL_PUSH_MODE_WATERMARK`. Now anything else is logged:

```text
SP11 module event: src %#x event %#x payload %u
```

using `data->hdr.src_port` for the source (the `apm_module_event` struct has
only `event_id` and `event_payload_size` — no instance field).

Backup: `q6apm.c.bak-before-protevents-20260801`. This passive logging is now
part of the boot-validated `7.1.5-sp11-audio-clean+` baseline. No unsolicited
SPv5 module event was observed during the controlled playback windows.

### Event registration experiment: rejected and removed

`q6apm_register_module_event()` was subsequently wired experimentally to
subscribe SPv5 instance `0x4027` to `0x08001511`. The DSP rejected the request
deterministically with `-EOPNOTSUPP` on every graph open. The subscription probe
was therefore removed before the clean rebuild. Do not reintroduce this event
ID through APM registration without new Windows call-site evidence that proves
the exact destination, payload and registration mechanism.

## Next step

Keep the passive logger and decode any genuine unsolicited module event if one
appears. Separately reverse the exact Windows registration call path before
another Linux subscription attempt. The absence of a returned calibration
event does not invalidate the independently proven active VI transport and
accepted protection configuration, but it means adaptive persistence remains
an open boundary.

## Why this matters for the loudness work

PA Volume is currently 24 (+27 dB), raised from the upstream cap of 6 (0 dB)
on the grounds that protection is in place. That reasoning is sound but rests
on protection being *configured*; it has never been observed *acting*. The
operator correctly backed off from 26 to 24 for exactly this reason.

An event return path would replace that inference with evidence.
