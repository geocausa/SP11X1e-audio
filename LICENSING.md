# Licensing and provenance

This is a **mixed-license repository**.  The top-level `LICENSE` is the default
for material that is original to this project, but it does **not** relicense
Linux kernel source, third-party source, firmware, binary artifacts, or other
material that already carries its own terms.

## Project-owned material

Unless a file or directory states otherwise, original material authored
specifically for the SP11 X1E audio project is available under the
**BSD-2-Clause** license in `LICENSE`.

That default is intended to cover independently authored project documentation,
analysis/test tooling, scripts, and userspace code such as the source-owned
UbiG implementation, where no other license notice applies.

The BSD-2-Clause attribution requirement is satisfied by preserving the
copyright/license notice when source or binaries are redistributed.

## geoca contribution grant

To remove ambiguity for downstream and upstream maintainers: to the extent a
change in this repository is an original copyrightable contribution by
**geoca**, it may be reused, modified, redistributed, and submitted upstream
under the license expression that already governs the file or patch in which
that contribution appears.

For independently authored standalone project material that is not derived
from third-party code, the BSD-2-Clause default above also applies unless an
explicit file-level notice says otherwise.

This grant applies only to geoca's contributions.  It does not change any
third-party copyright or license.

## Linux kernel source and patches

Kernel-derived files, source overlays, snapshots, and patches retain the
license of the Linux kernel file from which they came.  Their file-level
`SPDX-License-Identifier` is authoritative.

Examples already present in this repository include GPL-only files and files
that are dual licensed with BSD terms.  Do not replace those identifiers merely
to make the repository uniform.

A patch that modifies an existing kernel file is intended to be contributed
under the same license expression as that target file unless the patch states
otherwise and the alternative is legally compatible.

For future kernel work:

- preserve the target file's existing SPDX expression when modifying it;
- put the SPDX identifier at the first possible line, following Linux kernel
  placement rules;
- for a **new, fully original kernel file**, prefer
  `GPL-2.0 OR BSD-2-Clause` when that is acceptable to the target subsystem and
  no incompatible source material was used;
- otherwise use the license expected by the target subsystem, commonly
  `GPL-2.0`;
- use `Linux-syscall-note` only for genuine Linux UAPI files where that
  exception is appropriate.

Linux kernel licensing rules:
https://docs.kernel.org/process/license-rules.html

## Third-party and vendor material

Third-party source keeps its original notices and terms.  Proprietary firmware,
ACDB/tuning data, Windows binaries, and other vendor-owned payloads are **not**
covered by the BSD-2-Clause grant merely because hashes, observations,
reproduction instructions, or references to them appear here.

The project policy is to keep proprietary vendor binaries and owner-only tuning
payloads out of Git.  If such a file is ever required for testing, document how
the user obtains it lawfully rather than committing it.

## Provenance rules

When importing or adapting code, record enough provenance to let a future
maintainer answer:

1. Where did this material come from?
2. Which revision or commit was used?
3. What license governed that source?
4. Which parts are project-authored changes?

Preserve upstream copyright notices, SPDX identifiers, authorship, and commit
references.  Do not collapse a mixed-license source tree into a single blanket
license.

## Upstream sign-off is separate from licensing

A repository license or this contribution grant is not a Linux kernel
`Signed-off-by:` trailer.  Kernel submissions must independently satisfy the
Developer's Certificate of Origin and each signer must provide their own
sign-off.

See `CONTRIBUTING.md` and `UPSTREAMING.md`.
