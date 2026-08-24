# 2026-08-24 — Golden v33 independent reproduction gate

Golden v33 was independently replayed as a one-patch delta over the previously
completed and fully verified Golden-v32 clean reproduction.

Before applying v33, the completed v32 baseline was reverified against the
tracked recipe:

- all post-v32 source hashes passed;
- all five expected module srcversions passed;
- all five expected runtime ELF digests passed.

The verified v32 source/output tree was then reflink-cloned, leaving the accepted
baseline untouched. Only patch `0072` was applied to the clone.

Result:

- patch 0072 SHA-256:
  `3e4b214a78c9282af49c9b3d0b2e6f5249a5d9d9b93757dc506d90bac44a96e4`
- post-patch `lpass-wsa-macro.c` SHA-256:
  `4826cd591f3af594575474219f7f2215a297db29b647ccb15049ac33ba237d0b`
- rebuilt `snd-soc-lpass-wsa-macro.ko` srcversion:
  `3FAA616CDE10DDBF9D90D6F`
- runtime ELF digest:
  `c2f40153537bbcd309cc208d1c488846fc3bfbb8c4822a8ac49c880da010cd99`

This matches the promoted v33 runtime identity exactly. The raw `.ko` file hash
is intentionally not used as the equivalence gate because debug/build-path
metadata differs across clean worktrees; the source hash, srcversion and
normalized runtime ELF digest are the reproducible identity contract.
