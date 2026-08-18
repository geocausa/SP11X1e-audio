# 40 Hz real-key Volume-Up microtransient localization

Date: 2026-08-18
Status: **physical defect reproduced and strongly localized; root actuator still under investigation**

## Operator localization

The operator found a tiny crackle under abusive keyboard-volume stress and
reduced it to a deterministic case:

- strongest on low-frequency program material;
- one crackle per Volume-Up step;
- Volume-Down produces a much smaller/different artifact;
- effect falls toward zero as stimulus frequency rises;
- a steady 40 Hz sine makes the defect easy to reproduce;
- perceived click ceiling is not strongly coupled to ordinary listening level.

## Old-geometry physical proof

With SP11/SP7 side-by-side, a fully warmed 40 Hz real-media-key sweep produced:

- DOWN HP500 p95 about `6.61e-5`;
- UP HP500 p95 about `3.26e-3`;
- UP/DOWN p95 about `49.35x`.

A second A/B with the redundant per-event hidden-sink unity write suppressed
still produced about `41.82x` UP/DOWN p95.  Therefore that redundant write can
aggravate the edge but is not the root cause.

Old-geometry checkpoint SHA-256:
`e311af5647c15ede852dc5877d455bb456b1e920d549ec3d5bc15aa39f7b8da4`.

## New fixed-geometry bridge

The SP7 microphone was then moved to the fixed keyboard-length fixture described
in `docs/baseline/2026-08-18-SP7-ACOUSTIC-FIXTURE-KEYBOARD-LENGTH.md`.

Current live policy during this capture:

- v30 candidate boot;
- Windows-matched GNOME media-key step = 2%;
- exact DSP mute path enabled;
- redundant per-event hidden-sink unity write suppressed as an A/B;
- warm 40 Hz source at -36 dBFS;
- real XF86 Volume keys;
- sweep order DOWN 46->6%, UP 6->46%, DOWN 46->6%.

SP7 capture:

- path:
  `C:\Users\SurfacePro7\Documents\KDNET\Codex\acoustic-reference-keyboard-length-20260818\lf40-bridge\external-mic-20260818-092959.wav`
- SHA-256:
  `455BD642B91F7B7E61F60D5DBBF431F9A362C444ABCF9A05D6EC6CA35F2C65EB`

New-geometry result:

- combined DOWN HP500 p95 `6.0663158e-5`;
- UP HP500 p95 `2.7855235e-3`;
- UP/DOWN HP500 p95 `45.92x`;
- DOWN HP2k p95 `4.978438e-5`;
- UP HP2k p95 `1.683309e-3`;
- UP/DOWN HP2k p95 `33.81x`;
- DOWN HP6k p95 `3.820154e-5`;
- UP HP6k p95 `1.019966e-4`;
- UP/DOWN HP6k p95 `2.67x`.

The fixed geometry therefore validates the old-position directional finding; it
is not a microphone-placement illusion.

## Layers already ruled out or narrowed

### GNOME notification sound

Real hardware-key-path monitoring proves `audio-volume-change.oga` is produced
while idle but suppressed while continuous media is RUNNING, even under key
spam.  The 40 Hz defect is not a leaked notification chime during continuous
playback.

### Digital Dolby / normal PCM path

Synchronized 40 Hz captures at pre-Dolby, post-Dolby and the PipeWire hardware
boundary remain symmetric/clean across control edges.  The physical one-sided
UP impulse has no corresponding digital click in those taps.  The Dolby port
can affect LF energy delivered downstream, but it is not directly emitting the
observed click.

### WSA8845 per-volume programming

A WSA8845 `_regmap_write()` trace recorded zero codec-register writes during the
actual UP and DOWN sweeps.  Only normal stream START/STOP lifecycle writes occur
outside those control windows.  The artifact is therefore not a Linux codec
register being rewritten on each key press.

### Missing final VOL_CTRL ramp policy

The current v30 topology SHA-256 matches the recovered Windows ramp topology:
`1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d`.
Final `VOL_CTRL 0x4a63 / 0x08001037` is already configured with the recovered
Windows tuple: 10 ms period, 1000 us step, curve 3.

### Raw final-volume/GainStep transaction alone

With the production volume daemon stopped and the hidden hardware sink held at
unity, direct Windows-style final `0x1038` + GainStep transactions at 38->40%
were physically at the SP7 mic floor across several L->R spacing variants.
Thus the raw DSP transaction by itself is not sufficient to reproduce the
real-key defect.

