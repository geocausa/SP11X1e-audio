# SP11 audio recovery state assessment — 2026-07-25

## Verdict

The project is recoverable, but the currently installed Linux audio stack is
not a Windows-parity driver.

The installed topology is now structurally valid in the narrow Linux sense: it
loads, exposes working playback, and has no duplicate module instance IDs. It
is still mostly the Lenovo donor graph with the previously invalid additions
removed. It does not implement the Windows speaker-protection root, either
complete per-speaker render family, or the WSA voltage/current feedback path.

The recovered Windows evidence is substantially better than the current Linux
implementation. It is sufficient to replace the old speculative diagram with
an evidence-ranked graph and to begin a clean implementation. It is not yet
sufficient to deploy protection safely: graph lifecycle/co-selection,
cross-family selection policy, calibration payload provenance, and the exact
SoundWire VI transport behavior must still be closed.

No topology, UCM, kernel, device tree, mixer control, or PipeWire setting was
changed during this assessment.

## Current live Linux state

The final read-only snapshot is:

`artifacts/live/20260725T211836Z/`

### Boot and kernel

- Ubuntu 26.04, ARM64.
- Running kernel: `7.1.5-sp11+`.
- Boot image SHA-256:
  `1b7bbe1d0112afd8406028d370ede8e250fb6220e597a23f7b0419588bb9bb6a`.
- Boot DTB:
  `/boot/sp11-7.1.5/x1e80100-microsoft-denali-sp11-phase91.dtb`.
- DTB SHA-256:
  `dfbc3c49217aeeec91eadfc2a74a4dc88a8a76bf81458bd24194b61b5d0f0e72`.
- The phase-91 overlay changes the touchscreen DMA setup only. Its audio nodes
  are byte-equivalent at the property level to the preceding SP11 DTB.
- The recovered 7.1.5 Qualcomm AudioReach, X1E80100 machine-driver, WSA884x,
  WSA macro, and Qualcomm SoundWire source files are byte-for-byte identical
  to the pristine `linux-7.1.5.tar.xz` copies.
- The running module still issues the redundant `APM_CMD_GET_SPF_STATE`
  (`0x01001021`) query and times out. Therefore recovered patch `0006`, which
  removes only that probe-time query, is not present in the running kernel.

The deployed kernel is not exactly reproducible from the recovered kernel
directory:

- `/lib/modules/7.1.5-sp11+/build` points to the missing
  `/home/geoca/sp11-build-7.1.5/linux`;
- the recovered source tree is not a Git worktree and has no build products;
- the recovered `config-7.1.5-sp11-slim` names a different local version and
  is not proven to be the running configuration.

This is a provenance/reproducibility defect, not evidence of a hidden custom
audio implementation.

### Device tree and hardware exposure

The deployed DT correctly describes two WSA884x amplifiers on `swr0`:

- left SoundWire port map: `1 2 3 7 10 13`;
- right SoundWire port map: `4 5 6 7 11 13`;
- `WSA_CODEC_DMA_RX_0` is the only WSA DAI link in the sound card;
- the WSA macro and codec drivers expose VI-related controls and
  `WSA_AIF_VI Capture`;
- no `WSA_CODEC_DMA_TX_0` DAI link connects that VI interface to AudioReach.

The upstream kernel contains the required DAI identifiers and generic support
for `WSA_CODEC_DMA_TX_0`, speaker-protection modules, and the WSA VI macro. That
does not prove that the current SoundWire allocation and WSA884x port handling
will carry simultaneous playback plus VI correctly on this machine. Prior
experiments around shared PBR/VISENSE/CPS ports remain relevant and must be
revalidated on 7.1.5.

### Installed topology

Installed file:

`/lib/firmware/qcom/x1e80100/X1E80100-Microsoft-Surface-Pro-11-tplg.bin`

SHA-256:

`4e00057b8e316c217347bcdee0af0c6d4ff40e8e0f1870d7efeaddc2669ff54e`

Independent decode:

- 72 widgets;
- 68 AudioReach modules, four of which are preserved as raw-byte modules;
- 64 normal module definitions checked by the linter;
- no duplicate module instance IDs.

