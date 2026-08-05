# SP11 Dolby production-source manifest — 2026-08-05

This file answers one question: **what code is actually intended to build and
run the current Linux Dolby host?** It separates production from the large body
of RE probes and historical implementations kept in `dolby-port/`.

## Production entry point

```text
dolby-port/sp11_dolby_windows_chain_ladspa.c
```

Direct source dependencies currently include:

```text
dolby-port/sp11_vlldp_pe_loader.h
dolby-port/sp11_vr_outer_probe.c
dolby-port/sp11_vr_init_probe.c       (included indirectly)
```

The `*_probe.c` names are historical. Parts of those files now provide the
validated Windows-runtime/PE shims used by production. Refactoring that shared
runtime into neutrally named files is desirable technical debt, but must not be
done during an unrelated parity experiment because the current build is proven
and reproducible.

## Reproducible build

Canonical command:

```text
deploy/dolby/build-production.sh <output.so>
```

The script refuses DLL revisions other than:

```text
DolbyAPOvlldp150.dll
  a2553ff7b013b5a248e50bdcae46d08405e393c0085073975214d035cedf02c1

DolbyAPOVR.dll
  1d74477ea0dae66961a21bf6bc3ce0d8062836fc4dd96b59c14de11257f5eecc
```

The proprietary DLL bundle is private/local and is not intended to be added to
Git.

Current production-source SHA-256 values:

```text
sp11_dolby_windows_chain_ladspa.c
  c72575789c114f0fd72d9f581a67b969fe371cd7d55c889d5d9de30ba8c11fc4

build-production.sh
  4d1f07f1c29a0b6ebb9bb51ac551bd0f337c77236fb7d1f9d97e10f318129be1

sp11-dolby helper
  5aae8cfdc75523822f29a62a274f98d23c4277a0c31414a1887504a1adb3a973
```

A fresh rebuild during this consolidation produced:

```text
230932e53734c0fc0749eb54c8b8db462c739d7a7bf32cd937be4cb635d9be2b
```

and was **byte-identical** to the installed
`~/.local/lib/sp11-dolby/sp11_dolby_windows_chain.so`.

## Deployment files

```text
deploy/dolby/98-sp11-windows-dolby.conf
deploy/dolby/sp11-dolby-pe.conf
deploy/dolby/sp11-dolby
deploy/dolby/build-production.sh
deploy/dolby/README.md
```

Current observed live state at manifest creation:

```text
profile       dynamic
custom GEQ    off
Dolby sink    0.10
service       active/running
NRestarts     0
```

This state is an operator/deployment fact, not a claim that Dynamic matches the
July Firefox/YouTube oracle. The July retained VLLDP state is Movie/Music
family.

## What production executes

The host executes original Windows ARM64 DSP code for:

```text
DolbyAPOvlldp150
  -> original outer scheduler
  -> original inner accumulator
  -> original VLLDP core

DolbyApoVr
  -> original wrapper/core processing
```

Linux code supplies PE loading, Windows runtime/allocator/locking shims,
property/state setup, host-buffer adaptation and the proven inter-stage bridge.
It does **not** intentionally add the historical hand-written fake-bass DSP,
modern ASAR/AIDE, Surface EQ or the newly investigated Windows AudioEng limiter.

## Production-adjacent tests/oracles

These are high-value but are not installed as the product path:

```text
sp11_dolby_windows_chain_plugin_test.c
sp11_dolby_windows_chain_known_input.c
sp11_vlldp_state_oracle.c
sp11_vlldp_scheduler_replay.c
sp11_vlldp_orchestrator_replay.c
sp11_vr_init_probe.c
sp11_vr_outer_probe.c
sp11_vr_chunk_test.c
sp11_vr_outer_chunk_test.c
sp11_asar_parent_probe.c
```

## Historical / non-production implementations

The following families remain tracked for archaeology, regression comparisons
and decoded-algorithm documentation. They must not be mistaken for the current
installed chain:

```text
sp11_dolby_chain.*
sp11_dolby_modular.*
sp11_dolby_leveler.*
sp11_dolby_regulator.*
sp11_dolby_limiter.*
sp11_dolby_stage1.*
sp11_dolby_stage2.*
sp11_dolby_vlldp.*
bass_standalone.c
bass_coefficients.h
sp11_vlldp_v19.c
sp11_vlldp_exact.c
```

Some tracked `.so` files are historical artifacts. Their mere presence does not
make them production. The deployed host hash and build script above are the
source of truth.

## Deployment gate

Do not replace the live production plugin merely because a new candidate sounds
better. A code change is eligible for deployment only after:

1. evidence identifies the Windows behavior being reproduced;
2. the candidate passes offline finite-output/chunk/bypass regression tests;
3. relevant known-input/oracle behavior is no worse (or the expected change is
   explicitly explained by stronger evidence);
4. a rollback copy is made;
5. the source/evidence checkpoint is committed and pushed.
