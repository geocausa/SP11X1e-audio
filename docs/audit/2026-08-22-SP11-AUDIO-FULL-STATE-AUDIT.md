# SP11 audio full-state audit — 2026-08-22

## Scope and safety boundary

This audit reconstructs the built-in-speaker work from the current SP11 Linux
boot, the canonical Git repository and research branches, the local reverse-
engineering archive/kernel kitchen, the older Linux `SP11-AUDIO-AUDIT` copy,
and the live SP7 evidence store available through PiMaster.

The operator required **no reboot**. No GRUB default, boot image, initramfs,
DTB, kernel module, topology, UCM route or live amplifier control was replaced.
The running audio graph was not restarted. Read-only checks and previously
bounded instrumentation were used.

## Executive result

There is no evidence of a large hidden runtime mistake in the promoted
Golden-v32 kernel/protection stack. The machine is booted into the intended
hash-pinned v32 entry, every promoted module identity and the canonical
topology match the manifest, the exact UCM file matches Git, protection
feedback is active in the accepted Windows-shaped 8/24-kHz layout, and the
current boot contains no PA fault/recovery, SoundWire fault, XRUN or canonical
GLINK timeout.

The main remaining risk is **housekeeping and qualification, not a broken live
audio path**. The reproducibility risk identified in F01 was closed on
2026-08-23: Golden v32 is now replayable from a hash-pinned pristine Linux
7.1.5 base plus a compact tracked source overlay and ordered patches 0069-0071.
A zero-state build reproduced all five deployed v31 identities, all five v32
identities, and the runtime ELF payload digests of all five historical Golden
modules. The large historical kitchens are therefore no longer the sole source
of truth, although cleanup remains a separate manifest-driven task.

The active userspace graph is the disposable UbiG candidate, not the promoted
Windows-binary bridge. Its source and latest bounded gates are pushed, it is
W^X-hardened and maps no Windows DSP DLL, but it is **not Golden yet**. It still
lacks non-muted acoustic/physical parity, seek/program-content, more than eight
hours of continuous soak in this activation, and longer physical-output
protection telemetry. The private owner pack is also required to reproduce the
candidate deployment from public source.

## Authoritative current state

| Layer | Audited state | Result |
| --- | --- | --- |
| Boot | `sp11-audio-v32-feedback-exact-golden`; marker `sp11_entry=7.1.5-sp11-v32-feedback-exact-golden` | PASS |
| Kernel | `7.1.5-sp11-render-parity-v4+` | PASS |
| Saved GRUB default | `sp11-audio-v32-feedback-exact-golden`; no one-shot `next_entry` | PASS |
| Golden verifier | image/initrd/DTB/topology hashes plus five live module srcversions | PASS |
| Topology | SHA-256 `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d` | PASS |
| UCM | installed and tracked SHA-256 `d9cc675fd4d432f62fd3e01fba32a8afe22a64e988fac476120161e67c63fb54` | PASS |
| ALSA | one internal card; `MultiMedia1` playback | PASS |
| PipeWire | 1.6.2; expected visible control sink and hidden engine/physical path | PASS |
| Golden v32 tests | `197 passed, 3 skipped, 6 subtests passed` | PASS |
| UbiG tests | compiled C checks pass; `207 passed, 3 skipped, 6 subtests passed` | PASS |
| Current kernel faults | zero PA fault/recovery, zero SoundWire fault/XRUN, zero canonical GLINK timeout | PASS |

Exact promoted module srcversions observed live:

- WSA macro `F32C7A03F713D1B20F0BF78`;
- WSA884x `5859E70AFD0A1D420E8ADD4`;
- SP11 machine driver `13326073E27DFA035180C56`;
- Qualcomm SoundWire `D008A3D6B585C11BE023992`;
- q6apm `687B16CF9C43B43E90C0746`.

## Evidence reconciliation

### SP7 microphone/acoustic evidence

The four raw SP7 WAV files behind the published v32 acoustic-parity checkpoint
were independently re-hashed on SP7. All match the hashes recorded in
`artifacts/reviewed/2026-08-21-v32-windows-acoustic-parity-estimate.json`:

