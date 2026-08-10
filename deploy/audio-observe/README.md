# SP11 read-only protection observation candidate

This candidate reuses the accepted `7.1.5-sp11-audio-clean+` kernel, Phase91
DTB, full 107-frame protected topology, module ABI, and signing trust. Its only
replacement is `snd-q6apm`, embedded in a dedicated initramfs and force-loaded
before `switch_root`.

The module parameter
`/sys/module/snd_q6apm/parameters/sp11_protection_probe` defaults to zero. A
value from 1 through 6 arms exactly one GET_CFG request on the next protected
graph start and then clears itself:

1. SP library version
2. SP feature statistics
3. per-speaker TMax/XMax
4. SPVI per-speaker condition
5. CPS battery/die-temperature/gain statistics
6. thermal coil-resistance/temperature/gain statistics

Probe rejection is logged but never returned as a playback failure. No event
is registered, no calibration is sent, no topology frame is removed, no GPIO
ownership changes, and no codec register range is scanned.
