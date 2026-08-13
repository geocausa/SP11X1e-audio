# SP11 WSA884x cold-start double regcache replay — 2026-08-13

## Status

The abnormal delay before first speaker playback is now deterministically localized. It is not a YouTube/network delay and it is not primarily AudioReach graph programming. A cold local playback request with the speaker SoundWire path runtime-suspended takes about 18.5 seconds to reach ALSA `RUNNING` because each of the two WSA884x codecs performs two full dirty-regcache replays during one ordinary stream open.

This finding is about startup/lifecycle latency. It does **not** by itself explain the separate warm in-stream seek/volume transient, because the four long cache replays occur on cold speaker-path wake rather than on every warm volume change. It is nevertheless a material Linux lifecycle mismatch and may be relevant to start/re-entry behavior.

## Deterministic live reproduction

The user-facing endpoint was muted for the timing tests so speaker output was not required. Before the baseline run:

- `sdw-master-1-0`: runtime `suspended`, `power/control=auto`;
- WSA left `sdw:1:0:0217:0204:00:0`: runtime `suspended`, `power/control=auto`;
- WSA right `sdw:1:0:0217:0204:00:1`: runtime `suspended`, `power/control=auto`.

A local `pw-play` of the existing controlled tonal WAV took:

- **18.627 s** from client launch to `/proc/asound/card0/pcm0p/sub0/status` reporting `state: RUNNING`.

A second normal-PM run under function tracing took **18.464 s**, confirming the delay is repeatable and independent of YouTube.

## Runtime-PM discriminator

As a reversible discriminator only, the SoundWire manager and both WSA slaves were temporarily pinned `power/control=on`, allowed to become active, and the same muted stream was opened. Time to `RUNNING` fell to:

- **7.310 s**.

All three PM controls were then restored to `auto`.

This proves runtime autosuspend contributes materially to the latency, but does not account for all of it. Even while pinned active, the subsequent speaker-stream start still produced a SoundWire reattach/reinitialization sequence; the right codec completed before the left, and the left again transiently reported `VPHX_SYS_EN_STATUS=0x0` before graph start.

## Function-graph localization

A function-graph trace filtered to `regcache_sync()` during a normal cold muted run showed four WSA-scale calls of approximately:

- **4.598 s**;
- **4.602 s**;
- **4.278 s**;
- **4.292 s**.

Unrelated `regcache_sync()` calls in the VA/WSA macro path were only roughly 65–118 microseconds and are not material.

The four multi-second calls account for essentially the entire ~18.5-second cold-open delay.

A second function trace with stack capture proved the call sites:

1. `wsa884x_runtime_resume -> regcache_sync` for one codec;
2. `wsa884x_runtime_resume -> regcache_sync` for the other codec;
3. SoundWire IRQ -> `sdw_handle_slave_status -> sdw_update_slave_status -> wsa884x_update_status -> regcache_sync` for one codec;
4. the same attach-status path for the other codec.

Therefore one ordinary cold speaker open performs **two full cache replays per amplifier**.

## Source mechanism

The deployed-source lineage configures WSA884x with `REGCACHE_MAPLE`. The regmap defaults table contains about 390 register entries, while the explicit codec initialization table is much smaller (about 50 entries).

The relevant lifecycle is:

```c
static int wsa884x_runtime_suspend(struct device *dev)
{
    ...
    regcache_cache_only(regmap, true);
    regcache_mark_dirty(regmap);
    return 0;
}

static int wsa884x_runtime_resume(struct device *dev)
{
    ...
    regcache_cache_only(regmap, false);
    regcache_sync(regmap);
    return 0;
}
```

Separately, the SoundWire status callback does this on a normal fall-off/reattach sequence:

```c
if (status == SDW_SLAVE_UNATTACHED) {
    wsa884x->hw_init = false;
    regcache_cache_only(wsa884x->regmap, true);
    regcache_mark_dirty(wsa884x->regmap);
    return 0;
}
...
regcache_cache_only(wsa884x->regmap, false);
regcache_sync(wsa884x->regmap);
wsa884x_init(wsa884x);
```

