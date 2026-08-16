# SP11 WSA producer: Windows disproves Linux generic HD2 compensation

Date: 2026-08-16  
Status: **direct active-state mismatch; next one-variable candidate**

## Context

The `winproducer-init-v2` candidate added only two Windows/vendor-corroborated init corrections on top of the previous combined producer candidate: RX0/RX1 CFG1 base bit `0x08`, and `TOP_CFG1=0x03`. The candidate booted one-shot with WSA srcversion `A37D41B73EBF11162C751E2`; its initramfs was unpacked and byte-verified with the exact known RX84 X1E module, and CPS-v3 remained the persistent fallback.

With the WSA runtime device active, a read-only Linux regmap snapshot during the deterministic chirp at endpoint 12% and RX84/0 dB proved that the large producer state now agrees with the passive native Windows `qcaucd` corpus: TOP_CFG1 `03`, RX CFG1 `ef`, CFG2 `8f`, primary half-dB register `08`, endpoint RX volume `00`, DSMDEM `01`, boost path clock `10`, Surface curve bytes, VBAT/BCL state, softclip clocks, and CB_DECODE state all line up.

## Remaining direct RX mismatch

Linux still reports:

- RX0 `CFG0 0x0404 = 0x06`;
- RX1 `CFG0 0x0484 = 0x06`;
- RX0 `SEC3 0x0430 = 0x11`;
- RX1 `SEC3 0x04b0 = 0x11`.

Windows's marked native speaker lifecycle writes `CFG0=0x02` for each RX path and never sets bit `0x04`. The complete 330-transaction Windows corpus contains no accesses to RX0/RX1 SEC3 `0x0430/0x04b0`.

Mainline Linux identifies the extra bit exactly: `CDC_WSA_RX_PATH_HD2_EN_MASK = BIT(2)`. `wsa_macro_hd2_control()` unconditionally programs SEC3 alpha/scale to `0x10|0x01` and enables CFG0 bit `0x04` whenever the primary interpolator powers up. The older generic Qualcomm vendor driver contains the same HD2 routine, but the SP11 Windows implementation does not exercise it. Therefore generic Qualcomm behavior is not the oracle here; native SP11 Windows is.

The compander enable is the other active CFG0 bit (`0x02`) and must remain. The narrowly scoped Windows-parity target is therefore `CFG0=0x02` with no SEC3 HD2 programming.

## Protection-side observation

The same snapshot found WSA macro TX speaker-protection path-control registers at their v2.5 defaults. That is not being folded into the HD2 experiment. Independent retained evidence already proves both WSA8845 DP5 VISENSE source streams at Windows width/rate, VI feedback ready, SP/SPVI accepted, and the DSP protection graph not taking its bypass branch. Any WSA-macro TX lifecycle difference deserves a separate evidence-bound audit; changing it together with HD2 would destroy the one-variable discriminator.

## Next gate

Build `winproducer-nohd2-v3` from v2 with only `wsa_macro_hd2_control()` disabled for this SP11 candidate. Before acoustic testing, an active read-only regmap check must show:

- RX0/RX1 CFG0 `0x02`, not `0x06`;
- RX0/RX1 SEC3 no HD2 `0x11` programming;
- all other v2 Windows-matched producer bytes unchanged.

Only then run the synchronized acoustic oracle. DRE/CSR-off remains out of scope.
