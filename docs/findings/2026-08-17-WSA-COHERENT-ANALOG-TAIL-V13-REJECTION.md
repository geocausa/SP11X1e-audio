# WSA8845 coherent Windows analog-tail v13 — cycle-instability rejection

## Question

After provenance-clean v12R proved that moving only final `CURRENT_LIMIT=0x44` into initialization is cycle/history-sensitive, does replaying the complete **address-audited Windows qcaucd analog tail** produce a quiet and repeatable CSR-off state?

## Candidate

v13 starts from exact v5 and, on SP11 2S only, adds the corrected Windows positions 44--63 after `ANA_WO_CTL_0` and initial gain setup. All twenty operations are direct `regmap_write()` calls in recovered Windows order:

`REF d1 -> UVLO 33/60 -> BOP1 22 -> BOP2 44 -> ZX f8 -> STB 6a -> ILIM e3 -> CURRENT_LIMIT d4 -> TOP d2 -> OCP f6 -> VCM 06/14/19/1b/1c/02 -> CKWD 13 -> PWRSTAGE f1 -> CURRENT_LIMIT 44`.

The raw Windows address/value oracle is unchanged; the preceding 2026-08-17 symbol audit corrected the names `0x34d2`, `0x300b`, `0x3040`, and `0x306a`. v13 uses the audited names/addresses.

Fresh source-tree build provenance is clean: source after SHA-256 `5b747b52...91f1e`, patch `7d27d6f...caf27`, signed module `ed77754f...b62bb`, compressed module `2b6c7b8c...c45df`, srcversion `03E2506672EEBB27B234000`, initramfs `3dda06a7...74e1b`. Disassembly proves the twenty direct calls remain in order. Exact v5 producer/x1e modules were force-loaded.

## Muted digital-zero gate

The endpoint stayed at 1% and muted. The exact 10 s 48 kHz stereo PCM16 all-zero source is SHA-256 `87d8420d...fa22e`. Physical PCM was `RUNNING` during each stream and `closed` after the suspend timer.

### Cycle 1

SP7 capture SHA-256 `C366B0C5CDB07A9D2467C7AA090C87BB788044F6F548BFA8105EBE7C74A326EB`. Median steady diff-RMS `3.17524e-4`: about **0.47x v5**, but still **17.4x Windows**. Thus the coherent cold tail materially improves the first wake but does not reach Windows quiet.

### Cycle 2, same boot and stimulus

SP7 capture SHA-256 `F259A4E5A2F12C93503A8F906F59C96BD73DEA6A72EDC0E65D34924B7D584F14`. Median steady diff-RMS `3.92703e-3`: about **5.80x v5 / 215x Windows**. This is a sustained two-channel broadband plateau, not a room impulse.

## Decision

**Reject v13.** Cycle 3 and all program-audio escalation were cancelled. The coherent Windows cold-init tail has a real first-cycle effect, but Linux loses the beneficial state across PA teardown/reopen. H03 therefore moves from “cold initialization block” to **cycle-boundary re-arm/teardown semantics**.

The next decisive observation is a safe Windows qcaucd direct-slave-helper trace expanded beyond DRE/Class-H to the corrected analog-tail addresses across repeated PA cycles. Do not assume Windows replays or does not replay those registers: the older live trace simply did not filter them. Direct WSA macro MMIO remains prohibited.
