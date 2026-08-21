# Golden v32 — VI+CPS feedback parity

Golden v32 is the promoted SP11 Linux built-in-speaker baseline as of 2026-08-21. It keeps the Golden v31 kernel, DTB, q6apm and canonical topology, and adds only the three feedback-path deltas proven against native Windows.

## Added over Golden v31

- WSA protection TX clocks start only after both WSA8845 power amplifiers are active.
- Active SoundWire feedback ports 10/11/13 use Windows `Offset2=0`.
- CPS packetization/wake is enabled with controller `0x105c=0x0005000f` and DP13 `PCM_CTRL 0x1d54=3`.

## Acceptance

Canonical topology (no forced diagnostic topology) repeatedly produces:

- VI/TAP2: 8 kHz, native 640-byte payload, nonzero/unique, Windows-range magnitude.
- CPS/TAP3: 24 kHz, native 1920-byte payload, nonzero, about 449–452k RMS versus native Windows about 455k.
- zero PA faults/recoveries through repeated silence/tone cycles and a -12/-6/-3/0 dB 997-Hz staircase.
- zero canonical-runtime GLINK timeouts and clean v32→v32 reboot.

Run `./verify-golden-v32.sh` on the deployed SP11. `install-grub-entry.sh` recreates the hash-pinned GRUB entry and sets v32 as the saved default without rebooting.

Golden v31 is intentionally retained as the fixed-initrd fallback. Forced TAP2/TAP3 topology boots are diagnostic-only because they can stall ADSP/GLINK teardown at reboot; v32 does not need them for normal feedback operation.