The MM1 render chain is:

```text
WR_SHARED_MEM_EP 6001
  -> PCM_DEC 6002
  -> PCM_CNV 6003
  -> SWR_SINK 6008
  -> VOL_CTRL 600c (misnamed SAL_V2 in the topology)
  -> POPLESS_EQ 6009
  -> VOL_CTRL 6004 (misnamed SAL_V2 in the topology)
  -> SAL 6007
  -> MFC 6005
  -> SOFT_PAUSE 600a
  -> SPR 600b (misnamed UNKNOWN_0x32 in the topology)
  -> DATA_LOGGING 6006
  -> device105 DATA_LOGGING 6050
  -> MFC 6051
  -> CODEC_DMA_SINK 6052
```

This is donor-shaped, not the observed Windows chain.

The topology has no `SPEAKER_PROTECTION` (`0x070010e2`), no
`SPEAKER_PROTECTION_VI` (`0x070010e3`), no VI backend graph, and no Dolby
module. The removed `stream6` graft remains available as evidence, but it was a
disconnected hybrid and must not be restored wholesale.

One inherited warning deserves explicit tracking: topology graph set 106 is a
second `WSA_CODEC_DMA_RX_1` playback backend, while current kernel DAI number
106 is `WSA_CODEC_DMA_TX_0`. It is dormant because the DT exposes only backend
105, but it demonstrates why this topology must not be treated as a current
X1E80100 design source.

### UCM, controls, and userspace

The active UCM:

- sends speaker playback to `hw:0,0` / MM1;
- sends microphone capture to `hw:0,3` / MM4;
- sets both WSA digital volumes to 72, or -12 dB;
- fixes the amplifier PA controls at 6, or 0 dB;
- enables DAC, BOOST, COMP and PBR;
- explicitly disables VISENSE;
- leaves CPS off;
- leaves both `WSA_AIF_VI` mixer inputs off.

This is a reasonable damage-limited bootstrap profile, not Windows topology
parity.

PipeWire 1.6.2 currently makes
`effect_input.sp11_speaker_eq` the default sink. That filter applies a -4 dB
preamp, a fourth-order 140 Hz high-pass, and four additional tonal sections.
It must be bypassed for structural or Windows-reference comparisons. It is
userspace tuning and cannot substitute for missing DSP graph structure,
protection, or calibration.

## Recovered evidence inventory

The recovered project occupies approximately 26 GB:

| Area | Approximate size | Files | Assessment |
|---|---:|---:|---|
| `00-RE-archive` | 22 GB | 62,370 | High-value Windows and historical corpus survived |
| `01-audio` | 54 MB | 2,181 | Current repository, decoded evidence, tools, and recent captures |
| `02-kernel` | 1.9 GB | 93,900 | Pristine 7.1.5 source plus incomplete build recipe |
| `03-iso` | 2.3 GB | 7 | Deployable 7.1.5 ISO and support files |
| `90-archive-review` | 172 MB | 1,153 | Additional recovered material requiring deduplication |

High-value file classes in `00-RE-archive` include:

| Class | Count | Total bytes | Use |
|---|---:|---:|---|
| ETL | 41 | 4,483,923,968 | Windows timing, process/module and audio-engine evidence |
| Process dumps | 11 | 1,172,428,073 | `audiodg`, DAX3API and service static/runtime mining |
| DLL | 401 | 1,932,907,704 | Dolby, Surface APO and Windows audio RE |
| SYS | 648 | 271,029,000 | Qualcomm, Surface, SoundWire and codec driver RE |
| INF | 1,260 | 28,904,026 | Authoritative endpoint/effect/format registration |
| CSV | 1,123 | 4,433,688,811 | Decoded ETW/WPR and prior analyses |
| BIN | 8,880 | 781,410,097 | ACDB chunks, payloads, firmware and extracted structures |
| WAV | 379 | 1,371,942,888 | Historical measurements; not structural proof |

### Strong surviving sources

