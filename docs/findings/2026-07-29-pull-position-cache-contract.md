# Pull-mode position cache contract

Date: 2026-07-29

## Observed boundary

The fifth audio-v3 boot closed the configuration and lifecycle questions.
The Windows-ordered pre-start sequence reached `GRAPH_START`, and a
deterministic ALSA probe accepted repeated prepare calls after patch `0018`.

A muted zero-data stream then entered `RUNNING` and filled the complete ALSA
ring:

- 48 kHz, signed 16-bit, stereo;
- 480-frame / 1,920-byte period;
- 960-frame / 3,840-byte ring;
- `appl_ptr = 960`;
- `hw_ptr = 0`;
- `avail = 0`.

The hardware pointer remained zero across repeated samples. This moves the
first unresolved boundary beyond graph construction, calibration, format,
prepare and start into the DSP-to-host position transport.

## Recovered contract

The recovered `SH_MEM_PULL_MODE` implementation establishes ownership:

1. the host writes PCM into the shared circular buffer;
2. the DSP reads the ring continuously when the graph runs;
3. the DSP updates `frame_counter`, byte `index` and timestamp in the
   position structure;
4. the host double-reads `frame_counter` around `index`;
5. the DSP raises registered watermark events when it crosses configured
   levels.

There is no per-period host write or commit packet in pull mode. The GSL
datapath exposes the data and position mappings directly, and `gsl_dp_write`
does not implement a push/pull case.

The API explicitly requires the position page to be mapped uncached. The DSP
update function writes the structure directly and performs no cache flush.

## Exact Windows/Linux mismatch

The canonical QGPR capture begins with two 4 KiB map commands:

| Mapping | Windows `property_flag` | Linux before `0019` |
|---|---:|---:|
| PCM circular data page | `0x0` (cached) | `0x0` |
| DSP position page | `0x2` (uncached) | `0x0` |

Recovered `apm_memmap_api.h` defines bit 1 as
`APM_MEMORY_MAP_BIT_MASK_IS_UNCACHED` (`0x00000002`).

This mismatch explains how the pull configuration and graph start can be
accepted while Linux continues to observe the initial zero position: Linux
advertised the DSP-written page as cached, contrary to the module contract.

## Correction and validation

Patch `0019-audioreach-map-pull-position-buffer-uncached.patch` changes only
the dedicated position map to flag `0x2`. The circular PCM ring stays cached,
matching Windows and the API.

The patch reverse-applies to the exact V3 source, strict checkpatch reports
zero errors and warnings, all 67 repository tests pass, and the QDSP6 modules
build with `W=1`. The signed core override has source version
`5F2C814E2065E90C81BC333` and compressed SHA-256
`40b7acb45f2889bf74f56e1f2337df2e49ca52d0c38d7eb0d619ed4c7fab5c10`.

The previous module is preserved at
`02-kernel/v3-runtime-backups/pre-0019-position-cache/snd-q6apm.ko.zst`.
The saved clean fallback remains `sp11-7.1.5-clean`.
