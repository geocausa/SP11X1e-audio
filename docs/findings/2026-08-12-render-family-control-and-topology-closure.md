# Render-family control-link correction and NOTIFICATION topology offline closure — 2026-08-12

## Result

The Linux protected-topology generator contained a real control-link parity defect that was hidden by an incorrect source comment. The accepted DEFAULT graph data path remains valid, but its generated control-link private data used the first reviewed Windows control-link record only. That record contains:

1. `SP_VI 0x4024 <-> SPEAKER_PROTECTION 0x4027` / `INTENT_ID_SP`;
2. `CPS router 0x4028 <-> SPEAKER_PROTECTION 0x4027` / `INTENT_ID_CPS`;
3. `CODEC_DMA_SINK 0x4157 <-> external RATE_ADAPTED_TIMER 0x40df` / timer-drift intent.

The family-local EQ/headroom control is in a separate Windows record. Therefore the old generator **included an unresolved external timer-drift peer and omitted the internal DEFAULT EQ↔VOL headroom link** `0x4664 <-> 0x4663 / 0x08001118`.

This finding fixes the generator offline and extends it to the exact Windows NOTIFICATION family. No deployment or audible Linux test occurred here.

## Correct Linux-baseline control set

`tools/windows_render_control_link_data.py` now serializes only reviewed structural-model links explicitly marked `baseline_disposition=admitted`.

DEFAULT admitted set:

- `0x4024 <-> 0x4027` — speaker-protection intent;
- `0x4028 <-> 0x4027` — CPS intent;
- `0x4664 <-> 0x4663` — EQ/volume headroom intent.

Payload SHA-256:

`5014748ab564e8a44fa55a8e8bc09e813d7e87fef2ee8bba9811decb02ee58cc`

NOTIFICATION admitted set:

- `0x4024 <-> 0x4027` — speaker-protection intent;
- `0x4028 <-> 0x4027` — CPS intent;
- `0x46a2 <-> 0x46a1` — EQ/volume headroom intent.

Payload SHA-256:

`46341ee100bd157cf61883e5e1b71e73e0581bf8b4f08372ddd8ae77794ab7e1`

The external `0x4157 <-> 0x40df` timer-drift link remains preserved in the structural evidence but is excluded from the Linux baseline until the corresponding speaker-loopback peer exists. This is an explicit evidence disposition, not deletion of the Windows fact.

## Exact NOTIFICATION calibration stages

`tools/acdb_render_family_stage_builder.py` is a family-aware wrapper around the accepted protection-stage builder.

- `--mode default` delegates to the accepted DEFAULT builder; every emitted stage binary remains byte-identical to the old implementation.
- `--mode notification` resolves root + `0xb0000082` + `0xb0000083` directly from the hash-pinned REV_0D ACDB.
- shared root/protection/VI/CPS/endpoint/channel-mixer stages remain shared.
- family-local output volume is IID `0x4a5f`.
- the `0x83` volume-dependent calibration contains the exact notification-specific MSIIR `0x48a9 / 0x08001022` payload closed in `2026-08-12-windows-notification-acdb-calibration-closure.md`.

NOTIFICATION graph-calibration stage:

- size `10464` bytes;
- SHA-256 `abdd9ef1a683512c4575c600261ec7181d9ece6e46a7c419022cd65c0efeef09`.

The reviewed notification stage manifest is:

`artifacts/reviewed/2026-08-12-notification-render-stages-manifest.json`

## NOTIFICATION container correction

The first family-aware topology build stopped on unknown container `0xe0000071`. The correct response was to decode the Windows container-config parameters, not clone DEFAULT containers.

The exact NOTIFICATION-only containers are:

| container | capability | stack | graph position | parent | heap |
|---|---:|---:|---:|---:|---:|
| `0xe0000046` | `0x0b001001` | 4096 | `0xffffffff` | `0xffffffff` | 1 |
| `0xe0000120` | `0x0b001001` | 4096 | 1 | `0xffffffff` | 1 |
| `0xe0000071` | `0x0b001001` | 1024 | `0xffffffff` | `0xffffffff` | 1 |

These values come from the reviewed `0x82/0x83` GRAPH_OPEN container records.

## Family-aware topology generator

`tools/build_sp11_protected_topology.py` now supports both reviewed structural models.

DEFAULT family:

- frontend `0x4660`;
- PCM_CNV `0x465f` with Windows interleave token `3`;
- output volume `0x4a63`;
- root SAL input 12.

NOTIFICATION family:

- frontend `0x469e`;
- PCM_CNV `0x469d` with Windows interleave token `3`;
- output volume `0x4a5f`;
- root SAL input 18.

The shared protected root and the accepted VI/CPS sidechains remain unchanged.

## Offline compile and parser gate

Both generated configurations compiled and decoded with ALSA topology tools on the Fedora helper:

`alsatplg 1.2.16 / libasound 1.2.16.1 / libatopology 1.2.16.1`

Generated hashes:

- corrected DEFAULT config: `906b9366bc63267a657d97d7e11588a7c95d92821ffe53eb5a3abfeebfe13b8a`
- corrected DEFAULT TPLG: `ebdbfb209b2874bcbdcdcdd91cd59a7aa34c409240deeaf8e28054e1f84b931c`
- NOTIFICATION config: `d60d31b790e50a4889f61e0976385dea29a3758391c9a8fdd6f5b7f241a0d2dc`
- NOTIFICATION TPLG: `dd6a8378f43fb2ffda7bce1ce52831644948a3a392136b4165bb82f42e09ea40`

`alsatplg` emitted diagnostics for the three external DAI streams (`WSA_CODEC_DMA_RX_0 Playback`, `WSA_CODEC_DMA_TX_0 Protection`, `WSA_CODEC_DMA_TX_1 Protection`) but compiled and decoded both files with exit code zero. Those names intentionally terminate at kernel/codec DAIs outside the topology widget namespace.

The repo inventory/linter found:

- no structural issues;
- no duplicate IIDs;
- 30 widgets;
- 32 DAPM graph edges for each family.

Machine-readable gate:

`artifacts/reviewed/2026-08-12-render-family-topology-offline-gate.json`

## Deployment status

**Not deployed.** The content side of NOTIFICATION is now closed offline, but one policy question still materially affects the correct Linux integration: whether Windows keeps DEFAULT (`7e/7f`) and NOTIFICATION (`82/83`) active concurrently during overlapping media + alert playback, or serializes/replaces the family around the shared root.

Existing QGPR traces prove independent complete lifecycles for both families but do not establish A/B simultaneous overlap. That is the next Windows runtime gate before selecting the Linux policy mechanism.

## Safety

No physical MMIO, SoundWire register write, DSP write, or arbitrary driver-state mutation was used. No Linux deployment or audible Linux test was performed during this finding.
