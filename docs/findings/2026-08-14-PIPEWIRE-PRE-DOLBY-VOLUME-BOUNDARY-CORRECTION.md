# PipeWire pre-Dolby volume-boundary correction

Date: 2026-08-14 (Europe/London)

## Result

A deterministic same-file Windows/Linux comparison found the remaining major
steady-state acoustic mismatch outside the kernel and amplifiers.  The
production PipeWire graph exposed the user-facing virtual Dolby sink as the
capture side of the recovered VLLDP/VR filter itself.  WirePlumber therefore
applied the visible sink's cubic `channelVolumes` to the PCM **before** Dolby.

At the controlled 8% reference state the live node reported:

```text
visible scalar                 0.08
PipeWire channelVolumes        0.000512 = 0.08^3
PipeWire pre-Dolby gain        -65.8146 dB
Windows endpoint dB            -37.1832 dB
VLLDP postgain                 -595 (1/16 dB units)
final AudioReach VOL_CTRL Q28  0x0039db88
GainStep                       1
```

The v4 endpoint transaction correctly moved the hidden ALSA sink to unity only
after programming final `VOL_CTRL` and GainStep.  It did **not** remove the
virtual sink's own `channelVolumes` attenuation because that attenuation lived
on the same node that fed the Dolby processor.  The result was the wrong signal
ordering: endpoint/UI attenuation before VLLDP/VR, followed by the recovered
Windows endpoint attenuation again in final AudioReach.

The earlier V03 audit was too narrow.  It checked that `softVolumes` were unity
and inferred that the virtual sink contributed no gain.  The live 8% state has
`softVolumes = 1.0` but `channelVolumes = 0.000512`; the latter is the effective
capture-side volume and is sufficient to attenuate PCM before the LADSPA graph.

## Same-source Windows oracle

The controlled source is:

```text
The White Stripes - Seven Nation Army (Official Music Video).mp3
SHA-256 951a65cc63fee17622485c1d94708614005524c7e20f86d3d815327f6bd0e8b3
```

The exact file was copied to both operating systems.  Windows played it at 8%
endpoint scalar / `-37.18317 dB`, with application volume unity.  `audiodg.exe`
hosted the expected SP11 Dolby stack during the capture.

After correct source-onset alignment, Windows WASAPI loopback relative to the
decoded source showed approximately:

```text
RMS gain:  +5.57 dB left / +5.73 dB right
peak:      about -0.12 dBFS
correlation to decoded source: about 0.96 per channel
```

This is the expected VLLDP/VR leveler/DRC behavior: substantially raised average
level with the final AudioEngine limiter keeping peaks near its recovered
ceiling.

## Recovered Dolby implementation is not the fault

The exact production recovered Windows VLLDP/VR host was replayed offline with
the same 35-second PCM and exact 8% postgain `-595`.  The Movie profile ranked
first and matched the Windows loopback with average waveform correlation about
`0.99986`, average gain error around `0.01 dB`, and the same band-wise transfer.

A second, isolated **real-time** PipeWire filter instance was then run with
unity input gain, Movie profile and postgain `-595`.  Against the Windows
recording it measured:

```text
correlation L/R        0.9999618 / 0.9999259
fitted gain error      -0.0029 dB
residual relative RMS  -39.90 dB
```

Thus the recovered Dolby code, Movie state and limiter are already capable of
reproducing this Windows oracle in real time.  The failure is the production
PipeWire volume boundary around them.

## Corrected topology

PipeWire sink monitor ports remain unity while the visible sink's playback path
carries `channelVolumes`.  A controlled split-graph test therefore separated UI
state from PCM state:

```text
application
  -> visible control sink effect_input.sp11_windows_dolby
       channelVolumes retains the GNOME/WirePlumber scalar
       unity monitor_FL/FR
          -> hidden Stream/Input/Audio Dolby engine at unity
             -> recovered VR -> VLLDP -> AudioEngine limiter
                -> physical ALSA sink
                   -> protected AudioReach final VOL_CTRL / GainStep
                      -> WSA8845 amplifiers
```

