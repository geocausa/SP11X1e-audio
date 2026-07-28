# Verified DSP param engagement — consolidated rebuild spec

Date: 2026-07-23

Consolidates the prior archive mining (`SP11/AUDIO/build/mining-briefs/`,
briefs 04–10 + the Brief 09 param catalog) against **two real runtime dmesg
captures**, to separate what the Linux DSP *actually accepts at runtime* from
what was only inferred from firmware disassembly. This is the foundation the
speaker-path rebuild is allowed to stand on.

## Sources

| Source | Role |
|---|---|
| `findings/09-dsp-params/generated/catalog.csv` | Per-param grades (kernel-anchor / empirical-rc0 / firmware-set-dispatch-proven / …) |
| `build/brief10-preflight-20260524/dmesg.txt` | Runtime **protection** experiment (SP_VI/SPv5): 106 `rc=0`, 92 `rc=-22` |
| `runtime-captures/20260606-linux-msiir-unity-postreboot/dmesg-after-speaker-test.txt` | Runtime **main-path** capture (EQ/SAL_V2/MFC), all `rc=0` |
| briefs 04–10 `README.md` / `structural-verification.md` / `validation.txt` | Candidate topology status |

## Meta-finding (governs everything below)

**Firmware-dispatch evidence does NOT predict runtime acceptance.** Many params
graded `firmware-set-dispatch-proven` (a handler exists in `qcadsp8380.mbn`)
returned `rc=-22` ("param not supported") when actually sent. Trust order for
the rebuild:

1. `kernel-anchor` (in upstream `audioreach.h`) — safe.
2. `empirical-rc0` (observed `rc=0` in a capture above) — safe *syntactically*
   (see audio-behavior caveat).
3. `firmware-set-dispatch-proven` — **candidate only; must be runtime-tested**.
4. `firmware-helper-constant` / `firmware-dispatch-rejected` — do not send.

Caveat: `rc=0` means the DSP *accepted* the blob, **not** that it produces
correct audio. The catalog notes SAL_V2 "accepted-only set later caused no
sound." Acceptance is necessary, not sufficient.

## Per-module verified params

### EQ `0x07001045` — the tonal lever (highest value, lowest risk)
| Param | Size | Verdict | Evidence |
|---|---|---|---|
| `0x0800110c` | 112 | **ENGAGES (rc=0)** — parametric EQ table | main-path capture; firmware handler `0xb034d96c` |
| `0x08001026` | 4 | **ENGAGES (rc=0)** — common config | both captures |
| `0x08001112` | 4 | candidate (fw-proven, untested) | dispatcher `0xb034c274` |
| `0x08001113` | 4 | candidate (fw-proven, untested) | dispatcher `0xb034c280` |
Send as **separate SET_CFG** transactions (debug driver flag
`SP11_EQ_SEND_SECTIONDATA_AS_SEPARATE_SET_CFG`).

### SAL_V2 / VOL_CTRL `0x0700101b`
| Param | Size | Verdict |
|---|---|---|
| `0x08001035` `0x08001036` `0x08001038` `0x0800103d` | 40/4/16/92 | ENGAGES (rc=0) — **but "accepted-only caused no sound"; audio-risk** |
| `0x08001037` | 4/12 | context-dependent — rc=0 in main path, rc=-22 standalone |
| `0x08001039` | 104/16 | context-dependent — rc=-22 standalone |

### SAL `0x07001010`
| Param | Verdict |
|---|---|
| `0x08001016` OUTPUT_CFG, `0x0800101e` LIMITER_ENABLE | INCLUDE (kernel-anchor) |
| `0x08001039` | REJECTED when sent to SAL (rc=-22) — drop |

### SP_VI `0x070010e3` (protection VI)
- **ENGAGES (rc=0):** `0x080011f5` (R0T0), `0x08001203` (CHANNEL_MAP),
  `0x080011c2`, `0x08001364`, `0x08001384`, `0x08001510`, `0x08001026`.
- **Kernel-anchor:** `0x080011f4` (OP_MODE), `0x080011ff` (EX_MODE).
- **REJECTED (rc=-22), drop despite fw-proven:** `0x080010a6`, `0x080011f6`,
  `0x080014e8`.

### SPv5 `0x070010e2` (protection v5)
- **ENGAGES (rc=0):** `0x080011ea`, `0x080011ee`, `0x0800125a`, `0x08001314`,
  `0x08001367`, `0x0800150a`, `0x0800150c`, `0x08001026`.
- **Kernel-anchor:** `0x080011e9` (SP_OP_MODE).
- **REJECTED (rc=-22), drop despite fw-proven:** `0x08001061`, `0x080011e8`,
  `0x080011eb`, `0x080011ed`, `0x08001258`, `0x0800134a`, `0x0800135a`,
  `0x080013d5`, `0x0800150b`, `0x0800150e`.

## Candidate topology status

All brief 04–10 topology binaries are **workspace-only ("DO NOT DEPLOY")** and
were built on the older `sp11-realsplit` baseline, **not** our clean baseline
`4e00057b`. Reuse the *method*, not the binaries:

- **Backend binding (reusable):** Brief 07 is the working precedent — the
  protection side-graph (`sub_graph 0x4040`) must emit `token1 105` to bind to
  the live playback DAI (`device105`). Brief 06 got this wrong (bound to
  106/107) and its modules never dispatched.
- **SP_VI/SPv5 dispatch confirmed:** only DAI 105 triggers the per-module
  protection dispatch (brief10-preflight dmesg).
- **Closed-loop VI feedback: NOT achieved.** Brief 10 did no live deploy; the
  WSA-macro→TX-lane pairing (TX0/TX1 vs TX1/TX2) is UNKNOWN and the live sound
  node lacks WSA VI capture links. This is the deepest unresolved gap.

## Implications for the rebuild (on clean baseline `4e00057b`)

1. **Tonal first (safe, audible):** port the EQ params — especially the 112-byte
   `0x0800110c` table — into the speaker render chain via split SET_CFG. This is
   the one lever that is both empirically `rc=0` and directly tonal.
2. **Protection root (staged):** add SP_VI/SPv5 as a side-graph bound with
   `token1 105`, sending **only** the ENGAGES/kernel-anchor param subset above —
   never the full Windows blob (that is the rc=-22 cascade).
3. **Closed-loop VI feedback (blocked):** needs the WSA TX-lane DT wiring
   resolved first — candidate for the KDNET/hardware capture, not guesswork.
4. **Every param still needs an audio-behavior check**, not just `rc=0`.

The correct source of the actual tonal/protection *values* remains the REV_0D
ACDB (verified) filtered to this accept-list — not the raw Windows blobs.