| Evidence | Recorded and live SP7 SHA-256 |
| --- | --- |
| Windows two-pass consumer matrix | `4E6D764D009D296DCE4770A06DCACD6AAABF93EAB79C6E91427FA9663FB8C1C7` |
| Linux v32 two-pass consumer matrix | `D933392E1A86591FD5353B9C20C7448F56833BF22AED416CDEA7376461BF649C` |
| Windows Seven Nation Army | `8C9165073E487410AD8667688182694C91DD63F89BE676B1C9722F4E29F837B8` |
| Linux v32 Seven Nation Army | `13F5667A6BA3D713D5978EF814E13868557E5CD73F26E6DB5E8167DBE16C0B04` |

The underlying 72-file, 668,415,138-byte Aug-21 acoustic directory is now
preserved separately under
`00-RE-archive/SP7-v32-acoustic-evidence-preservation-20260822/`. The transfer
ZIP passed integrity testing, extracted to the same 72-file/668,415,138-byte
inventory and has SHA-256
`c2cb90948c1ac02724493ee67a5b4213f163d28e4dc2f7dba942b61a138bbe6d`.
It must not be confused with the older 74-file `SP11-AUDIO-AUDIT` folder.

The published ~0.29 dB mean-absolute difference from 315 Hz up and ~0.20 dB
from 630 Hz up are supported by the retained reviewed data and raw-file hashes.
The rough “98%” label remains an engineering status estimate, not a mathematical
sound-quality score. Sub-315-Hz evidence remains household-noise limited.

### Older SP7 audit folder

The live SP7 `SP11-AUDIO-AUDIT` folder contains 74 files / 133,219,644 bytes.
Compared with the older Linux `/home/geoca/Documents/SP11-AUDIO-AUDIT` copy:

- 17 files are exact matches;
- 55 files were absent locally;
- two narrative files, `SP11-AUDIO-PARITY-LEDGER.md` and
  `HANDOFF-TO-NEW-CHAT-2026-08-13.md`, differ because the Linux copies were
  later marked or updated.

The complete SP7 folder is now preserved at
`00-RE-archive/SP7-AUDIO-AUDIT-preservation-20260822T2025/` with transfer ZIP
SHA-256
`7ccc98400567a14541b708196ea542cb1b794b7db844546fcddd9a23b86422f8`.
The ZIP passed integrity testing and all 74 files were extracted. Only the
temporary transfer ZIP was deleted from SP7; original evidence was untouched.

### Gemini/MAX34417 claim

The Gemini PMIC notes are not an audio source of truth. The specific claim that
MAX34417/PA05 is a speaker-amplifier current monitor is contradicted by the
recovered Surface ACPI labels: those channels identify platform memory, Wi-Fi
and CPU rails. The optional devices also NACKed during Linux probing.

MAX34417 cannot identify left/right speaker current or prove that speaker
protection acted. The relevant sources are WSA8845 local status, native VI/CPS
feedback, DSP events/telemetry and controlled acoustics. No missing MAX34417
driver is blocking Golden v32.

## Findings

### F01 — CLOSED 2026-08-23: Golden v32 clean-source replay is proven

**Where:** `repro/golden-v32/`, `patches/0069-*` through `patches/0071-*`,
`artifacts/reviewed/2026-08-23-f01-golden-v32-clean-repro/`.

**Closure:** Golden v32 is no longer dependent on reconstructing state from the
mutable kernel kitchens. The accepted source lineage is expressed as:

- pristine `linux-7.1.5`, pinned by whole-tree SHA-256
  `7e5f8ccd76f625cb678028fe6bab2d3ef0c03878c2af21433c96f4a78b813fef`;
- a tracked 23-file Golden-v31 source overlay with per-file SHA-256 manifest;
- the pinned Golden kernel config SHA-256
  `4fed1ee935cff7589ed2941d0bf2ddec4ddd2a03d919b9dc30ce20f5d85665ca`;