The isolated split test used a visible 8% control sink, explicitly linked its
unity monitor ports into a hidden unity Dolby engine, and connected that engine
only to a recorder.  It matched the Windows oracle with:

```text
correlation L/R        0.9999612 / 0.9999194
fitted gain error      -0.0118 dB
residual relative RMS  -39.81 dB
```

No physical speaker or production sink was involved in that proof.

## Production candidate

`deploy/dolby/98-sp11-windows-dolby.conf` now implements the split architecture:

1. the existing `effect_input.sp11_windows_dolby` name remains the sole visible
   `Audio/Sink`, preserving all existing volume-sync/default-device logic;
2. its internal copy output is deliberately non-autoconnecting and unused;
3. `effect_input.sp11_windows_dolby_engine` is a hidden `Stream/Input/Audio`
   that alone hosts the recovered Dolby LADSPA processor;
4. `effect_output.sp11_windows_dolby` remains the physical-speaker-targeted
   output; and
5. `sp11_dolby_monitor_link.py` maintains only the exact two unity
   monitor-to-engine links across node recreation.

Idle/failure safety is unchanged: while the protected graph is not running, the
existing volume synchronizer leaves Windows-taper attenuation on the hidden
physical sink.  During protected playback the final AudioReach transaction is
sent first and only then is the hidden host sink moved to unity.  The topology
change removes the unintended **pre-Dolby** gain; it does not weaken the
fail-quiet handover.

This correction is independent of the remaining postgain-request/ack transition
ordering race documented in `2026-08-14-EVENT-DRIVEN-VOLUME-TRANSACTION.md`.
That race can still affect slider transitions after steady-state acoustic parity
is restored and must be evaluated separately.


## Live production validation

The split topology was installed in the user PipeWire filter service on the
running `7.1.5-sp11-render-parity-v4+` boot.  The existing visible node name and
original `media.name = "SP11 Windows Dolby"` were deliberately preserved so
WirePlumber continues to restore the stored desktop volume state.  The hidden
engine is `Stream/Input/Audio` and does not appear as a second speaker sink.

A filter-chain restart exposed one additional lifecycle hazard: PipeWire emits
a replacement sink's default-unity Props before WirePlumber restores the saved
8% state.  The event-driven transaction synchronizer previously consumed that
transient and briefly sent Q28 unity/GainStep 30 even though no user requested
100%.  `sp11_windows_volume_transaction_sync.py` now binds volume state to the
logical user control across object recreation: a new node ID immediately
inherits the last user scalar before any endpoint transaction is permitted.
Cold login also waits a bounded 300 ms node-settle window before its first
transaction.

The live recreation gate now reports only:

```text
recreated visible sink id=<new> restored ui_scalar=0.080000 muted=no
```

and no `pipewire_gain=1` / Q28-unity transaction.  Repeated 50 ms snapshots
showed the recreated visible node directly at `channelVolumes=0.000512` once it
became queryable, while the exact monitor-to-engine links were recreated by the
link keeper.

The final fresh-engine production playback used the exact hashed Seven Nation
Army MP3 at 8% visible scalar.  The hidden Dolby output was tee-captured while
both physical WSA8845 amplifiers were observed read-only.  Against the already
Windows-proven offline Movie oracle, production measured:

```text
correlation L/R              0.99995215 / 0.99990243
fitted production->Movie     +0.00688 dB
relative residual RMS        -38.82 dB
Movie RMS L/R                -10.0411 / -12.5917 dBFS
production RMS L/R           -10.0471 / -12.6018 dBFS
production peak L/R          -0.1228 / -0.1182 dBFS
```

Amplifier validation during the same playback:

```text
amp 0: 77/77 PA enabled, current-limit code 17, 0 PA errors
amp 1: 77/77 PA enabled, current-limit code 17, 0 PA errors
kernel: no XRUN, SoundWire timeout/error, or PA fault in the playback window
```

The repository suite after the topology and recreation-guard changes reports
`149 passed, 3 skipped, 6 subtests passed`.

This closes the steady-state pre-Dolby volume-boundary defect.  Subjective
speaker parity remains an operator listening gate, and the separate
postgain-request/ack ordering race remains a transition-quality item rather
than a steady-state render mismatch.
