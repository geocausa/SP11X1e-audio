# 2026-08-21 — Golden v32 rough Windows acoustic-parity estimate

Golden v32 is now close enough to current native Windows that the remaining
speaker difference is better described in **tenths of a dB over the useful
controlled band**, not in multi-dB broad-spectrum terms.

## Headline

**Rough engineering estimate: ~98% Windows speaker parity today, with a
reasonable 97–99% range for normal built-in-speaker use.**

This is deliberately **not** presented as a mathematical percentage of sound
quality. It is a project-status / engineering estimate combining digital state,
hardware lifecycle, repeatable controlled acoustics and real-program material.
The controlled 315 Hz+ evidence is substantially tighter than the percentage
headline; deep bass is the main uncertainty.

## Measurement environment / uncertainty

The SP7 RAW microphone fixture is used in an ordinary household environment,
not an anechoic room. Neighbours, cars, aircraft, floor movement and room noise
can change individual windows. Therefore this checkpoint does **not** score raw
whole-recording RMS as parity evidence.

Instead:

- SP7 capture gain is pinned at 0.000 dB and geometry is fixed;
- controlled tones use coherent fundamental extraction;
- two full matrix passes are compared;
- rows must repeat within <=1 dB on both OSes to enter the common comparison;
- real music uses source/digital fingerprint alignment;
- music band statistics trim the highest/lowest 10% of one-second windows to
  reduce household impulses;
- low-bass results are explicitly assigned lower confidence.

## Controlled consumer-matrix v3 — current Windows vs Golden v32

Source SHA256:
`ed983fb77f7f42ff4f593d75c981ad41e26f25eae7fd46d23c49a9867a8558fe`

Conditions: same source, 25% endpoint, SP7 RAW at 0 dB, two complete passes,
same-run physical/digital normalization.

On rows repeat-stable on **both** systems:

- **315 Hz and up:** ~`0.29 dB` mean absolute Windows-v32 level-law difference;
  bias ~`-0.04 dB`.
- **630 Hz and up:** ~`0.20 dB` mean absolute difference; bias ~`+0.02 dB`.

Golden v32's 0.05/0.20 coherent tone rows have roughly 20.6 dB minimum and
24.8 dB median local coherent SNR in this capture, so the useful-band result is
well above the household noise floor.

The old 2026-08-18 Windows-only ~`+7.7 dB` median expansion observation **does
not reproduce on current Windows** under the current matched two-pass /
repeat-gated acquisition. It is retained as a historical state/acquisition
observation, not a stable Windows behavior that Linux should chase.

## Seven Nation Army real-program check

Exact source:
`The White Stripes - Seven Nation Army (Official Music Video).mp3`

SHA256:
`951a65cc63fee17622485c1d94708614005524c7e20f86d3d815327f6bd0e8b3`

The same 19–49 s excerpt was used at a 25% endpoint. Windows had simultaneous
WASAPI loopback; Linux had the post-Dolby hardware-sink digital monitor; both
used the same SP7 RAW 0 dB microphone fixture.

Digital program drive is essentially overlaid: across 80 Hz–10 kHz, the maximum
absolute difference in median one-second band energy is only about `0.264 dB`.

After source fingerprint alignment and 10% high/low one-second trimming to
reduce household impulses, physical/digital transfer differs by:

- **315 Hz and up:** ~`0.34 dB` mean absolute, ~`-0.05 dB` bias, ~`0.56 dB`
  worst band;
- **630 Hz and up:** ~`0.28 dB` mean absolute, ~`0.49 dB` worst band.

The apparent residual rises below 315 Hz (~1.5 dB at 160–315 Hz and ~2.3 dB at
80–160 Hz). That region is both the weakest acoustic output of these small
speakers and the most vulnerable to household/room contamination. The Windows
music take also had materially lower fingerprint confidence than the Linux
capture. Therefore those low-bass numbers are **uncertainty**, not a justified
EQ or PA-gain correction.

## PA24 decision

Keep Golden v32 at **PA Volume 24**.

There is no current evidence that raising the PA control toward 31 improves
Windows parity. With real VI/CPS feedback active, PA24 already matches current
Windows within a few tenths of a dB over the controlled/useful band and remains
fault-free. Changing PA gain now would disturb a solved variable without a
measured benefit.

If a stronger sub-315-Hz claim is wanted later, repeat the low-bass matrix in a
quiet period or a more controlled acoustic fixture. Do not change Golden on the
basis of household-noise-limited low-frequency rows.

Reviewed machine-readable record:
[`artifacts/reviewed/2026-08-21-v32-windows-acoustic-parity-estimate.json`](../../artifacts/reviewed/2026-08-21-v32-windows-acoustic-parity-estimate.json)
