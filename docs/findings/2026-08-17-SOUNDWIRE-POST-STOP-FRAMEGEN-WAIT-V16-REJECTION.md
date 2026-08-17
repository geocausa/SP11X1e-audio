# SoundWire post-stop frame-generator wait v16 rejection — 2026-08-17

## Why v16 was tested

Windows qcaucd and Linux were proven to use the same ordinary WSA-bus SoundWire stop primitive: simple Clock Stop Mode 0 with a broadcast write to device `0xF`, `SCP_CTRL (0x44) = CLK_STOP_NOW (0x02)`. The important master-side sequencing difference was after that command.

Windows `FUN_14003b230` polls `SWRM_COMP_STATUS (0x14)` bit 0 at roughly 1 ms cadence, bounded to about 30 ms, and only then gates the lower SoundWire resource/clock path. Linux qcom SoundWire previously called `clk_disable_unprepare(hclk)` immediately after `sdw_bus_clk_stop()` with no equivalent post-stop completion wait.

## One-variable candidate

v16 starts from the exact v13 coherent Windows analog-tail boot bundle and changes **only** `drivers/soundwire/qcom.c`, behind:

`soundwire_qcom.sp11_wait_clk_stop_complete=1`

After `sdw_bus_clk_stop()` it reads `SWRM_COMP_STATUS` and waits for `SWRM_FRM_GEN_ENABLED` to clear, retrying at `usleep_range(1000,1100)` for at most 30 iterations. Timeout remains nonfatal, matching the Windows bounded/fallback style. The qcom 3000-ms autosuspend delay, WSA8845 state, producer, UCM and route behavior remain v13.

Reproducible delta:

`patches/0057-SP11-soundwire-post-stop-framegen-wait-v16-experiment.patch`

Fresh module srcversion `2D070888203825563207B6F`; signed compressed SHA-256 `ef786378021d31a458f7aaa35e23b8e93ae396c9fe8c93805b50500ce7299c46`; initramfs SHA-256 `4a76e05b9492265f0251017f90456176a5987561bd87c0c9530fb642cbfbdf9c`.

## Structural gate

The new physical completion test is real on SP11 hardware. Boot/login teardown and the controlled zero-stream cycle both logged:

`SP11 SoundWire: frame generator stopped before hclk gate`

The test cycle still entered PCM `RUNNING`, returned `closed`, and the SoundWire master plus both WSA8845 slaves reached runtime suspend. No SoundWire, state, PA or XRUN failure appeared.

## Physical zero-signal gate

The measured PA wake deliberately followed one earlier complete v16 zero-stream/stop-wait cycle on the same boot, directly testing the cycle-2 state-loss problem.

Capture:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\acoustic-zero-linux-v16-stopwait-cycleB-1pct-20260817\external-mic-20260817-030929.wav`

SHA-256:

`087606C3AC0FB4E2FBE544CFA70D403B7A2F7507B780C0EB1D73323398479ABF`

At 1% endpoint and muted, steady diff-RMS was `1.9126e-3 / 2.3001e-3`, median **`2.10635e-3`**. That is:

- **115.4x Windows**;
- **3.11x v5**;
- **6.63x v13 cycle 1**;
- **0.536x v13 cycle 2**;
- **0.735x v15**.

Thus the Windows-style completion wait reduces the severe repeat-cycle failure substantially relative to v13 cycle 2 and v15, but remains decisively noisy.

## Decision

**Reject v16 as a parity candidate.** No additional mic cycle, program audio or chirp escalation was run.

The direction matters: the master-side post-stop ordering is a real contributor, not noise in the experiment. H03 should preserve this evidence while continuing down the master low-power boundary. The Windows wake path visibly performs resource votes, about a 5-ms delay, `SWRM_V2_0_CLK_CTRL=1`, clash clear, and link polling. Linux additionally pulses the LPASS AudioCC `swr_audio_cgcr` reset on every WSA SoundWire resume. That reset must not be removed until Windows's Resource Hub votes are classified well enough to determine whether they are its equivalent. The other still-proven difference is Windows's `SwrClockStopTimerMS=500` versus Linux qcom's 3000-ms autosuspend delay.

Machine-readable result: `artifacts/reviewed/2026-08-17-v16-soundwire-post-stop-wait-rejection.json`.
