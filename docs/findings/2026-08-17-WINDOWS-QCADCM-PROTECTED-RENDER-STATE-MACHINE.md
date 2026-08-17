# Windows QCADCM protected-render graph-open and lifecycle oracle — 2026-08-17

## Result

Fresh KDNET on the SP11 Windows installation, followed by hash-matched Ghidra analysis of the exact live `qcadcm8380.sys`, closes the ordinary internal-speaker graph-open control sequence at semantic level. This is not a guessed AudioReach template: the runtime callsites, tag IDs, module-parameter headers, custom-event registrations, graph commands, and pause/stop transitions were captured on the real machine and then mapped back to named qcadcm routines.

Hash gate:

- `qcadcm8380.sys`: `37F76305AC8051B0B03B6D2CE1DF7A353253DEBF546E512E447C9D95EC661429`
- `qcaucd8380.sys`: `BD0C8276C51FC7A020C616E904DD613B6CCF187EC3E1FE6F94C2C811C8ADC8BF`

Controlled target markers for the final graph-open capture were CREATE `12:27:29.3476`, PLAY `12:27:31.0193`, STOP `12:27:36.0418`, RELEASE `12:27:36.1669`, DONE `12:27:39.0991` Europe/London.

## Exact custom-event registration

`AudioDspGraphOpen` registers three runtime events before the graph is started.

1. Pull watermark: GSL command `0x11`, MIID `0x4660`, event `0x0800101c`, extra payload size `0x0c`, register flag `1`, event payload `{2, 0x780, 0xf00}`.
2. SOFT_PAUSE completion: GSL command `0x11`, MIID `0x466b`, event `0x0800103f`, extra payload size `0`, register flag `1`.
3. SOFT_RESUME completion: GSL command `0x11`, MIID `0x466b`, event `0x08001043`, extra payload size `0`, register flag `1`.

The last two registrations are statically emitted in that order. Ghidra resolves `0x0800103f` to the failure-labelled `PAUSE_COMPLETE` registration and `0x08001043` to `RESUME_COMPLETE`. This independently confirms the already recovered Linux SOFT_PAUSE event identities.

The command-11 wrapper is `gsl_graph_register_custom_event`: dword 0 is MIID, dword 1 event ID, dword 2 optional event-payload length, dword 3 registration flag, followed by optional event bytes.

No explicit unregister of the two SOFT_PAUSE event constants was found at the same qcadcm callsites. Do not invent one: graph destruction may own that lifetime, but that is not required for the ordinary persistent render cycle observed here.

## Graph-open tagged/custom configuration order

The final runtime order is:

| Order | Tag | Parameter / operation | Recovered meaning |
|---:|---|---|---|
| 1 | `0x04000001` | `0x0800100c` | shared-memory endpoint PCM media format (`SetStreamDataFormat` / `CreateShmemEpMediaFormatPayload`) |
| 2 | `0x04000003` | `0x08001008` | decoder/encoder media format (`SetStreamDataFormat`) |
| 3 | `0x04000005` | `0x08001008` | PCM-converter output media format (`SetStreamDataFormat`) |
| 4 | `0x04000007` | `0x08001024` | stream sink/source MFC media format (`SetStreamDataFormat`) |
| 5 | `0x0400000a` | `0x08001024` | mix sink/source MFC media format (`SetMixDataFormat`) |
| 6 | `0x04000029` | `0x08001130` | Audiosphere configuration (`SetMixDataFormat` / `CreateAudiospherePayload`) |
| 7 | `0x0401000a` | `0x080011e9` | speaker-protection RX operating-mode payload (`CreateSpkrProtRxPayload`) |
| 8 | `0x0401000a` | `SET_CFG` | apply speaker-protection RX tag/TKV |
| 9 | `0x0401000b` | `0x080011f5` | speaker-protection VI R0/T0 configuration (`CreateSpkrProtR0T0ViPayload`) |
| 10 | `0x0401000b` | `0x080011f4` | speaker-protection VI operating-mode payload (`CreateSpkrProtVIPayload`) |
| 11 | `0x0401000b` | `0x080011ff` | speaker-protection excursion/ExVI mode payload (`CreateSpkrProtViExModePayload`) |
| 12 | `0x0401000b` | `SET_CFG` | apply speaker-protection VI tag/TKV |
| 13 | `0x04010005` | `SET_CFG` | VI endpoint hardware/effect configuration (`SetSpkrProtEffectEpConfig`); runtime TKV is built from sample rate, bit width and channel count |
| 14 | `0x04010003` | `SET_CFG` | source/sink endpoint hardware-interface configuration, endpoint-category 1 (`SetSrcSinkEpHwIfCfg`) |
| 15 | `0x04010009` | `SET_CFG` | device/audio orientation (`SetAudioOrientation` / `SetMSPPDeviceOrientation`) |