- ordered patch `0069` for post-PA protection-clock ownership, `0070` for the
  active feedback Offset2 setting, and `0071` for CPS wake/packetization parity.

The reconstruction also explains the historical identity mismatch that made
several obvious source trees look wrong: the promoted modules were produced by
a mixed Kbuild context. The exact recipe preserves that context deliberately:
WSA macro and X1E use in-tree `O=` outputs; WSA884x, SoundWire and q6apm use
scoped `M=` builds against the same clean output environment where required by
the historical module identity.

On 2026-08-23, `repro/golden-v32/build-and-verify.sh` deleted its work area and
started from the pinned pristine tree. The zero-state run completed at
`2026-08-23T08:04:08+01:00` with `GOLDEN v32 CLEAN REPRODUCTION PASS`. It first
reproduced all five deployed Golden-v31 srcversions, then applied only
0069-0071 and reproduced the five Golden-v32 srcversions:

- WSA macro `F32C7A03F713D1B20F0BF78`;
- WSA884x `5859E70AFD0A1D420E8ADD4`;
- SoundWire QCOM `D008A3D6B585C11BE023992`;
- X1E80100 machine driver `13326073E27DFA035180C56`;
- q6apm `687B16CF9C43B43E90C0746`.

The gate additionally hashes runtime-relevant ELF allocatable, relocation,
`.modinfo` and `__versions` sections while excluding path-sensitive DWARF,
symbol/string tables, compiler comments and build-id metadata. All five clean
outputs match the preserved historical Golden runtime payload digests exactly.
Raw `.ko` SHA-256 is intentionally not the replay criterion because build-path
DWARF and, for the historical q6apm copy, module-signing bytes are not runtime
semantic identity.

**Result:** the audit's only HIGH finding is closed. Old build trees are now
eligible for F07 manifest-driven cleanup, subject to keeping the tracked replay
recipe, reviewed closure evidence, pristine base, and chosen rollback boot
artifacts. No running Golden module, boot image, GRUB default or live audio path
was changed by this closure.

### F02 — MEDIUM, corrected: project entry documents were stale

**Where:** workspace `README.md`, `CURRENT-AUDIO-GOLDEN.txt`, and the old
canonical render ledger.

**What:** The workspace README still described the July-31 observation kernel,
absent CPS/PBR and runtime volume as future work. The current-Golden file still
named v31. The long render ledger's “Overall gate” still names v31 even though
v32 was promoted on Aug-21.

**Action:** The two workspace entry files were updated during this audit. The
ledger receives an explicit v32 audit banner in the audit commit; its detailed
v31 chronology remains valuable historical evidence and is not rewritten.

### F03 — MEDIUM, corrected: newer SP7 raw evidence was not mirrored locally

**Where:** SP7 `Documents\KDNET\Codex\acoustic-reference-keyboard-length-20260821`
and the Aug-22 `SP11-AUDIO-AUDIT` preservation paths.

**What/why:** Git deliberately stores reviewed summaries rather than large raw
WAVs, but the decisive raw evidence existed only on SP7. A single-machine loss
would leave hashes without the underlying capture.

**Action:** Preserve both the 74-file older audit set and the 72-file v32
acoustic set locally, with transfer ZIP hashes and extraction counts. Keep the
SP7 originals.

### F04 — MEDIUM: diagnostic lifecycle kprobes remain armed on the daily driver

**Where:** enabled `sp11-wsa-boot-lifecycle-trace.service`,
`/usr/local/sbin/sp11-wsa-boot-lifecycle-trace`, tracefs group `sp11`.

**What:** Eleven read-only lifecycle kprobes are armed, tracing is on, and an
8192-KB trace buffer is reserved. The probes do not write codec registers and
there is no observed audio fault, but this is diagnostic state rather than a
production requirement.

**Why it matters:** It adds avoidable tracing overhead, occupies global ftrace
state and can confuse later experiments about which observer owns tracing.

**Required closure:** after preserving the trace/service provenance, explicitly
disable the event group, remove only `sp11/*` probes, disable the service and
verify no other tracing owner is active. This can be done without reboot, but
was not mixed into this read-only runtime audit.

