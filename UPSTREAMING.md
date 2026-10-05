# Linux upstreaming workflow

This repository contains research history, reproducible deployments, kernel
source overlays, and patch experiments.  Those are useful for development but
are not themselves the preferred shape of an upstream Linux submission.

## 1. Start from current upstream

Before preparing a submission, identify the appropriate current maintainer tree
or mainline base and confirm that the problem still exists there.  Re-check
bindings and subsystem APIs because they can change while an experiment is in
progress.

## 2. Reduce the accepted result to a minimal delta

Recreate the final functional change against the clean upstream base.  Do not
submit whole source snapshots merely because they were convenient during
experimentation.

Keep diagnostic probes, temporary module parameters, laboratory-only controls,
raw dumps, and rejected experiments out of the final patch unless maintainers
specifically need them.

## 3. Preserve licensing and provenance

The existing SPDX expression on each target kernel file controls.  Preserve
copyright notices and authorship.  See `LICENSING.md`.

If a patch carries work from another contributor, retain their authorship and
relevant provenance.  Do not manufacture a `Signed-off-by:` or
`Co-developed-by:` trailer for them.

## 4. Build a reviewable series

Prefer a small series in dependency order, for example:

1. binding or documentation change;
2. generic subsystem support, if needed;
3. board/machine support;
4. optional cleanup that is independently reviewable.

Keep unrelated audio behavior changes in separate patches.

## 5. Validate

At minimum, as applicable:

```bash
./scripts/checkpatch.pl --strict <patches>
./scripts/get_maintainer.pl <patches>
```

Build the affected architecture/configuration and the modified subsystem.  Run
hardware tests on the SP11 and record enough detail to reproduce the result.
For device-tree changes, run the relevant DT schema checks.

## 6. DCO sign-off

Every contributor in the submission path must provide their own DCO sign-off.
Use `git commit -s` when creating commits you are entitled to certify.

Kernel DCO/submission rules:
https://docs.kernel.org/process/submitting-patches.html

## 7. Generate and send patches

Use normal kernel tooling (`git format-patch`, `git send-email`) and the output
of `scripts/get_maintainer.pl` to address the correct maintainers and lists.
Include testing and hardware identification in the cover letter or commit
message where useful.

## Audio-specific rule

The `patches/`, `deploy/kernel-patches/`, and `repro/*/source-overlay/` areas are
valuable provenance and reproduction material.  Treat them as inputs to an
upstream patch, not as proof that every historical delta belongs upstream.
Rebase the accepted behavior onto current ALSA/ASoC/SoundWire/Qualcomm code and
submit the smallest source change that reproduces the validated hardware
result.
