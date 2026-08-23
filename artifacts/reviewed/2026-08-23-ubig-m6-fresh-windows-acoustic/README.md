# UbiG M6 fresh Windows acoustic gate — reviewed evidence

Date: 2026-08-23

This directory contains the compact, public reviewed record for the final
objective M6 acoustic gate. Raw microphone and loopback WAVs are deliberately
not stored in Git; they are preserved locally under
`/home/geoca/Documents/SP11-PROJECT/00-RE-archive/ubig-m6-acoustic-20260823/`
with a self-verifying SHA-256 manifest.

The comparison is fresh-vs-fresh, not a reuse of the Aug-21 Windows capture:

- native Windows Dolby profile: Movie, Intelligent EQ Off;
- UbiG profile: Movie;
- deterministic consumer-matrix-v3 source SHA-256
  `ed983fb77f7f42ff4f593d75c981ad41e26f25eae7fd46d23c49a9867a8558fe`;
- 25% endpoint on both systems;
- SP7 RAW microphone at the same 0.000 dB capture gain;
- two complete passes per system;
- same-run digital normalization;
- rows are admitted only when repeat delta is <=1.0 dB on both systems.

Result on common repeat-stable normalized physical/digital-transfer rows:

- 315 Hz and up: 34 rows, 0.227030 dB MAE, -0.004343 dB bias;
- 630 Hz and up: 30 rows, 0.221240 dB MAE, +0.015177 dB bias;
- 0.05 -> 0.20 level-law comparison: 10 common stable pairs, 0.264778 dB
  MAE, 0.767633 dB maximum absolute difference.

This closes the objective physical acoustic matrix gate. It does **not** invent
an operator listening verdict. UbiG remains a disposable candidate until the
operator explicitly accepts the consecutive Windows/UbiG real-program A/B.
