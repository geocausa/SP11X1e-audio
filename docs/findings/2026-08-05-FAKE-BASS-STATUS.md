# Fake / virtual bass status — 2026-08-05

There are two different historical implementations and they must not be
conflated.

## 1. Early hand-written "fake bass" approximation

`sp11_dolby_stage2.c` contains a provisional time-domain Chebyshev harmonic
synthesizer. The source itself marks that design as inferred from standard
virtual-bass practice rather than decoded from the Dolby binary.

It is useful only as an optional psychoacoustic-bass experiment. It is **not**
part of the current production Windows-replica chain and must not be presented
as Dolby parity.

`bass_standalone.c` is also historical/prototype material. Its stage-3 spectral
implementation is specifically documented as incorrect and must not be used as
production DSP.

## 2. Real Dolby spectral harmonic synthesizer

The modern Dolby binary's harmonic generator was subsequently decoded from
machine code. The structural algorithm is understood:

- 256-point spectral domain;
- normalize selected complex bins to unit magnitude;
- mode 1: rotated fundamental `z`;
- mode 2: complex square `z^2` (second harmonic), with fractional bin mapping;
- mode 3: complex cube `z^3` (third harmonic);
- per-mode/ramp gain;
- accumulate into a shared complex spectrum;
- mirrored negative-frequency handling;
- inverse-transform / overlap-add wrapper required around it.

See `2026-08-02-dolby-stage3-re-log.md` for the machine-code derivation.

Some wrapper/runtime details for a standalone reimplementation remain outside
the extracted source (FFT/IFFT/OLA wrapper, runtime ramp arrays and gains), but
the harmonic-generation structure itself is decoded.

## 3. Why it is absent from the current live SP11 chain

The SP11 OEM tuning used by the studied internal-speaker profiles disables the
bass side-path:

```text
virtual_bass_process = disabled
bass-extraction      = disabled
sliding-bass         = disabled
```

The persistent Windows-hot speaker path instead derives its bass/loudness
character primarily from the original VLLDP leveler/regulator plus the VR
profile processing already ported and running on Linux.

Therefore adding synthetic/virtual bass to the production Windows-replica
chain would be an enhancement, not a parity fix.

## 4. Current policy

- **Windows replica:** fake/virtual bass OFF; preserve OEM behaviour.
- **Optional enhancement:** the historical hand-written fake-bass experiment
  may be retained, but should be rebuilt as a separate opt-in path if wanted.
- **Exact Dolby virtual-bass research:** keep the decoded spectral algorithm and
  finish its wrapper/runtime values only if we deliberately want a Dolby-style
  enhancement mode or obtain runtime evidence that a different Windows mode
  enables it.
