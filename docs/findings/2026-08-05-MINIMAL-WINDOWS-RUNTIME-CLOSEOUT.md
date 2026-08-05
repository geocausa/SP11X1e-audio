# Minimal Windows runtime closeout — prepared 2026-08-05

This is the only runtime experiment still required to close modern ASAR for the
same steady SP11 speaker condition tested on 2026-08-04. It does not reopen the
VLLDP/VR work.

## Why another runtime sample is needed

Static analysis now proves a successful AIDE pass necessarily executes module
RVA `0x3A438`, and the August hardware breakpoint at that address had zero hits.
AIDE is therefore ruled out for the tested steady stream.

The exact same 7.3.7 ASAR binary also contains an alternate OAR/crossfade path
which can be reached without first executing AIDE. Static analysis cannot tell
which branch the live Windows graph selects. Module presence is not enough.

## Minimal hardware-breakpoint set

Resolve the live base of `DolbyAudioProcessing.dll` in the active `audiodg.exe`
and set **hardware execution** breakpoints at:

```text
ASAR + 0x0241E8   OAR processing core (new decisive target)
ASAR + 0x01C378   high-level crossfade/state path entry
ASAR + 0x03A438   AIDE adaptive core (known-cold control)
```

Keep one already-proven persistent Dolby callback hot as the positive control
(VLLDP150 orchestrator or VR callback). Software-breakpoint non-hits are not
valid evidence in this KD setup; use hardware execution breakpoints only.

With steady stereo music already pumping, observe a short interval. The result
is sufficient if:

1. positive-control VLLDP/VR continues to hit;
2. OAR and crossfade both remain at zero hits;
3. AIDE remains at zero hits.

That closes modern ASAR as a steady per-buffer speaker-stage for that condition.
If OAR/crossfade fires, capture registers/object pointers at entry and add only
that proven branch to the parity target.

## Exact waveform oracle, same boot if desired

For final endpoint-level waveform scoring, play the existing deterministic
48-kHz stereo known-input WAV while simultaneously recording:

- Windows speaker loopback;
- Dolby Access selected profile;
- DAX `active_profile` and relevant effect state;
- Windows master volume and format;
- enhancement/spatial state.

The existing May loopback is useful but lacks contemporaneous profile identity.
The current original-code Dynamic Linux chain already matches its overall
transfer behaviour closely (about 1.49 dB average segment-gain error across the
usable test segments); do not fit the Linux implementation to that unlabeled
recording.
