# SP11 original Windows Dolby host

This directory contains only Linux-side deployment files. The proprietary
Dolby DLLs are intentionally not stored in Git.

The LADSPA bridge executes the original SP11 Windows render order:
`DolbyAPOvlldp150 -> DolbyApoVr`.

Profiles are chosen before plugin activation with `SP11_DOLBY_PROFILE`:
`dynamic`, `movie`, `music`, `game`, `voice`, `onlinecourse`, or `personalize`.
The helper persists the selected profile in `~/.config/sp11-dolby/profile`,
writes a systemd user-service environment drop-in, then restarts only the
dedicated `filter-chain.service`. The real-time audio callback never performs a
profile rebuild.

`sp11-dolby off` selects the separate transparent bypass sink; it does not
uninstall or mutate the Dolby processor.

## Rebuild

`build-production.sh` builds the host with its default DLL paths pointing at
the private `~/.local/lib/sp11-dolby/` bundle. Before compiling, it verifies the
exact SP11 VLLDP150 and VR SHA-256 hashes. The script intentionally fails rather
than silently running against a different Dolby binary revision.

### Personalize GEQ

The original VR graphic equalizer can be driven through the helper. A curve is
20 integer bands in DAX's public `-192..192` range:

```text
sp11-dolby geq set <20 integer values>
sp11-dolby geq
sp11-dolby geq reset
```

The curve is persistent but only active with the `personalize` profile. Other
profiles explicitly disable GEQ.
