# FullIO v19c production endpoint cleanup — 2026-08-26

## Result

The normal SP11 desktop speaker graph now exposes **SP11 UbiG** as the sole
user-facing built-in speaker endpoint.

- `effect_input.sp11_ubig` remains the persisted default output.
- `alsa_output.platform-sound.HiFi__Speaker__sink` remains instantiated as the
  real UbiG backend but is `node.hidden=true` with `priority.session=0`.
- `effect_input.sp11_ubig_bypass` is absent from the live production graph.
- `~/.config/pipewire/pipewire.conf.d/98-sp11-ubig-bypass.conf` is absent.
- The historical bypass config remains tracked only as an explicit diagnostic
  artifact and is marked non-production.

## Persistence / regression protection

`deploy/wireplumber/98-sp11-production-endpoint-policy.conf` carries the hidden
backend policy. `deploy/ubig/install-production.sh` installs that policy,
removes any active bypass config, restarts PipeWire/WirePlumber so both changes
are effective immediately, then rejects a visible raw backend or active bypass.

`deploy/native-audio-v19c/verify-native-audio-v19c.sh --live` now checks the same
production endpoint invariants.

## Runtime acceptance

After a real production reinstall/restart:

- production installer: PASS;
- live FullIO v19c verifier: PASS;
- UbiG -> hidden ALSA speaker links: present;
- playback through `effect_input.sp11_ubig`: `pcm0p` reached RUNNING;
- bypass node count: zero;
- all PipeWire/WirePlumber/UbiG services remained active;
- no new audio runtime fault was observed except the already accepted one-shot
  protected-graph `APM_CMD_SET_CFG 0x01001006 / AR_EUNSUPPORTED` marker.

Pre-push regression state after the policy change: **230 passed, 3 skipped, 6
subtests passed**, full UbiG DSP suite PASS, and FullIO topology reproduction
PASS.

System suspend/resume was not tested and remains outside this audio RE.
