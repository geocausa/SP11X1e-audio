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

Canonical services:

- `sp11-ubig-volume-sync.service`
- `sp11-ubig-monitor-link.service`
- `sp11-msiir-volume-sync.service`

`deploy/ubig-candidate/prepare.sh` stages the native engine and these helpers.
The old `deploy/dolby/` tree is retained only as historical Windows-vendor
bridge/research provenance and must not be used to name the active Linux sink.

## Production installation

`install-production.sh` builds and gates the native SP11 plugin, explicitly unmasks and enables the dedicated `filter-chain.service`, installs it at
`~/.local/lib/ubig/ubig-sp11.so`, installs the canonical UbiG helpers/services,
installs the WirePlumber production endpoint policy, removes any autoloaded
transparent bypass config, restarts the user PipeWire/WirePlumber graph and
verifies the live mapped plugin is not a deleted/stale inode. It never reboots
or changes the kernel/GRUB state.

```sh
deploy/ubig/install-production.sh
```


The production gate includes a seven-profile matrix. It requires six distinct
stereo outputs and preserves the one evidence-backed Windows alias: Music and
Game are bit-identical under the SP11 two-channel stereo-virtualizer-bypass
policy. A successful install verifies that `effect_input.sp11_ubig` is the persisted
default sink, that the physical ALSA speaker backend is `node.hidden=true`, and
that no `effect_input.sp11_ubig_bypass` node is active. The historical bypass
config remains in `deploy/pipewire/98-sp11-ubig-bypass.conf` for explicit manual
RE/debug use only; production never installs or autoloads it.

## Desktop endpoint policy

Normal GNOME output selection exposes **SP11 UbiG** as the SP11 built-in speaker
endpoint. The physical `alsa_output.platform-sound.HiFi__Speaker__sink` remains
fully instantiated because UbiG targets it internally, but
`deploy/wireplumber/98-sp11-production-endpoint-policy.conf` marks it hidden and
sets its session priority to zero. The transparent bypass is absent from the
production graph.