1. **Complete Windows driver store**

   `00-RE-archive/sp11-driverdump/`

   This includes `qcadcm8380.sys`, Surface endpoint/miniport INFs, Qualcomm
   proxy APOs, Surface APO files, the Dolby DAX3 package, profiles and tuning
   XML/JSON.

2. **SP11 REV_0D ACDB and decoded tables**

   Canonical recovered copy:

   `00-RE-archive/recovered-adata/ubi/Documents/SP11/AUDIO/Research_Hub_Audio/SOURCE/UbuntuConceptEliteX/windows-derived-sp11/acdb_0d/`

   Important raw chunks include:

   - `0000fa_GKVL.bin`;
   - `00ea12_SCLU.bin`;
   - `00f4be_SCDE.bin`;
   - `00f4f2_SCDO.bin`;
   - `01e842_POOL.bin`;
   - complete `acdb_cal_0D.acdb` copies elsewhere in the same recovered hub.

3. **Runtime QGPR activation evidence**

   `artifacts/live/windows-qgpr-activations.json`

   This preserves live GRAPH_START lists and binds them to exact static GKV
   rows and POOL bundles.

4. **Fresh raw KDNET graph bodies**

   `artifacts/live/kdnet-20260723/`

   Seven GRAPH_OPEN bodies and fourteen 120-byte SET_CFG bodies survived,
   together with the original debugger log.

5. **ETW and process memory**

   Notable surviving sources include:

   - `sp11-audio-verbose.etl`;
   - large WPR audio-active and verbose captures;
   - `audiodg_ON.dmp` and `audiodg_OFF.dmp`;
   - multiple `DAX3API_*.dmp` files;
   - dedicated Dolby gate traces and DAX profile-state outputs.

6. **Ghidra database**

   `00-RE-archive/ghidra/SP11-audio.gpr`
   with `SP11-audio.rep/`.

   The repository contains about 180 MB of indexed program/user databases and
   coherent project metadata. No Ghidra instance was running during this
   assessment, so it was not opened or modified. Its on-disk shape is
   consistent with a recoverable local Ghidra project; the empty `.gpr` marker
   is not itself corruption.

## Verified Windows structural facts

### Live root/protection graph

The fresh raw KDNET body independently confirms this 13-module root:

```text
SAL 4001
  -> CHMIXER 402c
  -> SPEAKER_PROTECTION 4027
  -> SPLITTER 4002
  -> DATA_LOGGING 4003
  -> CODEC_DMA_SINK 4157

CODEC_DMA_SOURCE 4026
  -> DATA_LOGGING 4025
  -> SPEAKER_PROTECTION_VI 4024

CODEC_DMA_SOURCE 402b
  -> DATA_LOGGING 402a
  -> MUX_DEMUX 4029
  -> CPS_DATA_ROUTER_V5 0x070010e4 / IID 4028
```

There is no direct data-port `SP_VI 4024 -> SP 4027` module connection in the
Windows GRAPH_OPEN body. The body instead contains an exact
`INTENT_ID_SP` module control link between them. Any implementation that adds a
data edge merely because both modules exist is inventing topology.

### DEFAULT render family A (`7f`/`7e`; previously labelled left)

The KD body contains root `b0000001` plus `b000007f` and `b000007e`.
The two non-root subgraphs contain 16 modules, including:

- per-channel conversion, volume, SWR, popless EQ, MFC and soft-pause stages;
- `VOL_CTRL` IID `4a63`;
- two MSIIR instances;
- the exact internal module connections.

The exact SCLU bridges previously recovered are:

```text
412b:1 -> 47e9:2
467a:1 -> 4001:12
```

### NOTIFICATION render family B (`83`/`82`; previously labelled right)

The KD bodies contain both:

- root `b0000001` + `b0000083` + `b0000082`; and
- an additional submission containing `b0000083` + `b0000082` without a
  duplicate root.

Family B mirrors family A structurally and uses `VOL_CTRL` IID
`4a5f`. Its exact SCLU bridges are:

```text
4137:1 -> 47ed:2
46b8:1 -> 4001:18
```