### F05 — LOW: one known GET-only record still produces aggregate SET_CFG noise

**Where:** graph calibration record 63, IID `0x412b`, PID `0x0800113d`;
`tools/acdb_protection_stage_builder.py` historical dirty-tree candidate.

**What:** Each graph birth logs `AR_EUNSUPPORTED` because the aggregate contains
the 28-byte `PARAM_ID_SPR_SESSION_TIME` GET-only readback record. The graph then
continues exactly under the proven Qualcomm GSL policy; the other 106 records
and all protected stages are accepted.

**Why it matters:** It is log noise and an ambiguity for future diagnostics,
not evidence that calibration or protection failed. A filtered 10,416-byte,
106-record body was previously built and live-accepted, with tests retained in
the mixed historical tree.

**Required closure:** port the filter and its regression into the clean tree as
a deliberately versioned future-topology change, then re-run topology hash and
physical gates. Do not silently mutate Golden-v32's canonical topology.

### F06 — MEDIUM: UbiG is a live candidate, not a completed replacement

**Where:** `/home/geoca/Documents/SP11-PROJECT/03-UbiG`, branch
`ubig/deblob-main`, current tip `a92e5ef`.

**Verified:** candidate plugin SHA-256
`1b3e3110c07bdc5b21e3c59ee75482a598c35e3acf27c2e600fbc6fa50355eb8`;
owner pack SHA-256
`c993c123f2cb3b92776754da2383217e00b5f290664571f12cfb62b9afb3a175`;
no mapped Dolby DLL; `MemoryDenyWriteExecute=yes`; control page healthy; filter
PID has not restarted; muted volume/CKV, 20-cycle playback, nonzero-muted DSP
and bounded dual-amp protection observer gates pass.

The MSIIR helper service being inactive is intentional: it exits successfully
because the combined Windows volume transaction, still installed under the
legacy unit name `sp11-dolby-volume-sync.service`, owns GainStep updates and is
active.

**Missing before promotion:** non-muted physical/acoustic Windows comparison,
seek/program-content lifecycle, more than eight continuous hours in one
candidate activation, longer physical-output PA/protection telemetry, and a
public/reproducible owner-pack generation/distribution story. Golden rollback
must remain installed until these pass.

### F07 — MEDIUM: diagnostic boot/build clutter consumes substantial space

**Where:** `/boot`, `/etc/grub.d`, `02-kernel`.

**What:** There are 29 SP11 audio GRUB fragments and roughly two dozen v31
diagnostic boot directories, mostly 179–201 MB each plus a 472-MB DP14 shadow
and 660-MB CPS-v3 rescue. The kernel workspace is 112 GB, dominated by:

- `build-softpause-full-20260813` — 32 GB;
- `build-cps-v3-20260811` — 32 GB;
- `candidates/` — 25 GB;
- `build-audio-powerlab-20260810` — 5.4 GB;
- source snapshots — several 1.8–2.4 GB trees;
- `v3-runtime-backups` — 1.9 GB.

**Dependency result:** Golden v32 is self-contained and hash-pinned in its own
boot directory. Its files are regular files, not symlinks into diagnostic boot
directories. Deleting unrelated diagnostic entries later will not break the
current v32 entry. Keep v31, v28 and CPS-v3 until the operator chooses a smaller
rollback set.

**Required closure:** F01 is complete, so manifest-driven cleanup is now
unblocked. Retain `repro/golden-v32/`, the reviewed F01 closure evidence, the
hash-pinned pristine Linux 7.1.5 base, one clean replay work tree while cleanup
is being reviewed, and the operator-selected rollback boot set. Then remove
rejected candidate initrd trees/modules and their matching `/etc/grub.d`
fragments in one reviewed manifest-driven cleanup.

### F08 — LOW: old condition-skipped services and one redundant SP7 file remain

Enabled but condition-skipped units include the old UCM overlay and first-boot
helpers. They do not alter this boot but make system state harder to read.
SP7's `SP11X1e-audio-adie` worktree also has one untracked 330-line passive WSA
extract; the full source log is already tracked on main. Preserve or remove it
only as explicit housekeeping, not as a missing discovery.

