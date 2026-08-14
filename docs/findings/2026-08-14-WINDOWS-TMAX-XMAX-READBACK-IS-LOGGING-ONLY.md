# Windows TMax/XMax readback is logging-only

Date: 2026-08-14

## Result

The missing completed Windows response for speaker-protection query
`0x4027:0x080011f2` remains an observability gap, but it is not a missing
render actuator and cannot explain the current Windows/Linux tonal mismatch.

Static causal tracing through the two hash-locked Windows drivers proves that
the returned maximum excursion/temperature records are copied to caller
storage and logged.  No gain, protection configuration, codec register or
AudioReach `SET_CFG` operation is downstream of this result.

## Exact Windows chain

In `qcadcm8380.sys` SHA-256
`37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429`,
`GetSpkrProtTMaxXMaxParameters` at `0x140075eb8` issues the tagged GET for PID
`0x080011f2`.  For each speaker it copies:

- maximum excursion and its count;
- maximum temperature and its count.

The function then returns those records to its caller and emits WPP logging.
There is no adjacent writeback path.

In `qcaudminiport8380.sys` SHA-256
`79b26804d05332304c736c4e03e942db6a07ea886a2b07f3a4ff5947d1d05531`,
function `0x140093cb0` invokes graph IOCTL selector `0x1e` through wrapper
`0x140062830`, supplying a 132-byte output buffer.  On success it iterates the
speaker records and calls `0x1400108a8`, a WPP/ETW logger for the channel,
excursion/count and temperature/count fields.  Its callers at `0x14008e680`,
`0x14008fb50`, and `0x140090700` then clear graph state; none consumes the
values as an actuator.

## Linux boundary already tested

Linux has already attempted the exact old Windows query.  The DSP returned a
well-framed 92-byte response with command and module status `1`, a zero-filled
body, and Linux correctly rejected it with `-EINVAL`.  The Windows diagnostics
event `0x0800138c` and all tested newer public protection telemetry IDs are
also unsupported by this firmware.

Therefore another Windows reboot is not required for tonal-parity work.  A
future completed Windows `0x080011f2` response would still be useful for
safety telemetry comparison, but only as observation.

Machine-readable evidence:
[`2026-08-14-windows-protection-readback-causality.json`](../../artifacts/reviewed/2026-08-14-windows-protection-readback-causality.json).
