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
W^X-hardened and maps no Windows DSP DLL, but it is **not Golden yet**. Follow-up
M6 work on 2026-08-23 closed every machine-verifiable promotion gate: the same
filter process exceeded eight continuous hours with zero restarts, deterministic
program seeks passed under exact DSP mute, a 30-second real-output WSA telemetry
run was fault-clean, and the tracked private-pack builder reproduced the active
v4 owner pack byte-for-byte from owner-supplied inputs. The only remaining M6
promotion gate is the matched physical Windows-vs-UbiG acoustic matrix / operator
listening verdict.

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
| UbiG tests | compiled C checks pass; `211 passed, 3 skipped, 6 subtests passed` | PASS |
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

### F04 — CLOSED 2026-08-23: diagnostic lifecycle kprobes disarmed

**Where:** `sp11-wsa-boot-lifecycle-trace.service`,
`/usr/local/sbin/sp11-wsa-boot-lifecycle-trace`, tracefs group `sp11`, and
`artifacts/reviewed/2026-08-23-f04-lifecycle-trace-teardown/`.

**Pre-state:** The service was enabled and active/exited since the Aug-21 boot.
Tracefs had `tracing_on=1`, `current_tracer=nop`, an 8195-KB buffer and eleven
enabled `sp11/*` kprobes. `set_event` contained exactly those eleven events.
A root-side inventory found no enabled non-SP11 trace events, no trace
instances and no process holding tracefs open. A bounded trace tail confirmed
the probes were still firing on normal PipeWire playback lifecycle activity.

**Action:** The exact unit, arming script, hashes, probe definitions and bounded
trace tail were preserved first. The `sp11` event group was then disabled,
global tracing was stopped only after confirming there was no other tracing
owner, and exactly the eleven `sp11/*` probes were removed. The arming service
was stopped and disabled without deleting its unit or script.

**Post-state:** `sp11-wsa-boot-lifecycle-trace.service` is disabled/inactive;
`tracing_on=0`; `current_tracer=nop`; `events/sp11` is absent; there are zero
SP11 kprobes, zero total dynamic kprobes, zero `set_event` entries, zero enabled
non-SP11 events and zero trace instances. The retained unit/script hashes remain
`23c14eb8606c26ffbcefb267f5f7d88db2d6c2aa70433b75ec0ab255312291ef`
and `674ceb77c93a7671f37b44b13322ce4c36c25cc78b46ddd3e036ca87ebffb51b`.
The UbiG filter-chain process remained at PID 599944 and no PA/SoundWire/XRUN,
canonical GLINK-timeout, kernel Oops or call-trace match appeared after teardown.
No reboot or live audio graph restart was required.

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

### F06 — MEDIUM, narrowed 2026-08-23: only matched physical acoustic promotion gate remains

**Where:** `/home/geoca/Documents/SP11-PROJECT/03-UbiG`, branch
`ubig/deblob-main`, pushed tip `1db43db`.

**Current candidate identity:** plugin SHA-256
`b57d9cf7ef0482ab0c6cb3089d3d456dc034a66c4244b835ca97989883de8e2c`;
private Stage-B v4 pack SHA-256
`30b9b8ce8dace4a9f5dee2c2defa7da2d9b8431cf68fb323f8d2c3e4e3c942df`;
static-window SHA-256
`707722c70b5792b3e9d7a237f61dbc601f3c92c1e8638717c34118e723997e22`.
The filter-chain maps the source-owned UbiG plugin and no Windows DSP DLL;
`MemoryDenyWriteExecute=yes` remains effective.

**Machine-verifiable M6 closure:** The same `filter-chain.service` process,
PID 599944, has been continuously active since 2026-08-22 23:11:10 BST with
`NRestarts=0` and exceeded nine hours during the follow-up. The exact retained
Seven Nation Army source reproduced the three deterministic FLUSH|ACCURATE
program seeks under exact endpoint DSP mute with no candidate restart,
control-generation movement, xrun/NaN/crash marker or kernel audio fault. A
separate 30-second unmuted physical-output run at the existing 14% visible
volume retained both WSA amps at PA `01`, status `2f/00`, errors `00/00`,
current-limit code 17/no override; the bounded kernel observer returned 40
samples per amp with `failed=0x0` and no WSA/SoundWire/XRUN/canonical-GLINK
fault. The observer was returned to zero afterward.

