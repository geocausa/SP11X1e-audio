# Windows WSA8845 DRE_CTL_1 lifecycle closes the Linux CSR-gain ambiguity

Date: 2026-08-15 (Europe/London)

## Result

`DRE_CTL_1` (`0x34b1`) is a real Windows/Linux codec-state mismatch in the
pre-candidate Linux stack. Windows initializes both WSA8845 amplifiers to
`0x34b1 = 0x00` and does not rewrite that register during ordinary PA
start/stop. Linux instead reached `0x34b1 = 0x0f` on both amplifiers because
SP11 UCM programmed `PA Volume 24` and the generic WSA884x unmute path set
`CSR_GAIN_EN`.

The previous statement that this difference was harmless because the
compander kept `CSR_GAIN_EN` clear is therefore false for the deployed Linux
driver. Live Linux readback has bit 0 set.

## Fresh KDNET proof

SP7 ran the classic Windows Kits kernel debugger over the existing SP11 KDNET
EEM link. On this boot:

- `qcaucd8380.sys` base: `0xfffff80285130000`
- direct slave-write helper: RVA `0x1bf80`, live
  `0xfffff8028514bf80`
- codec command FIFO: `0x06b15020`

The breakpoint filtered the packed codec command to `0x34b1` and four known PA
lifecycle registers. Two complete idle -> play -> stop cycles used the exact
local Seven Nation Army MP3 at endpoint 12%.

Runtime results:

| register | runtime hits |
|---|---:|
| `0x34b1 DRE_CTL_1` | **0** |
| `0x34d0 CLSH_CTL_0` | 8 |
| `0x3067 PWRSTG_DBG` | 8 |
| `0x304d PDRV_HS_CTL` | 8 |
| `0x3430 PA_FSM_EN` | 8 |

The positive controls reproduced the recovered Windows transition, including
`34d0=67 -> 3067=08 -> 304d=52 -> 3430=01 -> 3067=0c -> 304d=5a`, then
`3430=00 / 34d0=00` on stop. The helper was therefore definitely active while
`0x34b1` remained cold.

Raw KD log SHA-256:

`953ef108f0f821af860d15e488da4990be9bb2e240b8bb13f113e1752228bb6e`

The retained `CODEX_QCAUCD_V2CMD` initialization independently writes
`0x34b1=0x00` to logical devices 1 and 2. Available qcaucd static-disassembly
text contains no other `0x34b1` writer.

## Linux cause

The pre-candidate live state was `0x34b1=0x0f` on both amplifiers.

Two independent Linux mechanisms caused it:

1. SP11 UCM set `SpkrLeft/Right PA Volume` to 24. The ALSA control is inverted,
   so this populates CSR_GAIN bits 5:1 with raw code 7 (`0x0e`).
2. `wsa884x_mute_stream()` set `CSR_GAIN_EN` bit 0 on unmute, producing
   `0x0f`.

A bounded live control check confirmed ALSA value 31 clears the gain field but
still leaves bit 0 set (`0x01`), proving both mechanisms must be corrected.

## Candidate correction

Patch:

`patches/0055-ASoC-wsa884x-match-Windows-DRE-CTL1-lifecycle.patch`

It:

- writes `DRE_CTL_1=0x00` in the existing SP11/state-2 Windows codec init;
- keeps `CSR_GAIN_EN=0` on the 2S Surface unmute path;
- preserves generic non-2S behavior.

The matching SP11 UCM removes all `SpkrLeft/Right PA Volume` writes. Endpoint
volume remains the already-proven AudioReach final `VOL_CTRL`/GainStep path.

No EQ, fade, limiter, PBR or topology behavior is guessed by this correction.
