# SP11 MicArray EP16 hw_params ordering build gate — 2026-08-25

The staged ALSA lifecycle probe on the fail-closed `hwparams-gate-v3` candidate localized the first unsafe Linux runtime boundary to the TX codec `hw_params()` path immediately before `CDC_TXn_PCM_RATE` writes. `snd_pcm_open()` and `snd_pcm_hw_params_any()` survive; committed `snd_pcm_hw_params()` reaches the AudioReach pull/push position mapping and then the intentional TX gate returns `-EAGAIN` without resetting the machine.

The Windows MicArray EP16 qcaucd oracle orders the relevant hardware lifecycle as:

```text
TX DMIC selectors
VA-owned shared DMIC clock acquire
TX path rate/clock programming
```

Linux DAPM powers the DEC widgets only after the codec DAI `hw_params()` callback, so patch 0078's DEC `POST_PMU` acquisition was too late for the first TX rate writes.

Patch `0079-ASoC-lpass-tx-macro-SP11-order-VA-DMIC-before-rate.patch` acquires the existing VA-owned shared-DMIC resource from the exact Denali EP16 AIF1 DEC0+DEC1 `hw_params()` path before the TX PCM-rate writes. The later DEC `POST_PMU` request remains idempotent; the measured `POST_PMD` release boundary is unchanged.

Strict checkpatch result: 0 errors, 0 warnings, 0 checks.

Golden-v33 reproduction build used the exact `7.1.5-sp11-render-parity-v4+` kitchen. Candidate module SHA-256 values:

```text
snd-soc-lpass-macro-common.ko a75a88d94f26d961c9234b52711e0b96fd2aead306c79b81e1b37f839a0c8a4d
snd-soc-lpass-va-macro.ko     85847b0f256be62da2f897a140129c29a225c9f8f09ca6b6af456902307081f0
snd-soc-lpass-tx-macro.ko     d0b53ee9e0c616bfdecf7336a9f249ba9fc8704d0d371086189672a78a9f9d42
```

Common and VA are byte-for-byte identical to the proven 0078 build; only TX changes. All report exact Golden-v33 vermagic. The Golden kitchen source and prior codec build outputs were restored and hash-verified after candidate extraction.

An isolated boot package was created at `/boot/sp11-7.1.5-audio-mic-ep16-hwparams-order-v4`. It clones gate-v3 and replaces only `snd-soc-lpass-tx-macro.ko.zst`; an extracted content/metadata manifest proves every other initrd entry is identical. Kernel and DTB are byte-identical to gate-v3. The saved default remains Golden-v33; runtime testing should use the one-shot GRUB id `sp11-audio-mic-ep16-hwparams-order-v4`.
