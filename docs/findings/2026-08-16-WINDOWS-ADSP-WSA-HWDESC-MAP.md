# Windows ADSP WSA hardware-description map and address translation

Date: 2026-08-16  
Status: GREEN static provenance; live APPS MMIO remains forbidden

## Question

The prior Windows investigation established that direct APPS/KD reads of the LPASS WSA macro physical aperture at `0x06b00000` are unsafe, but static searches for that full physical address in the Windows host drivers and ADSP firmware also produced no useful register-program provenance. Was the search using the wrong address domain for ADSP-owned code?

## Exact firmware identity

The SP11 Windows ADSP image analyzed on SP7 is:

```text
qcadsp8380.mbn
SHA-256 921870A839EE2ABA647B04598D62ED96F3D2D5DFBB2499FC842F9A6FF0E0DA13
```

The Hardware Device Configuration descriptor table in that image names the WSA blocks explicitly and supplies ADSP-local child bases and sizes.

## Recovered WSA descriptor map

The descriptor table contains the following entries:

| Block | ADSP child/local base | Size |
|---|---:|---:|
| `LPASS_WSA_WSA_MACRO` | `0x00b00000` | `0x1000` |
| WSA clock/reset | `0x00b00000` | `0x20` |
| WSA top | `0x00b00080` | `0x80` |
| RX input mux | `0x00b00100` | `0x60` |
| VBAT/BCL | `0x00b00180` | `0xc0` |
| TX0 | `0x00b00240` | `0x10` |
| TX1 | `0x00b00260` | `0x10` |
| TX2 | `0x00b00280` | `0x10` |
| TX3 | `0x00b002a0` | `0x10` |
| interrupt controller | `0x00b00340` | `0xc0` |
| RX0 | `0x00b00400` | `0x50` |
| RX1 | `0x00b00480` | `0x50` |
| BOOST0 | `0x00b00500` | `0x20` |
| BOOST1 | `0x00b00540` | `0x20` |
| COMPANDER0 | `0x00b00580` | `0x50` |
| COMPANDER1 | `0x00b005e0` | `0x50` |
| SOFTCLIP0 | `0x00b00640` | `0x10` |
| SOFTCLIP1 | `0x00b00660` | `0x10` |
| EC_HQ0 | `0x00b00680` | `0x20` |
| EC_HQ1 | `0x00b006c0` | `0x20` |
| idle detect | `0x00b00780` | `0x20` |
| CB decode | `0x00b00900` | `0x60` |
| VBAT temperature | `0x00b00980` | `0xc0` |
| PBR | `0x00b00b00` | `0x100` |
| WSA SoundWire master | `0x00b10000` | `0x10000` |

This independently reproduces the block layout used by the Linux LPASS WSA macro driver, including the exact RX/boost/compander/softclip offsets that matter to the current parity investigation.

## Parent LPASS translation

The parent descriptor resolves the LPASS window as:

```text
name                 LPASS
physical parent      0x06000000
parent size          0x02000000
ADSP virtual parent  0xee000000
WSA child offset     0x00b00000
```

Therefore the same WSA macro aperture has three useful forms:

```text
ADSP child/local: 0x00b00000
APPS/physical:    0x06000000 + 0x00b00000 = 0x06b00000
ADSP virtual:     0xee000000 + 0x00b00000 = 0xeeb00000
```

This closes the provenance gap between the previously dangerous APPS physical aperture and the Windows ADSP hardware-description namespace.

## Why earlier absolute-address searches failed

A single-pass Ghidra scalar scan over the analyzed firmware found zero instruction-immediate references to the full `0x00b00xxx` child addresses and zero instruction-immediate references to the corresponding `0xeeb00xxx` virtual addresses for the targeted RX/compander/softclip/PBR blocks.

That is consistent with descriptor-driven mapping/access rather than code embedding complete WSA register addresses. Raw byte-pattern matches inside executable segments must therefore not be promoted to register accesses without instruction-level provenance.

The older scan for full APPS physical `0x06b00000` references was looking in a different address domain and its negative result is no longer surprising.

## Safety conclusion

This finding does **not** make direct APPS/KD reads safe. The preserved experiment that issued a direct physical read against `0x06b00580` produced fatal WHEA `0x124`; that route remains rejected and must not be repeated.

The useful consequence is instead that future Windows-side observation can target descriptor-driven ADSP/codec services using the correct WSA child offsets, without touching the physical aperture from APPS.

## Current parity relevance

The unresolved H03 path is still the WSA-macro COMP producer state. The descriptor map pins the exact Windows ADSP-owned blocks that correspond to Linux:

```text
RX0/1       child 0x0400 / 0x0480
COMP0/1     child 0x0580 / 0x05e0
SOFTCLIP0/1 child 0x0640 / 0x0660
PBR         child 0x0b00
```

This makes a safe ADSP-owned read mechanism substantially more valuable than another host-side heuristic.

## Evidence

SP7 retained analysis outputs:

```text
C:\Users\SurfacePro7\Documents\KDNET\Codex\qcadsp-wsa-hw-descriptors-20260816.txt
C:\Users\SurfacePro7\Documents\KDNET\Codex\qcadsp-parent-bytes-20260816.txt
C:\Users\SurfacePro7\Documents\KDNET\Codex\qcadsp-wsa-local-scalars-fast-20260816.txt
C:\Users\SurfacePro7\Documents\KDNET\Codex\qcadsp-wsa-virtual-scalars-20260816.txt
```

The extraction script is retained on SP7 as `ghidra_scripts/DecodeWsaHwDescriptors.java`.