### F09 — INFO: unmerged branches do not represent a missing runtime merge

The Windows DIAG, CPS/render-family, WSA lifecycle, Dolby and ASAR branches
contain unique historical commits. Later main commits and reviewed artifacts
semantically incorporate the conclusions used by v32. The Windows-DIAG branch's
single checkpoint is evidence history, while main contains the later working
0x1586 feedback capture and v32 closure. UbiG remains intentionally separate
because it is not promoted.

Do not mechanically merge all branches: that would reintroduce old binaries,
rejected candidates and superseded documentation. Preserve branch refs, classify
them in the audit, and only cherry-pick a specific missing reviewed artifact
when its absence is demonstrated.

## Branch/worktree disposition

| Location/branch | State | Disposition |
| --- | --- | --- |
| `01-audio-cps-review/main` | clean at `8118a64` before audit; matches `origin/main` | canonical Golden source/docs |
| `03-UbiG/ubig/deblob-main` | clean at pushed `a92e5ef` | active disposable userspace candidate |
| `01-audio/agent/audio-v2-clean-rebuild` | ahead and heavily mixed tracked/untracked history | evidence mine only; do not bulk commit |
| Windows DIAG branch | three unique evidence commits | retain branch; conclusions superseded/incorporated by later main |
| CPS/render-family branch | eight unique historical commits | retain branch; major findings/tools later incorporated |
| WSA lifecycle branch | nine unique diagnostics/provenance commits | retain; also source of the stale enabled kprobe service |
| Dolby/ASAR branches | large historical algorithm/RE histories | retain; UbiG uses their conclusions, not a blind merge |
| SP7 ADIE worktree | behind current main plus one redundant untracked extract | no loss; explicit later housekeeping |

## Safe cleanup classes

### Keep now

- `/boot/sp11-7.1.5-audio-v32-feedback-exact-golden`;
- Golden v31 and v28 boot directories and CPS-v3 rescue until the operator
  explicitly reduces rollback coverage;
- `repro/golden-v32/`, the reviewed F01 closure evidence and the hash-pinned
  pristine Linux 7.1.5 base;
- canonical main and UbiG Git worktrees;
- both new SP7 preservation archives and original SP7 evidence;
- private UbiG owner pack while the candidate is under test.

### Likely removable now after a manifest review

- forced TAP2/TAP3, DP14, host-clock, post-start, active-Offset2 and other
  rejected/diagnostic `/boot` directories plus their exact GRUB fragments;
- unpacked initrd copies duplicated inside rejected candidate directories;
- obsolete object trees once one exact-v32 clean rebuild tree has passed;
- condition-skipped diagnostic service units after their provenance is kept.

No deletion was performed in this audit.

## Completion gates after this audit

1. **DONE 2026-08-23:** normalize and test the exact-v32 source/patch/build
   recipe (F01); zero-state replay PASS.
2. Finish UbiG M6 without touching Golden rollback: non-muted acoustics,
   seek/program content, >8-hour soak, longer protection telemetry and owner-
   pack packaging.
3. If stronger bass certainty is desired, repeat sub-315-Hz RAW work in a quiet
   controlled window; do not tune against the current low-confidence residual.
4. Port the GET-only calibration filter as a future versioned topology change,
   not a silent v32 mutation.
5. Disarm stale diagnostic kprobes and then perform manifest-driven disk/GRUB
   cleanup.

Suspend/resume, microphone capture on SP11 and Bluetooth remain deliberately
outside the built-in-speaker sound-quality gate.

## Audit changes and publication

- completed and pushed the coherent UbiG pre-audit work as `a92e5ef`;
- refreshed the workspace README and current-Golden pointer;
- preserved the missing SP7 audit and v32 acoustic evidence locally;
- added this audit and a v32 warning banner to the canonical render ledger;
- did not reboot, arm a one-shot boot, replace runtime audio components or
  delete rollback/build data.
