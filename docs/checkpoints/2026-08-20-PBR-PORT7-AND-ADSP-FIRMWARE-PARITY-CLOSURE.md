# PBR port-7 shadow + ADSP firmware parity closure — 2026-08-20

Branch: `agent/psycho-bass-20260818`

## Port 7 / `0x06b11764` is not an active missing feedback route

The broad native-Windows WSA-master trace had one remaining unclassified Windows/Golden difference:

```text
0x06b11764 <- 0x00ff00c8
```

The SP11 master-port map independently identifies physical master port 7 as the shared **PBR** port.  VISENSE uses master ports 10/11 and CPS uses master port 13.

The preserved Windows command-FIFO trace shows the `0x11764` write is part of a batch of precomputed master timing tuples:

```text
0x06b11564 <- 0x0007041f
0x06b11664 <- 0x001f153f
0x06b11764 <- 0x00ff00c8   # port 7 / PBR
0x06b11a64 <- 0x00ff060f   # port 10 / left VISENSE
0x06b11b64 <- 0x00ff0d0f   # port 11 / right VISENSE
0x06b11d64 <- 0x00ff001f   # port 13 / CPS transition value
0x06b11e64 <- 0x00ff191f   # port 14 companion shadow
```

Only `0x06b11764` is written for port 7 in that capture. There is no corresponding port-7 bank-0/bank-1 ChannelEnable write.

This agrees with the independently decoded 328-record Windows WSA8845 slave FIFO: ordinary speaker playback contains **no positive DP4/PBR ChannelEnable/programming**; only a DP4 teardown write occurs on the right slave.

Therefore `0x11764` is an inactive PBR timing/template shadow, not evidence that Windows activates a feedback stream missing on Linux.  No Linux candidate is justified for this register.

## Active Linux uses the exact native-Windows qcadsp image

Golden Linux currently boots:

```text
/lib/firmware/qcom/x1e80100/microsoft/Denali/qcadsp8380.mbn
SHA256 921870a839ee2aba647b04598d62ed96f3d2d5dfbb2499fc842f9a6ff0e0da13
```

The preserved native-Windows/`denali-win` firmware image has the exact same SHA-256:

```text
921870a839ee2aba647b04598d62ed96f3d2d5dfbb2499fc842f9a6ff0e0da13
```

Thus CODEC_DMA, HWD4, and all qcadsp-side WSA/LPAIF code are byte-identical between the two OS boots.

## ADSP DT/devcfg image is also exact parity

Golden Linux:

```text
/lib/firmware/qcom/x1e80100/microsoft/Denali/adsp_dtb.mbn
SHA256 544bd795cb06cf8dee8119ede2a667f01066b2f1b9e4348f1772d080e2026ff4
```

Preserved native-Windows `denali-win/adsp_dtbs.elf`:

```text
SHA256 544bd795cb06cf8dee8119ede2a667f01066b2f1b9e4348f1772d080e2026ff4
```

So the DSP executable **and** its ADSP DT/devcfg image are exact Windows parity.

## Consequence

The Linux/Windows zero-vs-real-feedback divergence is not caused by different CODEC_DMA/HWD implementation code or a different ADSP devcfg image.

Together with the proven Linux HWD4 open and STM interrupt lifecycle, the remaining state divergence must be in runtime hardware/configuration feeding the identical HWD4 source path, most plausibly the HLOS-controlled WSA-master/PCM handoff into LPAIF WSA WRDMA.

Current frontier:

```text
WSA8845 feedback producers
 -> WSA SoundWire master ports 10/11/13
 -> [runtime master/PCM -> LPAIF WSA handoff]
 -> identical Windows/Linux HWD4 firmware
 -> HWD4 opens + interrupts on Linux
 -> Linux ring data remains zero
```
