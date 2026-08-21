# 2026-08-21 — exact-Golden v32 reboot and promotion gate

The exact-Golden-derived v32 feedback stack completed the remaining promotion gates.

## Long idle
The candidate remained booted for more than eight hours before final validation with:
- PA faults: 0
- PA recoveries: 0
- GLINK intent timeouts: 0

## Canonical lifecycle stress
Ten additional playback lifecycles (5 silence + 5 at -18 dB 997 Hz) completed with:
- PA faults: 0
- PA recoveries: 0
- protection-clock enables/disables: 10 / 10
- GLINK timeout delta: 0

## Canonical native VI/CPS proof
Without forced topology, native loggers repeatedly showed:
- VI tap2: 8 kHz, 640-byte payload, all frames nonzero and unique, Windows-range RMS
- CPS tap3: 24 kHz, 1920-byte payload, all frames nonzero, RMS ~449-452k versus Windows ~455k
- render tap1 nonzero

## Full-level acoustic fault gate
Short 997-Hz bursts at -12, -6, -3 and 0 dB all completed with:
- PA faults: 0
- PA recoveries: 0
- GLINK timeouts: 0

This closes the earlier ghost/static PA fault loop from the prematurely placed protection-clock candidate.

## Reboot gate
With DIAG router stopped and canonical topology active, v32 rebooted normally into Golden v31.
Previous v32 boot journal showed:
- GLINK intent timeout count: 0
- PA fault count: 0
- post-PA protection enable/disable count: 19 / 19
- normal shutdown/reboot handoff; no forced power cycle required

The multi-minute/failed reboot behavior is therefore isolated to the forced-TAP diagnostic environment, not the canonical v32 driver stack.

## Promotion decision
The exact v32 image satisfies the project promotion gate and is eligible to become the saved daily-driver entry. Golden v31 remains retained as an explicit fallback.
