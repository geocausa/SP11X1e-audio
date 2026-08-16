# Windows WSA compander curve isolation on RPV4/RX84

Date: 2026-08-16  
Status: REJECTED as a standalone parity improvement; safe negative experiment

## Question

The passive Windows qcaucd trace proved that the SP11 Windows producer actively programs a Surface-specific WSA v2.5 compander curve rather than the generic Qualcomm mainline defaults. Does changing only those recovered compander defaults on the proven RPV4 + RX84 stack improve the synchronized Windows acoustic residual?

## Candidate

`rpv4-macro84-wincomp-v1` retained:

- render-parity-v4 kernel and protected AudioReach/Dolby path;
- directly Windows-proven RX84 / 0 dB;
- the existing safe CSR-assisted WSA8845 lifecycle;
- the existing Linux M1P5 half-dB policy for this isolation step;
- all protection, WSA8845 init and SoundWire policy.

It changed only the directly recovered Windows compander defaults on both macro channels:

```text
CTL7   0x28 -> 0x2e
CTL11  0x12 -> 0x0c
CTL12  0x1e -> 0x15
CTL13  0x24 -> 0x15
CTL14  0x24 -> 0x15
CTL15  0x24 -> 0x15
CTL16  0x00 -> 0x0f
```

The signed module loaded successfully with live sysfs srcversion `0EAC059B39C96C5E3D44237`. The exact RX84 X1E module was force-loaded from the same initramfs. Persistent GRUB fallback remained `sp11-audio-cps-v3`.

## Safety result

At endpoint 12% with RX0/RX1 pinned to 84, four bounded deterministic chirp playbacks completed with:

- no new WSA/PA fault;
- no new SoundWire error;
- no XRUN/underrun/overrun;
- no new runtime DSP/GPR failure attributable to playback;
- no ugly/unsafe acoustic behavior.

DRE/CSR was not changed.

## Acoustic result

Three SP7 external-mic captures used the same geometry and exact 40 Hz -> 16 kHz / -24 dBFS deterministic chirp as the synchronized Windows/RX84 oracle.

The old ridge method was reconstructed from the saved evidence: channel 1, Hann STFT length 8192, hop 1024, nearest exponential-chirp ridge, mean of the roughly seven frames in each +/-1/24-octave band, then normalization by the 1--5 kHz median. As a validation, this implementation reproduces the known RX84 baseline as approximately `0.182 dB MAE / 0.208 dB RMSE` over 1--5 kHz, close to the earlier independently saved ~0.22/~0.26 ranking and preserving RX84 as the best old gain candidate.

Using exactly that extractor:

```text
RX84 generic-curve baseline vs Windows, 1--5 kHz:
  MAE  ~0.182 dB
  RMSE ~0.208 dB

RX84 Windows-curve-only median vs Windows, 1--5 kHz:
  MAE  ~0.644 dB
  RMSE ~0.704 dB

Stable-bin subset (run-to-run spread <=2 dB):
  MAE  ~0.649 dB
  RMSE ~0.716 dB
```

The wider 630 Hz--6.3 kHz band likewise worsened from roughly `0.184/0.214 dB` to `0.563/0.631 dB` MAE/RMSE.

Therefore the Windows compander curve **alone** is not a parity improvement when left paired with Linux's generic M1P5 half-dB policy.

## Interpretation

This negative result does not invalidate the Windows trace. Windows directly proves both:

1. the Surface-specific compander curve; and
2. primary `CDC_WSA_RX_PGA_HALF_DB` bit 0 **clear** on both RX paths.

The earlier HalfDB0 experiment tested half-dB-off on the *generic* curve and was also negative. The two Windows-proven producer properties therefore have a plausible coupled effect: each isolated against the wrong companion state is worse, while Windows uses them together.

## Next step

Build one bounded candidate that combines:

- RX84 / 0 dB;
- the recovered Windows compander curve;
- primary RX0/RX1 half-dB bit off;
- unchanged safe CSR-assisted WSA8845 lifecycle.

Do not alter mix-path half-dB registers without evidence, and do not retry `DRE_CTL_1=0` until producer parity is stable.
