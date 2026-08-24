# 2026-08-24 — GOLDEN v33 promoted

`sp11-audio-golden-v33-topcfg1-physical-vi` is the saved daily-driver entry.
Golden v32 remains the fixed-initrd rollback.

Exact module identities:

- WSA macro `3FAA616CDE10DDBF9D90D6F`
- WSA8845 `5859E70AFD0A1D420E8ADD4`
- machine `13326073E27DFA035180C56`
- SoundWire qcom `D008A3D6B585C11BE023992`
- q6apm `687B16CF9C43B43E90C0746` — unchanged Golden mapping

v33 adds one Denali-only producer delta over v32: physically write WSA
`TOP_CFG1=0x03` after each enabled VI pair. TAP2 therefore changes from v32
`I,V,I,V` to native-Windows `V,I,V,I` **before SP_VI**.

Acceptance: clean literal-zero starts; clean 160/997-Hz 50% stress; fresh
source-identical quiet-room Windows/Linux program A/B within about 0.1 dB at
10% and 50%; and 20/20 true-cold 50% cycles with 20 enables/20 disables, zero
PA faults, zero `err0=0x20`, and zero XRUNs.

A stricter 50 ms scan found one isolated broadband burst in soak cycle 1. It had
no kernel fault and did not recur in cycles 2–20 or a dedicated long-idle
literal-zero two-birth reproduction. It remains recorded as a non-reproducible
one-off rather than being omitted.

The active Linux userspace identity is UbiG. Golden v32 is retained untouched.
