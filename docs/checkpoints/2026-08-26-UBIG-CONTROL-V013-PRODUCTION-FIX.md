# 2026-08-26 UbiG control v0.1.3 production fix

## Operator report

Custom GEQ was audibly correct, but built-in profile switching appeared ineffective.

## Root causes and corrections

1. The GTK drop-down only changed selection; a separate `Apply profile` button submitted
   the request. v0.1.3 applies a non-Custom selection immediately and keeps Custom tied
   to the 20-band GEQ payload.
2. The desktop had been left on `effect_input.sp11_ubig_bypass` after diagnostic work,
   with `filter-chain.service` masked. The production installer now un-masks/enables the
   real UbiG host and refuses success if bypass remains the persisted default.
3. The controller could not distinguish an mmap page from a live consumer. The existing
   ABI-v2 `engine_flags` word now carries `UBIG_CONTROL_ENGINE_LIVE`; the GUI reports
   offline/bypass instead of falsely claiming an active preset.
4. Saved profile/GEQ state had no login restore hook. v0.1.3 installs a hidden GNOME
   autostart that runs `ubig-geq --restore`; with no saved state it is deliberately a
   no-op so the engine startup profile remains authoritative.
5. Native Audio v18 exposes exact DSP mute/volume transaction controls whose live graph
   target returns `ENODEV`. Repeated retries caused log/CPU churn and could leave the
   hidden sink fail-closed muted. ENODEV is now classified as actuator absence and uses
   the proven host/hardware fallback; all other errors remain fail-closed. The fallback
   is sticky until visible UbiG node recreation.

## Profile equivalence truth

The strengthened all-profile render does **not** require seven unique PCM hashes.
The final Windows two-channel policy itself makes Music and Game bit-identical after
stereo-virtualizer bypass. The source-owned engine reproduces that exact alias. All
other advertised profile classes remain distinct, and Custom retains its separately
proven non-flat 20-band path.

Profile hashes:

- Dynamic `5976ae0a7613bb0f`
- Movie `ed465af9641280e6`
- Music `0bdbb0c2104b4d84`
- Game `0bdbb0c2104b4d84`
- Voice `de6e80796a93213f`
- Course `ad63c62696124e84`
- Custom `4b342c40675cf948`

## Live acceptance

After reboot on saved-default Native Audio v18:

- `ubig-control 0.1.3 arm64` installed;
- real `filter-chain.service`, UbiG volume sync and monitor link enabled + active;
- default sink `effect_input.sp11_ubig`;
- bypass retained only as diagnostic fallback;
- saved Custom curve restored automatically;
- request/ack `3/3`, `last_error=0`;
- exact DSP optional controls classified once as ENODEV then host fallback stays quiet;
- 997-Hz speaker-to-internal-mic smoke: **48.71 / 49.63 dB prominence**.

The Debian package is byte-reproducible across two builds:

`b19997a28adbe7ad35927c3033b9d8cec297aba4bd66268179bf2d585b297224  ubig-control_0.1.3_arm64.deb`

Machine-readable evidence: `artifacts/reviewed/2026-08-26-ubig-control-v013-live-gate.json`.
