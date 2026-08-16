# WSA8845 v6 provenance retraction and DRE-cold v8 isolation

Date: 2026-08-16
Status: **v6 causal claim retracted; v8 REJECTED by active digital-zero noise gate**

## Why the v6 conclusion had to be corrected

The intended v6 source differed from v5 by one extra call in `wsa884x_mute_stream()` which cleared `DRE_CTL_1[5:1]` immediately before PA enable. During preparation of the next candidate, a byte-level audit found that the packaged v6 module did not contain that call.

The Kbuild invocation emitted the freshly rebuilt module under the source-tree `sound/soc/codecs/` directory, while the packaging step copied `snd-soc-wsa884x.ko` from the prepared `O=` tree. That `O=` file was stale. Direct disassembly proves packaged v6 `wsa884x_mute_stream()` is machine-code identical to packaged v5.

Consequences:

- the v6 acoustic recordings and hashes remain real historical observations;
- the previous claim that their instability was *caused by an unmute-time gain-zero write* is retracted;
- the old v6 boot image must not be used as a source-delta oracle;
- v7 remains independently valid because it deliberately reused known v5 module bytes and changed only UCM route-time programming.

## Remaining Windows/Linux write-history mismatch

The render-parity-v4/v5 driver already reproduces the Windows SP11 2S PA transaction recovered from qcaucd:

```text
CLSH_CTL_0=0x67
PWRSTG_DBG=0x08
PDRV_HS_CTL=0x52
PA_FSM_EN=1
PWRSTG_DBG=0x0c
PDRV_HS_CTL=0x5a
```

and stop includes `PA_FSM_EN=0` then `CLSH_CTL_0=0x00`.

Fresh Windows KDNET observes **zero ordinary runtime writes to `DRE_CTL_1`**. v5 nevertheless inserts `DRE_CTL_1.CSR_GAIN_EN=0` between the pre-PA Class-H sequence and `PA_FSM_EN=1`, and writes the same bit again before PA-off. Since earlier work has shown that DRE register write history can matter independently of final values, these redundant PA-boundary transactions are now the narrower mismatch.

## v8 one-variable isolation

DRE-cold v8 starts from the exact v5 source and changes only SP11 2S `mute_stream()` behavior:

- mute: skip the ordinary DRE bit-clear, then `PA_FSM_EN=0 -> CLSH_CTL_0=0`;
- unmute: keep `CLSH67 -> PWR08 -> PDRV52`, skip the DRE write, then `PA=1 -> PWR0c -> PDRV5a`;
- non-2S behavior remains generic;
- COMP-aware POST_PMU still performs its existing DRE setup;
- UCM remains PA Volume 24 / stored CSR code 7;
- RX84, Windows producer, no-HD2, Dolby, AudioReach and endpoint volume are unchanged.

This tests **write history only**, not another DRE value.

## Provenance

Candidate directory:

`/home/geoca/Documents/SP11-PROJECT/02-kernel/candidates/rpv4-macro84-winproducer-nohd2-drecold-v8-idlegated-20260816`

Key identities:

- v5 source SHA-256 `f5555cfde5f8c72001a779ac9d0dc0aac527284e88c6333a450027af4f340f97`;
- v8 source SHA-256 `ca99fb886f1fa2007d1580ceb7cdd4b9f906b7278af130283002cc63831d4a04`;
- v8 one-variable patch SHA-256 `946884bbc36538d94c2c1e6a8df37d4a0bd0252b57d914d40d69299b2821fa7c`;
- v8 WSA module srcversion `EFCD352343B5CD747194DDD`;
- signed v8 `.ko` SHA-256 `2266681914cc1aface687345f81103509f78f777006427bdd68d70e208d8837b`;
- compressed v8 `.ko.zst` SHA-256 `3665795bb59112a6d0444b9cd21d416961047a010db0806e8c855f5f1607e357`;
- v8 initramfs SHA-256 `2c36b5f0eb91135c89a27fa9f0fbb43b4ad986b1ff55a452f53a06c670fb5ab4`.

The initramfs carries the exact proven v5 producer and machine-driver modules:

