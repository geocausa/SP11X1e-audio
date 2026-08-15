# WSA active runtime producer state on canonical CPS-v3

Date: 2026-08-15  
Status: CLOSED for Linux active-state observation; Windows producer-gain semantics remain OPEN

## Why this capture was needed

The rejected Linux `DRE_CTL_1=0` cold-boot candidate proved that steady-state WSA8845 register parity is not enough. Previous work had already closed:

- COMP/DP2 slave mask, interval and offsets;
- Denali left/right COMP master-port routing;
- WSA-macro first-MCLK reset/default state;
- first `regcache_sync()` as an overwrite mechanism;
- producer-power ordering before PA unmute;
- a hidden qcadcm runtime codec-register payload at Windows speaker start.

The missing Linux evidence was the **actual WSA-macro register transition during the production PipeWire/DAPM activation**, on the exact accepted CPS-v3 binary.

## Source-provenance gate

A module-only baseline rebuild from the evolved reconstructed WSA-macro source did **not** reproduce the loaded CPS-v3 module:

- production loaded srcversion: `F681186BB3D24B32621905D`;
- rebuilt preserved-reconstruction srcversion: `EB4504CE396F67BCB14678E`.

The original Aug-11 CPS-v3 `.ko` with `F681...` still exists, but the build-tree source symlink has since moved to an evolved source snapshot. Multiple preserved `lpass-wsa-macro.c` copies were rebuild-scanned and none reproduced `F681...`.

Therefore no rebuilt WSA-macro module was deployed for this experiment. The observation instead used dynamic kprobes against the **currently loaded exact `F681...` production binary**.

## Capture method and safety

The one-boot diagnostic observed only existing ASoC APIs:

- `snd_soc_component_update_bits(component, reg, mask, val)`;
- `snd_soc_component_write(component, reg, val)`.

It filtered the WSA-macro register aperture and recorded stack traces. It did not:

- play audio;
- change ALSA, PipeWire or WirePlumber controls;
- read codec regmaps/debugfs/MMIO;
- load a replacement audio module;
- alter DRE/CSR state.

The service ran for 35 seconds and removed its own probes. Three stale read-only lifecycle-marker probes from the earlier passive diagnostic were also removed after capture. Tracefs was left with tracing off, stacktrace off and no `sp11_wsa_*` probe definitions.

Raw reviewed evidence:

- `artifacts/reviewed/2026-08-15-wsa-runtime-write-boot.trace`
- SHA-256 `9f7732546d805dcb5d0add234173257ea2a2d995f3de7de0584405538c06537d`
- size `193358` bytes
- metadata SHA-256 `bce5f24e288bfd0805dbdca51b68a16ce4f970d2da36a3e40720dcef7f187220`
- boot ID `b4d1cd65-2535-4cf5-b0de-0b5d52353839`
- kernel `7.1.5-sp11-cps-v3+`
- WSA-macro srcversion `F681186BB3D24B32621905D`.

The capture contains 190 filtered `snd_soc_component_update_bits()` calls, zero `snd_soc_component_write()` calls, and every update uses the same component pointer `0xffff000801517480`, identifying one WSA-macro component.

## Production PipeWire activation timeline

The permanent PipeWire activation is cleanly separated from an earlier WirePlumber probe/open cycle:

```text
13.316321  first WSA8845 hw_params
13.316426  SoundWire prepare
13.320801  SoundWire enable
13.323787  WSA macro interpolator PRE_PMU left
13.323799  WSA macro interpolator PRE_PMU right
13.323816  WSA macro interpolator POST_PMU left
13.323833  WSA macro interpolator POST_PMU right
13.323848  WSA8845 speaker POST_PMU left
13.323862  WSA8845 speaker POST_PMU right
13.353602  first real WSA8845 mute_stream(mute=0)
```

The final WSA-macro POST_PMU therefore completes **29.769 ms before first WSA8845 unmute**. This independently confirms that producer timing is not the cause of the COMP-only failure.

