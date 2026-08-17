# SP11 SoundWire 500 ms clock-stop isolation — v18/v19 closure

## Question

Windows qcaucd uses Surface policy `SwrClockStopTimerMS=500` and live WPP shows the WSA SoundWire clock-stop callback at about 0.485--0.499 s after the render stream closes. Could Linux's longer SoundWire runtime-PM delay be what destroys the quiet v13 WSA8845 state between speaker cycles?

## v18: master timer only

v18 is exact v16 plus a WSA-controller-only qcom SoundWire autosuspend override of 500 ms. The WSA8845 slave devices remained at their normal 3000 ms autosuspend delay.

The driver and sysfs both reported `500`, but the physical stop did not occur at 500 ms. The last `sdw_deprepare_stream()` in the timing trace was at 106.912424 s and the kernel logged frame-generator stop at 110.318061 s, about **3.406 s later**. The two WSA8845 children still had 3000 ms autosuspend delays and dominated the parent transition.

Therefore v18 is a structural intermediate, **not a valid Windows-timing acoustic test**. No acoustic conclusion is attributed to it.

Reviewed trace SHA-256: `b632db8c493de475827f2a654126da07f10dac2241134288e3035195c765c81a`.

## v19: coherent child/parent timing

v19 combines:

- exact v13 amplifier/producer state;
- v16's bounded `CLOCK_STOP_NOW -> COMP_STATUS[0] clear -> hclk gate` wait;
- WSA SoundWire master autosuspend 500 ms;
- both WSA8845 slave autosuspends 0 ms;
- the normal AudioCC WSA CGCR reset pulse retained.

This finally changed the physical boundary. Last deprepare was 99.629622 s and qcom master runtime suspend began at 100.256364 s: **0.626742 s**. That is the same timing family as live Windows rather than Linux's old multi-second transition.

Reviewed timing trace SHA-256: `7e661ac76ac83b2f7af42eb8174c9f139a9bde0c54429352fdf58b3477ffd5c6`.

The subsequent 1% muted digital-zero acoustic cycle nevertheless measured median steady diff-RMS **2.810566836e-3**. That is about **154x Windows** and **4.15x v5**. Capture SHA-256: `7A84796028227A680A5449209823D64BF2B56985C31686AF553725ADA3255352`.

## Decision

**Close the 500 ms SoundWire timing hypothesis as insufficient.** Linux can reproduce the Windows clock-stop timing family and the Windows-style frame-generator completion wait, yet the amplifier remains in the bad repeat-cycle state.

Do not spend additional acoustic cycles tuning 500 ms versus nearby timer values. H03 should stay focused on amplifier/producer state ordering across the speaker cycle.
