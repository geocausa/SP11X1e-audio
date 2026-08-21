# UbiG Stage-A multiband compressor v1

The Stage-A multiband compressor is implemented as native UbiG code by composing independently specified bounded state machines and band-domain transforms. Proprietary binaries are used only as private differential oracles.

## Native boundary

The former process boundary corresponding to reference routine `0x180021e80` is now represented by `ubig_stage_a_compressor_process()`.

The implementation owns:

- mode/channel/drive cache lifecycle and state reinitialization
- dual-plane channel-max tracking and rise flags
- global band-controller severity, gate, release and hold state
- per-channel slow-gain bound construction
- linked-channel deviation limiting
- nonlinear per-band correction and aggregate state
- direction-sensitive smoothing
- mask-neighbor limiting
- per-row gain routing
- matrix telemetry at scale 2080
- 20-band gain telemetry at scale 4160

## Direct differential gates

Private oracle comparisons established:

- cubic transition helper: 1,000,000 calls bit-exact
- dual-plane tracker: 30,000 complete calls bit-exact
- preserved warm 2-channel compressor fixture: full 0x900-byte state, both row descriptors, matrix telemetry, 20-band telemetry, and row count bit-exact
- lifecycle variants: warm, mode-cache change, input-count reinit, drive reinit, effective-count reinit, mode-zero linked path, and native-count-one path all bit-exact
- a fully synthetic UbiG-owned vector matches the reference exactly and produces public canonical hash `75c3a084f4f3b91a`

## Important state rule

The band-controller severity floor is recurrent across bands rather than independently recomputed. The carried scalar begins at `0.5 * drive - 1`, is shifted by `-0.5 * drive` on the next band, and then max-combined with that band's half-level knee. Preserving this recurrence is required for exact target behavior.

No proprietary tables, fixtures, or executable bytes are present in this specification or in the public regression.
