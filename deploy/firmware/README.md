# Local generated topology payload

The installed protected topology is generated locally as:

```text
X1E80100-Microsoft-Surface-Pro-11-tplg.bin
```

Its 2026-07-28 deployment SHA-256 is
`4e00057b8e316c217347bcdee0af0c6d4ff40e8e0f1870d7efeaddc2669ff54e`.
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
