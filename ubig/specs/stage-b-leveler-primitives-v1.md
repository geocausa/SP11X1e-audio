# UbiG Stage-B Volume-Leveler/DRC primitives v1

Status: **DECODED + directly executable-oracle tested**.

The first native Stage-B block is the bounded coefficient mapper used beneath the active SP11 Volume-Leveler/DRC long-memory controller. The reference boundary is retained only as a provenance anchor; UbiG exposes a project-native API and naming.

## Coefficient triplet

Inputs are a mode bit, a small coefficient record, a blend factor, a history value and a drive value. The helper:

1. nudges the history toward float32 `0x3f7ffffe` using an exact 0.1 FMA;
2. selects one of two scale constants according to mode;
3. evaluates the same three-coefficient float32 exp2 polynomial family used elsewhere in UbiG;
4. performs three reference-ordered double divisions converted back to float32;
5. maps those ratios through the exp2 approximation;
6. blends each mapped coefficient with the requested blend factor using FMA.

Observable constants are preserved as exact float32 bit patterns. No proprietary table or executable bytes are present in UbiG.

Private direct differential gate against the original ARM64 boundary: **300,000 randomized calls / 900,000 float32 outputs bit-exact**.