- LPASS WSA macro `05d19a94c21b5a7683922e024f714877d744b588cf59c1152ea694f401e4b530`;
- x1e machine driver `077a6e3f08c864c13f8b4864d0262e8c60018c18cea033323bc16903a12e0610`.

All three target modules are force-loaded in initramfs. The root module tree and `/etc/initramfs-tools/modules` were restored after the transactional build. GRUB entry `sp11-audio-rpv4-macro84-winproducer-nohd2-drecold-v8-idlegated` exists but was not armed at this checkpoint; persistent fallback is `sp11-audio-cps-v3`.

## Required gate

Boot one-shot from 1% muted/PCM-closed. Before sound, verify marker, loaded module identities and idle close. Then use read-only write/lifecycle tracing during a six-second fixed MP3. The decisive trace requirement is: COMP-aware DRE setup may occur before the PA transaction, but ordinary SP11 2S `mute_stream()` must add **no `DRE_CTL_1` write** between the Windows Class-H/power-stage writes and PA enable/disable. Any static/crackle or PA/SoundWire/XRUN/ADSP fault rejects v8 immediately.

## v8 runtime result

The one-shot boot loaded the exact intended module:

- WSA8845 srcversion `EFCD352343B5CD747194DDD`;
- LPASS WSA macro srcversion `4AF6F542C17BA6DD46586DA`;
- x1e machine-driver srcversion `13326073E27DFA035180C56`;
- persistent fallback remained `sp11-audio-cps-v3`;
- v7 UCM overlay was absent;
- idle physical PCM was `closed`.

A fixed MP3 at 1% reached physical PCM `RUNNING` and returned to `closed`. Read-only tracing proved the intended v8 write history. COMP-aware speaker POST_PMU still emitted one `DRE_CTL_1 mask=0x01 val=0` per amp before PA activation. At ordinary PA start, each amp then emitted exactly:

```text
CLSH_CTL_0  0x67
PWRSTG_DBG  0x08
PDRV_HS_CTL 0x52
PA_FSM_EN   bit0=1
PWRSTG_DBG  0x0c
PDRV_HS_CTL 0x5a
```

with no `DRE_CTL_1` write inside `mute_stream()`. Stop was `PA_FSM_EN bit0=0` then `CLSH_CTL_0=0`, again with no DRE write. PCM closed cleanly and a 90-second idle watcher recorded `90/90 closed`.

Evidence:

- `artifacts/reviewed/2026-08-16-v8-1pct-write-lifecycle.trace`;
- `artifacts/reviewed/2026-08-16-v8-idle90.log`.

## Digital-zero discriminator and rejection

Before any 5% or 12% escalation, a stricter noise test was run. The visible Windows-Dolby endpoint stayed at 1% and **muted** while a ten-second 48 kHz stereo S16_LE WAV containing only zero PCM samples was played. Stimulus SHA-256:

`87d8420ddaf7d56d3f5068c6a74362451fc2859197445490d15e7b3d456fa22e`

The zero stream correctly held the physical ALSA PCM `RUNNING`; it later returned to `closed`. SP7 external-mic capture SHA-256:

`3A12957D596502F7EF92E912D15CB56B048787FE93A3E9B354BDFD18CADFF25F`

Steady windows excluding the PA-start transition show:

| channel | baseline RMS | active-zero RMS | ratio | baseline diff-RMS | active-zero diff-RMS |
|---|---:|---:|---:|---:|---:|
| 0 | 0.00010174 | 0.00127825 | 12.6x | 0.00001805 | 0.00072272 |
| 1 | 0.00010549 | 0.00151195 | 14.3x | 0.00001907 | 0.00085739 |

The elevated broadband floor persisted after the zero file ended while PipeWire still held the PA open for its suspend delay. This is not source content and cannot be explained by the requested waveform: the waveform is all zeros and the visible endpoint remained muted.

Machine-readable result:

`artifacts/reviewed/2026-08-16-v8-zero-stream-static-rejection.json`

**Decision: reject v8 immediately.** No 5% MP3 or 12% chirp gate was run. The result proves that reproducing Windows' ordinary no-DRE-write PA transaction is still insufficient on the current Linux initialization state. An earlier WSA8845 initialization/latch/state dependency remains missing. Keep v5 as the bounded-safe reference and compare complete Windows/Linux amp write histories before another behavioral candidate.
