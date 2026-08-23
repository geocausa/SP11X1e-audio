# F06 — UbiG fresh Windows physical acoustic gate

Date: 2026-08-23
Status: **objective F06 acoustic matrix CLOSED; operator promotion verdict pending**

The final objective UbiG M6 acoustic comparison used a fresh native-Windows
capture in the same current SP7 microphone geometry as UbiG. Both systems used
Movie profile, 25% endpoint, the exact deterministic consumer-matrix-v3 source,
two passes, SP7 RAW at 0.000 dB and same-run digital normalization.

Common repeat-stable normalized physical/digital transfer differs by:

- 315 Hz+: 0.227030 dB MAE, -0.004343 dB bias, 34 rows;
- 630 Hz+: 0.221240 dB MAE, +0.015177 dB bias, 30 rows;
- 0.05 -> 0.20 level law: 0.264778 dB MAE across 10 stable pairs.

Full public row evidence is on `ubig/deblob-main` at UbiG commit `e128acd` under
`artifacts/reviewed/2026-08-23-ubig-m6-fresh-windows-acoustic/`. Raw WAVs and
loopbacks are preserved locally under
`/home/geoca/Documents/SP11-PROJECT/00-RE-archive/ubig-m6-acoustic-20260823/`
with a self-verifying SHA-256 manifest.

This closes the audit's objective F06 finding. UbiG itself remains unpromoted
and the Windows bridge remains available until the operator explicitly accepts
the subjective consecutive Windows/UbiG music A/B.
