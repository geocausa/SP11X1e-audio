# 40 Hz real-key Volume-Up microtransient localization

Date: 2026-08-18
Status: **physical defect reproduced and strongly localized; root actuator still under investigation**

## Operator localization

The operator found a tiny crackle under abusive keyboard-volume stress and
reduced it to a deterministic case:

- strongest on low-frequency program material;
- one crackle per Volume-Up step;
- Volume-Down produces a much smaller/different artifact;
- effect falls toward zero as stimulus frequency rises;
- a steady 40 Hz sine makes the defect easy to reproduce;
- perceived click ceiling is not strongly coupled to ordinary listening level.

## Old-geometry physical proof

With SP11/SP7 side-by-side, a fully warmed 40 Hz real-media-key sweep produced:

- DOWN HP500 p95 about `6.61e-5`;
- UP HP500 p95 about `3.26e-3`;
- UP/DOWN p95 about `49.35x`.

A second A/B with the redundant per-event hidden-sink unity write suppressed
still produced about `41.82x` UP/DOWN p95.  Therefore that redundant write can
aggravate the edge but is not the root cause.

Old-geometry checkpoint SHA-256:
`e311af5647c15ede852dc5877d455bb456b1e920d549ec3d5bc15aa39f7b8da4`.

## New fixed-geometry bridge

The SP7 microphone was then moved to the fixed keyboard-length fixture described
in `docs/baseline/2026-08-18-SP7-ACOUSTIC-FIXTURE-KEYBOARD-LENGTH.md`.

Current live policy during this capture:

- v30 candidate boot;
- Windows-matched GNOME media-key step = 2%;
- exact DSP mute path enabled;
- redundant per-event hidden-sink unity write suppressed as an A/B;
- warm 40 Hz source at -36 dBFS;
- real XF86 Volume keys;
- sweep order DOWN 46->6%, UP 6->46%, DOWN 46->6%.

SP7 capture:

- path:
  `C:\Users\SurfacePro7\Documents\KDNET\Codex\acoustic-reference-keyboard-length-20260818\lf40-bridge\external-mic-20260818-092959.wav`
- SHA-256:
  `455BD642B91F7B7E61F60D5DBBF431F9A362C444ABCF9A05D6EC6CA35F2C65EB`

New-geometry result:

- combined DOWN HP500 p95 `6.0663158e-5`;
- UP HP500 p95 `2.7855235e-3`;
- UP/DOWN HP500 p95 `45.92x`;
- DOWN HP2k p95 `4.978438e-5`;
- UP HP2k p95 `1.683309e-3`;
- UP/DOWN HP2k p95 `33.81x`;
- DOWN HP6k p95 `3.820154e-5`;
- UP HP6k p95 `1.019966e-4`;
- UP/DOWN HP6k p95 `2.67x`.

The fixed geometry therefore validates the old-position directional finding; it
is not a microphone-placement illusion.

## Layers already ruled out or narrowed

### GNOME notification sound

Real hardware-key-path monitoring proves `audio-volume-change.oga` is produced
while idle but suppressed while continuous media is RUNNING, even under key
spam.  The 40 Hz defect is not a leaked notification chime during continuous
playback.

### Digital Dolby / normal PCM path

Synchronized 40 Hz captures at pre-Dolby, post-Dolby and the PipeWire hardware
boundary remain symmetric/clean across control edges.  The physical one-sided
UP impulse has no corresponding digital click in those taps.  The Dolby port
can affect LF energy delivered downstream, but it is not directly emitting the
observed click.

### WSA8845 per-volume programming

A WSA8845 `_regmap_write()` trace recorded zero codec-register writes during the
actual UP and DOWN sweeps.  Only normal stream START/STOP lifecycle writes occur
outside those control windows.  The artifact is therefore not a Linux codec
register being rewritten on each key press.

### Missing final VOL_CTRL ramp policy

The current v30 topology SHA-256 matches the recovered Windows ramp topology:
`1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`.
Final `VOL_CTRL 0x4a63 / 0x08001037` is already configured with the recovered
Windows tuple: 10 ms period, 1000 us step, curve 3.

### Raw final-volume/GainStep transaction alone

With the production volume daemon stopped and the hidden hardware sink held at
unity, direct Windows-style final `0x1038` + GainStep transactions at 38->40%
were physically at the SP7 mic floor across several L->R spacing variants.
Thus the raw DSP transaction by itself is not sufficient to reproduce the
real-key defect.

## Current inference and next oracle

The remaining difference is associated with the complete live desktop
control path rather than ordinary PCM generation, Dolby output samples,
per-step WSA register programming, or absence of the recovered ramp policy.
The next decisive comparison is native Windows using the same fixed geometry,
40 Hz stimulus, 2% key step and warm DOWN/UP ordering.  Only after that should
the remaining Linux control-path delta be changed.
