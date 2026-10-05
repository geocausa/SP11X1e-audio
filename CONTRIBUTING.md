# Contributing

Contributions are welcome.  The goal is to keep experimental hardware work
reproducible while making accepted Linux changes straightforward to submit
upstream.

## Rights and provenance

Only contribute material you have the right to submit.  Preserve all existing
copyright, attribution, and `SPDX-License-Identifier` lines.

For copied or adapted material, record the upstream project, exact commit or
revision, source path, and license.  Do not commit proprietary Windows/Qualcomm
binaries, firmware, ACDB/tuning payloads, credentials, or material whose
redistribution rights are unclear.

See `LICENSING.md` before changing any license header.

## Developer Certificate of Origin

Changes intended for Linux kernel upstreaming must comply with the Developer's
Certificate of Origin (DCO).  Sign commits you are entitled to submit with:

```bash
git commit -s
```

The resulting `Signed-off-by:` line is a personal certification by that signer.
Never invent, copy, or add another person's sign-off without their explicit
authorization.

Likewise, use `Co-developed-by:`, `Reviewed-by:`, `Tested-by:`, `Acked-by:` and
other kernel trailers only when the named person has actually provided that
trailer or clearly authorized it under kernel process rules.

Kernel submission guidance:
https://docs.kernel.org/process/submitting-patches.html

## Kernel-facing changes

For changes destined for Linux:

- base the final patch on an appropriate current upstream tree, not on a large
  historical source snapshot;
- preserve the target file's existing SPDX license expression;
- keep each patch focused on one logical change;
- separate diagnostic instrumentation from the final functional patch;
- explain hardware evidence and why the change is needed;
- avoid board-specific guesses when a value can be demonstrated from hardware
  or an existing binding;
- add or update bindings/documentation when required by the subsystem;
- run the relevant build/tests and `scripts/checkpatch.pl` before submission.

## Evidence

Hardware claims should be tied to reproducible observations: logs, hashes,
captures, register/state observations, or repeatable tests.  Reverse-engineered
behavior should be implemented independently; do not paste decompiled or
proprietary vendor source into kernel patches.

## Commit messages

For upstream-intended commits, write a kernel-style subject and body that
explain the problem and the reason for the change, not just what lines changed.
Preserve original authorship and provenance when carrying another person's
patch.
