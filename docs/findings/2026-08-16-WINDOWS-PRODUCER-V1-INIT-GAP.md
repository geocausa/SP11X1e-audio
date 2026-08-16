# RPV4 RX84 Windows-producer v1: acoustic failure exposes missing WSA init state

Date: 2026-08-16  
Status: **v1 not promotable; two additional Windows/vendor-corroborated init gaps identified**

## v1 scope

`sp11-audio-rpv4-macro84-winproducer-v1` combined all producer differences already isolated earlier: Windows RX84/0 dB, the passively recovered Surface compander curve, primary half-dB off, legacy VBAT/BCL lifecycle, v2.5 CB_DECODE, and the existing safe CSR-assisted WSA8845 state. No DRE/CSR change was made. The one-shot boot loaded WSA srcversion `269A524D9743AD9FAA72A6B`; the initramfs was unpacked and byte-verified before boot and CPS-v3 remained the persistent fallback.

Five deterministic SP7 external-mic captures completed without a new WSA/PA/SoundWire/XRUN fault. The first two runs were already worse than the RX84 generic baseline, and later runs became strongly non-stationary. Five-run median over 1--5 kHz was about **0.496 dB MAE / 0.570 dB RMSE**, versus **0.182 / 0.208 dB** for RX84 generic. Individual 1--5 kHz MAE/RMSE were approximately A `0.573/0.726`, B `0.410/0.528`, C `2.792/4.375`, D `2.331/2.872`, E `2.898/3.500`. Therefore v1 is not a parity improvement and its late-run acoustic state is not acceptable as an oracle.

The large WAVs remain on SP7; hashes and the five-run analysis hash are recorded in `artifacts/reviewed/2026-08-16-rpv4-macro84-winproducer-v1-result.json`.

## Why v1 was not actually the complete Windows producer

Rechecking the full 330-transaction passive native `qcaucd` corpus found two steady-state WSA init values that v1 had not carried over:

1. **RX0/RX1 `RX_PATH_CFG1` bit `0x08`.** Linux regmap defaults both `0x0408` and `0x0488` to `0x64`. Windows enters the speaker lifecycle with both at `0x6d`; VBAT then performs the exact `6d -> ed -> ef` progression. Mainline Linux's normal channel-enable bit supplies bit0, but without bit `0x08` its corresponding base is `0x65`, so the v1 VBAT progression cannot equal Windows. Critically, the older Qualcomm vendor `wsa_macro_reg_init[]` independently sets mask/value `0x08/0x08` on both RX CFG1 registers. The non-speculative Linux correction is therefore to retain the existing bit0 lifecycle and change the regmap base from `0x64` to `0x6c`, yielding `0x6d` when the normal path is active.

2. **`CDC_WSA_TOP_TOP_CFG1 = 0x03`.** Current mainline Linux defaults `0x0084` to `0x00` and has no corresponding producer write in this driver. Windows writes `0x03` twice during the marked native speaker/protection sequence. The older Qualcomm vendor init table independently sets bit `0x02` then bit `0x01`, also producing `0x03`. This is especially relevant because the Windows writes occur amid TX speaker-protection/VI-path bring-up, so leaving Linux at zero may change the protection-side producer state even when the render path itself appears healthy.

Other entries in the old vendor init table are **not** being copied blindly: several are not corroborated by the SP11 Windows trace and some conflict with v2.5 values already observed. The next candidate is intentionally limited to these two independently corroborated init corrections on top of v1.

## Decision

Do **not** interpret the v1 acoustic failure as proof that the complete Windows producer state is wrong for Linux; v1 still omitted two proven Windows init settings. Build a v2 containing only:

- RX0/RX1 CFG1 default `0x64 -> 0x6c` (normal path bit0 then yields Windows `0x6d`), and
- TOP_CFG1 default `0x00 -> 0x03`.

Keep RX84, Surface curve, primary half-dB off, VBAT/BCL/CB_DECODE, protection, Dolby, and CSR-assisted WSA8845 behavior unchanged. Do not revisit DRE/CSR-off until this v2 is measured and its lifecycle is stable.
