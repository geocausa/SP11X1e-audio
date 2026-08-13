# Windows SOFT_PAUSE lifecycle recovery — 2026-08-13

## Status

Windows' DEFAULT internal-speaker graph contains SOFT_PAUSE instance `0x466b` (module `0x07001019`) between MFC `0x466a` and the downstream speaker/SAL path. Linux already instantiates the module and registers its pause/resume completion events, but the current q6apm PCM trigger path does not drive the Windows lifecycle and the generic module-event path drops non-watermark events after logging them.

The Windows stream state machine, pause-control key vector, ACDB row selection, and resulting runtime module commands are now recovered. The mapping is exact: pause selects `0x0800102e` on iid `0x466b` with a zero-length payload; release/resume selects `0x0800102f` with a zero-length payload. This is independently supported by REV_0D ACDB and the previously captured common-GPR Windows `APM_CMD_SET_CFG` inventory.

## Correction to the earlier KD interpretation

Fresh KD had shown two pairs of buffers associated with iid `0x466b`:

- param `0x0800102d`, 24-byte zero-initialized buffer
- param `0x0800103e`, 4-byte zero-initialized buffer

Those buffers were previously described as runtime SET payloads. That interpretation is incorrect.

The matching archived Windows driver is:

`C:\Users\SurfacePro7\Documents\KDNET\Gemini\DUMP\qcadcm8380.sys`

SHA-256:

`37F76305AC8051B0B03B6D2CE1DF7A353253DEBF546E512E447C9D95EC661429`

Disassembly proves `gsl_get_tagged_custom_config()` reaches the low-level function at `qcadcm8380+0x57c00` for buffers <= 0x100 bytes. That is the exact function where the KD breakpoint captured the zero-initialized `0x466b` buffers. They are GET/readback request buffers before the DSP response, not evidence that Windows writes zero values to those params.

The static ACDB/topology values therefore remain meaningful configuration state:

- `0x0800102d`, 24 bytes: `{20, 1000, 3, 20, 1000, 3}`
- `0x08001030`, 2 bytes: zero
- `0x0800103e`, 4 bytes: `25`
- `0x08001170`, 4 bytes: zero

## Recovered Windows state machine

The Windows driver contains explicit routines/strings:

- `GetStreamSoftPauseConfigParams`
- `SetStreamPause`
- `RegisterPauseCompleteEvent`
- `GRAPH_STATE_RUN PAUSE STARTED`
- `GRAPH_STATE_RUN PAUSE ENDED`
- `GRAPH_STATE_STOP PAUSE ENDED`

`SetStreamPause` is at RVA `0x758b0`. It accepts only modes 3 and 4.

It builds a stream-pause TKV with:

- tag: `0x04010008`
- key: `0x01000021`
- mode 3 -> value `1`
- mode 4 -> value `0`

It then calls `gsl_set_config()` with the graph's active GKV, that tag, and the TKV. For the actual DEFAULT render subgraph `0xb000007e`, REV_0D's module-tag table contains exactly two rows for this tag/key: key value `0` selects iid `0x466b`, param `0x0800102f`, zero-length payload; key value `1` selects iid `0x466b`, param `0x0800102e`, zero-length payload. The Aug-10 Windows common-GPR capture independently observed `APM_CMD_SET_CFG` headers `0x466b/0x0800102e/size 0` three times and `0x466b/0x0800102f/size 0` three times. Therefore the generated commands are evidence-backed rather than inferred.

After applying the TKV, `SetStreamPause` calls `GetStreamSoftPauseConfigParams` and reads the SOFT_PAUSE timing/config through tagged custom GETs. The driver reads `0x0800102d` and `0x0800103e`; if the relevant returned timing values are zero it falls back to 20 and 25. It logs `Rampdown time: %d DownStrmDelay: %d`, derives a completion timeout from those values plus 5 ms, and waits on the registered pause-complete signal.

The surrounding graph-state routine maps the Windows state codes as follows:

- `1` = STOP
- `2` = SUSPEND
- `3` = PAUSE
- `4` = RUN

Observed control flow:

