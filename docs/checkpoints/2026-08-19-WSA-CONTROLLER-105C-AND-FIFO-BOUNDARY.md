# 2026-08-19 WSA controller 0x105c and slave FIFO boundary

## Status

This checkpoint extends commit `573ab6f` and the earlier protected-path handoff. It records the post-handoff Windows/Linux correlation work and closes several additional hypotheses. Golden v31 remains authoritative and unmodified.

## Decisive pre-existing boundary

- Native Windows DATA_LOGGING tap2 carries real 8 kHz VI PCM.
- Native Windows DATA_LOGGING tap3 carries real 24 kHz CPS PCM.
- Corrected Golden Linux tap2/tap3 can emit correctly formatted packets during acoustically proven render, but their PCM payloads remain zero.
- Therefore the remaining failure is sample delivery into the AudioReach `CODEC_DMA_SOURCE` path, not logger semantics or acoustic render.

## Trigger-order experiment closed

A disposable v31-derived candidate moved the protected frontend `GRAPH_START` behind WSA RX0 / VI TX0 / CPS TX1 backend starts using the correct AudioReach topology-created FE `trigger = POST` placement.

Result: Windows-like backend-before-protected-root start ordering was achieved, but forced tap2/tap3 still produced no real feedback PCM. Simple FE/BE start ordering is therefore closed as a root cause.

## Windows qcaucd master-register oracle

A single-owner KDNET session on SP7, using a relocation-safe symbolic breakpoint on `qcaucd8380+0x1bf80`, captured the following native Windows WSA-master MMIO writes:

- physical `0x06b1105c` <- `0x0005000f`
- physical `0x06b11d54` <- `0x00000003`

The first `0x105c` hit and the first `0x1d54` hit occur in the same higher WSA-owner flow but through distinct immediate qcaucd subpaths:

- `0x105c`: qcaucd `+0x3c6c0` below `+0x3f7a8`
- `0x1d54`: qcaucd `+0x3c094` below `+0x3f7a8`

The native qcaucd image used by SP11 Windows was verified byte-identical to the Ghidra image:

- SHA256 `BD0C8276C51FC7A020C616E904DD613B6CCF187EC3E1FE6F94C2C811C8ADC8BF`
- file version `1.0.0.10344`

Preserved Windows trace on SP7:

`C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11-Feedback-Boundary-20260819\pcm-route-105c_0490_2026-08-19_18-11-00-323.log`

Trace SHA256:

`F7FC8C39D44C2B8A41A42518C505CA2C3BD908535FDAC3C626895F76CDB087FD`

## Linux combined 0x105c + CPS PCM_CTRL experiment

Disposable candidate:

`/home/geoca/Documents/SP11-PROJECT/02-kernel/candidates/v31-cps-pcm-port-ctrl-105c-20260819`

Candidate `soundwire-qcom.ko`:

- srcversion `801511EA5B3957C10977AF5`
- SHA256 `3acb78e38c9a8bfda4f126afb87875879d811635ed015b58054896c9b03b0b5b`

Candidate initrd SHA256:

`9d478b8291bba4dd5deec9a20c628566510587a6f7f77e98d07b8d7b014f9f7c`

The candidate changed only the Denali WSA master path for physical CPS master port 13:

- controller-global `0x105c` -> `0x0005000f`
- DIN DP13 PCM control `0x1d54` -> `0x00000003`

The boot-time discriminator proved both were real Windows/Linux parity gaps:

- Linux `route_old=0x0`, then `route_new=0x5000f`
- Linux `pcm_old=0x0`, then `pcm_new=0x3`

No Golden source was promoted. The source file was restored byte-for-byte after building the disposable module.

## Decisive combined test result

With forced tap3 and an acoustically proven 997 Hz render:

- DIAG total packets: 274
- cmd16 audio packets: 273
- all audio packets identified as tap3
- payload length: 192 bytes per tap3 packet
- tap3 nonzero frames: **0 / 273**
- S32 median RMS: `0.0`
- S32 peak: `0`
- median nonzero payload bytes: `0`

Simultaneous external-microphone control showed the physical speakers were unquestionably rendering the test tone:

- channel 0 997 Hz increase: about `81.4 dB`
- channel 1 997 Hz increase: about `69.8 dB`

Conclusion: `0x105c=0x0005000f` and `0x1d54=0x3` are genuine Windows operations missing from Golden Linux, but they are **not sufficient** to attach real CPS samples to `CODEC_DMA_SOURCE`.

Do not reopen either value as a standalone root-cause hypothesis without new evidence.

## KDNET single-owner lesson

Multiple persistent PiMaster/KD terminal jobs from earlier turns were found on SP7. Two obsolete sessions were stopped. A stale/rebased qcaucd image also caused early breakpoint false negatives.

For all future Windows work:

1. Treat KDNET port 50005 as a single-owner resource.
2. Check/close old KD jobs before a new capture.
3. Use relocation-safe `bu qcaucd8380+RVA` breakpoints for unload/reload paths.
4. Validate the live qcaucd image/base before interpreting a missed breakpoint.

At checkpoint creation there is no `kd.exe` process left on SP7.

## Newly re-exposed next boundary: WSA slave command FIFO

The qcaucd SoundWire slave-write helper `FUN_14003ac60` packs direct slave transactions and sends them via logical register `0x5020`, physical WSA-master address:

`0x06b15020`

This is the slave-command FIFO used for WSA8845 register/dataport transactions.

Important capture-history nuance: one broad Windows master-MMIO capture filtered the range as `< 0x06b15000`, so it excluded `0x06b15020`. Separate older captures *did* explicitly trace this FIFO, including `CPS_DP6_SLAVES_20260810_2007BST...` and the Aug-14 full-FIFO review. Therefore the FIFO itself is not newly discovered, but it is now the correct remaining boundary after the master-side `0x105c/0x1d54` experiment failed.

## Current root-cause box

Known live/correct or closed:

- acoustic render path
- WSA codec producer operation broadly
- SoundWire DP5/DP6 geometry and extended transport programming
- VI/CPS BE prepare/enable
- LPASS backend generic graph starts
- protected-root graph start
- Windows-like FE/BE start order
- `WSA_CODEC_DMA_TX_0 -> AFE 0xb001`
- `WSA_CODEC_DMA_TX_1 -> AFE 0xb003`
- controller-global `0x105c=0x0005000f` alone/in combination
- CPS master-port `0x1d54=0x3` alone/in combination

Still missing:

**The first Windows WSA slave/dataport or closely coupled AFE/hardware-client operation that turns the clocked, packetized VI/CPS feedback path into real sample delivery to AudioReach `CODEC_DMA_SOURCE 0x4026/0x402b`.**

## Next work

Correlate the Windows `0x06b15020` slave-command sequence immediately surrounding WSA feedback activation against Golden Linux WSA8845/SoundWire slave traffic, with special attention to operations that are not merely the already-closed generic DP5/DP6 geometry or codec START sequence.

Only after one specific missing transaction is identified should another disposable v31-derived Linux candidate be built.

Promotion gate remains unchanged: real nonzero tap2 8 kHz VI PCM and tap3 24 kHz CPS PCM during acoustically proven render, with no faults.

## Safety state at checkpoint creation

Golden boot restored:

- `saved_entry=sp11-audio-golden-v31`
- `next_entry=` empty
- canonical topology SHA256 `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`
