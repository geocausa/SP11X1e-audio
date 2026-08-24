# UbiG M6: muted volume sweep, repeated playback, and nonzero DSP gate

Date: 2026-08-22  
Candidate base: `9bcb848`  
Result: **PASS for the tested disposable gates; candidate remains non-Golden**

The live source-owned UbiG candidate remained active while the next M6 lifecycle gates were exercised. The filter-chain process stayed at PID 180679 throughout and the public control page remained healthy at main generation/ack `3/3`, postgain generation/ack `1/1`, Movie active, endpoint postgain `-365`, and `last_error=0`.

## Muted endpoint-volume transaction sweep

The visible sink started at raw PipeWire channel gain `0.010282`, corresponding to UI scalar `0.217449893773`, with `wpctl` displaying 0.22. Exact endpoint DSP mute was asserted before changing the slider. Three persisted points were then transaction-verified:

| UI scalar | PipeWire raw | Windows endpoint dB | final Q28 | GainStep |
| ---: | ---: | ---: | ---: | ---: |
| 0.15 | 0.003375 | -28.229 | `0x00a31090` | 1 |
| 0.25 | 0.015625 | -20.747 | `0x0182aff5` | 3 |
| 0.40 | 0.064000 | -13.765 | `0x03491fb5` | 12 |

The original scalar was restored while still muted, then mute was cleared. A post-run `pw-dump` returned exactly to raw `[0.010282, 0.010282]`, mute false. Crucially, live endpoint-volume changes did **not** move the UbiG postgain cadence: request/ack stayed `1/1`, matching the recovered Windows policy that postgain is frozen for the lifetime of one Dolby/UbiG generation while final VOL_CTRL/GainStep changes live downstream.

An attempted 0.10 UI point did not persist in the current visible-sink session and emitted no transaction update, so it is intentionally not counted as a passing sweep point. The same visible copy-node contract is shared by Golden and candidate; this is tracked as a session/control-policy observation rather than being attributed to UbiG DSP.

## Repeated playback lifecycle

Twenty independent 9,600-frame, 48-kHz stereo float32 silence streams were opened and closed through `effect_input.sp11_windows_dolby`. The filter-chain PID stayed 180679 for all 20 iterations, visible volume stayed 0.22, UbiG generations remained unchanged, and the candidate/volume/monitor-link services remained active. There were zero filter-chain xrun/NaN/crash markers and zero new WSA/SoundWire/q6apm audio faults. One known graph-birth `APM_CMD_SET_CFG 0x01001006` status-3 record appeared, followed by the existing Qualcomm-GSL continue path; this is the established Golden baseline event.

## Nonzero PCM path without physical sound

To exercise real nonzero arithmetic in the live PipeWire candidate without producing speaker output, exact endpoint DSP mute was asserted and a five-second float32 stereo signal was streamed: left `0.035*sin(733 Hz) + 0.012*sin(137 Hz)`, right `0.030*sin(977 Hz) + 0.010*sin(211 Hz)`. The candidate completed the stream with the same process PID, unchanged UbiG control generations, zero filter-chain fault markers, zero new kernel audio faults, and exact visible-state restoration after unmute.

This is a live nonzero-DSP stability gate, **not** an acoustic parity result because final endpoint mute deliberately suppressed physical output.

## Protected graph readback

Fourteen bounded GET_CFG replies since candidate activation decode cleanly and are all accepted. The unique SP configuration is 48 kHz, 16-bit, two speakers, feature mask 31 with notch/high-pass, thermal, feedback excursion, DC prediction and feedback DC all enabled, 40-Hz pilot. SPVI remains two speakers at 8 kHz, 40-Hz pilot, 200-ms warmup, DC detection disabled. The running kernel does not currently emit the bounded `SP11 WSA live sample=` observer records, so full per-amplifier PA/error/interrupt/ADC/temperature telemetry parity is not claimed by this gate.

Reviewed machine-readable evidence is in `artifacts/reviewed/2026-08-22-ubig-native-candidate-m6-lifecycle-v2.json`.

Remaining M6 work is physical non-muted/acoustic validation, seek/program-content lifecycle, full PA/protection telemetry when available, >8-hour stability, and the final Windows acoustic matrix. Golden rollback remains intact.