- PAUSE -> `SetStreamPause(3)` -> pause TKV value `1`; pause-in-progress state is set.
- RUN/resume, when pause is outstanding -> `SetStreamPause(4)` -> pause TKV value `0`; Windows logs `GRAPH_STATE_RUN PAUSE ENDED`.
- STOP -> performs the normal graph-stop path; if pause is still outstanding, it also calls `SetStreamPause(4)` to close the pause state and logs `GRAPH_STATE_STOP PAUSE ENDED`.

During graph open, Windows registers separate PAUSE_COMPLETE and RESUME_COMPLETE custom events against the SOFT_PAUSE MIID.

## Linux gap

Current `q6apm-dai.c` contains the explicit TODO on STOP:

`/* TODO support be handled via SoftPause Module */`

The PCM trigger handler currently does no DSP lifecycle work for START/RESUME/PAUSE_RELEASE or SUSPEND/PAUSE_PUSH. For the SP11 pull graph, STOP only changes host-side state and resets `queue_ptr`.

Linux already registers the SOFT_PAUSE module events:

- pause complete: `0x0800103f`
- resume complete: `0x08001043`

However, `q6apm.c`'s `APM_EVENT_MODULE_TO_CLIENT` branch forwards only the pull-watermark event. Other module events are logged and discarded, so the registered soft-pause completion events do not currently reach the PCM state machine.

## Relation to the YouTube seek issue

Controlled Windows Edge `currentTime` seeks did not generate additional SOFT_PAUSE lifecycle activity beyond normal stream start/stop behavior. SOFT_PAUSE is therefore a real Windows lifecycle parity gap, but it is not evidence for the in-stream YouTube seek smoothing mechanism. The physical seek verdict remains a separate gate.

## Exact command mapping and safe implementation gate

For DEFAULT render subgraph `0xb000007e`, the recovered command mapping is:

- PAUSE / state 3 / TKV `0x01000021=1` -> iid `0x466b`, pid `0x0800102e`, payload size `0`.
- RUN/release / state 4 / TKV `0x01000021=0` -> iid `0x466b`, pid `0x0800102f`, payload size `0`.

This mapping is supported independently by the REV_0D MTLU/MTDE/MTDO rows and by the Aug-10 Windows common-GPR SET_CFG capture. The zero-filled `0x0800102d`/`0x0800103e` buffers remain GET/readback buffers and must not be confused with these zero-length command parameters.

The remaining implementation work is host lifecycle plumbing: send the exact `0x102e`/`0x102f` commands only for the SP11 protected pull graph, forward PAUSE_COMPLETE/RESUME_COMPLETE events to the PCM layer, wait with the Windows-derived bounded timeout, and preserve the existing persistent-pull STOP/reprepare behavior. Build/test the patch before any live module replacement; do not install the existing experimental q6apm module tonight.

## Isolated Linux candidate status — 2026-08-13 follow-up

The bounded host-side implementation has now been converted into a clean reviewable patch:

`/home/geoca/Documents/SP11-AUDIO-AUDIT/softpause-lifecycle-candidate-20260813/0001-q6apm-sp11-windows-soft-pause-lifecycle.patch`

SHA-256:

`9dc808bbbf4dbe5240bd4bec4282a0e6fd1b956c657258c2f0ba24489d0dd05e`

The implementation derives its completion bound explicitly from the recovered current Windows timing rather than a naked constant: 20 ms rampdown + 25 ms downstream delay + 5 ms qcadcm completion allowance = 50 ms. It forwards the separate PAUSE_COMPLETE/RESUME_COMPLETE events, drives the exact zero-length `0x0800102e`/`0x0800102f` SET_CFG commands, and preserves the persistent pull graph across STOP/reprepare.

Strict `checkpatch` reports 0 errors, 0 warnings and 0 checks. A clean isolated rebuild with regenerated `LOCALVERSION=+` metadata produced exact running-release vermagic `7.1.5-sp11-cps-v3+` for both `snd-q6apm.ko` and `q6apm-dai.ko`.

The candidates remain unsigned while the live modules use the build-time autogenerated kernel key, and **no candidate module has been installed or loaded**. Signing/staging plus a bounded pause/resume/STOP lifecycle regression are the remaining live gates.
