# CPS-Lab reviewed build evidence

This directory preserves the compact, reviewable inputs and inventory for the
rejected CPS-Lab topology. The runtime result and rejection are documented in
`docs/deployment/2026-08-10-audio-cps-lab-candidate.md`.

Tracked evidence:

| File | SHA-256 |
|---|---|
| `X1E80100-Microsoft-Surface-Pro-11-CPS-Lab-tplg.bin` | `f385a5d83127cf8f83dab0cbc86f418514f9c8839f2da6aac97e3e2ee782d121` |
| `cps-lab-topology.conf` | `53f14ca58a418bde1ebdb521fd791a2115e46fe1c543f25bfd2aa12d4bc914de` |
| `cps-lab-topology-inventory.json` | `455a00aca6277eb39f449c966d582630e90470ef41c62aa1e625282c379cd6ab` |
| `cps-lab-topology-inventory.md` | `e4dd4d806e3a8a49b88d5658bf1c4f1c6d2e8e757f3aeb839cb595e93abd5edd` |

The local `artifacts/audio-cps-lab-candidate-20260810/` directory is excluded
from Git. It contains a reproducible 147 MB initrd, rebuilt modules and a DTB.
Those generated files are intentionally not published as source evidence.
Their identities remain recorded in the deployment document:

| Generated file | SHA-256 |
|---|---|
| initrd | `f2663cceb9fd8b7ab380b075c9a57c72a4b7431ce3827fab62f50ebef1914d61` |
| DTB | `ab72a157824291baafa4e3b37af45819097c19a02f25f45b9bc47fa6145060d4` |
| `soundwire-qcom` | `b4524693a5458c5e672d248da6d77c8dad7abed6dcbec31e81c55225de65ae0` |
| `snd-soc-wsa884x` | `3106227bac14fb342eb6adf841f52c81e9b33e846ccfe4698e9753f39d8bf78b` |
| `snd-soc-lpass-wsa-macro` | `44e352cb610cf8fc122140efb02c25a4865707b709459cc22c58ea3f6db3bce3` |
| `snd-q6dsp-common` | `91c133cd030a23c7c075c480b19c2a77480570025f0616c85de60cd062e58b30` |
| `q6apm-lpass-dais` | `f842b8be5b78192597da6a1adbd2144895a0ce558c8dad3ab194addcea39f5b2` |
| `snd-soc-x1e80100` | `55766f4880eb0c4d36aecd0fbb16187e0c99a023eca8b6feb79f1e6565f20e36` |
