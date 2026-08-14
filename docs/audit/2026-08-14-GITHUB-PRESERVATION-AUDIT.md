# SP11 audio Git/GitHub preservation audit

Date: 2026-08-14

## Executive result

The reproducible current CPS/render-parity line is on GitHub, including the
kernel patch series, deployment recipes, reviewed evidence, tests and live
validation records.  Not every local byte under `SP11-PROJECT` is on GitHub:
large build trees, raw captures and old diagnostic candidates remain local.
They must not be confused with the curated reproducibility record.

Repository: `https://github.com/geocausa/SP11X1e-audio`

## Published branches checked

### Current render/parity line

Branch `agent/render-parity-20260812` is published through `d928001` and is the
canonical current line.  Its ancestry includes the published CPS integration,
Windows render/notification study, endpoint taper, volume-dependent MSIIR,
final VOL_CTRL/GainStep transaction, SOFT_PAUSE, Dolby pause-drain, render-v2,
VISENSE DP5 `0x03`, and event-driven volume transaction work.

The current worktree was clean and local/remote commit IDs matched at audit.
Draft PR: `https://github.com/geocausa/SP11X1e-audio/pull/4`.

### Historical Dolby completion line

The worktree `04-dolby-re-work` contained 63 committed Dolby reverse-
engineering commits not reachable from any then-current remote branch (217
unique blobs, 2,198,024 bytes).  They cover the native DAP path, Windows
profiles, in-place state persistence, VR/VLLDP, ASAR, endpoint feedback,
limiter analysis and known-input comparisons.

The deleted remote branch was restored without changing its history:

```text
branch  agent/dolby-completion-2026-08-05
tip     e2c4e4b3433df9ea530aca6c242ee4df4eeb1bf3
remote  origin/agent/dolby-completion-2026-08-05
```

### Other checked worktrees

- `01-audio` branch `agent/audio-v2-clean-rebuild` reports seven commits ahead
  of its same-named tracking branch, but those commit objects are already
  reachable on GitHub through `origin/agent/asar-linux-breakthrough-20260809`.
  The work is preserved; its local tracking label is merely stale.
- `05-audio-integration` tip `d4e2e42` is published as
  `origin/agent/audio-integration-protection-20260810` and is an ancestor of
  the current render/parity branch.
- `SP11-AUDIO-AUDIT/worktree-7c6ad09` is clean at published commit `7c6ad09`,
  also an ancestor of the current line.

## Kernel-source kitchens

The reconstructed kernel source worktrees under `02-kernel/` have no GitHub
remote of their own.  They include local kernel commits and, in some kitchens,
tracked working-tree changes used to build later candidates.  Their canonical
portable representation is the patch/deployment series already published in
this audio repository, notably:

- `patches/0045-q6apm-sp11-windows-soft-pause-lifecycle.patch`;
- `patches/0046-ASoC-wsa884x-avoid-full-cache-dirty-on-clock-stop.patch`;
- `patches/0047-q6apm-add-SP11-final-endpoint-volume-Q28-control.patch`;
- `patches/0048-ASoC-q6apm-add-SP11-Windows-volume-transaction.patch`;
- `patches/0049-ASoC-q6apm-account-for-volume-transaction-TLV-header.patch`;
- `patches/0050-ASoC-wsa884x-denali-use-native-VISENSE-channel-mask.patch`;
- `patches/0051-ASoC-q6apm-quiesce-pull-watermarks-before-soft-pause.patch`;
- `deploy/render-parity*`, `deploy/softpause`, and `deploy/visense-parity`.

This means the current kernel changes can be reconstructed from GitHub even
though the multi-gigabyte kernel worktrees/build products are not uploaded.

## Local-only material still requiring care

The older `01-audio` worktree has three modified tracked source/test files and
many untracked trees.  Most space is in raw/candidate material, including:

- about 1.2 GiB `artifacts/live-provenance-20260729`;
- about 236 MiB `audio-mapdiag-candidate-20260802`;
- about 233 MiB `audio-clean2-candidate-20260802`;
- about 167 MiB `protectdiag-candidate-20260802`;
- about 59 MiB `audio-clean-candidate-20260801`;
- about 53 MiB `offline-audit-20260729`;
- smaller KD/ETW captures, diagnostic logs, tools and experimental patches.

`05-audio-integration` also retains about 2.2 MiB of untracked CPS runtime
artifacts.  These were not blindly committed: build outputs and raw captures
can exceed GitHub limits, contain duplicates, and mix accepted results with
failed experiments.  Reviewed conclusions and reproducible deltas have been
copied into the current published branch, but the local-only sets must not be
deleted until separately classified as evidence, reproducible output, or
obsolete duplicate.

## Preservation rules

1. Treat `agent/render-parity-20260812` plus PR #4 as the current source of
   truth.
2. Treat `agent/dolby-completion-2026-08-05` as retained historical Dolby
   archaeology, not the current deployment branch.
3. Preserve kernel changes as numbered patches and deployment provenance in
   the audio repository; do not rely only on a cooked kernel directory.
4. Never use `git clean`, bulk deletion, or bulk `git add` in the old dirty
   worktrees.
5. Curate local-only raw artifacts separately; do not claim “everything is on
   GitHub” until that inventory is complete.
