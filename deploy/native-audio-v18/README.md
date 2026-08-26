# SP11 native audio v18 deployment

This directory pins the exact DTB and combined AudioReach topology used by the accepted Surface Pro 11 native microphone parity candidate. The kernel base is Golden v33. The production microphone code delta is **0072 + 0078 only**.

## Production components

- `patches/0072-ASoC-lpass-va-macro-SP11-match-Windows-DMIC-divider.patch` — Denali VA DMIC divider uses the native Windows 19.2 MHz MCLK basis, yielding VA DMIC control `0x05` (DIV4 + enable) during capture.
- `patches/0078-ASoC-lpass-SP11-share-VA-DMIC-clock-with-TX-capture.patch` — TX capture requests the VA-owned shared DMIC clock through the LPASS common broker.
- `x1e80100-microsoft-denali-sp11-native-audio-v18.dtb` — exact accepted DTB; adds the TX DMIC backend and TX DMIC0/1 `vdd-micb` routes while retaining Golden v33 playback/protection links.
- `X1E80100-Microsoft-Surface-Pro-11-VA-TX-AB-v16-tplg.bin` — exact accepted combined playback + EP16 capture topology.
- `X1E80100-Microsoft-Surface-Pro-11-VA-TX-AB-v16.conf` — `alsatplg -d` decode of that binary. Recompiling it with the installed `alsatplg` reproduces the binary byte-for-byte.
- `deploy/ucm2/Qualcomm/x1e80100/SP11-HiFi.conf` — exposes `hw:0,2` as the internal two-channel Mic device and applies Windows lane mapping DEC0<-DMIC1 / DEC1<-DMIC0.

## Accepted boot package hashes

- kernel: `bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a`
- DTB: `09dcf2832487b1523ab2cdecba4ef9f2335d4e95e1bcd87a2dad41208d20ae0a`
- initrd: `ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d`
- topology: `4e00057b8e316c217347bcdee0af0c6d4ff40e8e0f1870d7efeaddc2669ff54e`
- UCM: `9d36df8570b85f1dcecc385a8f85fa2d1e1058ef8efedee6ae2ce49dc259a06a`

LPASS module raw hashes / srcversions:

- common: `0698d60676385d7e1bd9459a8b57834809b4a8125e73c766425552687dd6683f`, `2EA7312A851E75A7C860F82`
- VA: `161fe5e40e48d6797821414cd0d2e31a91271084264ecdd288f502dd02ffeb47`, `DC4373218C279E16F550900`
- TX: `19d4a65a03de6e120767874657072251b68e9383c8b2f637b3f912c22f1cd402`, `835AF5272E94DB266E85D55`

All three use vermagic `7.1.5-sp11-render-parity-v4+ SMP preempt mod_unload modversions aarch64`.

## Explicitly not production

0079/0080 and the later 0081–0086 ladders/endpoint-owner experiments are diagnostic history, not present in the accepted v18 initrd. `deploy/native-audio-v34` is also not the topology used for the accepted parity run.

See `docs/checkpoints/2026-08-26-MICARRAY-NATIVE-V18-WINDOWS-PARITY-ACCEPTANCE.md` and `artifacts/2026-08-26-native-mic-v18-parity/parity-summary.json`.