Subsequent hash-bound driver and ACDB analysis resolved this ambiguity. Both
families use speaker endpoint key value `1`; family A selects DEFAULT
processing value `2`, while family B selects NOTIFICATION value `7`. They are
mode alternatives, not physical left/right halves. See
`docs/findings/2026-07-26-windows-render-mode-selection.md`.

### Dynamic parameters observed

All fourteen captured small SET_CFG bodies target only:

- IID `4a63` or `4a5f`;
- parameter `0x08001038` or `0x08001039`;
- parameter body size `0x68`.

Most `0x08001038` bodies carry value `0x0013615a`; later bodies show
`0x007dda19`, including a transitional body with old and new values. This is
strong evidence of live per-channel volume/control updates. It is not evidence
that these two parameters are the only runtime configuration Windows sends:
the probe deliberately excluded bodies below/above its filters and was not
placed on every GPR boundary.

### Runtime-selection boundary

The earlier QGPR trace proves GRAPH_START triples for several graph families,
including:

- `01 + 7f + 7e`;
- `01 + 83 + 82`;
- `44 + 40 + 41`;
- `01 + 27 + 26` in the captured music window.

The 2026-07-23 probe captured GRAPH_OPEN and SET_CFG, but not START, STOP,
FLUSH, or CLOSE. Therefore:

- it proves the submitted graph bodies and their order;
- it does not alone prove lifetime overlap or final co-selection;
- the older START evidence proves each listed triple was activated, but at
  different captured times;
- claims that every left and right submission was simultaneously active must
  remain a hypothesis until lifecycle packets are captured in the same run.

## Dolby boundary

The recovered endpoint INF registers the internal speaker with:

- a Dolby wrapper SFX;
- a composite Dolby wrapper plus Microsoft Surface Render APO as MFX;
- a Qualcomm proxy EFX;
- corresponding offload SFX/MFX registrations;
- MFX support even for the Windows RAW processing mode.

The Dolby package registers one-input/one-output APO objects and wrapper
CLSIDs. This is a Windows audio-engine/APO pipeline layer. No Dolby-specific
AudioReach DSP module appears in any newly captured root, left, or right
GRAPH_OPEN body.

The user's model—Dolby remains instantiated while its processing is bypassed
when the UI toggle is off—is compatible with the INF evidence. It must not be
implemented by inventing a proprietary DSP module ID in the Linux topology.

For this project the correct separation is:

```text
application
  -> Linux userspace Dolby placeholder (identity/bypass)
  -> AudioReach hardware topology
  -> WSA macro / SoundWire
  -> WSA884x amplifiers
```

The identity node is an architectural insertion point for the separate Dolby
project. It should initially make no sample changes. Mandatory Windows
hardware graph stages—conversion, per-speaker filters, gain/control, protection
and feedback—belong below it and are not optional merely because the Dolby UI
is off.

## Corrections to recent project narrative

The recent work contains useful changes:

- topology binary detection now uses the `CoSA` magic instead of filename
  extensions;
- the live-state capture now preserves controls, payloads, hashes, UCM and
  package ownership;
- the KD probe successfully recovered the missing graph bodies;
- the installed structural baseline correctly removes the duplicate IID and
  the disconnected hybrid graft.

It also contains claims that must not become implementation premises:

1. `stream6` was not discarded merely because a linter silently parsed zero
   modules. It had already been decoded and shown to be disconnected,
   cross-family, and structurally amputated.
2. Recovering its SP/SP_VI payload bytes does not make its 18-module layout a
   Windows replica.
3. The new KD run does not contain lifecycle packets, so “simultaneous L/R” is
   not yet a direct observation from that run.
4. The current topology's successful load proves syntax and unique IIDs, not
   Windows parity.
5. The current PipeWire EQ is a safety-minded tuning experiment, not a driver
   correction.

## Gap ledger

