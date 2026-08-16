# WSA8845 CSR stored-gain zero v6 isolation

Date: 2026-08-16
Status: **REJECTED — acoustic repeatability failure at 12%**

## Question

After `csren0-v5-idlegated` proved that keeping `CSR_GAIN_EN=0` can be bounded-safe under the corrected Windows-like PA idle lifecycle and directionally improves the matched 1--5 kHz Windows residual, one `DRE_CTL_1` difference remained.

Linux UCM leaves `SpkrLeft/Right PA Volume=24`. The WSA8845 PA Volume control is inverted and maps to `DRE_CTL_1_CSR_GAIN_MASK=0x3e`; value 24 therefore leaves raw CSR gain code 7 (`0x0e` in bits [5:1]). v5 clears the enable bit but intentionally leaves those stored bits untouched. Windows active speaker playback has the complete register at `0x00`.

The direct mixer-write experiment was not used. Instead a new one-shot kernel candidate isolated the stored field in the already-proven v5 unmute path.

## Exact v5 -> v6 delta

SP11 2S only:

```diff
+		if (wsa884x->supply_config == WSA884X_SUPPLY_2S)
+			snd_soc_component_write_field(component, WSA884X_DRE_CTL_1,
+						      WSA884X_DRE_CTL_1_CSR_GAIN_MASK, 0x0);
+
 		snd_soc_component_write_field(component, WSA884X_DRE_CTL_1,
 					      WSA884X_DRE_CTL_1_CSR_GAIN_EN_MASK,
 					      wsa884x->supply_config == WSA884X_SUPPLY_2S ?
 					      0x0 : 0x1);
```

No UCM change, no full-register write, no producer change, no endpoint-volume change, no non-2S change.

Candidate module:

```text
srcversion: E4EDBF0D41BABDB3E8A4B77
vermagic: 7.1.5-sp11-render-parity-v4+ SMP preempt mod_unload modversions aarch64
zstd SHA-256: c00e2b0e28d9c2991bef4f9793ab2621c60cd50c7e725b02ef3d7c4aa2863246
```

The initramfs was extracted before boot and the embedded WSA8845 module hash matched exactly. The normal root module was restored to its prior `b2441452...` hash after initramfs creation. The one-shot GRUB entry was consumed and the persistent fallback remained `sp11-audio-cps-v3`.

## Low-level lifecycle safety

Cold boot v6 started with:

- endpoint 1%;
- Dolby/speaker graph suspended;
- physical ALSA PCM `closed`;
- no new audio-hardware fault evidence.

A six-second fixed MP3 at 1% reached real hardware (`PCM RUNNING`). Read-only tracing showed producer POST_PMU well before first PA unmute, then PA mute before speaker/producer teardown and SoundWire deprepare. PCM returned `closed`.

A 90-second idle watcher then recorded:

```text
90 / 90 samples: closed
```

A 5% fixed-MP3 pass also completed with 44 lifecycle events and returned PCM `closed`. No new WSA/PA/SoundWire/XRUN/ADSP fault was observed.

Evidence:

```text
artifacts/reviewed/2026-08-16-csrgain0-v6-idlegated-real-1pct.trace
artifacts/reviewed/2026-08-16-csrgain0-v6-idlegated-real-5pct.trace
artifacts/reviewed/2026-08-16-csrgain0-v6-idlegated-idle90.log
```

The external microphone takes contain unrelated room/impulse events and are not used alone as a safety discriminator.

## Exact-oracle 12% failure

Five byte-identical `c8782c...208` log-sweep captures were taken at 12%. Every run returned PCM to `closed` and no kernel/audio fault appeared. Alignment quality was actually better than the earlier v5 set (roughly 0.17--0.20), so the analyzer was not simply failing to find the sweep.

But acoustic response was catastrophically non-repeatable:

```text
v6 per-run 1--5 kHz Windows MAE:
7.771, 6.675, 7.347, 5.369, 0.592 dB
```

The five-run v6 median is:

```text
1--5 kHz:       MAE 5.989 dB, RMSE 6.404 dB
630 Hz--6.3 kHz: MAE 5.697 dB, RMSE 6.093 dB
```

For comparison, the otherwise-matched v5 median is:

```text
1--5 kHz:       MAE 0.432 dB, RMSE 0.489 dB
630 Hz--6.3 kHz: MAE 0.451 dB, RMSE 0.526 dB
```

More important than the Windows residual itself, v6's **own five-run spread** is enormous:

```text
1--5 kHz pointwise spread: median ~10.19 dB, max ~12.34 dB
```

whereas v5 was approximately ~1.11 dB median / ~1.83 dB max in the same band. One v6 run looked nearly normal while four were far away. That makes the complete stored-gain-zero state unacceptable even though low-level playback/idle lifecycle does not fault.

Reviewed summary:

```text
artifacts/reviewed/2026-08-16-csrgain0-v6-exact-acoustic-summary.json
```

Full raw captures and analysis remain retained on SP7 with hashes recorded in the summary.

## Interpretation

The assumption that `CSR_GAIN[5:1]` becomes irrelevant when `CSR_GAIN_EN=0` is false on this current Linux hardware/producer state. The field is either still coupled into the effective DRE/compander behavior in silicon or is serving as a proxy for another producer/consumer state dependency that Windows satisfies and Linux still does not.

This also explains why the older bundled raw `DRE_CTL_1=0x00` experiment could be dangerous: exact register equality is not sufficient when the surrounding producer/PA state is not semantically identical.

## Decision

- **Reject v6. Do not promote or re-arm for ordinary playback.**
- Keep v5 (`CSR_GAIN_EN=0`, stored CSR code 7) as the current best H03 experimental state: bounded-safe, demand-driven at idle, and directionally closer to Windows in the matched stable band.
- Do not sweep arbitrary CSR gain codes acoustically; that would optimize around an unexplained coupling instead of matching Windows.
- Next target is to explain why `CSR_GAIN` affects response with `CSR_GAIN_EN` clear: inspect WSA8845 DRE/CSR register semantics and any remaining Windows/Linux producer-side state that gates that path, using existing qcaucd/static traces rather than unsafe MMIO.
