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

The generator explicitly emits PCM-converter token `252` with value `3` for
Windows IID `0x465f`. This is the captured DSP-internal
`PCM_DEINTERLEAVED_UNPACKED` layout; omitting the token serializes invalid
value zero in the Linux `PARAM_ID_PCM_OUTPUT_FORMAT_CFG` transaction.
