# Current native chain vs archived Windows known-input — 2026-08-05

The May 18 Windows known-input recording does not contain a trustworthy saved
Dolby profile identity, so it remains a validation fingerprint rather than
exact ground truth. It is still useful for testing whether the fully recovered
native chain has the right endpoint-scale dynamics.

The obsolete early comparison was made before the real Dynamic VR/DAP profile
was applied and showed nearly unity gain. Re-running the exact same stimulus
through the current original-code VLLDP->VR chain changes the result
substantially.

For Dynamic, representative RMS gain errors versus the archived Windows
loopback are:

```text
whole-file/usable region  about -0.3 dB
log sweep                  +0.43 dB
55 Hz                      -0.80 dB
90 Hz                      -0.90 dB
140 Hz                     +0.95 dB
```

The 1 kHz startup/reference segment is the main outlier at about +4.74 dB. The
six usable 75-Hz stepped-level windows follow the same nonlinear gain trend but
vary by roughly -2.5..+0.7 dB versus the old recording.

Across twelve segment-gain measurements (whole/1k/sweep/bass and six usable
level steps), the seven current profiles score approximately:

```text
profile        mean abs error   RMS error
Dynamic             1.49 dB       1.93 dB
Movie               1.76 dB       2.19 dB
Personalize         1.83 dB       2.23 dB
Game                2.09 dB       2.56 dB
Music               2.49 dB       3.13 dB
Online Course       3.81 dB       4.57 dB
Voice              11.23 dB      11.58 dB
```

Dynamic is the closest overall even though the historical recording was not
profile-labeled. This is strong external evidence that the recovered
leveler/regulator/limiter behaviour is in the Windows endpoint's operating
range, but it is not sufficient to relabel that historical file as Dynamic.

A purpose-built same-stimulus Windows capture with contemporaneous profile/RPC
state remains the only rigorous waveform-level endpoint oracle.