| Priority | Gap | State | Required proof/action |
|---|---|---|---|
| P0 | Linux render topology is donor-shaped | Confirmed | Build a new graph from verified Windows records, not incremental grafts |
| P0 | Windows SP root absent | Confirmed | Implement exact root modules, ports and edges |
| P0 | Per-speaker render families absent | Confirmed | Select the correct live family and reproduce exact bridges/modules |
| P0 | VI feedback transport absent | Confirmed | Add and validate WSA TX/VI DAI path, topology backend and SoundWire behavior |
| P0 | Protection calibration provenance incomplete | Confirmed | Identify exact REV_0D SP/SP_VI parameter sets and runtime order |
| P0 | Active PipeWire EQ contaminates parity tests | Confirmed | Bypass during structural/reference validation |
| P0 | Running kernel build is not reproducible | Confirmed | Rebuild from a clean pinned tree/config/patch ledger before kernel changes |
| P1 | Graph family selection/lifetime not closed | Open | One synchronized GRAPH_OPEN/START/STOP/CLOSE capture with toggle/mode markers |
| P1 | Dormant topology backend 106 conflicts with current DAI numbering | Confirmed | Remove donor backend assumptions in the new topology |
| P1 | Large SET_CFG and module payload order incomplete | Open | Capture all relevant GPR SET_CFG bodies, especially SP and SP_VI |
| P1 | WSA port-allocation behavior on 7.1.5 not proven | Open | Instrument/trace master-port allocation during simultaneous playback + VI |
| P2 | Dolby processing itself | Intentionally separate | Keep an identity userspace insertion point; do not block hardware parity |
| P2 | Loudness spikes | Still unlocalized | Revisit only after the structural baseline and userspace bypass are controlled |

## Recommended execution order

### Phase 1 — make the evidence reproducible

1. Preserve the final Linux snapshot and hashes.
2. Add a decoder for the 16-byte-header KD GRAPH_OPEN format and promote the
   three unique captured bodies to reviewed JSON.
3. Produce one canonical graph ledger keyed by evidence class:
   live KD, live QGPR, static ACDB/SCLU, or hypothesis.
4. Keep the old `stream6` binary and payloads as a source quarry only.

### Phase 2 — close the remaining Windows structural boundary

Run one version-locked Windows KDNET session that captures, in one ordered log:

- GRAPH_OPEN full bodies without the current `>= 0x500` blind spot;
- GRAPH_PREPARE/START/STOP/FLUSH/CLOSE;
- all SET_CFG bodies needed for root SP/SP_VI and both render branches;
- the six-value selectors;
- explicit markers for cold start, Dolby UI off, RAW, Media/Default, volume
  changes, and stream close.

This is the first point at which physical user action is needed: boot Windows,
attach the USB EEM KDNET link, and exercise the marked playback cases.

### Phase 3 — construct a minimal Linux hardware-parity graph offline

1. Start from a current-kernel topology skeleton, not the renamed T14s binary.
2. Implement only MM1 plus the required WSA playback and VI backends first.
3. Add the exact selected Windows per-speaker family and cross-subgraph
   bridges.
4. Add the complete root graph exactly as captured.
5. Load only parameter blocks whose origin and target are proven.
6. Keep VISENSE/CPS and speaker output disabled until graph lint, port lint,
   payload validation, and a rollback plan all pass.

### Phase 4 — kernel/DT transport validation

1. Create a clean, pinned 7.1.5-or-newer kernel worktree with a committed
   config and patch series.
2. Add the WSA VI DAI link and validate four-channel or required-channel
   feedback format.
3. Instrument SoundWire port allocation and confirm no duplicated/clashing
   master ports when playback, PBR, VISENSE and CPS requirements coexist.
4. Make any kernel change only after the trace identifies a source-level gap.

### Phase 5 — controlled hardware bring-up

Bring up in increasing-risk order:

1. graph opens with amplifiers muted;
2. VI transport with no speaker drive;
3. low-level fixed test signal with telemetry;
4. SP/SP_VI calibration validation;
5. normal playback with fixed gains;
6. userspace identity/Dolby placeholder inserted;
7. only then investigate presentation tuning and the historical loudness
   symptom.

## Immediate next action

No additional broad disk mining is needed before implementation work starts.
The next local task should be Phase 1: normalize the fresh KD bodies into
reviewed machine-readable evidence and build the canonical Windows/Linux graph
ledger. The next physical capture should wait until the lifecycle/SET_CFG probe
is widened and preflighted offline.
