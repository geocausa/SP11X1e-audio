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

### 2026-08-01 correction: the Windows call site is now proven

The earlier file offsets `0x79018`, `0x7901c`, `0x7cfa8`, and `0x7cfb8` were
incorrectly carried into one analysis pass as image addresses. They are raw
offsets in the PE `PAGEAR` section. For the hash-bound driver, `PAGEAR` has raw
offset `0x21600` and image base address `0x14002d000`, producing these correct
virtual addresses:

| Raw offset | Virtual address | Value |
|---:|---:|---:|
| `0x79018` | `0x140084a18` | `0x0800138c` |
| `0x7901c` | `0x140084a1c` | `0x08001511` |
| `0x7cfa8` | `0x1400889a8` | `0x0800138c` |
| `0x7cfb8` | `0x1400889b8` | `0x08001511` |

Fresh Ghidra xrefs at those mapped addresses close both the receive and
registration paths:

- `FUN_140083d00` (`cbGslGraphfunc`) dispatches `0x0800138c` as
  `EVENT_ID_SPv5_SPEAKER_DIAGNOSTICS` and `0x08001511` as
  `EVENT_ID_VI_CALIBRATION`.
- `FUN_140085270` (`AudioDspGraphOpen`) resolves tag `0x0401000b` with
  `GetMidAndMiid`. `GetMidAndMiid` returns `{module_id, module_instance_id}`;
  the driver uses the returned instance ID in a 16-byte
  `GSL_CMD_REGISTER_CUSTOM_EVENT` request.
- In ordinary speaker-protection mode, this code path prepares registration of
  event `0x0800138c` on the **SP_VI tag instance** when the driver's hardware
  event-registration flag is enabled. In the captured root this is SP_VI IID
  `0x4024`, not SPv5 IID `0x4027`.
- Windows registers `0x08001511` on that same SP_VI instance only when the
  requested mode is calibration mode (`mode == 2`).

Qualcomm's public GSL implementation closes the remaining translation layer.
`gsl_graph_register_custom_event()` accepts the same 16-byte request used by
the Windows call site, allocates opcode `APM_CMD_REGISTER_MODULE_EVENTS`
(`0x0100100e`), copies the instance, event, registration flag, and optional
configuration into `apm_module_register_events_t`, and sends it to SPF. This is
the same 24-byte APM structure already implemented by Linux
`q6apm_register_module_event()`; no Windows-only registration transport is
missing.

Therefore the first rejected Linux probe tested the wrong pair twice over:
`0x4027:0x08001511` instead of the normal-playback Windows pair
`0x4024:0x0800138c`. Its rejection says nothing about support for the actual
Windows diagnostics subscription.

The diagnostics event payload parser is also explicit: a leading `u32`
speaker count followed by one `u32` condition per speaker. Condition `3`
invokes the Windows DC-protection callback; condition `4` reports a speaker
temperature overshoot. Other conditions are logged and ignored.

Evidence:

- `artifacts/offline-audit-20260729/qcadcm-protection-event-va-xrefs-20260801.txt`
- `artifacts/offline-audit-20260729/qcadcm-getmidmiid-20260801.txt`
- Qualcomm GSL `gsl/src/gsl_graph.c`, function
  `gsl_graph_register_custom_event()`, graphservices commit
  `8445aee939cb8b37d80eccf6b43baf778fef23c4`
- `qcadcm8380.sys` SHA-256
  `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429`

## 2026-08-02 isolated runtime result

The corrected diagnostic kernel sent the exact APM registration structure for
SP_VI `0x4024`, event `0x0800138c`, registration flag `1`, and zero event-config
bytes. SPF rejected opcode `0x0100100e` with status `3` (`AR_EUNSUPPORTED`) on
every protected graph open. The rejection was deliberately non-fatal; all
protection configuration stages and `GRAPH_START` still succeeded.

A complete search of the surviving live Windows QGPR captures found many
successful `APM_CMD_REGISTER_MODULE_EVENTS` packets for pull, pause, and other
modules, but no registration packet for `0x4024:0x0800138c`. The registration
pair and transport are static-code proven, while its execution during the
captured ordinary playback sessions is **not** live proven. The conditional
hardware flag visible at the Windows call site explains why the code can be
real without appearing in those traces.

This changes the disposition:

- keep the passive bounded module-event logger in the clean driver;
- do not send the rejected diagnostics subscription by default;
- do not treat its absence as a playback-parity failure;
- revisit it only if a future Windows trace captures the registration or its
  enabling hardware-policy state.

The diagnostic boot emitted no unsolicited protection event. That is expected
when no subscription was accepted and does not contradict the independently
accepted SP/SP_VI configuration.

## Why this matters for the loudness work

PA Volume is currently 24 (+27 dB), raised from the upstream cap of 6 (0 dB)
on the grounds that protection is in place. That reasoning is sound but rests
on protection being *configured*; it has never been observed *acting*. The
operator correctly backed off from 26 to 24 for exactly this reason.

An event return path would replace that inference with evidence.