## Actual active producer transition

The canonical production binary performs the following material runtime changes for both channels before PA unmute:

- enables the RX path clock;
- enables SmartBoost in RX path configuration;
- enables BOOST0/BOOST1 path clocks;
- enables both compander blocks and pulses their reset bits;
- sets the RX0/RX1 `RX_PATH_CFG0` COMP enable bit;
- enables the RX-path DSMDEM state;
- enables the half-dB PGA bit on both primary and mix paths.

Examples from the exact trace:

```text
0x0500 mask 0x10 -> 0x10   BOOST0 path
0x0540 mask 0x10 -> 0x10   BOOST1 path
0x0580 mask 0x01 -> 0x01   COMPANDER0 enable
0x05e0 mask 0x01 -> 0x01   COMPANDER1 enable
0x0404 mask 0x02 -> 0x02   RX0 COMP enable
0x0484 mask 0x02 -> 0x02   RX1 COMP enable
0x0428 mask 0x01 -> 0x01   RX0 primary half-dB PGA
0x0444 mask 0x01 -> 0x01   RX0 mix half-dB PGA
0x04a8 mask 0x01 -> 0x01   RX1 primary half-dB PGA
0x04c4 mask 0x01 -> 0x01   RX1 mix half-dB PGA
```

The reset/default compander coefficient registers are not rewritten during this activation; the active transition is principally clock/reset/path/gain-state enabling around the already-captured defaults.

## Reopened macro gain item: Linux -3 dB safety cap

Before the producer activates, UCM/WirePlumber writes:

```text
0x0414 RX0_RX_VOL_CTL = 0xfd
0x0494 RX1_RX_VOL_CTL = 0xfd
```

The live ALSA controls report:

```text
WSA WSA_RX0 Digital Volume: value 81, max 81
WSA WSA_RX1 Digital Volume: value 81, max 81
dB scale: -84 dB + 1 dB/step
```

Therefore value 81 is **-3 dB**, and raw signed register value `0xfd` is consistent with -3.

This must no longer be treated as Windows-proven parity. The generic X1E Linux machine driver itself says the Digital Volume limit was introduced "to reduce the risk of speaker damage until we have active speaker protection in place." SP11 now has active Windows-matched protection, but the -3 dB limit was retained historically.

The first-MCLK observation showed the common firmware/silicon RX volume pre-state is `0x00`, while Linux later applies `0xfd` through its own policy. Windows uses the same `qcadsp8380.mbn`, and the fresh qcadcm playback trace found no host hardware-resource table that applies a WSA-macro gain write. This makes 0 dB the leading Windows-equivalent hypothesis, **but not yet a direct Windows register observation** because the Windows WSA macro aperture is not APPS-readable and the firmware may still alter it internally.

Accordingly the Linux macro -3 dB state is reopened as an AMBER parity item, not changed yet.

## Half-dB/gain-offset item

The production binary also explicitly enables the WSA-macro half-dB PGA bits on both primary and mix paths whenever the compander producer is active. The reconstructed generic driver associates this with its `WSA_MACRO_GAIN_OFFSET_M1P5_DB` policy. The active writes themselves are proven by the exact binary; Windows equivalence of that policy is not.

This is another producer-level AMBER item. It must not be changed simultaneously with macro Digital Volume or DRE/CSR because doing so would destroy causal isolation.

## Current conclusion

The unsafe DRE-disabled result is no longer plausibly explained by producer startup timing, reset-state overwrite, DP2 routing/channel count or a hidden host resource table. The remaining gap is concentrated in **active WSA-macro producer gain/COMP semantics and its contract with WSA8845 DRE**.

No new `CSR_GAIN_EN=0` candidate is permitted until a producer-side variable is isolated. The next safe candidate, if Windows evidence remains indirect, should change at most one producer gain variable while retaining the known-good CSR-assisted amp mode and using low endpoint level plus protection telemetry.