The runtime-resume replay and the attach-status replay are thus both present in the same stream-open lifecycle.

The WSA supplies and reset line are not runtime-power-cycled by these callbacks: supplies are enabled at probe and disabled only by the devm teardown action; reset is deasserted at probe and powered down only on removal. The codec also advertises `simple_clk_stop_capable=true`. This makes a blanket full dirty-cache replay on every ordinary clock-stop wake especially suspicious, and the duplicate replay is indisputably redundant from the observed call sequence.

## VPHX log-order correction

Earlier logs appeared to show several seconds being spent reading the VPHX supply state because the `detected VPHX ...` / `unsupported VPHX ...` messages were separated by multi-second gaps. Source review corrects that interpretation: the VPHX read/log occurs inside `wsa884x_init()` **after** the attach callback's long `regcache_sync()`.

The multi-second gap is therefore dominated by the cache replay, not by the single VPHX status read itself. The occasional left-codec `VPHX=0x0` result remains a separate wake/state-quality issue worth retaining, but it is not the main timing mechanism.

## SoundWire side

The Qualcomm SoundWire manager uses normal clock-stop suspend/resume for this controller. Its relevant waits are bounded in the 100–300 ms range, not seconds. The live WSA nodes advertise simple clock-stop capability. This is consistent with the trace: the seconds are consumed in codec regcache replay, not manager timeout constants.

## Safety / restoration

All diagnostic changes were temporary:

- speaker endpoint restored to 20% and unmuted;
- Dolby and MSIIR volume-sync services remain active;
- no playback client left running;
- SoundWire manager and both WSA slaves restored to `power/control=auto`;
- ftrace restored to `nop`, tracing disabled, stack tracing disabled;
- no kernel module was replaced or installed.

## Next implementation gate

Build an **isolated**, no-install candidate that removes redundant full-cache replay from the ordinary clock-stop/reattach path while preserving a full restore path for genuine codec context loss. Static compile and source-level lifecycle tests must precede any boot/install test.

A safe fix must distinguish at least:

- ordinary SoundWire Mode-0/simple-clock-stop wake, where supplies/reset remain resident;
- genuine context loss/reset/unbind, where a full hardware restore is required;
- cached control writes made while the codec is suspended, which still need to reach hardware exactly once.

Do not simply delete all `regcache_sync()` calls or clear dirty state unconditionally.

## Isolated candidate A — static gate

A conservative first candidate was built without touching the live module. It removes only the unconditional `regcache_mark_dirty()` from `wsa884x_runtime_suspend()`.

This is evidence-backed by the regmap contract: `regcache_mark_dirty()` is intended when hardware registers were reset or power/context was lost. Cache-only writes already set cache dirty state and are flushed by the later `regcache_sync()`. SP11's ordinary WSA runtime suspend does not disable supplies or assert reset, and the slave advertises simple clock-stop support.

The attach-side `UNATTACHED -> ATTACHED` path deliberately retains its existing `regcache_mark_dirty()` plus full restore as a conservative context-loss fallback. Candidate A therefore targets only the first of the two full replays per amplifier proven by the stack trace; it does not assume all reattach restores are unnecessary.

Artifacts:

- patch: `patches/0046-ASoC-wsa884x-avoid-full-cache-dirty-on-clock-stop.patch`;
- patch SHA-256: `b653431eb855d7dccc709841f2104fb20a519f15a7756732dac2ae2e37ae4ba2`;
- strict checkpatch: 0 errors, 0 warnings, 0 checks;
- exact-release candidate module SHA-256: `4ccf7565dd4e8457d61b3482c45cf4605305128d4ff1ad8b02a9665236442688`;
- candidate srcversion: `B7F5D7D97DD31C77EFB6F01`;
- candidate vermagic: `7.1.5-sp11-cps-v3+ SMP preempt mod_unload modversions aarch64`.

The candidate is unsigned while the installed module is signed by the build-time autogenerated kernel key. It has **not** been installed or loaded. The next live gate is a signed, isolated boot candidate and the same muted cold-start timing/ftrace test; ordinary production state must remain untouched until then.
