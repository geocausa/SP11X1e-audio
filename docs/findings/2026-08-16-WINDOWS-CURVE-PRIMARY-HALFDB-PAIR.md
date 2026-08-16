# Windows WSA curve + primary half-dB-off pair on RPV4/RX84

Date: 2026-08-16  
Status: REJECTED as a production parity improvement; safe informative negative

The passive Windows qcaucd trace directly proves two producer properties: a Surface-specific compander curve and primary RX0/RX1 `CDC_WSA_RX_PGA_HALF_DB` bit clear. Because each had previously been tested against the wrong companion state, this candidate combined them while retaining the safe CSR-assisted WSA8845 lifecycle.

The signed candidate loaded with WSA srcversion `354F5F37490E6A8DE668870`; the exact RX84 X1E oracle remained force-loaded. It changed the recovered Windows compander CTL7/CTL11-16 defaults on both channels and changed only the primary RX0/RX1 POST_PMU half-dB updates to DISABLE. Mix-path half-dB registers remained untouched because the Windows speaker trace does not establish them.

At RX84 / 0 dB and endpoint 12%, five bounded chirps completed with no new WSA/PA/SoundWire/XRUN/DSP faults and no unsafe acoustic behavior.

Using the reconstructed prior ridge method, the current RX84 baseline reproduces at about `0.182 dB MAE / 0.208 dB RMSE` over 1--5 kHz. The combined candidate's three-run median is about `0.376 / 0.452 dB`; stable-bin subset is about `0.360 / 0.446 dB`. Wider 630 Hz--6.3 kHz median is about `0.391 / 0.468 dB`.

Thus the pair is a substantial recovery from curve-only (`~0.644 dB MAE`) but remains clearly worse than the present RX84 generic-curve acoustic baseline. Do not promote it and do not combine it with DRE/CSR-off.

The result strongly suggests the Windows native 330-transaction WSA trace contains another material producer/lifecycle state not represented by merely changing regmap defaults plus the primary half-dB bit. The next work item is a transaction-level diff against the preserved full Linux WSA runtime trace, with particular attention to RX_PATH_CFG1, COMPANDER_CTL0 lifecycle, runtime speaker-mode writes, and softclip CRC/clock sequencing.