The private-owner-pack distribution/reproduction story is also now explicit and
proven. `ubig/tools/build_stageb_v4_pack.py` regenerated a mode-0600,
1,139,037-byte v4 pack byte-for-byte identical to the active pack from the
owner's private v3 pack plus locally owned `DolbyAPOVR.dll`; no private/vendor
payload is stored in Git. Candidate control checks pass and the full Python
suite is `211 passed, 3 skipped, 6 subtests passed`. One stale test expectation
for the retired v3 pack hash was corrected as part of the M6 checkpoint.

Reviewed evidence is under
`03-UbiG/artifacts/reviewed/2026-08-23-ubig-m6-longrun-seek-physical-telemetry/`
and the corresponding finding is
`03-UbiG/docs/findings/2026-08-23-UBIG-M6-MACHINE-GATES-CLOSED.md`.

**Remaining before promotion:** only a matched **physical Windows-vs-UbiG
acoustic matrix / operator listening verdict**. This cannot be inferred from
PipeWire samples or protection telemetry. Golden v32 rollback must remain
installed until that physical gate passes. The Windows userspace bridge must not
be retired before then.

### F07 — CLOSED 2026-08-23: diagnostic boot/build clutter cleaned

**Where:** `/boot`, `/etc/grub.d`, `02-kernel`, and
`artifacts/reviewed/2026-08-23-f07-cleanup/`.

**Pre-state:** There were 29 SP11 GRUB fragments and 26 SP11 boot directories.
The kernel workspace reached roughly 129 GB during the F01 clean replay, with
only about 12 GB filesystem free space at 96% usage. The largest obsolete
objects were the 32-GB soft-pause build, the 32-GB CPS-v3 build, the 5.4-GB
power-lab build and a 25-GB candidate tree dominated by duplicate initrd,
verification and module payloads.

**Manifest and rollback policy:** The cleanup retained exactly four boot menu
IDs and their boot directories: Golden v32 (current/saved), Golden v31, Golden
v28 and CPS-v3 rescue. The manifest accounted for every SP11 boot directory and
fragment before deletion. There were no symlinks under the SP11 boot trees, and
the current `/proc/cmdline` plus GRUB environment both identified Golden v32.
Hashes of all 24 files in the four retained boot trees were captured before
cleanup and verified unchanged afterward.

**Action:** Twenty-five disposable/diagnostic GRUB fragments and 22 matching
boot directories were removed, reclaiming about 4.4 GB, then `update-grub`
regenerated a menu containing only the four retained SP11 IDs. Compact
provenance (`.config`, `Module.symvers`, kernel-release/compiler metadata and
key hashes) was preserved before deleting the obsolete 32-GB soft-pause,
32-GB CPS-v3 and 5.4-GB power-lab object trees. In `candidates/`, an explicit
302-item manifest removed generated initrd/unpack/verify/extract trees and
standalone candidate initrd/vmlinuz/module copies (about 20.3 GB of manifested
payload) while preserving source, provenance and psycho-acoustic evidence.
The candidate tree shrank from 25 GB to 6.0 GB.

**Post-state:** `02-kernel` is 56 GB, including the retained ~33-GB successful
clean Golden-v32 replay tree and pristine Linux 7.1.5 base. Filesystem usage is
about 60% with roughly 103 GB free. `/boot` and `/etc/grub.d` each contain only
the four selected SP11 rollback/current entries, and `saved_entry` remains
`sp11-audio-v32-feedback-exact-golden`. The Golden-v32 verifier passed after
cleanup, including all five live module srcversions and the v32 feedback
parameters. No reboot occurred and no running Golden boot artifact was changed.

**Result:** the audit's storage/boot clutter finding is closed. Smaller source,
staging and historical backup trees remain intentionally because they are
provenance/evidence rather than the rejected duplicate payload class targeted
by F07.

### F08 — CLOSED 2026-08-23: stale condition-skipped services and redundant SP7 extract cleaned

