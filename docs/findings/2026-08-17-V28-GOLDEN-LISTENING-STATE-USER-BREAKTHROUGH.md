# v28 golden listening state — user breakthrough checkpoint — 2026-08-17

## Status

Preserve this state before any further mute or bass-parity work.

The user manually auditioned normal YouTube playback on the live v28 stack and reported the best Linux speaker behavior of the project so far:

- music is stable and coherent without the prior quality loss / oddness;
- the recovered endpoint leveler behavior is subjectively excellent;
- the previously reported forward/reverse seek spike was not apparent during manual YouTube seeking;
- mute UI state currently does **not** silence playback;
- low-bass / psychoacoustic-bass parity still needs a direct Windows comparison before closure.

This is a subjective listening checkpoint, not a replacement for the retained objective acoustic gates.

## Exact live stack

Kernel:

`7.1.5-sp11-render-parity-v4+`

Boot entry marker:

`sp11_entry=7.1.5-sp11-rpv4-macro84-winproducer-nohd2-wsa-windows-3state-retain-dp2offset2-v28-idlegated`

Relevant live feature markers include:

- `sp11_wsa_windows_3state_retain_v27=1`
- `sp11_wsa_dp2_offsetctrl2_v28=1`
- `sp11_wsa_winproducer_nohd2_v3=1`
- `sp11_wsa_csren0_v4=1`
- `sp11_headroom_link=1`
- `sp11_softpause=1`
- `sp11_volume_transaction=1`

Loaded module identities:

- `snd_soc_wsa884x` srcversion `EB74C0F5E4405EEE429136C`
- `snd_soc_lpass_wsa_macro` srcversion `4AF6F542C17BA6DD46586DA`
- `soundwire_qcom` srcversion `406975A3ED60935B31491BF`

Deployed Dolby host:

- `~/.local/lib/sp11-dolby/sp11_dolby_windows_chain.so`
- SHA-256 `ee02ff299146b0ed8387fda1da820a8ed7c9612fc4a5946ed921e5c0dca715d9`
- this is the already accepted production host retained by the pause-drain and idle-PA findings.

Live profile:

- Movie
- custom GEQ off
- VLLDP endpoint postgain `-785` = `-49.062 dB`
- Dolby generation `90`

The visible endpoint is `effect_input.sp11_windows_dolby`. During the checkpoint it was being moved manually by the user; one snapshot observed `15%` while physical PCM was RUNNING. The recovered volume-sync daemon continued to emit Windows endpoint/final-Q28/GainStep transactions across those slider changes.

## Important state interaction

Generation 90 was created at UI scalar approximately `0.032711`, so the VLLDP generation retained the queued/frozen postgain `-785` (`-49.083 dB` Windows endpoint value at creation). Later user slider changes did **not** reconstruct the Dolby generation. Instead the live volume-sync path changed the recovered final endpoint transaction independently.

This is consistent with the recovered split between generation-time VLLDP state and runtime endpoint volume transaction, and it is a candidate explanation for why the present playback feels especially stable/coherent. Do not collapse these two controls into one gain actuator without new Windows evidence.

## Mute defect localized

The current mute defect is above the kernel/WSA path.

`sp11-dolby-volume-sync` correctly reads PipeWire `mute` / `softMute` and logs `muted=yes`. However `apply_state()` currently only:

1. writes a postgain request to the Dolby profile control page; and
2. calls `set_hardware_volume()` on the hidden ALSA sink.

It does **not** propagate a mute actuator to the hidden hardware sink. Because the VLLDP postgain is frozen for the active generation, requesting minimum postgain while muted does not silence an already-running generation. This matches the user's observation that the mute button changes state but playback continues.

Do not fix this by rebuilding or perturbing the v28 kernel / WSA path. Recover or verify the Windows endpoint mute semantics, then make the smallest userspace volume-sync correction.

## Next gates

1. Preserve this exact stack as the listening reference.
2. Recover/verify Windows mute semantics and repair the userspace mute actuator without changing the audio coloration/state split.
3. Perform a direct Windows-vs-v28 low-bass / psychoacoustic-bass listening and, if needed, deterministic acoustic comparison.
4. Repeat the real YouTube seek audition only if a reproducible audible spike reappears; the user currently reports no forward/reverse spike in normal manual use.
