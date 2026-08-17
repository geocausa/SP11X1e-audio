# SP11 SoundWire 500 ms timing isolation — v18/v19

## Question
Windows qcaucd delays ordinary WSA SoundWire clock stop by about 500 ms. Is Linux's much longer effective clock-stop delay the cause of the repeat-wake noise?

## v18: master timer only
v18 retained the v16 Windows-style `STOP_NOW -> COMP_STATUS[0] clear -> hclk gate` completion wait and changed only the SP11 WSA master (`6b10000.soundwire`) autosuspend from 3000 ms to 500 ms. The master sysfs value was 500 ms, but the physical stop still occurred about 3.406 s after the last stream deprepare. Both WSA8845 child devices still had 3000 ms autosuspend and dominated the dependency chain.

## v19: coherent child/master timing
v19 kept master=500 ms and made the two SP11 WSA8845 child autosuspend delays 0 ms. This produced a measured last-deprepare to qcom master runtime-suspend interval of **0.626742 s**, close to Windows live qcaucd's roughly **0.485–0.499 s** delayed stop family.

The subsequent 1% muted digital-zero PA wake was still catastrophically noisy: median steady diff-RMS **0.0028105668363381793**, about **154x Windows** and **4.15x v5**.

## Conclusion
The Linux/Windows SoundWire timing mismatch was real and is now experimentally reproduced, but it is **not sufficient** to explain H03. Do not spend further acoustic cycles tuning only the 500 ms timer. Move back to coherent amplifier/producer state and ordering.