**Where:** system units `sp11-audio-first-boot.service`,
`sp11-tap3diag-restore.service`, `sp11-wsa-ucm31-v7-overlay.service`; SP7
worktree `C:\Users\SurfacePro7\Documents\SP11X1e-audio-adie`; reviewed
evidence `artifacts/reviewed/2026-08-23-f08-stale-service-cleanup/`.

**Pre-state:** all three units were enabled but inactive with
`ConditionResult=no`. They target, respectively, the obsolete
`sp11_entry=7.1.5-sp11-audio-vi` first-boot capture, the removed TAP3 diagnostic
boot, and the old `sp11_wsa_ucm31_v7=1` bind-mount UCM experiment. No UCM bind
mount was active. Their exact unit/helper bytes and SHA-256 values were
preserved before changing enablement.

**Action/post-state:** only those three stale units were disabled. The active
`sp11-audio-v32-verify.service` and `sp11-bt-fix.service` remain enabled and
active/exited. The unit files/scripts themselves remain on disk for provenance;
only their boot-enable symlinks were removed.

SP7's ADIE evidence worktree contained one untracked 12,721-byte / 330-line
normalized passive WSA extract, SHA-256
`87E636E3C65F149F9B0E60C80CF9BE61FAB9C034E7F7DF7F9F3C74F964417673`.
That exact extract hash is already recorded in tracked
`2026-08-16-windows-qcaucd-wsa-passive.json`, and the full tracked raw source log
matches its recorded SHA-256
`DD8EF0672EB28CD55719FBDE7D840D23A98D1BF8A9CF32A50A246E65F7B6A47F`.
The redundant untracked extract was therefore removed explicitly; the SP7
worktree is clean afterward.

**Result:** F08 is closed without altering any active audio service, UCM file,
Golden boot artifact or tracked evidence.

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
| `03-UbiG/ubig/deblob-main` | clean at pushed `1db43db` | active disposable userspace candidate; all machine-verifiable M6 gates GREEN |
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

### Removed in the 2026-08-23 F07 follow-up

- forced TAP2/TAP3, DP14, host-clock, post-start, active-Offset2 and other
  rejected/diagnostic `/boot` directories plus 25 exact GRUB fragments;
- generated initrd/unpack/verify/extract and duplicate module payloads inside
  rejected candidate directories, via the reviewed 302-item manifest;
- obsolete 32-GB soft-pause, 32-GB CPS-v3 and 5.4-GB power-lab object trees,
  after compact build provenance was preserved.

The original read-only audit deleted nothing; the later F07 closure performed
these deletions only after F01 clean replay and explicit manifest validation.

## Completion gates after this audit

1. **DONE 2026-08-23:** normalize and test the exact-v32 source/patch/build
   recipe (F01); zero-state replay PASS.
2. Finish UbiG M6 without touching Golden rollback: only the matched physical
   Windows-vs-UbiG acoustic matrix / operator listening verdict remains; all
   machine-verifiable M6 gates are GREEN as of 2026-08-23.
3. If stronger bass certainty is desired, repeat sub-315-Hz RAW work in a quiet
   controlled window; do not tune against the current low-confidence residual.
4. Port the GET-only calibration filter as a future versioned topology change,
   not a silent v32 mutation.
5. **DONE 2026-08-23:** manifest-driven disk/GRUB/build cleanup (F07);
   Golden v32 verifier PASS after cleanup.

Suspend/resume, microphone capture on SP11 and Bluetooth remain deliberately
outside the built-in-speaker sound-quality gate.

## Audit changes and publication

- completed and pushed the coherent UbiG pre-audit work as `a92e5ef`, then follow-up M6 machine-gate closure as `1db43db`;
- refreshed the workspace README and current-Golden pointer;
- preserved the missing SP7 audit and v32 acoustic evidence locally;
- added this audit and a v32 warning banner to the canonical render ledger;
- later closed F04 and F07 without rebooting or replacing runtime audio
  components; F07 retained the selected v32/v31/v28/CPS-v3 rollback set and
  removed only manifest-reviewed diagnostic/build payloads.
