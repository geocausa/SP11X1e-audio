# SP11 X1E audio

Reproducible Linux audio bring-up for the Microsoft Surface Pro 11 using the
Qualcomm X1E80100 AudioReach, SoundWire, and WSA884x stack.

The project starts from the upstream Linux implementation.  It treats the
kernel/DT, AudioReach topology, ALSA UCM policy, and PipeWire policy as separate
layers so that a result in one layer is not mistaken for a driver fix in
another.

## Current baseline

On the test machine running `7.1.3-sp11-baseline1+`, the MM1 speaker route can
run 48 kHz, 16-bit, two-channel PCM through both WSA884x amplifiers without a
new kernel error.  The installed topology is nevertheless invalid: module
instance ID `0x6020` is assigned to both `stream0.msiir0` and
`stream2.logger1`.  The kernel rejects the later definition.

Speaker voltage/current telemetry and closed-loop protection have **not** been
proved.  The current local WSA884x change excludes PBR, VISENSE, and CPS ports
from the playback stream globally and is not considered an upstream-quality
solution.

The present work is diagram-first: tuning, EQ changes, Dolby emulation, and
deployment are frozen while the Windows ACDB, donor topology, active Linux
topology, kernel, UCM, and codec port map are reconciled. See the
[pipeline provenance audit](docs/audit/2026-07-22-pipeline-provenance-audit.md).

See [the initial live orientation](docs/baseline/2026-07-22-live-orientation.md)
for the evidence and boundaries of these findings.

## Safety rules

- Never overwrite the currently bootable kernel, DTB, topology, or UCM files.
- Capture exact hashes before every experiment.
- Build a separate boot entry with an explicit rollback path.
- Do not treat a successful PCM open as proof of speaker protection.
- Keep raw observations separate from hypotheses derived from Windows traces.
- Do not publish firmware or vendor binaries without confirming redistribution
  rights.

## Tools

Capture the current machine state without opening an audio stream:

```sh
./tools/capture-live-state.sh
```

Check an `alsatplg` decoded configuration or binary topology for duplicate
AudioReach module instance IDs:

```sh
./tools/ar_topology_lint.py topology.conf
./tools/ar_topology_lint.py topology.bin
```

Create a full structural inventory, including hand-injected raw-byte modules
that `alsatplg` does not render as normal tuples:

```sh
./tools/ar_topology_inventory.py topology.bin --json inventory.json --markdown inventory.md
```

Decode containers, module lists, port declarations, and connections from a
raw Windows ACDB `POOL` GRAPH_OPEN bundle:

```sh
./tools/ar_graph_open_inventory.py 01e842_POOL.bin --offset 0x35d84
```

Decode raw GKV schemas and bind their rows to `POOL` bundles:

```sh
./tools/acdb_gkv_inventory.py GKVT.bin GKVL.bin --pool 01e842_POOL.bin --json windows-gkv.json
```

Recover the in-band subgraph activation lists from a decoded QGPR capture and
resolve each list against that GKV inventory:

```sh
./tools/qgpr_activation_inventory.py qgpr.decoded.csv windows-gkv.json --json activations.json
```

The remaining Windows selector and GRAPH_OPEN-body gap has a version-locked,
read-only [KDNET capture runbook](docs/runbooks/windows-kdnet-structural-gap-capture.md).

Generated captures are placed under `artifacts/live/` and ignored by Git until
they have been reviewed and deliberately promoted into documentation.

The loudness-event collector is retained for a later phase. Do not use it as a
substitute for closing the topology ledger:

```sh
./tools/capture-loudness-event.sh --duration 120 --record-sink
```

Press Enter whenever the jump is heard. The tool records PipeWire volume,
PipeWire graph events, ALSA control events, before/after control state, kernel
messages, and optionally the digital signal at the default-sink monitor. It
does not change the volume or any audio control.
