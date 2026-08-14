# SP11 original Windows Dolby host

This directory contains only Linux-side deployment files. The proprietary
Dolby DLLs are intentionally not stored in Git.

The LADSPA bridge executes the sample dependency proved from the Aug-8 full-memory graph:
`DolbyApoVr -> DolbyAPOvlldp150`. Older hardware breakpoints observed the VLLDP callback before the VR callback, but that was scheduler invocation order; exact buffer provenance and captured-state replays prove the PCM dependency in the opposite direction.

Profiles are `dynamic`, `movie`, `music`, `game`, `voice`, `onlinecourse`, or
`personalize`. For a fresh SP11 built-in-speaker deployment with no saved user
selection, the parity default is **`movie`**. A fresh Windows oracle captured on
2026-08-12 had no `SelectedMainProfile` override; the shipped operator policy
selects Movie for the internal speaker with spatial audio enabled, and the
29.45-second Linux Movie render matched that Windows loopback at
`0.999999473836` correlation / `59.778 dB` residual SNR. Explicit user profile
selections continue to override this fallback.

`SP11_DOLBY_PROFILE` remains the cold-start/default source, while
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
deploy/dolby/sp11_dolby_monitor_link.py
deploy/dolby/sp11-dolby-monitor-link.service
```

The monitor-link keeper is intentionally exact-name based; it creates only the
left/right unity monitor -> hidden Dolby engine links. The transaction sync also
tracks the visible sink object ID. If filter-chain recreates the node, the new
object inherits the previous user scalar before any final VOL_CTRL/GainStep
transaction, so PipeWire's transient default-unity Props cannot become a 100%
endpoint command.

`sp11-dolby postgain` shows the raw request/applied values.
`sp11-dolby sync-volume` performs one manual volume->postgain synchronization.
The long-running user service is event-driven by PipeWire updates rather than a
tight polling loop. The runtime control page is intentionally preserved across
filter-chain restarts (and naturally disappears at user logout), so a volume
request can be queued before a new Dolby instance is created. The service is
ordered before `filter-chain.service` at login and remains subscribed while the
Dolby node is recreated.

### Windows volume-dependent Qualcomm MSIIR calibration

Windows qcadcm does not leave speaker MSIIR `0x489e` at the full-volume row.
It converts endpoint gain to Q28, selects the nearest gain-table entry, maps
that index to CKV GainStep 1..30, and reapplies the corresponding ACDB
calibration. `sp11-msiir-volume-sync.service` reproduces that policy using the
same endpoint-dB/postgain state already supplied to VLLDP.

Deployment files:

```text
deploy/dolby/sp11_msiir_volume_sync.py
deploy/dolby/sp11-msiir-volume-sync.service
tools/tlv_write.c / tools/bin/tlv_write
```

Install the helper as `$HOME/.local/lib/sp11-dolby/tlv_write`, the Python
service as `$HOME/.local/bin/sp11-msiir-volume-sync`, and enable the user unit.
The embedded 30-row coefficient table is generated from reviewed SP11 REV_0D
ACDB (`a0a8635b...cde`) and must not be hand-tuned. The service re-applies the
selected row every time the protected PCM enters RUNNING because kernel graph
construction still initially loads CKV 30.

### Windows endpoint taper parity

The user-facing Dolby sink keeps the ordinary PipeWire/WirePlumber scalar, but
Windows and PipeWire do not map the same scalar to endpoint dB. Fresh live SP11
Windows `IAudioEndpointVolume` capture on 2026-08-12 measured the built-in
speaker endpoint over scalar `0.000..1.000` in `0.005` increments. The endpoint
reported `-75..0 dB`, 0.5-dB hardware granularity and 51 software step
positions.

`sp11-dolby-volume-sync` recovers the visible PipeWire scalar from its cubic
`channelVolumes` and maps it through the pinned Windows taper. The visible sink
is now a **control sink only**: its unity monitor ports feed a separate hidden
Dolby engine, so its cubic `channelVolumes` never attenuate VLLDP/VR input PCM.
`sp11-dolby-monitor-link` maintains those exact two monitor-to-engine links.

When the protected graph is idle, the hidden downstream ALSA sink carries the
Windows endpoint attenuation as the fail-quiet actuator. When the v4 protected
graph is running, the combined volume transaction programs final AudioReach
`VOL_CTRL` plus GainStep and only then moves that hidden sink to unity. VLLDP
postgain and MSIIR selection continue to consume the same Windows-equivalent dB
state. This preserves one visible desktop slider while matching Windows ordering:
full-scale host PCM -> Dolby/AudioEngine -> endpoint attenuation.

At the 25% reference point the live result is:

```text
virtual scalar        0.25
Windows endpoint dB  -20.7474098
downstream gain       0.091755
VLLDP postgain        -332
MSIIR CKV             2
```

Do not replace the pinned taper with a fitted exponent; the measured curve is
strongly non-power-law at low scalar values.
