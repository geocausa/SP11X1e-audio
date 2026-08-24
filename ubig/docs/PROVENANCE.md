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

### SP11 Stage-A Dynamic-family tuning

- `DEVICE_TUNING`: 48 kHz multiband-compressor coefficient groups, distribution, severity weights, applied 20-band base/side rows, mask, stress vector and runtime controls in `specs/sp11-stage-a-dynamic-tuning-v1.json`.
- `DECODED`: the byte layout and state ownership consumed by the native compressor constructor/workers.
- `DERIVED`: conversion of stored 1/2080 integer tuning units into exact float32 constants.
- The public generator `tools/gen_sp11_stage_a_tuning.py` consumes only the UbiG JSON spec; it does not read a proprietary executable.
- The config image contains one internal pointer to the generated severity array. Public regression hashing normalizes that pointer before comparison.

### SP11 Stage-A profile-family state

- `DEVICE_TUNING`: common family one-group payload and Movie/Music four-group payload plus channel-deviation / slow-gain scalar values.
- `OBSERVED`: VLLDP-only Dynamic, Movie and Music outputs are bit-identical across five generated 16,000-frame stress stimuli.
- `DECODED`: family switches are in-place state retunes rather than Stage-A object reconstruction.
- `DERIVED`: UbiG preserves the family state marker but uses one exact Stage-A audio configuration for all seven profiles; downstream profile differences are not inferred to be absent.
