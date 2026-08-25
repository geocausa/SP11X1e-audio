# SP11 MicArray EP16 0079 runtime result and 0080 ladder — 2026-08-25

Runtime test of `sp11-audio-mic-ep16-hwparams-order-v4` confirmed the 0079 TX module was actually loaded (`/sys/module/snd_soc_lpass_tx_macro/srcversion = CBB00512E87DAE89AB0C188`). `MultiMedia3 Capture` remained card 0, device 2 and no PipeWire/WirePlumber client was active.

Staged probe results:

```text
open   -> success
any    -> success
params -> hard reset
```

The persisted marker is `2026-08-25T20:21:16+01:00 stage=params begin`. SP7 independently observed the network disappearance and recovery. The one-shot GRUB boot was consumed before the reset, so SP11 automatically recovered on saved Golden-v33. No pstore crash record was produced, consistent with the previous abrupt hardware reset behavior.

Therefore 0079 proves that moving VA DMIC acquisition before TX PCM-rate programming is not sufficient by itself. The remaining reset boundary is inside or after the exact EP16 TX `hw_params()` sequence.

Patch `0080-ASoC-lpass-tx-macro-SP11-add-EP16-hwparams-diagnostic-ladder.patch` adds a Denali-only fail-closed module parameter:

```text
sp11_ep16_hwparams_gate=0  unrestricted 0079 behavior
sp11_ep16_hwparams_gate=1  stop before VA DMIC acquisition (default)
sp11_ep16_hwparams_gate=2  stop immediately after VA DMIC acquisition
sp11_ep16_hwparams_gate=3  stop after DEC0 PCM-rate write
sp11_ep16_hwparams_gate=4  stop after DEC1 PCM-rate write
```

Strict checkpatch result is 0 errors, 0 warnings, 0 checks. Golden-v33 build produced:

```text
snd-soc-lpass-macro-common.ko a75a88d94f26d961c9234b52711e0b96fd2aead306c79b81e1b37f839a0c8a4d
snd-soc-lpass-va-macro.ko     85847b0f256be62da2f897a140129c29a225c9f8f09ca6b6af456902307081f0
snd-soc-lpass-tx-macro.ko     28339abc7f70c9536e17e65fd5d5ee6bd4faa79f2f1fde5a65c9dad034942d10
```

The Golden kitchen was restored and hash-verified. An isolated boot package exists at `/boot/sp11-7.1.5-audio-mic-ep16-hwparams-ladder-v5`. It is content/metadata-identical to the 0079 package outside the TX module; kernel and DTB are unchanged. GRUB id: `sp11-audio-mic-ep16-hwparams-ladder-v5`. Saved default remains Golden-v33.
