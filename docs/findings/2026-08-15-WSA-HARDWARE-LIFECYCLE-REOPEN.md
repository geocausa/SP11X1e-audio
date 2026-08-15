# SP11 WSA hardware lifecycle reopened after unsafe DRE cold boot

Date: 2026-08-15
Status: OPEN / safety-gated

## Why this layer is reopened

The isolated `sp11-audio-wsa-dre-ctl1` cold boot reached the Windows-observed WSA8845 `DRE_CTL_1=0x00` state on both physical amplifiers but produced an ugly/unsafe acoustic noise. The candidate is rejected and must not be armed again.

The Windows observation remains valid: fresh qcaucd KDNET proves both amplifiers initialize `0x34b1=0x00` and do not rewrite it on ordinary PA start/stop. The failed Linux boot proves only that copying this one register into the native Linux WSA8845 lifecycle is insufficient.

## Candidate confound resolved: PA Volume 31

Linux `SpkrLeft/Right PA Volume` is the ALSA control directly backed by the `WSA884X_DRE_CTL_1_CSR_GAIN` field:

```
SOC_SINGLE_RANGE_TLV("PA Volume", WSA884X_DRE_CTL_1,
                     WSA884X_DRE_CTL_1_CSR_GAIN_SHIFT,
                     0x0, 0x1f, 1, pa_gain)
```

The control is inverted, so user value 31 writes raw CSR gain code zero. It does not program a separate PA-gain register. The rejected candidate therefore changed the CSR fallback field and CSR enable lifecycle, not an unrelated analog PA gain.

## DP2 COMP transport is already proven

The full Windows qcaucd FIFO capture and later Linux comparison already prove ordinary speaker playback schedules only DAC + COMP + BOOST on the WSA8845 receive side. Linux matches Windows DP2/COMP mask and transport timing. Playback `hw_params()` explicitly excludes PBR, VISENSE and CPS from the speaker RX stream.

Therefore the cold failure is not adequately explained by a missing DP2 lane. A scheduled COMP transport can still carry invalid/not-yet-ready producer data.

## Producer-side lifecycle remains unproven

Linux has separate state machines for the producer and the consumer:

- WSA macro policy `WSA_COMP1/2 Switch` sets software intent.
- WSA macro DAPM `POST_PMU` performs the actual compander hardware transition: clock enable, reset sequencing and `RX_PATH_COMP_EN`.
- WSA8845 `hw_params()` admits the COMP SoundWire port according to `port_enable[]`.
- WSA8845 `mute_stream()` separately controls CSR fallback and `GLOBAL_PA_EN`.

No existing proof establishes that Linux WSA-macro compander producer readiness precedes the WSA8845 PA becoming audible on every cold/open/teardown lifecycle. Windows can safely leave CSR fallback disabled; Linux cannot be assumed safe until this ordering is directly measured.

## Clock-stop lifecycle mismatch

Fresh live Windows REV_0D registry:

- `SwrSleep = 1`
- `SwrClockStopTimerMS = 500`

The current Qualcomm Linux SoundWire master uses:

```
pm_runtime_set_autosuspend_delay(dev, 3000);
```

This 500 ms vs 3000 ms mismatch is real. It is not yet claimed as the acoustic root cause, but it is part of the hardware lifecycle parity gate and is a plausible contributor to the repeated service-teardown/clock-stop wedges seen during the investigation.

## Safety observations

Live reads of suspended WSA8845 debugfs regmaps and aggressive PipeWire/filter-chain teardown have both coincided with complete SP11 loss of reachability on the modified stack. Until proven otherwise:

- do not arm the rejected DRE candidate;
- do not use suspended-device debugfs register reads as a lifecycle probe;
- do not repeatedly stop/start the production user audio stack to measure ordering;
- do not use nonzero program audio during the trace phase.

## Next candidate: trace only

The next kernel candidate must make **no audio behavior changes**. It should emit one bounded `SP11TRACE` stream containing monotonic-order evidence for:

1. WSA8845 speaker `hw_params()`:
   - rate;
   - `port_enable[]` values;
   - actual selected SoundWire port numbers/channel masks;
   - active port count.
2. WSA macro interpolator lifecycle:
   - PRE_PMU;
   - POST_PMU before and after `wsa_macro_config_compander()`;
   - compander index and `comp_enabled[]` policy state.
3. WSA8845 speaker `mute_stream()`:
   - mute/unmute entry;
   - COMP-selected state for the admitted stream;
   - point immediately before `GLOBAL_PA_EN=1` and after the Windows-matched PA restoration writes.
4. Qualcomm SoundWire master runtime suspend/resume entry/exit and stream prepare/enable ordering.

No trace point may read WSA debugfs/regmap state. Software state and existing function arguments only.

## Acceptance gates before any new DRE experiment

A future DRE-disabled candidate is forbidden until the trace proves all of the following:

- speaker `hw_params()` admits DAC + COMP + BOOST before stream prepare;
- WSA macro compander producer is fully enabled before either WSA8845 unmute/PA-enable;
- both amplifiers see the same ordering;
- teardown disables PA before producer/COMP transport is removed;
- no SoundWire runtime-PM wedge occurs;
- the Windows 500 ms clock-stop policy is either reproduced or explicitly shown not to affect the ordering under test.

Only after those gates pass should `DRE_CTL_1=0` be reconsidered, and then as a separate one-variable candidate with UCM left at the known-good pre-DRE state unless evidence requires otherwise.
