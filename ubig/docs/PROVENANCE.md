# UbiG provenance and clean implementation boundary

UbiG is an independent native implementation informed by reverse-engineering evidence and behavioral observation of the SP11 Windows reference path.

## Allowed implementation inputs

- documented input/output behavior
- documented state layout and state-transition behavior
- equations and algorithms independently described from analysis
- recovered numeric tuning/configuration values, tracked with provenance
- generated test vectors and oracle outputs
- public standards and public DSP literature

## Separation rule

Do not mechanically translate a decompiler listing into UbiG source. Legacy reverse-engineering work may identify behavior, structure, constants and equations; UbiG source should implement the resulting specification in project-native organization and naming.

## Proprietary material

Proprietary Windows DLLs, process dumps and private captured state remain outside `ubig/` and must never be committed under the UbiG source tree. The existing legacy project may retain private local oracle material under its established ignore rules.

## Naming

New implementation symbols, files, services and UI use the UbiG name. Legacy product names may appear only in historical/provenance references necessary to identify an oracle or archived finding.

## Review classes

Every imported fact should eventually be tagged as one of:

- `OBSERVED` — measured input/output or runtime state behavior
- `DECODED` — algorithm/state semantics established by analysis
- `DERIVED` — mathematical consequence of observed/decoded facts
- `DEVICE_TUNING` — recovered SP11-specific numeric configuration
- `CHOICE` — UbiG design choice not claimed to match the reference internally

This distinction is especially important for filter topology, smoothing constants and Custom-EQ units.

### SP11 Stage-A filterbank descriptor

- `DERIVED`: 320-point pre/post rotation matrix generated from standard roots with angle step `2*pi/1280`.
- `DERIVED`: 64-point edge window generated as `sin^2((n+1)*pi/130)`.
- `DECODED`: 20-band start/count geometry and two synthesis phase start/count geometries.
- `DEVICE_TUNING`: 20-band reduction coefficients and synthesis phase coefficients stored as exact float32 tuning values in `specs/sp11-filterbank-tuning-v1.json`.
- The public generator consumes only the UbiG tuning spec and mathematical formulas; it does not read a proprietary executable.
