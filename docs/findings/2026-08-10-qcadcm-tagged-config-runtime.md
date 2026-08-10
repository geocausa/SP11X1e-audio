# qcadcm tagged custom-config runtime capture

Date: 2026-08-10 (Europe/London)

## Result

A live Windows speaker playback cycle was traced at the hash-locked
`qcadcm8380.sys` tagged custom-config boundary. The runtime bytes confirm the
expected AudioReach module-parameter headers for the pull/render/protection
startup sequence, and they also narrow the remaining CPS search: the HLOS CPS
LPASS parameter `0x08001259` did **not** traverse this particular tagged-custom
boundary during the captured playback cycle.

This is a negative observation only for this boundary/scenario. It does not
prove that Windows never constructs or transports `0x08001259` elsewhere.

## Hash gate and runtime address

The driver remains the handoff-locked binary:

- `qcadcm8380.sys` SHA-256:
  `37f76305ac8051b0b03b6d2ce1df7a353253debf546e512e447c9d95ec661429`

After the controlled reboot, KD resolved:

- image base: `0xfffff801da5b0000`;
- `gsl_set_tagged_custom_config` RVA `0x60d68`;
- live VA: `0xfffff801da610d68`.

Static Ghidra decompilation of the hash-matched image shows this wrapper taking
`graph`, `tag`, `payload`, and `size`, resolving tagged module IIDs, copying the
caller payload into GPR packets, and submitting the resulting graph SET_CFG.

## Live observed bytes

The breakpoint was read-only and printed only the first 16 bytes of each
payload. Every captured payload was at least `0x16` bytes, so the observation
stayed inside the caller-declared payload extent.

| Tag | Size | First four dwords | Parameter ID |
|---|---:|---|---|
| `0x04000001` | `0x2e` | `00000000 0800100c 0000001e 00000000` | `0x0800100c` |
| `0x04000003` | `0x2e` | `00000000 08001008 0000001e 00000000` | `0x08001008` |
| `0x04000005` | `0x2e` | `00000000 08001008 0000001e 00000000` | `0x08001008` |
| `0x04000007` | `0x20` | `00000000 08001024 00000010 00000000` | `0x08001024` |
| `0x0400000a` | `0x20` | `00000000 08001024 00000010 00000000` | `0x08001024` |
| `0x04000029` | `0x16` | `00000000 08001130 00000006 00000000` | `0x08001130` |
| `0x0401000a` | `0x18` | `00000000 080011e9 00000008 00000000` | `0x080011e9` |
| `0x0401000b` | `0x28` | `00000000 080011f5 00000018 00000000` | `0x080011f5` |
| `0x0401000b` | `0x28` | `00000000 080011f4 00000018 00000000` | `0x080011f4` |
| `0x0401000b` | `0x18` | `00000000 080011ff 00000008 00000000` | `0x080011ff` |

The repo already identifies the major members of this sequence:

- `0x0800100c`: pull media format;
- `0x08001008`: PCM converter configuration;
- `0x08001024`: MFC/output-format path;
- `0x080011e9`: speaker-protection operating mode;
- `0x080011f5`: SP_VI R0/T0 configuration;
- `0x080011f4`: SP_VI operating-mode configuration;
- `0x080011ff`: SP_VI excursion-mode configuration.

`0x08001130` is preserved here as an observed identifier without assigning a
new field name in this finding.

## CPS implication

`PARAM_ID_CPS_LPASS_HW_INTF_CFG` (`0x08001259`) was not present in the observed
headers from this tagged-custom path. This removes one plausible qcadcm
boundary from the remaining search and supports moving the trace either to the
raw `gsl_set_custom_config` wrapper or below both wrappers at the common GPR
SET_CFG submit path.

The already-completed SoundWire transport result is unchanged: both WSA8845
speakers use CPS DP6 with ChannelEnable `0x03`, with left/right differentiated
by OffsetCtrl1 `0` / `25`.

## Evidence and safety

Raw debugger log (kept outside Git pending final closeout/hash):

`C:\Users\SurfacePro7\Documents\KDNET\Codex\QCADCM_DMA_CFG_20260810_2119BST_2448_2026-08-10_21-19-15-709.log`

Reviewed machine-readable extraction:

`artifacts/reviewed/2026-08-10-qcadcm-tagged-config-runtime.json`

No direct debugger physical-MMIO read was used for this capture. No MMIO, DSP,
or driver-state write was performed, and classic WinDbg was not run alongside
`kd.exe`.
