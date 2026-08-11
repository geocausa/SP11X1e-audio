# Windows internal-speaker processing-mode ACDB family map — 2026-08-12

## Result

The hash-pinned Windows REV_0D ACDB contains exact six-key internal-speaker graph rows for five qcadcm processing values:

| Windows mode | qcadcm value | miniport flag | exact speaker row | graph family |
|---|---:|---:|---|---|
| RAW | 1 | `0x02` | **absent** | none in REV_0D for the exact speaker vector |
| DEFAULT | 2 | `0x01` | present | root + `0x7e/0x7f` |
| SPEECH | 3 | `0x08` | present | root + `0x7c/0x7d` |
| COMMUNICATIONS | 4 | `0x04` | present | root + `0x7a/0x7b` |
| MOVIE | 5 | `0x28` | **absent** | none in REV_0D for the exact speaker vector |
| MEDIA | 6 | `0x14` | present | root + `0x80/0x81` |
| NOTIFICATION | 7 | `0x0a` | present | root + `0x82/0x83` |

The exact speaker key vector is the reviewed six-key qcadcm graph-key form with render stream/mix type `2`, instance `1`, internal-speaker endpoint `1`, and stream/mix processing set to the mode value.

This is an ACDB lookup fact. It does **not** claim that every supported miniport processing-mode flag is selected by ordinary applications, and absence of RAW/MOVIE rows does not prove whether Windows rejects those explicit modes or falls back to another row. That behavior remains a live Windows gate.

## Exact ACDB rows

- DEFAULT: POOL `0x0003d164` → `0xb0000001`, `0xb000007e`, `0xb000007f`
- SPEECH: POOL `0x0003eca8` → `0xb0000001`, `0xb000007c`, `0xb000007d`
- COMMUNICATIONS: POOL `0x0003ffe8` → `0xb0000001`, `0xb000007a`, `0xb000007b`
- MEDIA: POOL `0x00041328` → `0xb0000001`, `0xb0000080`, `0xb0000081`
- NOTIFICATION: POOL `0x00042668` → `0xb0000001`, `0xb0000082`, `0xb0000083`

Every present speaker row contains 29 modules. The ACDB graph bundles contain the core graph structure but **zero control-link records**. This is significant: the extra runtime control links recovered from Windows GRAPH_OPEN (SP↔SPVI, CPS↔SP, timer drift, family EQ↔VOL) are added by the Windows runtime around the ACDB graph body. Therefore the control-link set for SPEECH/COMMUNICATIONS/MEDIA must not be synthesized from module symmetry; it needs live/runtime provenance.

## Exact additional calibration stages

`tools/acdb_render_family_stage_builder.py` now resolves exact shared-root + family calibration for all ACDB-present speaker modes. DEFAULT remains the byte-for-byte regression oracle.

Graph-calibration hashes:

- DEFAULT `7e/7f`: `2a654ffa7a4467c93ecfc64f380974df0bccdd5c67959ba6ac7c59a008358ca1`
- SPEECH `7c/7d`: `9d59bb40621a91c0ffeb012176f6da27814fbbd0e823090bd25aa6cc740cd6dc`
- COMMUNICATIONS `7a/7b`: `fd2c0c21a7f30dfc699c5f3b2fc3691f054c1b8ad69aa8d87c9bd91428fb8d51`
- MEDIA `80/81`: `db9223c7ac4e5d13446097158480db82d588e33fce4d0c0f9382583b716543c7`
- NOTIFICATION `82/83`: `abdd9ef1a683512c4575c600261ec7181d9ece6e46a7c419022cd65c0efeef09`

All are 10,464 bytes for root + the two family subgraphs under the reviewed runtime calibration CKV.

Family-local output-volume IIDs are:

- DEFAULT `0x4a63`
- SPEECH `0x4a62`
- COMMUNICATIONS `0x4a61`
- MEDIA `0x4a60`
- NOTIFICATION `0x4a5f`

Reviewed stage manifests were generated for SPEECH, COMMUNICATIONS, MEDIA and NOTIFICATION. They record all exact calibration parameter references and serialized stage hashes.

## Structural family identities

The additional Windows ACDB speaker bodies expose the expected role-equivalent chains but with distinct module/container identities:

### SPEECH `0x7c/0x7d`

- frontend `SH_MEM_PULL 0x4641`
- PCM converter `0x4640`
- EQ `0x4645`
- SPR `0x412f`
- output volume `0x4a62`
- MSIIR `0x48a2 -> 0x48a3`

### COMMUNICATIONS `0x7a/0x7b`

- frontend `SH_MEM_PULL 0x463c`
- PCM converter `0x4621`
- EQ `0x4625`
- SPR `0x4131`
- output volume `0x4a61`
- MSIIR `0x48a4 -> 0x48a5`

### MEDIA `0x80/0x81`

- frontend `SH_MEM_PULL 0x4826`
- PCM converter `0x4824`
- EQ `0x4683`
- SPR `0x4133`
- output volume `0x4a60`
- MSIIR `0x48a6 -> 0x48a7`

These structural facts come from the ACDB POOL graph bundles. Runtime-added control-link and lifecycle policy remains a separate evidence class.

## Runtime status / remaining gates

Live Windows evidence already closes:

- DEFAULT selection and full runtime family;
- NOTIFICATION selection and full runtime family;
- ordinary `AudioCategory=Media` and `AudioCategory=Movie` test clients selected DEFAULT, showing that application category names alone do not force the corresponding processing-mode values.

Still needed for a complete mode policy:

1. explicit SPEECH processing-mode selection/runtime GRAPH_OPEN;
2. explicit COMMUNICATIONS processing-mode selection/runtime GRAPH_OPEN;
3. explicit MEDIA processing-mode selection/runtime GRAPH_OPEN;
4. explicit RAW and MOVIE request behavior (fallback/rejection/other mapping);
5. A/B simultaneous DEFAULT+NOTIFICATION lifetime overlap/switch behavior.

Until those are captured, Linux may prebuild the exact ACDB-present family content but must not invent runtime mode-routing policy or control-link payloads for the uncaptured modes.

## Reproducibility

- decoder: `tools/windows_speaker_mode_family_inventory.py`
- reviewed inventory: `artifacts/reviewed/windows-speaker-render-mode-acdb-families.json`
- reviewed stage manifests:
  - `artifacts/reviewed/2026-08-12-speech-render-stages-manifest.json`
  - `artifacts/reviewed/2026-08-12-communications-render-stages-manifest.json`
  - `artifacts/reviewed/2026-08-12-media-render-stages-manifest.json`
  - `artifacts/reviewed/2026-08-12-notification-render-stages-manifest.json`

REV_0D source SHA-256 remains:

`a0a8635ba65127180a1caef46af61c00171c9a93cbf8b5f5650709b4638decde`

## Safety

This closure is offline ACDB decoding only. No DSP, SoundWire, driver-state, MMIO, boot, or audible test was performed.
