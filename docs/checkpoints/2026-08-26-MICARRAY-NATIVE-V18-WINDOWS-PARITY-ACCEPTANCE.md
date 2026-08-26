# MICARRAY native v18 — Windows parity acceptance

Date: 2026-08-26

## Result

**Accepted.** The Surface Pro 11 Linux native microphone implementation passes the requested 95–99% Windows/Linux acoustic parity gate. The equal-weight mean of the two direct channels' envelope, frequency-response and time-frequency correlations is **98.27%**.

The accepted implementation is Golden v33 + the TX DMIC backend DT delta + the combined `VA-TX-AB-v16` topology + UCM Mic route + kernel patches **0072 and 0078**.

## Root cause closed by v18

v17 proved the cross-macro ownership model but left VA DMIC control register 0x3084 at `0x01`. The Windows oracle uses `0x05`: DMIC enable plus DIV4 selector. A live A/B changing only 0x3084 from 0x01 to 0x05 collapsed the bad Linux channel from roughly 1020 RMS broadband garbage to roughly 10 RMS.

Patch 0072 changes Denali DMIC sample-rate validation to use the native 19.2 MHz VA MCLK basis, so the normal driver path selects DIV4. v18 changes only the VA module relative to v17; kernel, DTB, LPASS common, TX macro and topology are unchanged.

## Binary isolation

Against Golden v33, the v17/v18 initrd changes only three regular files: `snd-soc-lpass-macro-common.ko.zst`, `snd-soc-lpass-va-macro.ko.zst`, and `snd-soc-lpass-tx-macro.ko.zst`. v18 differs from v17 in only the VA module.

The DTB differs from Golden v33 in one sound-node hunk: model selection, TX DMIC0/1 `vdd-micb` routes, and the `TX DMIC Capture` DAI link (`TX_CODEC_DMA_TX_3`, backend id 120).

The accepted topology binary SHA-256 is `4e00057b8e316c217347bcdee0af0c6d4ff40e8e0f1870d7efeaddc2669ff54e`. Decoding it with `alsatplg -d` and recompiling the decoded configuration reproduces that SHA byte-for-byte.

## Acoustic parity test

Stimulus: retained Seven Nation Army MP3, SHA-256 `951a65cc63fee17622485c1d94708614005524c7e20f86d3d815327f6bd0e8b3`, excerpt 19–49 s, SP7 endpoint scalar 0.25.

Recordings:

- Windows WASAPI RAW: 48 kHz stereo, 40 s, SHA-256 `62f2e77232c202a32c46bba8117c1741eaa993975ab0a6ca16b27358e0a07ba7`.
- Linux v18 `hw:0,2`: 48 kHz stereo S16_LE, 40 s, SHA-256 `8d3926ab271c47d3de435be0d047180dac1830ff70634d4e4a5b6544da0e3f0e`.

The Windows NTFS volume was mounted read-only under Linux and both exact WAVs were processed by the same SciPy/NumPy analyzer. One common stereo-average 100 ms acoustic alignment selected a 30 s window (Windows 9.9 s, Linux 6.6 s; relative offset -3.3 s). No per-channel alignment was used.

Direct mapping results:

| Metric | Windows ch0 ↔ Linux ch0 | Windows ch1 ↔ Linux ch1 | Mean |
| --- | ---: | ---: | ---: |
| Envelope correlation | 98.62% | 98.44% | 98.53% |
| 48-band response correlation | 97.58% | 96.55% | 97.07% |
| Time-frequency fingerprint | 99.12% | 99.30% | 99.21% |

Equal-weight correlation mean: **98.27%**.

Frequency-response residual RMS after scalar-gain removal is 3.27 dB / 3.78 dB. Channel balance is +1.52 dB on Windows and +1.92 dB on Linux (0.39 dB difference). Windows stereo correlation is 0.923; Linux is 0.873. Direct channel mapping is retained; swapped mapping is not preferred overall.

## Production / rejected history

Production kernel changes are **0072 + 0078**. 0079/0080 were diagnostic ordering/ladder patches and are not in the accepted source state. The uncommitted 0081–0086 power/lifecycle/endpoint-owner experiments were useful for localization but are not present in the accepted v18 initrd. `deploy/native-audio-v34` is a later experimental topology and is not the accepted topology.

## Deployment identifiers

GRUB candidate id: `sp11-audio-dmic-broker-div4-v18`.

Accepted package hashes:

- initrd `ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d`
- kernel `bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a`
- DTB `09dcf2832487b1523ab2cdecba4ef9f2335d4e95e1bcd87a2dad41208d20ae0a`

Golden v33 remains the rollback entry.

## Saved-default deployment smoke

After the acceptance commit was pushed, v18 was promoted from one-shot candidate to the persistent GRUB default. A cold reboot returned on `sp11_entry=7.1.5-sp11-dmic-broker-div4-v18` with the accepted common/VA/TX srcversions and `saved_entry=sp11-audio-dmic-broker-div4-v18`.

PipeWire/WirePlumber exposed the normal desktop nodes `Built-in Audio Speaker playback` and `Built-in Audio Internal microphone array`; the default output remained the existing `SP11 UbiG Boundary (Bypass)` pipeline and the internal microphone array was the default input.

A final simultaneous desktop smoke used `pw-play` to send a low-level 997 Hz stereo tone through the default output while `pw-record` captured the default input. Playback returned exit code 0. The 5.333 s captured WAV (`badeced4212f12b42cb1d7cc19736110894da9da9cc9bcf935b0eb600edca4f3`) contained sustained data on both channels and the acoustic 997 Hz return measured 26.80 dB / 27.01 dB above the local spectral floor.

Runtime PM was `suspended/suspended` for TX/VA before capture, `active/active` during capture, and returned to `suspended/suspended` after autosuspend. This verifies the finished desktop output→acoustic→input path while retaining native DAPM/runtime-PM ownership.