## Current inference and next oracle

The remaining difference is associated with the complete live desktop
control path rather than ordinary PCM generation, Dolby output samples,
per-step WSA register programming, or absence of the recovered ramp policy.
The next decisive comparison is native Windows using the same fixed geometry,
40 Hz stimulus, 2% key step and warm DOWN/UP ordering.  Only after that should
the remaining Linux control-path delta be changed.

## Matched native-Windows oracle — fixed geometry

A one-shot native Windows boot repeated the exact warm 40 Hz torture with the
SP7 microphone left unmoved in the keyboard-length fixture.  The generated
Windows source is byte-identical to Linux:

- source SHA-256 `D900CE43A0C815FA8AC054629E65E3042BDB6E0CE9F1AD44135AA3F8F889B3E3`;
- 48 kHz stereo PCM16, 40 Hz, -36 dBFS, 80 s;
- native Windows 2% media-key step;
- sweep `46 -> 6 -> 46 -> 6 -> 46%`;
- original Windows endpoint state restored to 10%, unmuted.

SP7 external-mic capture:
`0891CF1AB213BB8DB0A2A8B340A96E3500850722E2CF090BD41B6420C13CA458`.

Windows physical result:

- DOWN HP500 p95 `6.1545971e-5`;
- UP HP500 p95 `6.1937309e-5`;
- UP/DOWN HP500 ratio `1.00636x`;
- DOWN HP2k p95 `4.9303873e-5`;
- UP HP2k p95 `4.8846152e-5`;
- DOWN HP6k p95 `3.9526043e-5`;
- UP HP6k p95 `3.6975579e-5`.

Thus native Windows is physically at the same floor in both directions while
Linux v30 in the same geometry remains about `45.92x` UP/DOWN at HP500.  This
closes room geometry, source content and inherent WSA8845 hardware behavior as
explanations for the one-sided Linux transient.

Windows WASAPI loopback SHA-256:
`32E529E9CA0044A59E8F9C108B6CC8B69479C5F96064C67AE623206D14974885`.
Around the same native key events its local RMS and first-difference peaks are
identical for UP and DOWN.  The Windows endpoint control therefore remains
sample-stable upstream while the physical output also stays clean.

**Updated boundary:** the 40 Hz Volume-Up defect is a Linux downstream
endpoint-control implementation mismatch, not a generic speaker/rail limit and
not a Dolby-generated PCM click.

## Left-only / right-only causal split

Using the fixed keyboard-length microphone fixture, two independently authored
40 Hz -36 dBFS sources were replayed with only one source channel active at a
time.  Each side was warmed, then exercised by a real 6->46% Volume-Up sweep
and a 46->6% Volume-Down sweep.

SP7 capture SHA-256:
`ED723FC871230CFED27AE7756BCBA97F687CBE6EEF41B11EE209A1AA4DB538C0`.

Source hashes:

- left-only: `501223E3999CB344C3A7BF20A5243B8FD33FFEDEEBDB9D89AE298EB94B9F06D7`;
- right-only: `4F3503A95EB2F8EF8D3DAE16349CD0D2CDE7315AEB0AF3B21323CD070CAE84C4`.

Physical HP500 results:

- left-only UP p95 `1.0430952e-3`;
- left-only DOWN p95 `5.7946738e-5`;
- left UP/DOWN ratio `18.00x`;
- right-only UP p95 `1.9494859e-3`;
- right-only DOWN p95 `6.1277137e-5`;
- right UP/DOWN ratio `31.81x`;
- right-UP / left-UP p95 `1.87x`.

The defect therefore exists on **both** source/speaker channels.  It is not a
left-only failure and does not directly explain the separate fixed-geometry
left-path physical parity gap.  The larger right-only microphone transient is
consistent with the fixture's stronger physical sensitivity to the right-side
speaker.

The next software split must test the full problematic upward control ladder:
(1) GainStep/MSIIR row progression at fixed final Q28, and (2) final-Q28
progression at a fixed GainStep row.  Earlier isolated 32/34% and row7/row9
A/Bs did not cover the region where the real sweep produces its largest edges.

## Direct high-row Q28 / MSIIR causal split

