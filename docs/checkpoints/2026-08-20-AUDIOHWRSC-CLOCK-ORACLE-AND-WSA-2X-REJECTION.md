# Native AudioHwRsc clock oracle and WSA TX 2X rejection

Date: 2026-08-20
Status: **direct Windows clock oracle corrected; real 0x315 gap proven and rejected as sufficient fix**

## Correction to the earlier static interpretation

An intermediate static reading of qcaucd's pre-owner resource selector suggested that the LPASS v2.6 / generation-7 branch might request `RX_CORE_TX_MCLK / 0x312`. A reversible Linux 0x312 experiment was performed from that hypothesis.

The preserved **native Windows marked playback runtime trace** was subsequently re-read at the actual qcadcm `AudioHwRscIoctl` / Q6 PRM hardware-resource boundary. Runtime evidence supersedes the static inference: **0x312 is not requested in the marked native speaker playback interval.**

The earlier 0x312 experiment remains useful only as a negative experiment; it must not be described as a proven Windows parity gap.

## Direct native Windows clock sequence

Within `PLAYDELTA_START_20260815 .. PLAYDELTA_END_20260815`, qcadcm sends `PARAM_ID_RSC_AUDIO_HW_CLK = 0x0800102c` requests for:

1. `0x30c TX_CORE_MCLK`, 19.2 MHz, attribute 1;
2. `0x314 WSA_CORE_TX_MCLK`, 19.2 MHz, attribute 1;
3. **`0x315 WSA_CORE_TX_2X_MCLK`, 19.2 MHz, attribute 1.**

The marked teardown contains releases for `0x30c`, `0x315`, and `0x314`.

Reviewed oracle:

`artifacts/runtime-20260820-wsa-core-tx-2x-mclk/windows-audiohwrsc-clock-oracle.json`

## Golden Linux comparison

The earlier live Golden kprobe at `q6prm_set_lpass_clock()` during direct speaker playback showed:

- `0x30c TX_CORE_MCLK @ 19.2 MHz`;
- `0x314 WSA_CORE_TX_MCLK @ 19.2 MHz`.

It did **not** show `0x315`.

Therefore the direct, runtime-proven Windows/Linux clock-vote gap is **WSA_CORE_TX_2X_MCLK / 0x315**, not 0x312.

Linux clock-provider index 67 maps to raw PRM `0x315`. A temporary GPL clock-framework consumer acquired it at 19.2 MHz and released it cleanly on unload. Source:

`artifacts/runtime-20260820-wsa-core-tx-2x-mclk/sp11_wsa2x_vote.c`

## Strong zero-ring A/B

The experiment used the already-established CPS zero-ring discriminator:

- disposable `soundwire_qcom` srcversion `801511EA5B3957C10977AF5`;
- WSA master `0x105c = 0x0005000f`;
- DP13 PCM control `0x1d54 = 0x3`;
- isolated CPS tap3 / IID `0x402b`;
- canonical real-root topology automatically restored before measurement;
- persistent GRUB saved entry remained Golden v31.

Without 0x315:

- 392 cmd16 frames, all length 296;
- zero frames with any nonzero byte in the final 192-byte PCM payload.

With `0x315 WSA_CORE_TX_2X_MCLK @ 19.2 MHz` held:

- 392 cmd16 frames, all length 296;
- zero frames with any nonzero byte in the final 192-byte PCM payload.

Reviewed A/B:

`artifacts/runtime-20260820-wsa-core-tx-2x-mclk/combo-105c-1d54-tap3-315-ab-analysis.json`

## Conclusion

`0x315` is a **real native-Windows hardware-resource vote missing from Golden**, but it does not restore CPS data even after `0x105c + 0x1d54` have already made the signal-triggered source container/ring process hundreds of frames. The payload remains byte-for-byte zero.

Do not promote `0x315`, and do not reopen 0x312 as a Windows parity requirement.

The remaining AudioHwRsc families actually observed in the same Windows playback interval are:

- `PARAM_ID_RSC_HW_CORE = 0x08001032`;
- endpoint DSP GPIO configuration `0x080014f3`.

Those are the next evidence-backed resource-vote boundaries to compare against Golden.
