# UbiG volume-law localization saved — 2026-08-23

Status: **checkpoint only; UbiG promotion remains blocked**.

Source-owned UbiG work is saved on `ubig/deblob-main` at:

`b23c478 ubig: correct live volume law and VR VLLDP order`

That checkpoint records two confirmed UbiG defects and fixes:

1. VLLDP endpoint postgain must follow live Windows endpoint volume (`round(endpoint_dB * 16)`) rather than remain frozen for one filter-chain generation.
2. The native candidate sample dependency must be `VR -> VLLDP`, matching later Windows full-memory RE and the proprietary Golden bridge; the deblob candidate had regressed to the obsolete `VLLDP -> VR` order.

A dedicated candidate-order regression now rejects the old high-volume signature and passes the corrected Windows-like 500/997-Hz law.

The corrected source-owned chain matches the proprietary Golden VR->VLLDP oracle to thousandths of a dB across the 10..50% volume sweep. The authoritative physical sweep is correspondingly tight above 315 Hz, with adjacent-volume MAE approximately 0.19 / 0.06 / 0.05 / 0.07 / 0.04 dB for 10->15->20->30->40->50%.

One residual remains at 160 Hz at high endpoint volume. A reversible live production-path MSIIR test at fixed 40% forced CKV12 -> CKV16 -> CKV12 through the actual TLV control. The physical 160-Hz / 1-kHz ratio moved by -1.121 dB versus an approximately -1.25 dB decoded filter prediction and returned near baseline after CKV12 restore. Therefore the remaining 160-Hz question is localized downstream of VR/VLLDP and functionally working MSIIR, in the WSA8845 / CPS / amplifier / actuator region.

Do not compensate this residual with guessed EQ or alter CKV/MSIIR calibration without new boundary evidence.

Promotion gate remains unchanged:

- do not declare UbiG Golden;
- do not retire the Windows bridge;
- do not mutate protected Golden-v32 kernel/topology/UCM/GRUB baseline;
- subjective A/B remains postponed until the downstream 160-Hz residual is dispositioned.