A valid fixed-geometry direct-control experiment kept a warmed stereo 40 Hz
stream running while the production volume synchronizer was stopped only after
the protected PCM had reached RUNNING state.  The hidden hardware sink was
moved to unity once, then the fixed `SP11 Windows Volume Transaction` control
was driven directly.  Every transaction returned `rc=0`; production service
ownership and the 6% visible endpoint were restored afterward.

SP7 external-mic capture:

- SHA-256 `5BDCF37C5E62CE767CC7A61BC7747AF54B4867052281BA9F9F41F0672965B6C5`;
- fixed keyboard-length microphone fixture;
- 40 Hz -36 dBFS source, SHA-256
  `D900CE43A0C815FA8AC054629E65E3042BDB6E0CE9F1AD44135AA3F8F889B3E3`.

Raw direct-stage log SHA-256:
`80DD7318F0B8EF204459926DCD6F0AF157130FE8CF4E9B4CE9636BB7685C95A7`.

Three four-repeat upward ladders covered the real-key problem region
34 -> 36 -> 38 -> 40 -> 42 -> 44%:

1. **REAL simultaneous stereo**: Q28 and GainStep rows walked together
   9 -> 10 -> 11 -> 12 -> 13 -> 14.
2. **MSIIR-only**: Q28 held at 34% while GainStep rows walked 9 -> 14.
3. **GAIN-only**: GainStep held at row 9 while Q28 walked 34 -> 44%.

The repeated reset-to-34 transactions provide an internal down/control floor.
Physical HP500 p95 values:

- reset/control: `5.7389689e-5`;
- REAL: `5.7957624e-5` = `1.0099x` reset;
- MSIIR-only: `5.9252651e-5` = `1.0325x` reset;
- GAIN-only: `6.2333261e-5` = `1.0861x` reset.

HP2k and HP6k ratios are approximately unity or below as well.  No individual
36/38/40/42/44 step produced the `10^-3` class transient seen on the live
media-key path.

**Conclusion:** high-row final Q28 values and the Windows GainStep/MSIIR
calibration ladder are not individually sufficient to create the 40 Hz
Volume-Up click.  The test above used one simultaneous-stereo transaction per
step, so the complete high-row Windows two-call L-new/R-old -> both-new ladder
must still be closed before excluding runtime q6apm sequencing entirely.

## Exact high-row Windows L/R two-call ladder

The final direct-DSP sequencing caveat was then tested with the complete
recovered Windows stereo master sequence at every problematic high-row step:

```text
L=new, R=old + louder/new GainStep
L=new, R=new + final GainStep
```

Four 34 -> 36 -> 38 -> 40 -> 42 -> 44% ladders were sent while the production
volume daemon was stopped, the 40 Hz graph remained RUNNING, and the hidden
sink was held at unity.  Natural per-call helper/DSP spacing ranged from about
2 ms to 14 ms; every call returned `rc=0`.

SP7 capture SHA-256:
`9BDC669D31FFDC3B5E3F7C9F572C527E173ABB45010D8ADB95637F841E930E87`.

Raw stage log SHA-256:
`C24363A87450C25C70494E09B71141C09175F8F9E90FF46FE44B2D61A471638E`.

Direct WLR physical result:

- HP500 median `5.8794e-5`;
- HP500 p95 `2.7728e-4`;
- HP500 max `4.5081e-4`;
- HP2000 p95 `1.2707e-4`;
- HP6000 p95 `3.9660e-5` (floor class).

The LF outliers are concentrated at the 44% step: the four HP500 values there
were approximately `4.51e-4`, `1.45e-4`, `6.91e-5`, and `2.68e-4`.  The
36/38/40/42% medians remain close to the microphone floor.

This proves that the exact two-call channel-ordered sequence can contribute a
small low-frequency physical transient, unlike the simultaneous-stereo direct
ladder.  However it is **not sufficient** to explain the production defect:
fixed-geometry real-key Linux UP p95 is `2.7855e-3`, roughly an order of
magnitude larger than direct WLR p95, while native Windows remains at
`6.1937e-5` under the same torture.

**Updated boundary:** q6apm channel-order timing is a minor LF contributor at
the upper step, but a second live-desktop-path effect is required to produce
the full Linux Volume-Up artifact.  The next direct discriminator is
host/PipeWire visible-endpoint volume movement with q6apm held fixed.

## Full-history direct q6apm oracle — PipeWire control movement not required

