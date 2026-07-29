# Pull transport runtime validation

## Result

The seventh audio-v3 boot validates the cumulative core and closes the
pull-transport implementation boundary. The DSP-written position page now
advances, direct ALSA sustains the 960-frame circular ring, and the normal
PipeWire route sustains the same stream through the Dolby identity/bypass
boundary.

No audible-quality claim is made here. Transport correctness and perceptual
parity are separate gates.

## Runtime identity

The one-shot V3 boot loaded:

- `snd_q6apm` source version `B81C31D91BEE0320DA11F97`;
- `q6apm_dai` source version `2F2511DFBA83E7B2099E507`;
- `q6apm_lpass_dais` source version `DB0C4EDB6BE0ED19BA8AB30`.

The saved fallback remained `sp11-7.1.5-clean`, and the one-shot next entry
cleared after boot. ALSA MM1 registered and both WSA884x SoundWire amplifiers
reported `Attached`.

## Lifecycle gate

Twelve frontend transactions reached the named `GRAPH_START accepted`
boundary. There were zero `GRAPH_START` timeouts. Six repeated prepares reused
the already configured pull graph without resending its one-time ring
configuration.

This confirms that the cumulative rebuild retains patches `0017` and `0018`.

## Position-page gate

The first deterministic probe wrote one 960-frame ring of zero samples and
then deliberately stopped refilling it. Linux observed pull hardware
positions `0`, `480` and `192` frames. The first status sample already showed
`avail = 192` and `delay = 768`, where the previous cached mapping remained
permanently at `avail = 0`, `delay = 960`, `hw_ptr = 0`.

The probe then reached XRUN as expected after consuming the only supplied
ring. That XRUN is positive evidence for this bounded test: the DSP consumed
the complete buffer and returned its capacity to ALSA.

Patch `0019` is therefore validated. The dedicated position mapping is
coherent with the DSP updater.

## Sustained direct-ALSA gate

A second probe continuously refilled one 480-frame period for five seconds:

- input: all-zero signed 16-bit stereo at 48 kHz;
- completed frames: `240960`;
- watermark events: `501`;
- final ALSA state: `RUNNING`;
- XRUNs: zero.

The hardware-pointer callback observed changing positions throughout the
circular ring. Delay stayed bounded between 768 and 960 frames.

## PipeWire and Dolby-bypass gate

PipeWire, WirePlumber and the PulseAudio compatibility service were restored.
The graph contained:

- `effect_input.sp11_dolby_bypass` as the configured default sink;
- `effect_output.sp11_dolby_bypass` as its passive output;
- `alsa_output.platform-sound.HiFi__Speaker__sink` as the physical target.

A five-second all-zero PipeWire stream produced 510 watermarks. Hardware
positions covered the ring from 0 through 912 frames. The PipeWire nodes
reported no errors, and the kernel reported no XRUN, underrun, SoundWire or
amplifier fault.

This validates the required Dolby module boundary in bypass mode without
claiming any Dolby dynamic processing.

## Remaining diagnostics

The single `0x01001021` timeout is the long-standing optional
`GET_SPF_STATE` startup query. It predates audio-v3 and is not on the playback
transaction.

Each graph-open attempt also receives status 3 for the optional aggregate
graph-calibration record. Every occurrence is immediately followed by
`graph calibration returned AR_EUNSUPPORTED; continuing as Qualcomm GSL
does`. This is the already recovered Windows/GSL policy, not an unexpected
module-stage failure. All individually ordered protection and render stages
were accepted afterward.

## Audible gate

A four-second 440 Hz left/right test was sent at combined heavy attenuation:
the bypass sink was `0.02` and the physical sink `0.10`. Both sinks were
automatically returned to volume zero and muted.

The transport remained error-free. Whether both tones were audible, balanced
and free of level modulation requires the user's listening report. No
perceptual result is inferred from the software trace.
