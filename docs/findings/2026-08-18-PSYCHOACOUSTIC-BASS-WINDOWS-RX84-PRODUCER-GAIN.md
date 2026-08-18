# Psychoacoustic bass: native Windows RX84 producer gain closes the main v31 LF deficit

Date: 2026-08-18
Status: Windows-proven producer policy mismatch isolated; v31 userspace lifecycle candidate ready for live validation.

## Native Windows oracle

A fresh native-Windows boot was compared against Golden v31 from the fixed SP7
RAW microphone geometry (SP7 centered/square-on, one attached SP11 keyboard
length from the SP11). Both operating systems used byte-identical deterministic
sources and a 10% fresh-state bootstrap followed by a 25% endpoint transition
inside source pre-silence.

The first psycho-bass source SHA-256 was:

`01558cac5ec08ef3ed7ad172b7c64c69482bdb494638165e01d929c5a77b759f`

Windows evidence:

- WASAPI loopback SHA-256 `3d8ae3edc68aeddc8203ee36f5594560f256041d2e54f3559adf651a864e18f9`
- SP7 RAW physical SHA-256 `ed4461c56a273a1ff7b6a37924f70e0260e4e98e7280011fde8f265c71f7b12c`
- loopback analysis SHA-256 `ffb007efdb903f3fd77f4739daa996ca6b3fe49519a04887a5446c563c33a117`
- physical analysis SHA-256 `9bdedf8a0edf6aabf9081b6634c53051c4d7c283bbaa537b04ac1fd135a741d4`

The fresh Windows loopback and fresh Linux Movie post-Dolby PCM agree extremely
closely through the useful bass band; representative 75/100-Hz values differ by
hundredths of a dB in settled blocks. The residual is therefore downstream of
Dolby/upstream PCM.

## Level-dependent transfer probe

A second deterministic source varied 75/100/150-Hz input amplitude and included
left-only/right-only 100-Hz conditions:

`psycho-bass-transfer-v2.wav`
SHA-256 `28461206bc70e017196088025cd3f4c9ac397f5ea3a18ccd18e412b11ea352c8`

Windows evidence:

- loopback SHA-256 `8d16da6107e1f20f22e591c447e806a0774ddcb31499e2eb1f94c61630af8b6e`
- SP7 RAW SHA-256 `38e54816f204af7d3e63ee47cc7c07f176be0fc1148c59768969adb0cfb90f60`
- loopback analysis CSV SHA-256 `f6329c0c4173a6d806b8f840c8100f345d94bfbd24ef0d1b6866a0cb45173ab6`
- physical analysis CSV SHA-256 `86953947cc9b77a89588cf354cb953af246019edc69307b83b39dc5ffbbb3737`

Golden v31 baseline RX81 evidence:

- post-Dolby SHA-256 `04cc5d7b74c720e1d0771acd6f02ade02df984b3b28326e2b46b6dc06078636a`
- post-Dolby analysis CSV SHA-256 `f52c9e737ded9fc2bda25dfe5e0d83773983e145c1e96df783bc045f510d4f09`
- SP7 RAW SHA-256 `650f102f148ff492e6628b3fbae5e5548e694ed3be3eceda269ae4f107343fe4`
- physical analysis CSV SHA-256 `14357aeb277eb591678dbfd5b66cafb85793d68e453fdbe2714dd7e966ffe86c`

After normalizing physical microphone fundamental by the immediately upstream
Windows-loopback/Linux-post-Dolby fundamental, RX81 Golden v31 is roughly
2--3 dB below Windows over ordinary 75--150-Hz test levels. The residual is
nearly level-independent over source amplitudes 0.05--0.20, arguing against a
simple protection threshold or limiter/compressor onset.

## Direct Windows producer state

This is not a guessed gain change. The already-reviewed native qcaucd WSA
programming corpus directly proves:

- RX0 child `0x0414 RX0_RX_VOL_CTL = 0x00`;
- RX1 child `0x0494 RX1_RX_VOL_CTL = 0x00`.

On the RX84-capable Linux control scale, value 84 is 0 dB and value 81 is -3 dB.
Golden v31 carries the exact final Windows producer module lineage
`winproducer-nohd2-v3` (`snd_soc_lpass_wsa_macro` srcversion
`4AF6F542C17BA6DD46586DA`) but normal UCM/mixer policy still leaves both live
RX controls at value 81.

## Continuous-stream actuator proof

A single continuous 100-Hz stream was held at one endpoint state while only
RX0/RX1 were alternated 81 -> 84 -> 81 -> 84 -> 81.

- SP7 RAW SHA-256 `d50b639cfe15ddbe732ddcfc334b1bca36690c396253eccd8e35089f0729e86c`
- analysis SHA-256 `4c1a0c9274ec88fc3041a4f7d9fcfae9637bebab2b78c7f2c6d51adcb9785a21`

Measured 100-Hz fundamental:

- RX81 A: -53.9075 dBFS
- RX84 A: -50.8052 dBFS (+3.10 dB)
- RX81 B: -53.8963 dBFS
- RX84 B: -50.8845 dBFS (+3.01 dB)
- RX81 C: -53.9021 dBFS

Thus the mixer control is a real active producer gain actuator with the expected
3-dB delta.

## Producer-lifecycle ordering

An RX84 write made while idle can be overwritten by producer POST_PMU/register
restoration even when the ALSA cached control still reads 84. The reliable
Windows-parity ordering is:

1. open/protect the speaker graph;
2. complete final AudioReach endpoint/GainStep handover;
3. write RX0/RX1=84 while the WSA producer is active.

A corrected transfer-v2 run used exactly that ordering and restored RX81 after
the test:

- stage log SHA-256 `4899bee3d396211854ab8bd3d7a2413c4872a0a9f344ad18a2b8dbfa869d19f0`
- post-Dolby SHA-256 `fbf52b1426e6d9c3c00655c8e07a965632c53d2616c3d2c4aa1f7dacc79dc9a7`
- post-Dolby analysis CSV SHA-256 `fee9badc988577f4190abe093cd560a4cedea9be444aede5c1618fcec714cc9a`
- SP7 RAW SHA-256 `b0975fb2527d7d9427f809beb5fec53be42b693e17e8e3411942029462155ad1`
- physical analysis CSV SHA-256 `0d2eec90327c2328867ae4bae62e73adb12224cc185c2b3b258dee1ddd6b5ce4`

With active RX84, 75--100-Hz downstream transfer moves to approximately
Windows class (typically within about 0--0.6 dB in the stable conditions).
150 Hz tends somewhat hot versus Windows in this fixed-geometry run, consistent
with the already-known remaining WSA8845 consumer-lifecycle/CSR-assisted gap.
Do not use that residual as justification to force `DRE_CTL_1=0`; the earlier
isolated CSR/DRE experiment was unsafe and remains rejected.

## Candidate policy

The safe next candidate is userspace-only and v31-capability-gated:

- keep RX81 while graph idle / on service exit;
- after the first successful v31 DSP handover, set RX0/RX1=84 once;
- do not rewrite RX gain on ordinary volume or mute events;
- reset the lifecycle state on idle and reapply 84 after the next producer wake;
- do not touch RX gain on v28/CPS/legacy kernels.

This isolates the directly Windows-proven producer gain without another kernel
or GRUB deployment.
