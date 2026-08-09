# SP11 Dolby sample-order correction: VR -> VLLDP

Date: 2026-08-09

## Correction

Earlier hardware breakpoints proved a repeating callback invocation order of
`VLLDP callback -> VR callback`, but the original finding explicitly warned
that this did not establish sample-buffer ownership.  Full-memory Aug-8 buffer
provenance now closes the actual PCM dependency in the opposite direction:

```text
source -> DolbyApoVr -> DolbyAPOvlldp150
```

Original vendor wrapper disassembly proves both wrappers use `this+0x10` as
input staging and `this+0x18` as output staging.  In the Aug-8 dumps, the
external VR input is the exact 0.25 test tone while VR output is the historical
~0.528/~0.523 oracle signal.  The VLLDP input staging shares the same VR-output
samples byte-for-byte for exactly its captured accumulator fill.

Using the exact captured VLLDP heap/DLL state and feeding only the unconsumed
VR-output continuation reproduces the frozen VLLDP output bit-for-bit at both
75 Hz and 997 Hz (512/512 floats, RMSE 0, max difference 0).

The integrated research bridge is therefore changed from the old sample order:

```text
source -> VLLDP -> VR
```

to:

```text
source -> VR -> VLLDP
```

The old callback-order evidence remains valid as scheduler timing evidence; it
must no longer be described as sample dependency.

## Regression tests

The order-corrected bridge was tested with the exact private SP11 Dolby DLLs.
No physical audio playback was used.

Profile lifecycle/in-place switching:

```text
PROFILE_LIFECYCLE_RESULT PASS
identity=YES
state_preserved=YES
profile_applied=YES
```

One-million-frame host-chunk determinism test:

```text
reference hash=131e8cf1cf8594a5
64        diff=0
480       diff=0
1024      diff=0
127/353   diff=0
mixed     diff=0
PLUGIN_RESULT PASS
```

Thus correcting sample order does not break persistent Dolby state, profile
switching, realtime chunk independence, or determinism.

## Deployment boundary

This commit corrects the research/source bridge only.  Do not replace the live
installed plugin solely from this commit; deployment should follow a separate
fresh-build/hash/graph smoke-test checkpoint.

## Production-build offline gate

The corrected source was built through the repository production build script
against the hash-gated private DLL bundle:

```text
corrected production .so SHA-256
c5cfe341b44b4b7085ede38fbc0cfbd20e8920d459ba0898f02701c3bbc7d440
```

`analyseplugin` accepts the artifact and reports the expected six LADSPA ports
(Input L/R, Output L/R, Bypass, Profile).  The actual production-built `.so`
then passed the same one-million-frame chunk-independence test:

```text
reference hash=131e8cf1cf8594a5
64        diff=0
480       diff=0
1024      diff=0
127/353   diff=0
mixed     diff=0
PLUGIN_RESULT PASS
```

The currently installed plugin was deliberately left untouched during this
offline gate.  Its pre-correction SHA-256 remains:

```text
1e7cc8cb4ec441ee890b73bf90f738c64df88a03e953be502a60025515a3534a
```
