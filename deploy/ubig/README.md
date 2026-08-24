# UbiG SP11 production deployment

This is the canonical Linux userspace deployment namespace for the SP11 UbiG
speaker engine. User-visible PipeWire nodes, helper commands, systemd units and
runtime state use **UbiG** naming. Historical Windows-oracle material elsewhere
in the repository may retain the vendor name because it identifies the source
being reverse-engineered; it is not Linux product branding.

Canonical graph names:

- `effect_input.sp11_ubig` — visible/default Audio/Sink
- `effect_input.sp11_ubig_engine` — hidden engine input
- `effect_output.sp11_ubig` — engine output to physical speakers
- `effect_input.sp11_ubig_bypass` — transparent diagnostic bypass

Canonical services:

- `sp11-ubig-volume-sync.service`
- `sp11-ubig-monitor-link.service`
- `sp11-msiir-volume-sync.service`

`deploy/ubig-candidate/prepare.sh` stages the native engine and these helpers.
The old `deploy/dolby/` tree is retained only as historical Windows-vendor
bridge/research provenance and must not be used to name the active Linux sink.

## Production installation

`install-production.sh` builds and gates the native SP11 plugin, installs it at
`~/.local/lib/ubig/ubig-sp11.so`, installs the canonical UbiG helpers/services,
expands the UbiG PipeWire graph, retires candidate-only drop-ins, restarts only
the dedicated userspace graph and verifies the live mapped plugin is not a
deleted/stale inode. It never reboots or changes the kernel/GRUB state.

```sh
deploy/ubig/install-production.sh
```
