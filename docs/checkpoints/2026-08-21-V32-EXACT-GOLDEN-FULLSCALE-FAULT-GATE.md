# 2026-08-21 — v32 exact-Golden full-scale PA fault gate

The exact-Golden-derived v32 feedback candidate was tested after canonical native VI/CPS proof.

Diagnostic router was stopped first. Short 997-Hz bursts were played sequentially at:
- -12 dB
- -6 dB
- -3 dB
- 0 dB / reference full level

The test used the ordinary canonical topology and normal playback path.

Results:
- PA faults: 0 before, 0 after
- PA recoveries: 0 before, 0 after
- GLINK intent timeouts: 0
- total observed post-PA protection-clock enables/disables at completion: 19 / 19

No fault gate tripped at any level. This closes the earlier ghost/static fault-loop regression caused by enabling the same protection clocks too early from machine `prepare()`. The corrected implementation enables them only after both WSA8845 PAs report active.
