# Hidden Dolby engine unity cold-boot guard — 2026-08-18

## Symptom

Golden v31 appeared structurally healthy but ordinary playback through the
production PipeWire virtual speaker was effectively silent.  The failure was
not in the protected kernel graph:

- direct ALSA `hw:0,0` playback produced a strong 997-Hz acoustic line;
- `pw-play` directly to the PipeWire ALSA speaker sink also produced the tone;
- the same source sent to `effect_input.sp11_windows_dolby` or the legacy
  `effect_input.sp11_dolby_bypass` did not produce measurable 997-Hz output.

The SP7 RAW reference microphone was used as the independent physical oracle.

## Root cause

The split production topology deliberately makes the visible
`effect_input.sp11_windows_dolby` node control metadata only.  Its unity monitor
ports feed the hidden `effect_input.sp11_windows_dolby_engine`, and endpoint
attenuation is applied after Dolby by the recovered final AudioReach transaction.
The hidden engine therefore has one invariant: its PipeWire input volume must be
unity.

On the failing boot:

```text
visible control sink                  0.14
hidden Dolby engine                   0.06   <-- invalid
hidden downstream ALSA sink idle      0.33
```

Changing only the hidden engine to `1.00`, while leaving the visible endpoint
and downstream taper untouched, immediately restored the 997-Hz tone.  The SP7
measurement showed approximately 64.2 dB of 997-Hz prominence above the local
spectral noise floor.

The fault therefore lived in userspace stream-volume state/recreation, not in
Golden v31, CPS, SoundWire, WSA8845, or the recovered Dolby processor.

## Guard

`sp11-dolby-monitor-link` now also protects the hidden-engine unity invariant.
For each hidden-engine node incarnation it:

1. resolves the current PipeWire node ID;
2. checks the engine volume/mute state during a bounded five-second bootstrap
   window;
3. repairs only that hidden node to volume `1.00`, unmuted, if a late session
   restore changes it;
4. continues to maintain the exact FL/FR visible-monitor -> hidden-engine links.

It does not modify the visible endpoint scalar or the downstream hardware taper.

## Validation

### Deliberate static fault

The hidden engine was forced to `0.06` and the keeper restarted.  It returned to
`1.00` in under 200 ms while the visible endpoint remained `0.14` and the idle
hardware taper remained `0.33`.

### Delayed restore race

The keeper was restarted and, two seconds later, the hidden engine was forced to
`0.06` to model a late WirePlumber restore.  It returned to `1.00` within about
350 ms.  The visible and hardware nodes were unchanged.

### True cold boot

After a full reboot into protected Golden v31:

```text
saved GRUB entry                      sp11-audio-golden-v31
root topology sha256                  1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d
visible endpoint                      0.14
hidden Dolby engine                   1.00
Dolby output                          1.00
idle hardware taper                   0.33
monitor -> engine links               present
```

The keeper logged the hidden-engine guard before the monitor path became linked.
A post-boot 997-Hz production-path run measured approximately **64.5 dB** above
local spectral noise on the SP7 RAW microphone.  After the normal v31 handover
and idle timeout, PCM closed and the hardware sink returned from active unity to
the `0.33` fail-quiet taper.

## Consequence for CPS/VI diagnostics

The earlier DIAG logger experiments that observed zero post-SP PCM were driven
through the then-silent virtual-sink path.  Their transport/configuration audits
remain useful, but their sample-domain conclusions must be repeated with the
now-proven non-silent production stimulus before any CPS/VI data-plane failure
is claimed.
