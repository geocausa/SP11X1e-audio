# 2026-08-20 — POST-PA PROTCLK + active Offset2 + CPS wake restores CPS tap3

## Scope
Disposable Golden-v31 derivative only. Persistent GRUB default remained `sp11-audio-golden-v31` and `next_entry` was consumed automatically.

## Stack
- WSA protection TX clocks enabled only after both WSA8845 PAs report successful PA-up.
- SoundWire active feedback Offset2 forced to the native-Windows value 0 for master ports 10/11/13.
- CPS controller wake/packetization pair enabled: controller `0x105c=0x0005000f`, DP13 PCM control `0x1d54=0x3`.
- Forced TAP3 topology SHA256 `e81a6deb919240d20c0479f64bbd0e8e1204673c3a899e0d9ba2464d158eb42a` through the validated `/run/sp11-fw` initramfs override; real-root topology remained canonical.

Loaded module identities:
- `snd_soc_lpass_wsa_macro`: `F32C7A03F713D1B20F0BF78`
- `snd_soc_wsa884x`: `4CAF1D70524C80B0B43C50D`
- `soundwire_qcom`: `D008A3D6B585C11BE023992`
- q6apm remained Golden `687B16CF9C43B43E90C0746`.

## Safety gate
Both digital-silence and -18 dB 997-Hz renders completed with:
- 0 PA faults
- 0 PA recoveries
- one POST-PA PROTCLK enable/disable lifecycle per render.

This is materially different from the earlier machine-prepare PROTCLK candidate, which produced repeated `ERR_COND0=0x20` PA recovery and audible ghost/static.

## TAP3 result
Silence capture:
- 101 tap3 frames, 24 kHz, 192-byte PCM payload per forced logger frame
- 99/101 nonzero
- median S32 RMS ~462,635

-18 dB 997-Hz capture:
- 162 tap3 frames, 24 kHz, 192-byte PCM payload per forced logger frame
- 161/162 nonzero
- median S32 RMS ~462,635

Native-Windows reference from `2026-08-19-PROTECTED-PATH-HANDOFF.md`:
- tap3 = 24 kHz CPS
- 38/38 nonzero
- 1920-byte logger payload (10 ms framing versus the forced Linux 1 ms framing)
- median RMS ~455,456

The Linux CPS magnitude/format now closely matches the native-Windows CPS stream, whereas the historical `0x105c + 0x1d54` candidate produced hundreds of correctly framed tap3 periods with byte-for-byte zero payload.

## Interpretation
The zero CPS ring is closed by the interaction of three independently evidenced Windows parity operations:
1. WSA protection TX clocks, placed at the correct post-PA lifecycle boundary;
2. active SoundWire Offset2=0;
3. CPS `0x105c + DP13 0x1d54` wake/packetization state.

Alongside the clean forced-TAP2 result already checkpointed in `2026-08-20-POSTPA-OFFSET2-CLEAN-VI.md`, both CODEC_DMA_SOURCE feedback branches now deliver nonzero data without PA fault recovery at reduced-level stimulus.

## Remaining promotion gates
Do not promote yet. Before Golden replacement:
- consolidate the three deltas without diagnostic logging/forced topology;
- boot canonical topology;
- verify normal speaker playback and teardown across repeats;
- verify VI and CPS remain live using temporary logger instrumentation or isolated diagnostic boot;
- verify no PA faults/ghost/static over representative volume levels;
- preserve Golden v31 as rollback until the consolidated candidate passes all gates.
