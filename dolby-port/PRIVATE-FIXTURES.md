# Private Dolby evidence fixtures

Several historical native-bridge tests consume small `.bin` files that are not
ordinary generated build products. They contain captured Windows VLLDP runtime
state, including original address-space values/pointers, plus deterministic
input/output vectors.

They are therefore treated as **private evidence**, not public source. The files
are ignored by Git and may be placed in a local directory such as:

```text
~/.local/share/sp11-dolby/fixtures/
```

Set:

```sh
export SP11_DOLBY_FIXTURES="$HOME/.local/share/sp11-dolby/fixtures"
```

before running evidence-only native bridge checks.

## Exact identities

| File | Bytes | SHA-256 |
|---|---:|---|
| `analyzer_inputs.bin` | 9,908 | `cde48666858e1f184f8d0023bc7cae1e51f9ff39bda9591584c230f56ee19e00` |
| `leveler_inputs.bin` | 2,916 | `635d8c3f056ce7b32fce21affa289fd9c93dd2ef435f67e7e0a34f403751aabd` |
| `synth_inputs.bin` | 8,892 | `a82de8d7225480b6612d4b639443324add701fad473414977deb4422534e0a49` |
| `full_chain_seed.bin` | 18,208 | `7f250ab12e9f21d3ce678856710bbb3ce1cd08194db7e5dd9cb588f17c3fec96` |
| `full_chain_seed_pre081602_p0.bin` | 18,628 | `b090d4184f5dcad80842a19bafff292a1cdc6c61f5a47f0030e9216c8aff7886` |
| `full_chain_seed_pre081602_p1.bin` | 18,628 | `81efa22294d5d9057355026e518713927dda104d9e245a592b04379df052d517` |
| `full_chain_seed_post081812_p0.bin` | 18,628 | `d0a50123df9ff79958a369a89aba45a5aca83e62e8bac1f8f6c34c6ce04ea532` |
| `full_chain_seed_post081812_p1.bin` | 18,628 | `224bd9ebf396cf9f41a21633e553cfaba4140ebf71c44dad4ae22e9a3e3f6928` |

Verify a local fixture directory with:

```sh
python ../tools/dolby/verify_private_fixtures.py "$SP11_DOLBY_FIXTURES"
```

The current Makefile targets use only the first four files; the additional
pre/post variants are retained in the identity ledger because they belong to
the historical research corpus.

## Boundary

Do not solve a missing-fixture error by re-adding these binaries to Git. Tests
that depend on them are evidence-regression tests and should fail/skip explicitly
when the private bundle is unavailable.