The decisive direct-DSP test held the visible PipeWire control sink fixed at
6% for the entire measured section and stopped the production volume
synchronizer.  A warmed stereo 40 Hz stream remained RUNNING, the hidden
hardware sink was moved to unity once, and q6apm was driven directly through
four complete exact Windows-style channel-ordered sweeps:

```text
6 -> 8 -> 10 -> ... -> 46 -> 44 -> ... -> 6%
```

For every UP master step:

```text
L=new, R=old + mixed/louder GainStep
L=new, R=new + final GainStep
```

For every DOWN master step the first call retained the old/louder GainStep and
the second selected the new lower GainStep.  Every transaction returned
`rc=0`.  The visible PipeWire endpoint never moved during the direct sequence.

SP7 external-mic capture SHA-256:
`6E07E01E071ED5DE3643C30922F40A9376C6DB951CE1F6AA244947A6C9A0BDE4`.

Raw direct-stage log SHA-256:
`04C6B57720132EF98769BF67DDFF7787EF1680B1D2174F9D2B8C86599FCF0807`.

Driver script SHA-256:
`BCE0755A197592063E83DE58C7E23BD50249BC8511EB6F1695309C1B56AC1687`.

Physical result over 80 UP and 80 DOWN steps:

- UP HP500 median `6.2824e-5`;
- UP HP500 p95 **`1.04619e-2`**;
- UP HP500 max `1.29157e-2`;
- DOWN HP500 p95 `6.28276e-4`;
- UP/DOWN HP500 p95 **`16.65x`**;
- UP HP2k p95 `8.48943e-3`;
- DOWN HP2k p95 `5.78279e-4`;
- UP HP6k p95 `4.79135e-4`;
- DOWN HP6k p95 `1.60680e-4`.

This direct q6apm-only UP p95 is about **3.76x larger** than the fixed-geometry
production real-key Linux UP p95 (`2.7855e-3`) and about **169x** the matched
native-Windows UP p95 (`6.1937e-5`).  Therefore neither GNOME media-key
handling nor visible PipeWire endpoint movement is required to create the
core defect.

The full history matters.  Direct tests that started from a settled 34% state
were mostly clean, whereas the full 6->46 progression develops large edges,
especially from the mid/high GainStep region onward.  This is a stateful
runtime calibration/volume-lifecycle defect.

## Newly exposed prior/new CKV semantic gap

Recovered Qualcomm GSL runtime calibration does not merely 'send the selected
GainStep row'.  `gsl_graph_send_nonpersist_cal()` receives both the prior and
new CKVs.  ACDB `AcdbComputeDeltaCKV()` constructs a delta containing only keys
whose values changed, and `AcdbFindModuleCKV()` selects the GainStep-dependent
four-frame `0x489e` group only when the speaker GainStep key `0x01000011`
changed.

Current Linux `SP11 Windows Volume Transaction` has no prior-CKV state and
always appends the selected GainStep group to every volume call.  Consequently
Linux re-applies identical GainStep calibration when:

- endpoint Q28 changes but remains within the same GainStep row; and
- the second L-new/R-new channel call retains the CKV selected by the first
  L-new/R-old call.

For example, Windows 8->17% selects GainStep 2 on the first channel update; the
second channel update remains in GainStep 2 and therefore has no GainStep-key
CKV delta.  Linux currently sends row 2 calibration on both calls.

This is now the leading exact-parity defect.  The next candidate must preserve
all recovered Q28 and row values while adding a **final-volume-only** runtime
actuator and sending the OOB GainStep group only when prior/new CKV comparison
shows the GainStep changed.  No EQ/ramp coefficient guesses are justified.

## v31 prior/new CKV candidate — first physical closure

Golden v31 implements the recovered prior/new CKV semantics with a fixed
final-volume-only q6apm control.  In the unchanged SP7 keyboard-length fixture,
the exact v30/Windows 40 Hz real-key torture falls from v30 HP500 UP p95
`2.7855e-3` to v31 `6.6466e-5`; native Windows is `6.1937e-5`.  v31 UP/DOWN
p95 is `0.975x` versus Windows `1.006x`.

The one-sided low-frequency crackle is therefore absent in the first v31
physical gate.  See
`docs/findings/2026-08-18-GOLDEN-V31-CKV-DELTA-40HZ-PHYSICAL-GATE.md`.
