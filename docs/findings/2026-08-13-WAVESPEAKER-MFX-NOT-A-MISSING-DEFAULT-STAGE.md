# WaveSpeaker EQ / Bass Boost / DRC declarations are not a missing ordinary-speaker stage — 2026-08-13

## Result

The Qualcomm `WaveSpeaker` EQ / Bass Boost / DRC GUID declarations in the Surface extension INF must not be interpreted as proof that ordinary SP11 built-in-speaker playback runs three additional sample-processing blocks that Linux still lacks.

For the captured REV_0D ordinary built-in-speaker conditions, the missing-stage hypothesis is now rejected strongly enough to remove it from the audible completion queue. The declarations remain relevant as endpoint capability metadata, especially for host/offload mode support, but any future mode-specific claim must be proven from that mode's runtime graph/state rather than enabled from the INF alone.

## Exact INF evidence

`surface_audiominiext8380.inf` declares the internal `QCAUD\\WaveSpeaker` endpoint with:

- `FormatsAndModes0`: type `engine`;
- `FormatsAndModes1`: type `host`, with RAW/DEFAULT/MEDIA/MOVIE/COMMUNICATIONS/NOTIFICATION modes;
- `FormatsAndModes2`: type `offload`;
- `FormatsAndModes3`: type `loopback`.

The host and offload mode entries advertise three MFX GUIDs:

- `{6f64adc3-8211-11e2-8c70-2c27d7f001fa}` — EQ;
- `{6f64adc5-8211-11e2-8c70-2c27d7f001fa}` — Bass Boost;
- `{6f64adce-8211-11e2-8c70-2c27d7f001fa}` — DRC.

There is no per-mode enable/default-state record next to these declarations in the Surface extension. The entries describe effects exposed for those format/mode contracts; they do not prove the effects are active on every stream.

## Runtime graph evidence

The reviewed Windows DEFAULT and NOTIFICATION speaker structural models contain neither:

- `MODULE_ID_DRC` (`0x07001066`), nor
- `MODULE_ID_IIR_MBDRC` (`0x07001017`).

The only equalizer-family AudioReach module in those captured speaker graphs is the already-reconstructed `POPLESS_EQUALIZER` (`0x07001045`).

Therefore the live graph evidence does not support adding a separate Qualcomm DRC/MBDRC stage to the Linux ordinary-speaker topology merely because the INF advertises a DRC MFX GUID.

## Surface APO evidence is separately negative

This is distinct from the Microsoft Surface Render APO. That path was independently closed:

- every REV_0D render MFX EQ entity is disabled;
- media-relevant coefficients are identity;
- the EFX prerequisite is absent;
- live Windows stacks hit the disabled-copy path and never the enabled biquad routine.

So neither the Surface APO EQ nor an inferred Qualcomm WaveSpeaker EQ/DRC block is available as an evidence-backed explanation for the remaining Linux transient.

## Binary implementation clue

The exact `qcaudminiport8380.sys` binary contains the WaveSpeaker EQ and Bass Boost GUID values and a concrete internal descriptor referencing the EQ GUID with the literal name `Equalizer`. The same miniport image does not contain the advertised DRC GUID value, even though the Surface INF lists DRC for every host speaker mode. This reinforces that the INF list is a capability contract rather than a literal one-GUID/one-active-DSP-module inventory.

This binary clue is supporting evidence only; the graph and waveform evidence carry the closure.

## Waveform constraint

The fresh deterministic Windows-vs-Linux Movie oracle already reaches approximately:

- correlation `0.99999947`;
- fitted gain `~1.00016`;
- residual SNR `~59.8 dB` over the full comparison, with a colder-state comparison much closer still.

A materially active, unmodelled EQ/Bass-Boost/DRC stage in this ordinary path would be inconsistent with that transfer match unless it were effectively transparent in the tested state. It therefore cannot be invoked as a generic missing coloration/dynamics block without new mode-specific runtime evidence.

## Scope of closure

Closed:

- ordinary REV_0D built-in-speaker DEFAULT/Movie-profile parity work must **not** guess-enable separate WaveSpeaker EQ / Bass Boost / DRC processors from the INF;
- Q11 is no longer an audible completion blocker for the current browser/media path.

Still open if later required:

- a specifically requested Windows hardware-offload stream;
- another processing mode whose runtime graph proves a different effect state;
- an explicit runtime property transaction proving one of these GUID controls changes a real sample-processing module.

Until such evidence exists, the canonical Linux path should remain unchanged.
