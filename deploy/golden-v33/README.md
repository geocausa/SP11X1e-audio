# Golden v33 — TOP_CFG1 physical VI parity

Golden v33 is the promoted SP11 built-in-speaker kernel/hardware baseline as of
2026-08-24. It keeps Golden v32 intact except for one Denali-only WSA-macro
delta: physically materialize `TOP_CFG1=0x03` after each enabled VI pair,
matching native Windows.

That correction occurs **before SP_VI**. Golden v32 first-valid TAP2 is
`I,V,I,V`; native Windows is `V,I,V,I`. Golden v33 is `V,I,V,I` from its first
valid packet while `snd_q6apm` remains the ordinary Golden mapping. The rejected
downstream SP_VI `[2,1,4,3]` experiment is not part of v33.

Acceptance includes source-identical quiet-room Windows/Linux A/B at 10% and
50%, literal-zero startup captures, 160/997-Hz stress and 20/20 true-cold 50%
protection cycles with zero PA faults, zero `err0=0x20` and zero XRUNs.

The active Linux userspace speaker identity is **UbiG** (`effect_input.sp11_ubig`).
Historical Windows vendor names are retained only in oracle/provenance material.

## Hardening

The v33 WSA macro must be present both in the fixed v33 initrd and the ordinary
root module tree. Expected compressed module SHA-256:

`39674078b0781323464b3de647caf9db0b25cde51d447e8e0253630de91d3f2d`

`sync-root-module.sh` backs up a displaced v32 root copy, installs the exact v33
module, runs `depmod`, and verifies the result. Golden v32's fixed initrd remains
untouched rollback.
