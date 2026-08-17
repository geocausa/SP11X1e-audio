# SP11 WSA8845 analog-tail pre-SoundWire re-arm — v21

## Isolation
v21 changes only the placement of the already-audited v13 20-write Windows analog tail. The existing `wsa884x_apply_sp11_windows_analog_tail()` is called from the main speaker `hw_params` before `sdw_stream_add_slave()`. There is no POST_PMU replay and no new register/value state.

Executable audit proves the helper still contains exactly 20 direct writes. Runtime trace proves:

`wsa884x_hw_params -> analog tail -> sdw_stream_add_slave -> sdw_prepare -> sdw_enable`

For the two physical amps, replay preceded `add_slave` by roughly 0.6 ms; the first stream prepare occurred only after both amp additions.

## Acoustic result
A strictly gated repeat capture held PCM closed for the entire 10-second baseline with replay marker count unchanged, then the deliberate 1% muted zero wake produced exactly two replay markers and physical PCM RUNNING. Its median steady diff-RMS was **0.00035726712135182375**:

- ~0.53x v5;
- ~1.13x v13's good cold first cycle;
- ~0.091x v13's catastrophic second cycle;
- still ~19.6x Windows.

A separate 30-second zero hold, bound to the actual kernel replay timestamp rather than an acoustic onset heuristic, showed a sustained median active diff-RMS **0.0004980538799985231** (q25 **0.0003934248791836541**, q10 **0.00026267394431712124**). The remaining gap is therefore genuine sustained hardware noise.

## Next isolation
Do not reopen full-register DRE1 zero or the rejected watchdog-latched path. Compare the Windows pre-tail state families against Linux's explicit cold-init writes. The DSM coefficient block is a strong safe next target: Linux already writes most Windows values but omits explicit A4_1/A5_1 writes and reaches C_2=0xf2 via a later correction rather than the Windows coherent DSM burst.
