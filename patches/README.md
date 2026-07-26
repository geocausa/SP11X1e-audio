# Kernel patch candidates

Files in this directory are evidence-backed candidates for offline review
and build validation.  They are not automatically applied to the live
kernel, boot files, ALSA UCM configuration, or speaker controls.

## `0001-sp11-add-single-wsa-vi-backend.patch`

Adds only the missing transport boundary proven strongly enough to build:

```text
WSA_CODEC_DMA_TX_0 -> lpass_wsamacro DAI 2 -> q6apm
```

The backend is constrained to the Windows VI format:

- 8,000 samples/second
- 32-bit little-endian samples
- two channels mapped front-left/front-right

The patch deliberately does not:

- enable the two `WSA_AIF_VI` mixer inputs;
- enable WSA8845 `VISENSE`;
- install or start an AudioReach speaker-protection graph;
- change playback gain, PA state, or the live boot configuration;
- add `WSA_CODEC_DMA_TX_1`, `WSA_CODEC_DMA_TX_2`, or a second WSA macro.

Those activation steps must wait for an instrumented, muted test and a
known-correct protection graph.

Validation against the preserved 7.1.5 source:

- patch dry-run: pass;
- strict kernel style check: pass, with only the intentionally absent
  submission sign-off ignored;
- ARM64 machine-driver object build: pass;
- `x1e80100-microsoft-denali-oled.dtb` build: pass;
- compiled DTB inspection: one `wsa-vi-dai-link`, macro DAI 2, CPU
  `WSA_CODEC_DMA_TX_0`.

Exact source, patch and output hashes are recorded in
`artifacts/reviewed/linux-single-wsa-vi-candidate-validation.json`.

## `0002-qcom-soundwire-log-static-port-allocation.patch`

Adds one dynamic-debug observation point to the Qualcomm SoundWire master
allocator. For every active amplifier slave port it records:

- SoundWire stream and bus;
- slave device number and slave port;
- selected master port;
- channel mask;
- whether the master port was already in use.

The last field is needed because both SP11 amplifiers map PBR to master port 7,
while their VISENSE paths map separately to ports 10 and 11. The SoundWire core
does not de-duplicate the allocator's master-port list, but static analysis
alone does not establish whether the duplicate programming is harmful.

The patch only uses `dev_dbg()`. It does not change allocation, stream setup,
register programming or mixer state, and remains silent unless dynamic debug
is explicitly enabled during an instrumented kernel boot.
