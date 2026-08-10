# SP11 CPS transport reconstruction

Date: 2026-08-10 (Europe/London)

> **Status: transport candidate rejected.** The split-mask design below was
> tested and caused SoundWire bus clashes. It must not be deployed. See
> `docs/deployment/2026-08-10-audio-cps-lab-candidate.md` for the runtime result
> and `docs/findings/2026-08-10-qcslimbus-max34417-cps-closure.md` for the
> repository-wide closure and the exact missing Windows runtime evidence.

## Result

CPS is not another lane in the 48 kHz speaker playback stream.  The recovered
Windows graph gives it an independent source endpoint:

```
CODEC_DMA_SOURCE 402b -> DATA_LOGGING 402a -> MUX_DEMUX 4029
                         -> CPS_DATA_ROUTER 4028
4028:80000000 <-> 4027:80000001, INTENT_ID_CPS 08001537
```

The endpoint contract recovered from the Windows configuration is fixed-point
S32_LE, 24 kHz, two channels/mask `0x3`, LPAIF WSA interface index 3.  Qualcomm
AFE numbering maps WSA interface 3 to `WSA_CODEC_DMA_TX_1` (AFE port `0xb003`).
The SP11 SoundWire table independently assigns shared master port 13 an
interval of 800 bus clocks (`19.2 MHz / 800 = 24 kHz`).

## Live falsification of the old model

The 2026-08-10 power-lab boot temporarily placed both amplifier CPS ports in
DAI 0, alongside the 48 kHz render payload.  Playback selected mask `0x2f`
(DAC, COMP, BOOST, PBR and CPS) and five ports per amplifier.  Enabling both
CPS switches immediately produced SoundWire bus-clash interrupts on both
WSA8845 amplifiers.  Disabling CPS restored clean playback.  A subsequent
PBR plus VISENSE test remained clean, isolating CPS-in-DAI0 as the failed
assumption rather than PBR or the established VI feedback path.

## Linux design selected for the next isolated boot

- Keep DAI 0 at 48 kHz for DAC/COMP/BOOST/PBR only.
- Keep DAI 1 at 8 kHz for two-channel VISENSE on `WSA_CODEC_DMA_TX_0`.
- Add DAI 2 at 24 kHz for CPS on `WSA_CODEC_DMA_TX_1`.
- Treat amplifier CPS data port 6 as a SoundWire source, not a sink.
- Give the left and right CPS ports disjoint channel masks `0x1` and `0x2`.
  The shared master port 13 then coalesces to the Windows two-channel mask
  `0x3`, instead of scheduling two overlapping two-channel transports.
- Bind topology instance `0x402b` to integrated backend ID 108
  (`WSA_CODEC_DMA_TX_1`) and couple that backend into the protected render
  graph through DAPM, mirroring the already proven VI bridge.
- Use a distinct card model/topology filename for CPS-Lab so the installed
  clean boot and clean topology remain the fallback.

## Scope and remaining proof

This closes the missing host/ASoC/SoundWire/backend path from the amplifiers to
the already reconstructed DSP CPS modules.  It does not by itself prove CPS
algorithm behaviour, long-duration high-volume safety, or exact Windows
loudness.  Those require the isolated reboot to show: no bus clash, all three
backend streams prepared at 48/8/24 kHz, the `0x402b` topology backend active,
and stable protection telemetry during controlled playback.
