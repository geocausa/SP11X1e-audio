# Linux bounded WSA protection observation — 2026-08-14

## Result

The current `7.1.5-sp11-render-parity-v2+` boot can observe both WSA884x
amplifiers during protected playback. A bounded 440 Hz stereo run collected 12
samples per amplifier at 100 ms intervals. Both devices stayed enabled, returned
independent changing raw ADC/temperature words, retained current-limit register
`0x44`, and reported no read failure, status error, interrupt, PA fault or XRUN.

The reviewed machine-readable result is
`artifacts/reviewed/linux-render-parity-wsa-observer-20260814.json`.

## Exact live gate

- Kernel: `7.1.5-sp11-render-parity-v2+`
- Entry marker: `sp11_entry=7.1.5-sp11-visense-parity`
- WSA884x srcversion: `B7F5D7D97DD31C77EFB6F01`
- Stimulus: `speaker-test -D pipewire -c 2 -t sine -f 440 -l 1`
- Visible volume: 41%
- Transport: DP1 `0x01`, DP2 `0x0f`, DP3 `0x03`, DP5 `0x03`, DP6 `0x03`
- Common readback: PA `0x01`, status `0x2f/0x00`, errors and interrupts all
  zero, failed-read mask zero
- SP/SPVI enable with VI+CPS feedback: accepted

The observer module parameter is global and does not clear itself after one
playback. It was therefore explicitly returned from `12` to `0` immediately
after the run. The PCM closed normally after idle and
`sp11-dolby-volume-sync.service` remained active.

## What this closes

This closes the basic question of whether Linux can inspect both speaker amps
while the protected path is actually running. The feedback/measurement surface
is live on both sides, rather than dead, single-sided, or failing reads.

It does **not** convert the raw register words into calibrated current,
excursion, power or temperature. It also does not reproduce or prove equivalence
with the private Windows DSP diagnostic callback/result protocol. Those narrower
questions keep P09 AMBER.

`CPS_CTL=0x00` must not be misread as an absent CPS path. It is one local codec
register; the live DP6 `0x03` programming and accepted SP/SPVI graph with VI+CPS
feedback are the relevant transport evidence.

## Physical-channel gate

The same session played the standard spoken `Front Left` / `Front Right`
stereo test and a channel-alternating sine test. Software channels and SoundWire
ports are distinct, but the operator still needs to confirm that the spoken
labels came from the matching physical sides. Until that answer is recorded,
H07 remains AMBER and no left/right calibration reassignment is justified.

## Deployment decision

No sound-changing patch is justified by this observation. The deployed
ordinary-stereo Windows transaction already has the expected DP1/2/3/5/6
schedule, and Windows did not positively schedule DP4 in the retained capture.
The observer is evidence instrumentation only and remains disabled by default.
