# Windows seek KDNET: no host qcad control transaction

## Result

Fresh SP11 Windows KDNET on 2026-08-15 closes the remaining host-control question around warm in-stream seeks.

With the same local `Seven Nation Army` MP3 playing steadily at a fixed 25% endpoint setting, three deterministic position jumps were executed while classic Windows Kits KD on SP7 trapped:

- qcadcm8380 `SetVolume` at RVA `0x6e038`;
- `GetGraphCkv` at RVA `0x91888`;
- the established SET_CFG boundary at RVA `0x60b78`, filtered to iids `0x4663`, `0x4664`, `0x4669`, `0x466b`, `0x4a63`, `0x489e`, `0x48a1`, and `0x412b`.

The seek-only capture produced **zero runtime breakpoint hits** across all three seeks. The clean raw KD log SHA-256 is:

`b53f54ee731c9b0a14df1acc102e3e82e468f99cb2d0b7611e5f7f7236e7572a`

A separate positive control left the same breakpoints armed and changed endpoint volume `25% -> 17% -> 25%`. It produced 16 runtime markers including `SetVolume`, final `0x4a63 / 0x08001038` bodies and `GetGraphCkv`. Positive-control log SHA-256:

`6f89140d2a42da7706bd12831faa3fec7d76f0067d16b15f8b80a114ae206a29`

Therefore the seek non-hit is a real negative result, not a bad address, disabled breakpoint or filter error.

## Consequence

Windows does **not** issue a seek-specific host qcad transaction for:

- final endpoint volume;
- GainStep / MSIIR selection;
- SOFT_PAUSE;
- POPLESS_EQ;
- the adjacent render VOL_CTRL blocks;
- SPR session-time state.

The physical smoothing heard on Windows must therefore be autonomous behavior of the already-configured DSP graph and/or a property of the PCM discontinuity presented to that graph.

This is consistent with the existing Windows DEFAULT graph evidence: POPLESS_EQ iid `0x4664` is enabled with the exact five-band flat calibration and is control-linked to VOL_CTRL iid `0x4663` through `INTENT_ID_P_EQ_VOL_HEADROOM (0x08001118)`. Linux now carries the same calibration, link and surrounding ramp policy.

## Synchronized local-file loopback discriminator

A same-process Windows WASAPI-loopback capture was also made so recorder/player timing could not drift across PiMaster calls. The exact MP3 source remained:

`951a65cc63fee17622485c1d94708614005524c7e20f86d3d815327f6bd0e8b3`

Windows synchronized loopback WAV SHA-256:

`1c812d500a8e340ce5cb5b8e6f0db41c6337fed8d78dc2a4bf781de63fa64381`

For this Windows MediaPlayer case, no signal-surrounded exact-zero run occurs at the three seeks; 5 ms energy briefly falls to roughly `-68`, `-91`, and `-86 dBFS` before returning. That behavior must **not** be generalized into a Linux speaker-path fade: the already-reviewed real Edge/YouTube Windows capture reaches about `0.987` full-scale on post-seek loopback re-entry while the physical Windows speakers remain smoothly capped. The local MediaPlayer dip is therefore application/source-path evidence, not a universal endpoint actuator.

## Fresh corrected Linux seek

The corrected `sp11-audio-volume-channel-order` candidate was tested at the same 25% endpoint / VLLDP generation state. It performed three flush/accurate local-file seeks with:

- no volume transaction during the seeks;
- no APM, XRUN, SoundWire, WSA or PA fault;
- every obtained WSA8845 sample PA-enabled, current-limit code 17 / register `0x44`, and zero PA errors.

The post-Dolby/pre-AudioReach capture contains exact-zero source gaps of about `74.667 ms`, `10.667 ms`, and `80.0 ms`; this is source-pipeline behavior, not evidence for a missing qcad seek command.

The historical physical seek RED was recorded before the pre-Dolby volume-boundary, VLLDP-lifecycle and per-channel final-volume defects were corrected. The current technical state is therefore structural GREEN / physical-validation AMBER: no additional guessed seek transaction or host fade is justified by Windows evidence.
