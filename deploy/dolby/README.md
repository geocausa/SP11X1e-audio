# SP11 original Windows Dolby host

This directory contains only Linux-side deployment files. The proprietary
Dolby DLLs are intentionally not stored in Git.

The LADSPA bridge executes the sample dependency proved from the Aug-8 full-memory graph:
`DolbyApoVr -> DolbyAPOvlldp150`. Older hardware breakpoints observed the VLLDP callback before the VR callback, but that was scheduler invocation order; exact buffer provenance and captured-state replays prove the PCM dependency in the opposite direction.

Profiles are `dynamic`, `movie`, `music`, `game`, `voice`, `onlinecourse`, or
`personalize`. `SP11_DOLBY_PROFILE` remains the cold-start/default source, while
the production LADSPA bridge also exposes a `Profile` control (`0` = keep the
startup profile, `1..7` = the seven profiles). PipeWire 1.6.2 exposes these
custom graph controls as initialization state rather than a writable live API,
so the helper's production runtime path uses a small mapped control page in
`$XDG_RUNTIME_DIR`. Bytes 0/1 remain requested/applied profile. Aligned int32
slots at offsets 4/8 are requested/applied VLLDP endpoint postgain. The
audio callback only reads/writes the mapped bytes; it performs no filesystem
I/O. The helper persists the selection in `~/.config/sp11-dolby/profile` and the
systemd drop-in for the next cold start, then requests an in-place retune. The
bridge applies only profile-dependent original Dolby setters; VLLDP/VR are not
reconstructed and adaptive history is preserved. A dedicated-service restart
is retained only as a compatibility fallback for an older plugin or missing
live control file.

`sp11-dolby off` selects the separate transparent bypass sink; it does not
uninstall or mutate the Dolby processor.

## Rebuild

`build-production.sh` builds the host with its default DLL paths pointing at
the private `~/.local/lib/sp11-dolby/` bundle. Before compiling, it verifies the
exact SP11 VLLDP150 and VR SHA-256 hashes. The script intentionally fails rather
than silently running against a different Dolby binary revision.

The shipped PipeWire fragment expects the rebuilt Linux LADSPA bridge at
`/usr/lib/ladspa/sp11_dolby_windows_chain.so`. Install/copy the verified build
there (or adjust the local deployment fragment deliberately); the vendor DLLs
remain in the private bundle and are compiled into the bridge's verified default
paths.

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


### Endpoint-volume feedback

Windows DAX continuously converts endpoint master attenuation to VLLDP postgain:

```text
postgain = round(endpoint_dB * 16)
```

with the recovered SP11 range `-1200..0` (`-75..0 dB`). The Linux volume-sync
helper subscribes to `pw-dump -m`, reads PipeWire's raw linear
`channelVolumes`, converts the actual attenuation to dB, and writes only the
postgain request slot in the mapped control page. It does **not** use the
`wpctl` displayed percentage as linear amplitude: WirePlumber presents the UI
volume cubically (for example the live `wpctl 0.16` state corresponded to a raw
linear gain of about `0.003908`).

The plugin consumes a postgain request atomically before the next audio block,
uses the original VLLDP setter plus original apply routine, and acknowledges it
without reconstructing either Dolby engine. The dedicated regression proves
zero-audio updates preserve VR long-memory state and all object identities.

Files:

```text
deploy/dolby/sp11_dolby_volume_sync.py
deploy/dolby/sp11-dolby-volume-sync.service
```

`sp11-dolby postgain` shows the raw request/applied values.
`sp11-dolby sync-volume` performs one manual volume->postgain synchronization.
The long-running user service is event-driven by PipeWire updates rather than a
tight polling loop. The runtime control page is intentionally preserved across
filter-chain restarts (and naturally disappears at user logout), so a volume
request can be queued before a new Dolby instance is created. The service is
ordered before `filter-chain.service` at login and remains subscribed while the
Dolby node is recreated.
