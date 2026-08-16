# Repository consolidation checkpoint — 2026-08-16

## Purpose

Audit Git/GitHub state before promoting the current SP11 built-in-speaker work
toward `main`. This checkpoint distinguishes branch ancestry from useful
research provenance and records evidence selectively rescued from side branches.

## Main/current relationship

After `git fetch --prune --all`:

- `origin/main` = `849350d` (`Integrate publishable Dolby evidence and state-pinned oracle`);
- current branch = `agent/render-parity-20260812`;
- pre-checkpoint current HEAD = `a90492a`;
- `origin/main` is a **strict ancestor** of the current branch;
- pre-checkpoint divergence was `0` commits unique to main and `129` commits
  unique to the current branch.

Therefore no current `main` work needs to be merged *into* render-parity. Main
promotion is a fast-forward/integration decision after this checkpoint, not a
conflict resolution exercise. The stale local `main` pointer was advanced to
match `origin/main`.

## GitHub state

The repository had three open stacked draft PRs:

- #2 `agent/audio-integration-protection-20260810 -> main`;
- #3 `agent/cps-parity-review-20260811 -> agent/cps-dp6-runtime-closure-20260810`;
- #4 `agent/render-parity-20260812 -> agent/cps-parity-review-20260811`.

PRs #2 and #3 were closed as superseded because their integrated branch content
is already contained in the current render-parity line. PR #4 is retained as
the single draft integration path and, after checkpoint commit `a343458`, was
retargeted directly to `main`.

There are no open GitHub issues.

GitHub Actions included usage is exhausted for the current billing period. The
repository file `.github/workflows/tests.yml` is unchanged, but the GitHub
workflow is temporarily `disabled_manually` so checkpoint pushes cannot create
runner jobs. Re-enable it when runner allowance is available again.

## Branch classification

### Fully contained in the current line

These remote tips are ancestors of `agent/render-parity-20260812` and need no
content merge:

- `agent/audio-integration-protection-20260810`;
- remote `agent/audio-v2-clean-rebuild` tip;
- `agent/cps-parity-review-20260811`;
- `agent/vr-vlldp-order-correction-20260809`.

Do **not** delete their local worktrees blindly. `/01-audio` is heavily dirty and
`/05-audio-integration` has untracked runtime artifacts. Branch retirement should
wait until those worktrees are separately harvested or explicitly abandoned.

The local-only `dolby-native-progress-2026-08-04` branch was safe to remove: its
tip was fully contained in the preserved Dolby-completion branch and its remote
was already gone.

### Preserve as research archives for now

These side lines are not simple ancestors of current HEAD:

- `agent/asar-linux-breakthrough-20260809` — genuine ASAR/HRTF/DAP Linux-host
  closure and native Windows spatial-object oracle work;
- `agent/aug8-asar-dual-lane-checkpoint` — historical pre-VLLDP/encoder
  checkpoint, mostly superseded but still provenance;
- `agent/cps-dp6-runtime-closure-20260810` — render-family/notification/CPS
  evidence not all originally present in current;
- `agent/dolby-completion-2026-08-05` — deep Dolby completion history; most
  production source/docs already exist in newer form on current;
- `agent/wsa-lifecycle-trace-20260815` — qcadcm/ACDB/WSA lifecycle closeout
  evidence, partly replayed later with different commit IDs.

Do not delete these remote branches until the selective rescues below have been
reviewed and `main` has been promoted.

## Selective evidence rescue performed

### WSA lifecycle / qcadcm

Rescued the curated early-boot qcadcm/ACDB closure evidence, unsafe-MMIO WHEA
record, compander provenance, inventory tool and unit test. This makes the
current branch self-contained for claims that qcadcm/REV_0D ACDB are not the
normal hidden WSA-macro register producer. The 39 KB reviewed boot log is also
preserved because the finding cites it directly.

### CPS / render families

Rescued Windows DEFAULT/NOTIFICATION/COMMUNICATIONS/SPEECH/MEDIA family
manifests, notification calibration evidence, control-link/topology evidence,
VR-before-VLLDP localization, stereo-matrix falsification, supporting tools and
unit tests. Intentionally **not** copied:

- the 178 KB Git source-lineage bundle (history remains available on the archive branch);
- the obsolete parallel Aug-12 deployed-render ledger;
- the chat-transfer handoff.

The canonical render ledger remains `docs/audit/2026-08-12-SP11-RENDER-PARITY-LEDGER.md`.

### ASAR / spatial

Rescued the final public ASAR closure findings, the directly referenced DAX
runtime harness source, graph analyzer, and native Windows genuine spatial-
object oracle source/build/capture documentation. Private vendor DLLs, private
seed/input blobs and compiled `.so` files were deliberately not copied.

The full exploratory `dolby-port/linux-harness` branch history remains archived
on `agent/asar-linux-breakthrough-20260809`.

## Stale front-door information corrected

Before this checkpoint:

- README still said the Aug-14 render-parity candidate had never booted;
- the Aug-8 Dolby integration status still presented an unknown stereo matrix as
  the next decisive experiment;
- H03 did not yet record v7 route-time-zero results.

This checkpoint updates those front doors. The later Aug-12 Windows captures
prove the pre-VLLDP drive occurs inside VR and reject the old hidden-matrix/
blanket-gain hypothesis. H03 remains AMBER, with v7 only partially gated.

## Local checkpoint validation

The GitHub Actions workflow was disabled before publication work, so no hosted
runner minutes were used for this checkpoint. The public suite was run locally
on SP11 with `python3 -m unittest discover -s tests -v`:

- `127` tests run;
- `127` passed;
- `3` expected private-Windows-capture tests skipped;
- `0` failures/errors.

The first import exposed one real side-branch integration defect in the rescued
ACDB driver-data inventory tool: it assumed `tools/` was already on
`PYTHONPATH`. The tool was corrected to use the repository's normal
`tools.acdb_setcfg_inventory` import with the existing direct-script fallback,
then the full suite passed.

A file-type/privacy scan of the checkpoint payload found no vendor DLL, compiled
ELF/PE `.so`, WAV, `.bin`, Git bundle, dump or ETL payload. `git diff --check`
passes. Historical findings still intentionally name some local/raw evidence
that publication policy excludes from Git; those are not new runtime
dependencies.

## Remaining cleanup before main promotion

1. Finish v7 5% and byte-identical 12% acoustic gates or record its rejection.
2. Keep draft PR #4 as the single `agent/render-parity-20260812 -> main`
   integration path while H03 remains AMBER.
3. After final technical review, fast-forward/merge main through that single
   integration PR.
4. Only after main contains the checkpoint: retire fully-contained remote
   branches and optionally replace valuable research branches with explicit
   archive tags/branches.
5. Separately harvest the dirty `/01-audio` and untracked `/05-audio-integration`
   worktrees before deleting their branches or directories.

## Post-checkpoint publication state

Checkpoint commit `a343458` was pushed to
`origin/agent/render-parity-20260812` while GitHub workflow `tests` remained
`disabled_manually`. No new Actions run was created by the push. PR #4 was then
retargeted to `main` through the GitHub REST API (the installed `gh pr edit`
client hit a deprecated Projects Classic GraphQL field). The retarget also
created no Actions run. Main is intentionally **not merged yet** because v7's
5% and 12% acoustic gates remain open.
