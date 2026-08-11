# SP11 Linux CPS reconstruction and offline compile gate — 2026-08-11

## Scope

This finding records the Windows-evidence-driven replacement for the rejected
`sp11-audio-cps-lab` transport. **No SP11 Linux boot or deployment was performed.**
The candidate was reconstructed and compiled offline only.

## Source provenance recovered

The working kernel line is anchored to the official Linux `v7.1.5` source
archive. The archive used for the reconstruction has SHA-256:

`22a0196b3cbcdf34dc27b77561f4d040585fd3447edc9ab3531a1ac79e3041e7`

Replaying patches `0003` through `0014` from pristine v7.1.5 reproduces the
old blob IDs recorded as the base of `0020`. This closes the previously
ambiguous pre-`0020` source state without requiring the deleted historical
worktree.

The repository's historical cumulative patches are not treated as immutable
exports; see `2026-08-11-patch-provenance-correction-0020-0025.md`.

## Windows transport facts implemented

The new candidate follows the live qcaucd CPS capture, not the rejected split-mask model:

- one physical WSA SoundWire master port: **13**;
- left WSA8845 identity `0x0000000402170220`, local DP6, ChannelEnable `0x03`, OffsetCtrl1 `0`;
- right WSA8845 identity `0x0000000402170221`, local DP6, ChannelEnable `0x03`, OffsetCtrl1 `25` (`0x19`);
- both DP6 slaves use SampleCtrl1 `0x1f`, SampleCtrl2 `0x03`, HCtrl `0xff`, BlockCtrl1 `0x18`, BlockCtrl3 `0x00`;
- interval is 800 SoundWire clocks = 24 kHz at the observed 19.2 MHz clock;
- slot 14 is a right-slave-only companion and **does not** imply physical master port 14.

## Kernel design

`patches/0026-sp11-cps-windows-parity-candidate.patch` is generated against a
freshly reconstructed pre-CPS effective tree. It is intentionally framework-level:

1. the SoundWire master computes its normal common schedule;
2. a slave runtime may override only transport parameters that are genuinely
   slave-specific (the SP11 right DP6 Offset1=25 case);
3. SIMPLE DPNs can opt into the additional banked fields that Windows proved
   DP6 implements, without declaring the whole port FULL;
4. WSA8845 exposes a dedicated CPS DAI using DP6 and native `0x03` channel mask;
5. board DTS carries the stable physical left/right Offset1 values rather than
   relying on boot-dependent logical device numbers;
6. the Qualcomm DIN side advertises 24 kHz for the dedicated feedback stream;
7. a dedicated WSA-macro CPS transport DAI shares clocks but does **not** reuse
   the VI feedback callback that programs the WSA TX0..TX3 speaker-protection
   rate registers;
8. the machine driver fixes WSA_CODEC_DMA_TX_1 to 24 kHz/S32/two channels;
9. speaker protection is enabled only when **both VI and CPS** sidechains report
   ready; playback remains available with protection bypassed if either fails.

This avoids physical MMIO, raw slave-register hacks, split CPS masks, master
port 14, and a blind SIMPLE-to-FULL conversion.

## Offline compiler gate

Compiler:

`aarch64-linux-gnu-gcc (GCC) 16.1.1 20260501 (Red Hat Cross 16.1.1-1)`

The following changed ARM64 translation units compiled successfully with `W=1`:

- `drivers/soundwire/stream.o`
- `drivers/soundwire/qcom.o`
- `sound/soc/codecs/wsa884x.o`
- `sound/soc/codecs/lpass-wsa-macro.o`
- `sound/soc/qcom/qdsp6/q6apm.o`
- `sound/soc/qcom/qdsp6/audioreach.o`
- `sound/soc/qcom/x1e80100.o`

Both Denali DTBs also compiled successfully:

- `qcom/x1e80100-microsoft-denali-oled.dtb`
- `qcom/x1p64100-microsoft-denali.dtb`

The only warning in the gate is the pre-existing `audioreach_populate_graph()`
`ncontainer` unused-variable warning; it is unrelated to CPS.

The exact compile transcript is
`artifacts/reviewed/2026-08-11-sp11-cps-compile-v2.log`.

## Status / limits

This candidate is **compile/DTB validated, not runtime validated**. It must not
replace the accepted clean boot. Remaining Windows-side closure items include
simultaneous DEFAULT/NOTIFICATION graph lifetime and a live DP5/VISENSE trace
before changing any existing Linux VISENSE channel semantics.