The endpoint-HW tag family is selected by qcadcm's endpoint-category helper: category 1 -> `0x04010003`, category 2 -> `0x04010004`, category 3 -> `0x04010005`. The observed normal speaker source/sink uses category 1; the speaker-protection VI endpoint setup uses category 3.

This corrects an earlier over-broad description of `0x04010003` as the speaker-protection VI endpoint configuration. The actual VI endpoint configuration callsite uses `0x04010005`; `0x04010003` is the ordinary category-1 source/sink endpoint HW-interface tag.

## Start, pause, stop and persistent graph lifetime

The GSL command wrapper maps:

- command `0x5` -> `gsl_graph_add_new`;
- command `0x6` -> `gsl_graph_remove_old`;
- command `0x0` -> `gsl_graph_start`;
- command `0x4` -> `gsl_graph_stop_with_properties`;
- command `0x11` -> `gsl_graph_register_custom_event`.

The captured graph is added/configured, then START uses command `0x0`. The host state correlation is exact:

- KS RUN/state 3 -> qcadcm graph state 4 -> GSL START `0x0`.
- KS PAUSE/state 2 -> qcadcm graph state 3 -> `SetStreamPause(3)` -> tag `0x04010008`, key `0x01000021=1` -> DEFAULT iid `0x466b`, pid `0x0800102e`, zero-length payload -> wait for `0x0800103f` PAUSE_COMPLETE.
- KS STOP/state 0 after an outstanding pause -> qcadcm graph state 1 -> GSL STOP `0x4` -> `SetStreamPause(4)` -> tag `0x04010008`, key `0x01000021=0` -> iid `0x466b`, pid `0x0800102f`, zero-length payload -> `0x08001043` RESUME_COMPLETE closes pause state.

In the final ordinary playback/release capture there was no GSL remove-graph command `0x6` in the observed release window. The QCADCM protected render graph therefore survives the ordinary application stream close in this scenario while qcaucd/WSA/SoundWire independently execute their lower transport/PA idle lifecycles. This distinction matters for Linux parity: do not conflate application close, DSP graph destruction, SoundWire dataport teardown, and physical PA shutdown.

## Speaker-protection parameter closure

For tag `0x0401000a`, the live `0x080011e9` payload is built by `CreateSpkrProtRxPayload`; its body carries the selected speaker-protection RX/calibration operating mode before the tag is applied with `gsl_set_config`.

For tag `0x0401000b`, the three live parameter IDs are now named from the exact creators:

- `0x080011f5`: R0/T0 values sent by `CreateSpkrProtR0T0ViPayload`;
- `0x080011f4`: VI operating-mode payload sent by `CreateSpkrProtVIPayload`;
- `0x080011ff`: excursion/ExVI operating mode sent by `CreateSpkrProtViExModePayload`.

Conditional FTM/calibration and SPv5 speaker-diagnostics event branches also exist in `SetSpkrProtEpEffect`, but they were not part of this normal render graph-open transaction and are not promoted into the ordinary lifecycle.

## Linux consequence

The remaining implementation work is no longer to guess a generic Windows graph. The ordinary Windows protected-render control plane is sufficiently pinned to use as a regression oracle:

1. preserve the already-live persistent protected pull graph and exact SOFT_PAUSE lifecycle;
2. keep graph-open media-format/protection ordering fixed as above;
3. treat QCADCM graph lifetime separately from qcaucd SoundWire and WSA8845 ordinary idle lifetime;
4. implement the independently recovered exact WSA8845 cold-init/10-write START/6-write STOP state machine instead of replaying cold-only analog/DRE/watchdog state on every Linux PA cycle;
5. validate with the same two real MP3 cycles plus idle, checking both DSP lifecycle and physical WSA/SoundWire write history.

Machine-readable companion: `artifacts/reviewed/2026-08-17-windows-qcadcm-protected-render-state-machine.json`.
