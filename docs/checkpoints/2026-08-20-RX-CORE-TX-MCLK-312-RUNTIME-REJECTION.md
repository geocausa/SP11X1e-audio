# SP11 RX_CORE_TX_MCLK 0x312 runtime proof and rejection

Date: 2026-08-20
Status: **proven Windows/Linux parity gap; rejected as sufficient VI/CPS fix**

## Native Windows resource semantics

The qcaucd pre-WSA-owner resource path was traced through its AudioHwRsc provider into qcadcm. qcadcm identifies operation 5 as `AUDIO_HW_RSC_ENABLE_CLOCK` and operation 6 as `AUDIO_HW_RSC_DISABLE_CLOCK`.

For the actual Surface speaker descriptor (type-0x14 resources 7/8), `FUN_140039010()` always takes the `{0x0c,2}` resource tuple and then selects a second clock according to the LPASS codec-generation record returned by `FUN_140028d90()`.

The relevant Q6 PRM IDs are:

- `0x30c` = `TX_CORE_MCLK`;
- `0x312` = `RX_CORE_TX_MCLK`;
- `0x314` = `WSA_CORE_TX_MCLK`;
- `0x316` = `WSA2_CORE_TX_MCLK`.

The resource-9/10 branch is required for `0x316`; the native speaker descriptor has only resources 7/8, so `0x316` is not part of this speaker path.

## Exact SP11 generation proof

qcaucd derives its generation from VA-macro core ID bytes:

- `0x0100 -> generation 4`;
- `0x0201 -> generation 6`;
- `0x0260 -> generation 7`.

A temporary read-only Linux helper called the already-exported `lpass_macro_get_codec_version()` and returned enum `7`, which is `LPASS_CODEC_VERSION_2_6`. That is the Linux driver's decoded form of the same VA-macro hardware revision and corresponds to qcaucd's `0x0260 -> generation 7` branch.

Therefore the native Surface speaker-start path **does request `RX_CORE_TX_MCLK / 0x312`**.

## Golden Linux clock trace

A temporary kprobe on `q6prm_set_lpass_clock()` during direct `hw:0,0` speaker playback showed Golden requesting only:

- `0x30c TX_CORE_MCLK @ 19.2 MHz`;
- `0x314 WSA_CORE_TX_MCLK @ 19.2 MHz`.

No `0x312` request occurred in the render interval. This proves a real Windows/Linux clock-vote discrepancy.

## Reversible 0x312 test

A tiny temporary GPL clock-framework consumer acquired Q6PRM clock-provider index 64, which maps to raw PRM ID `0x312`, set 19.2 MHz, prepared/enabled it, and released it on module unload. No direct PRM packet construction or MMIO was used.

Source is retained at:

`artifacts/runtime-20260820-rx-core-tx-mclk/sp11_rxcoretx_vote.c`

With isolated forced CPS tap3 on otherwise Golden v31:

- without `0x312`: zero `0x1586` audio frames;
- with `0x312`: zero `0x1586` audio frames.

Thus `0x312` does not by itself wake the dead CPS source/logger path.

## Stronger zero-ring discriminator

The previous `0x105c + DP13 0x1d54=3` SoundWire candidate is known to wake CPS source processing while leaving every PCM payload byte zero. A new one-shot boot combined:

- exact candidate `soundwire_qcom` srcversion `801511EA5B3957C10977AF5`;
- WSA master `0x105c = 0x0005000f`;
- CPS DP13 PCM control `0x1d54 = 0x3`;
- forced CPS tap3 topology;
- optional reversible `RX_CORE_TX_MCLK / 0x312 @ 19.2 MHz` vote.

Persistent GRUB remained `sp11-audio-golden-v31`; the topology restore service returned the real-root firmware file to the canonical Golden hash before measurements.

### Same-boot A/B

Without `0x312`:

- 392 cmd16 frames, all length 296;
- 0 frames with nonzero bytes in the final 192-byte PCM payload;
- total nonzero PCM bytes = 0.

With `0x312` held:

- 392 cmd16 frames, all length 296;
- 0 frames with nonzero bytes in the final 192-byte PCM payload;
- total nonzero PCM bytes = 0.

Reviewed summary:

`artifacts/runtime-20260820-rx-core-tx-mclk/combo-105c-1d54-tap3-ab-analysis.json`

## Conclusion

`RX_CORE_TX_MCLK / 0x312` is a **genuine native-Windows speaker-start resource vote missing from Golden Linux**, but it is not sufficient to restore VI/CPS feedback. Even after the already-known `0x105c + 0x1d54` operations make the CPS signal-triggered container emit hundreds of frames, adding the exact missing clock does not change a single PCM byte from zero.

Do not promote `0x312` alone, and do not stack it with `0x105c/0x1d54` as a presumed fix.

The next evidence-backed Windows AudioHwRsc families to audit are the hardware-core and endpoint-GPIO transactions observed in the same native playback interval. The working fault boundary remains upstream of the proven HWD4/STM interrupt/ring processing and downstream of or orthogonal to the largely parity-closed WSA/SoundWire producer programming.
