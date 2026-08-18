# Golden v31 active RX84 compatibility: 40 Hz, program, mute and seek

Date: 2026-08-18  
Status: **GREEN objective compatibility gate for active Windows RX84 policy**

## Scope

Golden v31 already closes the prior/new GainStep CKV 40 Hz Volume-Up defect and
implements exact final-VOL_CTRL DSP mute.  The active-RX84 candidate adds one
Windows-proven producer state only: after a successful protected graph handover,
WSA RX0/RX1 Digital Volume moves from the old Linux safety value 81 (-3 dB) to
native-Windows value 84 (0 dB), and returns to 81 when the graph idles.

This gate asks whether that +3 dB producer state reopens any already-closed
physical transient behavior.

## Recorder-side contamination discovered and corrected

An initial RX84 40 Hz capture was rejected because the SP7 default capture
endpoint had drifted to **+20.0 dB hardware microphone gain**.  A reversible
RAW-WASAPI A/B proved the effect:

- +20 dB HP500 idle RMS: ~1.92e-4;
- 0 dB HP500 idle RMS: ~2.09e-5;
- retained quiet v31 reference: ~1.39e-5.

The 0 dB raw peak (~0.00143) also returns closely to the retained reference
(~0.00153).  SP7 System Settings had been left open and was closed; the
measurement endpoint is now pinned to 0 dB for these gates.

This is a measurement-system correction, not an SP11 audio change.

## 40 Hz control stress at active RX84

Source is the exact retained Windows/v31 oracle:

`D900CE43A0C815FA8AC054629E65E3042BDB6E0CE9F1AD44135AA3F8F889B3E3`

48 kHz stereo PCM16, 40 Hz, -36 dBFS.  The run warms for 15 s and then drives
four 20-step 2% endpoint ladders: 46->6->46->6->46 percent.

Pinned-0-dB SP7 RAW WAV:

`1C29A96E672DEBBB3CF5CAED638D1713E68DB026EE903863D9C1C495F7FB0751`

Using the retained fourth-order zero-phase high-pass/per-edge discriminator:

- HP500 DOWN p95: `1.01356e-4`;
- HP500 UP p95: `9.97991e-5`;
- UP/DOWN: `0.98464x`;
- maximum HP500 UP edge: `1.04064e-4`;
- HP2000 UP/DOWN: `1.01327x`;
- HP6000 UP/DOWN: `0.98881x`.

The current room floor is somewhat higher than the morning v31 reference, but
UP and DOWN rise together.  No edge returns to the `10^-3` class that defined
the v30 defect.  RX84 therefore does **not** resurrect the prior/new-CKV
Volume-Up crackle.

SP11 stage log SHA-256:
`690C247545B38C3E9869430A782BA5DE1EFC982C0BB08314BE6C806573126766`

No transaction failure, WSA/SoundWire/XRUN fault, or new qcom-apm runtime error
occurred.  The only qcom-apm status is the known graph-start `0x01001006`
status-3 record.

## Timed RAW recorder

The earlier RAW helper stamps its filename before C# / WASAPI setup, so the
filename second is not a sub-100-ms capture clock.  An instrumented copy was
validated which records `IAudioClient.Start()` and `Stop()` UTC timestamps.
The successful combined gate below used exact StartUtc:

`2026-08-18T14:44:11.8840564Z`.

## Program + exact mute + deterministic seek gate

Program source is the retained v28/v31 local file:

`951A65CC63FEE17622485C1D94708614005524C7E20F86D3D815327F6BD0E8B3`

(Seven Nation Army MP3.)  Playback used GStreamer playbin into the Windows-Dolby
sink at 25%, with active RX84.  Two exact DSP mute/unmute cycles were followed
by the retained seeks:

- 25.756 -> 55 s;
- 58.931 -> 12 s;
- 16.001 -> 90 s.

SP7 timed RAW WAV SHA-256:
`FBA40772CADD89C6D05576D0EF2103DE75DA3412AAF43540ADD52807728EB07B`

SP11 action log SHA-256:
`7446EF928447D3A8205353E731A5084CEF13F84C6639C53E2D43E69623BCCCAA`

The two mute intervals independently fit one common SP11->SP7 clock/transport
offset: mute 1 prefers +0.680 s, mute 2 +0.660 s.  One fixed +0.670 s offset was
therefore used for **all** corrected scoring; no event-specific alignment was
allowed.

### Exact DSP mute

At the fixed offset, physical mute boundaries stay below the local program
p99.99 derivative scale:

- mute 1 worst boundary: ~0.198x local p99.99;
- mute 2 worst boundary: ~0.721x local p99.99.

Stable muted-middle broadband RMS is ~2.2e-4..3.4e-4, matching the current SP7
0-dB room-floor scale and far below adjacent active music.  No unique mute or
unmute click is exposed by RX84.

### Seeks

The retained +/-20 ms first-difference discriminator, compared with local
+/-0.75 s program material excluding central +/-60 ms, gives worst corrected
ratios versus local p99.99:

- seek 1: `0.696x`;
- seek 2: `0.271x`;
- seek 3: `0.975x`.

All seeks remain below the program material's own local p99.99 extreme on both
SP7 channels.  RX84 therefore preserves the existing seek/discontinuity closure.

## Lifecycle proof

The volume service log shows:

1. 25% `windows-lr:init` protected handover;
2. one `wsa_rx_active=84` transition;
3. exact DSP mute `1 -> 0 -> 1 -> 0` with **no volume transaction**;
4. no volume transaction during any seek;
5. final 25% -> 6% prior/new CKV transition;
6. `wsa_rx_active=81` when the graph idles.

Final state is 6%, unmuted, PCM closed, RX81.

## Decision

**GREEN:** active Windows RX84 / 0 dB is compatible with Golden v31's closed
40 Hz control-transient behavior, exact DSP mute, deterministic program seeks,
and protected producer lifecycle.  The remaining gate is operator normal
listening / bass judgment, not another objective rollback blocker.
