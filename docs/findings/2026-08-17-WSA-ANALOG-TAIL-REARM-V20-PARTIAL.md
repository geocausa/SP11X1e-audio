# SP11 WSA8845 exact analog-tail re-arm — v20 partial success

v13 established the recovered Windows 20-write analog tail once during codec initialization. Its first muted-zero cycle was unusually good (~3.175e-4 diff-RMS), but the second cycle collapsed to ~3.927e-3.

v20 replayed the **same audited 20 writes** before every PA wake at speaker POST_PMU. Disassembly proved exactly 20 `regmap_write()` calls with the recovered sequence and no register/value additions.

Repeat cycles improved to **8.03e-4** and **6.12e-4**, preventing the old v13 multi-millithreshold collapse. The full write trace also proved why v20 was not an exact cold-init reconstruction: the replay happened only after the main SoundWire stream had already been prepared/enabled and after WSA-macro interpolator startup.

Thus the analog-tail state family matters, but **lifecycle placement matters too**. This directly motivated v21: same writes, moved before SoundWire stream construction.
