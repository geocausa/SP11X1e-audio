# SP11 audio reverse-engineering workflow — 2026-08-05

This is the repository operating policy for the current research phase.

## Branch roles

```text
main
  accepted historical protected-audio baseline; do not rewrite casually

agent/audio-v2-clean-rebuild
  older/current hardware bring-up worktree with unrelated dirty work; never
  reset/clean it from Dolby experiments

dolby-native-progress-2026-08-04
  historical checkpoint before the Aug-5 completion work

agent/dolby-completion-2026-08-05
  current canonical Dolby RE/integration branch
```

Normal Dolby research continues linearly on
`agent/dolby-completion-2026-08-05`. Do **not** create a new Git branch for each
hypothesis. Create a separate branch/worktree only when an experiment is risky,
long-lived, or must proceed independently.

## Experiment lifecycle

```text
question
 -> identify competing hypotheses
 -> choose discriminator / oracle
 -> run isolated experiment
 -> classify result: confirmed / eliminated / unresolved
 -> preserve evidence
 -> update canonical model only if warranted
 -> regression test
 -> commit + push
```

Investigating several hypotheses is expected. Leaving all of them half-open is
not. Every exploratory branch in the reasoning tree should either produce a
new evidence-backed target or be explicitly closed/downgraded.

## Commit policy

Commit when one of these happens:

- a high-value runtime fact is recovered;
- an ABI/state/algorithm is reproducibly decoded;
- production behavior changes;
- an important hypothesis is eliminated by a reusable experiment;
- the canonical topology/confidence model changes.

Do not wait for the whole project to finish before preserving a gold-mine
finding. Conversely, do not create a commit for every throwaway grep or failed
probe.

## Documentation roles

- `docs/audit/2026-08-05-CANONICAL-DOLBY-PIPELINE.md` — current system model.
- `docs/audit/2026-08-05-DOLBY-EVIDENCE-LEDGER.md` — high-value proof ledger.
- `docs/audit/2026-08-05-DOLBY-FINDINGS-INDEX.md` — navigation/supersession.
- `docs/findings/` — detailed experiments and corrections.
- `docs/deployment/2026-08-05-DOLBY-PRODUCTION-MANIFEST.md` — executable source
  of truth.

Old findings are preserved; misleading executive conclusions receive a
superseded banner rather than silent rewriting/deletion.

## Safety boundaries

- Never reset/clean the unrelated dirty `/01-audio` checkout.
- Use the isolated Dolby worktree for experimental source changes.
- Do not mutate the Windows NTFS partition during forensics; mount read-only.
- Do not deploy a speculative DSP improvement to make subjective sound better.
- Keep proprietary vendor binaries out of new Git commits; record hashes/paths.
- Keep local generated binaries out of Git unless there is a specific evidence
  reason to version one.

## Merge policy

The Dolby completion branch is a research/integration branch, not automatically
`main`. Merge/promote only after the canonical parity gates are satisfied or a
consciously scoped milestone is accepted. Preserve the branch even after merge
so its RE history remains navigable.
