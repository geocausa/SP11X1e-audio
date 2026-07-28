# Windows live GRAPH_OPEN capture — decoded (KDNET 2026-07-23)

Grade **A** (live runtime). Source: `artifacts/live/kdnet-20260723/capture.log`
(SP7→SP11 KDNET, `qcadcm8380.sys` version-locked). Raw bodies + payloads in
`artifacts/live/kdnet-20260723/`.

## Captured
- **7 GRAPH_OPEN OOB bodies**: 5× `0xb18` (root `0xb0000001`), 2× `0x5e0`
  (`0xb0000083`). The `0xb18` = `0xac8 + 0x50` split the audit predicted but
  never dumped — now recovered in full.
- **14 SET_CFG payloads** — all to two SAL_V2 modules `0x4a5f`/`0x4a63`, params
  `0x08001038` and `0x08001039` (104 B **limiter config**), sent repeatedly.
- **72 selector captures** (x8 unmapped on many hits; x1-struct fallback caught
  them). Dominant 3-key GKV vector `[01000001=1, 01000002=1, 01000003=1]`.

## Windows speaker root `0xb0000001` (live module connections)
```
SAL 4001 → CHMIXER 402c → SPv5 4027 → SPLITTER 4002 → LOGGER 4003 → CODEC_DMA_SINK 4157
SPLITTER 4002 :5/:9/:11 → external MFC 4747 / 47c9 / 4730
VI-SENSE FEEDBACK:  CODEC_DMA_SRC 4026 → LOGGER 4025 → SP_VI 4024
                    CODEC_DMA_SRC 402b → LOGGER 402a → MUX_DEMUX 4029 → 4028
```
Render chain (feeds root, contains the dynamically-tuned limiter):
```
47e9 → SAL_V2 4a63 → 4675 → 489e → 48a1 → 467a
```
This **live** capture confirms the audit's previously static (grade-B) root and
upgrades it to grade A.

## The concrete Linux divergences (below the EQ layer)
1. **Closed-loop VI-sense feedback runs on Windows** (`amp → CODEC_DMA_SRC →
   LOGGER → SP_VI 4024`). Linux has no WSA VI capture link (cf. Brief 10) — the
   loop cannot run.
2. **Dynamic limiter config**: Windows writes SAL_V2 `0x08001039` (104 B) to both
   channel limiters continuously. Linux DSP returns `rc=-22` for that param
   (verified-param-engagement spec) — Linux cannot apply it as-is.
3. **Full protection root** (`SAL→CHMIXER→SPv5→SPLITTER` + VI feedback) is absent
   from the Linux topology; the cleaned baseline removed even the broken stub.

Corroborates the live Linux finding that the WSA884x smart amps come up
**unconfigured** (no controls, no VI feedback, no limiter). Windows runs active
closed-loop protection + limiting on these speakers; Linux runs them bare — a
root-cause class that no userspace EQ can address.

## Full mine (complete 2026-07-23) — the live Windows speaker topology

The `0xb18` open builds **root `0xb0000001` + `0xb000007f` + `0xb000007e`**; the
`0x5e0` open builds `0xb0000083 + 0xb0000082`. Together = two symmetric L/R
render chains feeding one shared protection root:

```
L render (SG7e→SG7f): RD_SHMEM → SAL_V2 4663 → DATA_LOG → POPLESS_EQ 4664 →
    SAL_V2 4669 → MFC 466a → UNKNOWN_0x32 412b
    412b:1 → SG7f: SAL_V2(limiter) 4a63 → MSIIR 489e → MSIIR 48a1 → 467a
    467a:1 → root SAL 4001:12
R render (SG82→SG83): RD_SHMEM → SAL_V2 46a1 → DATA_LOG → POPLESS_EQ 46a2 →
    SAL_V2 46a7 → MFC 46a8 → UNKNOWN_0x32 4137
    4137:1 → SG83: SAL_V2(limiter) 4a5f → MSIIR 48a8 → MSIIR 48a9 → 46b8
ROOT 0xb0000001: SAL 4001 → CHMIXER 402c → SPv5 4027 → SPLITTER 4002 →
    DATA_LOG 4003 → CODEC_DMA_SINK 4157;  SPLITTER:5/9/11 → MFC 4747/47c9/4730
    VI feedback: CODEC_DMA_SRC 4026 → DATA_LOG 4025 → SP_VI 4024
                 CODEC_DMA_SRC 402b → DATA_LOG 402a → MUX_DEMUX 4029 → 4028
```
So each channel runs POPLESS_EQ + **two MSIIR stages** + a SAL_V2 limiter before
the shared SPv5 protection + VI-sense loop. This is the DSP-graph blueprint.

### What the capture DOES contain (grade A)
- Complete graph **structure**: every subgraph, container, module (by type),
  connection, and cross-subgraph bridge for the live L/R speaker path.
- The **dynamic limiter** SET_CFGs (SAL_V2 0x4a5f/0x4a63; `0x08001038` carries
  value 0x0013615a×2, `0x08001039` near-zero).

### What it does NOT contain (⇒ the targeted follow-up)
- **Per-module tuning values are NOT in the GRAPH_OPEN bodies — they are
  structure-only.** No EQ table / MSIIR coeff / SPv5 config blocks appear. Those
  come from the **REV_0D ACDB** (which we have) applied by graph key, or a
  SET_CFG path this breakpoint set didn't catch. → extract the tuning for the
  live instances (4664/46a2 EQ, 489e/48a1/48a8/48a9 MSIIR) from the ACDB.
- **Codec / SoundWire side absent** — how the WSA884x amps get registers, gains,
  R0/T0 thermal cal (different Windows driver; `qcadcm` breakpoints never see it).
- **Physical L/R → WSA port mapping** only partially inferable (root sink side).

### Precise spec for a proper follow-up KDNET session
Add a **codec-driver breakpoint set** (WSA/SoundWire register writes + R0/T0),
broaden SET_CFG/SET_PARAM coverage to catch host-sent module params, and cover
all speaker families + clean selectors. Everything else is now derivable from
this capture + the REV_0D ACDB.
