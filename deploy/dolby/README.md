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
