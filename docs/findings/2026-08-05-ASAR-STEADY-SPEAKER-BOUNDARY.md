# Modern ASAR steady-speaker boundary — 2026-08-05

## Binary provenance

The modern `DolbyAudioProcessing.dll` on the still-installed Windows partition
and all current archived copies are byte-identical:

```text
SHA-256 900944a1f96292813ff5c56d30d49663851fe368e709f53681ee7a0c0a84d0d3
version  7.3.7.0 / 7.3.7.rel
```

Therefore fresh static analysis applies directly to the DLL loaded in the
2026-08-04 KDNET session.

## August live facts retained

During active stereo/music playback, hardware execution traps showed:

- DAPVR DABS speaker wrapper `0x18004E7B0`: 0 hits;
- embedded VLLDP `0x1800922F8`: 0 hits;
- AIDE adaptive core `0x18003A438`: 0 hits.

The persistent DAX/VLLDP150/VR path was simultaneously hardware-hot.

## Stronger AIDE conclusion

Fresh Ghidra/string-xref analysis anchors the high-level
`dolby::oar::AIDEModule::Process` path inside the exact DLL. In the active
high-level branch, an initialized AIDE module reaches this unconditional direct
call:

```text
0x18001BC2C -> BL 0x18003A438
```

The module's initialization/readiness gate is checked before the call; failure
leaves the successful processing branch rather than performing an alternate
AIDE algorithm.

Consequently a successful per-buffer AIDE processing pass in this branch cannot
occur without executing `0x18003A438`. The zero-hit hardware trap therefore
rules out AIDE as a steady per-buffer contributor in the tested Windows speaker
stream. This is stronger than the old wording “AIDE core unproven/cold.”

## OAR / Crossfade boundary

The same high-level function contains a distinct OAR path. When initialized it
calls the large OAR/object-metadata renderer at:

```text
0x18001BAC8 -> BL 0x1800241E8
```

Static decompilation shows this routine parses OAMDI/object metadata and also
performs substantial floating-point buffer work. It cannot be dismissed as
logging-only.

The high-level function also contains a crossfade/state-transition branch later
in the same path. A separate alternate modern-DAP branch exists around
`0x18001C0C0` and can reach crossfade bookkeeping without first entering the
AIDE block.

Therefore the August zero-hit AIDE/DAPVR/embedded-VLLDP traps do **not** by
themselves prove that every possible OAR/crossfade operation is cold. Static
analysis cannot determine the live branch state at the missing runtime instant.

### Exact remaining runtime discriminator

A future Windows run only needs targeted hardware execution traps on the newly
identified high-level boundary, especially:

```text
0x1800241E8   OAR processing core
```

plus the relevant crossfade execution entry once its exact top-level boundary
is selected. If those remain cold while the known DAX/VLLDP150/VR controls are
hot, modern ASAR can be closed for that speaker condition without further DSP
porting.

Do not add ASAR/OAR/Crossfade to the production Linux speaker chain solely from
module presence. Conversely, do not claim OAR is inactive without that final
runtime discriminator.
