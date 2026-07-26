# Linux AudioReach control-link gap — 2026-07-26

## Result

The current Linux AudioReach topology path cannot represent the Windows
speaker graph even if every module and data edge is copied correctly.

Linux 7.1.5 parses up to eight data-port destinations per topology module and
serializes them as `APM_PARAM_ID_MODULE_CONN`. It has no topology token,
private-data path, graph model or `GRAPH_OPEN` serializer for
`APM_PARAM_ID_MODULE_CTRL_LINK_CFG`.

This is a driver capability gap, not a missing mixer setting.

## Exact affected Windows links

The reviewed DEFAULT graph contains four live control links:

| Peer 1 | Port | Peer 2 | Port | Intent | Heap |
|---|---:|---|---:|---|---:|
| SP_VI `4024` | `80000000` | SP `4027` | `80000000` | `08001204` SP | 1 |
| CPS `4028` | `80000000` | SP `4027` | `80000001` | `08001537` CPS | 1 |
| DMA sink `4157` | `80000007` | external `40df` | `c0000001` | `080010c2` timer drift | 1 |
| EQ `4664` | `80000000` | volume `4663` | `80000000` | `08001118` EQ/volume headroom | 1 |

The root record's three-link payload is exactly 148 bytes with SHA-256
`06d19ed6a84dc956529fa90a462ba827f2db65dae04ecf450aed28efd40008be`.
The DEFAULT-family record's one-link payload is exactly 52 bytes with SHA-256
`b3685b5cdb5db755f0d8a68202de4ead74df1ebc65a56b706040e2b7d1f0b285`.

Both were reconstructed from the reviewed semantic fields and matched their
original captured payload hashes. The machine artifact is
`artifacts/reviewed/windows-default-control-link-topology-data.json`.

## Candidate implementation

`patches/0003-audioreach-add-topology-control-links.patch` adds:

1. a distinct private topology byte-array type for the standard control-link
   payload;
2. strict bounds validation for every link header and property;
3. retained per-module control-link data;
4. aggregation into a standard `APM_PARAM_ID_MODULE_CTRL_LINK_CFG` parameter
   in `GRAPH_OPEN`;
5. propagation of module-widget load failures which the old switch discarded.

The private array is:

```text
audioreach_module_priv_data header
  size = exact control payload size
  type = 08001061
  priv = 0, 0
data
  num_ctrl_link_cfg
  apm_module_ctrl_link_cfg_t[]
  property headers and values
```

The wrapper is not sent to the DSP. Only the standard payload beginning at
`num_ctrl_link_cfg` is serialized.

Linux currently flattens all selected subgraphs and data connections into one
`GRAPH_OPEN`. The candidate likewise combines the two captured Windows
control-link record bodies into one 196-byte, four-link parameter. The link
objects, ordering, ports, properties, intents and heap IDs are unchanged; only
the parameter boundary is coalesced. Exact Windows record-boundary replay
would require a larger redesign of Linux graph construction and is not needed
to reproduce the graph semantics.

## Validation

- clean 7.1.5 patch dry-run: pass;
- kernel `checkpatch.pl --strict`: zero errors, warnings or checks;
- ARM64 `audioreach.o` and `topology.o`, `W=1`: compile successfully;
- reconstructed captured payload hashes: exact match;
- Python unit tests cover exact SP-link encoding and length rejection.

The single-module `.ko` link attempt reached modpost but could not complete
because the disposable object build directory has no `vmlinux.o`/symbol
table. That is an environment limitation after a partial build, not a C
compile failure. No binary from this work was installed.

## Safety boundary and next work

This patch only makes the correct graph expressible. It does not make the
existing speaker-protection startup safe or Windows-equivalent.

The current kernel automatically places SP and SP_VI in normal mode, sends a
Linux `08001203` VI channel-map command not present in the recovered Windows
startup sequence, and enables both modules as part of generic media-format
setup. A clean topology must not include active SP/SP_VI until that behavior
is replaced with an explicit disabled-by-default state and the recovered
Windows calibration order.
