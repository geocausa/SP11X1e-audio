# 2026-08-20 — Post-PA PROTCLK + active Offset2 yields clean stimulus-dependent VI

## Result

Native Windows cold-start KDNET ordering proved:

1. `WSA_START_OWNER`;
2. both WSA8845 `GLOBAL_PA_EN=1` writes;
3. qcaucd protection-clock resource kind 5;
4. qcaucd protection-clock resource kind 6.

The earlier Linux PROTCLK-in-machine-prepare candidate was therefore too early and caused repeated WSA8845 PA recovery / audible ghost-static. A new disposable candidate moved ownership to the correct hardware boundary: each WSA8845 reports its completed PA-up transition; the WSA macro enables its existing VI-sense TX primitive only after both PAs are up. Teardown disables the protection TX clocks before the first PA stop sequence.

Candidate modules:

- `snd_soc_lpass_wsa_macro` srcversion `F32C7A03F713D1B20F0BF78`;
- `snd_soc_wsa884x` srcversion `4CAF1D70524C80B0B43C50D`.

The candidate is combined with the independently Windows-proven SoundWire active Offset2=0 module:

- `soundwire_qcom` srcversion `CE1DADE19E1CE61B7FC8843`.

Golden q6apm remains `687B16CF9C43B43E90C0746`.

## Safety / PA-fault gate

On the combined post-PA + Offset2 candidate:

- 5 s digital-silence render: 0 PA faults, 0 recoveries;
- 6 s -18 dBFS 997-Hz render: 0 PA faults, 0 recoveries;
- one `SP11POSTPA` enable and one disable per render.

This closes the audible ghost/static regression at reduced stimulus level: the fault was lifecycle ordering, not Offset2 itself.

## Forced TAP2 / VI acceptance

Disposable initramfs topology selector was used; real-root topology stayed canonical. Forced TAP2 topology SHA256:

`b5e4331b79957837d3625867e0bfa81709f4f1e8c3eab3336613888ff905d624`

The running boot used:

- post-PA WSA macro `F32C7A03F713D1B20F0BF78`;
- post-PA WSA8845 `4CAF1D70524C80B0B43C50D`;
- active Offset2 SoundWire `CE1DADE19E1CE61B7FC8843`;
- Golden q6apm `687B16CF9C43B43E90C0746`.

Forced logger output was exactly tap ID 2 / 8000 Hz / 64-byte payload.

Digital silence:

- 69 tap2 frames;
- 69/69 unique payloads;
- sentinel (`-32768`) ratio 0.0;
- mean absolute packed-S16 magnitude ~184.92;
- median ~178;
- peak 891.

-18 dBFS 997-Hz stimulus:

- 82 tap2 frames;
- 82/82 unique payloads;
- sentinel ratio 0.0;
- mean absolute packed-S16 magnitude ~275.45;
- median ~209;
- peak 1791.

Multiple lane positions rise strongly under tone, for example approximate mean absolute values:

- lane 1: 215.86 -> 355.88;
- lane 5: 211.71 -> 390.87;
- lane 7: 203.23 -> 441.94;
- lane 9: 216.33 -> 449.27;
- lane 13: 203.51 -> 442.07.

This is qualitatively cleaner than the early-PROTCLK candidate: no alternating `0x8000` stale half-period, no near-full-scale garbage, and no PA-recovery loop.

Reviewed data:

`artifacts/reviewed/2026-08-20-postpa-offset2-tap2-clean-vi.json`

## Decision

Treat VI / `CODEC_DMA_SOURCE 0x4026` as functionally restored on the disposable post-PA + active-Offset2 stack, pending final production cleanup and broader acoustic validation. Do not promote yet because CPS remains unresolved and full-scale 997-Hz on post-PA-only had produced PA faults; current clean acceptance deliberately uses silence and -18 dBFS.

Next experiment: keep this exact post-PA + Offset2 base and add only the already-proven CPS packetization/wake delta (`0x105c=0x0005000f` plus DP13 `0x1d54=3`), then run forced TAP3 at reduced stimulus level. Persistent GRUB saved entry remains Golden v31.
