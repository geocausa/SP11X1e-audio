# Local generated topology payload

The installed protected topology is generated locally as:

```text
X1E80100-Microsoft-Surface-Pro-11-tplg.bin
```

Its current 2026-07-29 deployment SHA-256 is
`5cda975f1559311b979b4e81554231629725d365c46cf55692d6e33b5132c704`.
The pre-candidate reference hash and installed paths are recorded in
`docs/deployment/2026-07-28-protected-audio-candidate.md`.

Opaque topology binaries and recovered vendor calibration payloads are
intentionally excluded from Git. The repository versions the reviewed graph
structure, builders, tests, kernel integration and immutable payload hashes,
but not redistributable vendor bytes.

Generate the topology from the locally retained reviewed inputs with
`tools/acdb_protection_stage_builder.py` followed by
`tools/build_sp11_protected_topology.py`. Both tools provide `--help` with the
required input and output arguments.

Graph calibration defaults to `windows-full`, the Golden/Windows parity policy.
It preserves the 10,464-byte / 107-record selected aggregate and the scoped
warning behavior verified in qcadcm/GSL. A diagnostic `settable-v1` variant can
be generated only with `--graph-calibration-variant settable-v1`; the topology
builder requires the same explicit option before it will consume that manifest.
That variant removes only GET-only `0x412b:0x0800113d`, is not Golden, and must
not be deployed silently. See
`docs/findings/2026-08-23-F05-GRAPH-CALIBRATION-WARNING-POLICY-CLOSED.md`.

The generator explicitly emits PCM-converter token `252` with value `3` for
Windows IID `0x465f`. This is the captured DSP-internal
`PCM_DEINTERLEAVED_UNPACKED` layout; omitting the token serializes invalid
value zero in the Linux `PARAM_ID_PCM_OUTPUT_FORMAT_CFG` transaction.
