# Aug-8 oracle buffer provenance: the ~0.52 lift is inside LibWrapperVr

Date: 2026-08-09

## Result

The frozen Aug-8 75-Hz and 997-Hz oracle blocks were traced physically through the full-memory dumps.
The characteristic VLLDP-staging sample fragments occur in the unique live `DolbyApoVr.dll`
`LibWrapperVr` object in both dumps.

The live wrapper is identified by the exact primary vtable RVA `0x1D8AE0`.  In the two dumps:

```text
75 Hz:  LibWrapperVr object 0x1ec9413c2f0
997 Hz: LibWrapperVr object 0x2161d13c2f0
```

Its buffer pointers contain:

```text
                        +0x10 peak       +0x18 peak
75 Hz / input 0.25       0.250000        0.527785122
997 Hz / input 0.25      0.250000        0.522893488
```

The `+0x18` samples match the Aug-8 oracle/VLLDP-staging waveform fragments byte-for-byte.

## Direction proved from original code

The exact vendor `LibWrapperVr::process` function is RVA `0x0F65E0` in this build.
The ARM64 code proves the direction, rather than inferring it from amplitudes:

- `ldp x8,x9,[this,#8]` obtains configuration and the internal `this+0x10` buffer;
- the external input argument is copied with `memcpy` into the `this+0x10` buffer;
- the original VR processing method is invoked;
- the internal `this+0x18` buffer is subsequently copied to the external output argument.

Therefore:

```text
LibWrapperVr +0x10 = VR input staging
LibWrapperVr +0x18 = VR output staging
```

For the Aug-8 oracle dumps the observable amplitude transformation is therefore physically present
inside the external `DolbyApoVr.dll` wrapper:

```text
0.25 source tone -> DolbyApoVr/LibWrapperVr -> ~0.528 (75 Hz)
0.25 source tone -> DolbyApoVr/LibWrapperVr -> ~0.523 (997 Hz)
```

This is a major correction to the current ASAR parity target.  The ~0.52 values must not be used as
an HRTF/DAP-only target unless the surrounding DAX3/VLLDP/VR ordering proves that boundary
independently.  The next task is to reconcile this direct buffer provenance with the older ETL-derived
`VLLDP -> VR` ordering and identify which VLLDP staging object the Aug-8 parser actually labels as
`input_staging`.

No proprietary binaries or dump bytes are committed here; only addresses, hashes/results, and
reverse-engineered semantics are recorded.
