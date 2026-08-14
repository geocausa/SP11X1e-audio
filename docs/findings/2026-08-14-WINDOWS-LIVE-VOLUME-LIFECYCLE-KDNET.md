# Windows live endpoint-volume lifecycle — final VOL_CTRL/CKV live, VLLDP frozen

Date: 2026-08-14

## Result

Fresh same-day SP11 Windows KDNET, WASAPI loopback and exact-binary static analysis correct an important lifecycle assumption in the Linux parity implementation.

For an already-created Windows Dolby/APO instance, endpoint master-volume changes do **not** retune VLLDP postgain. The live Windows volume path is instead:

```text
IAudioEndpointVolume change
  -> qcadcm SetVolume
  -> final speaker VOL_CTRL iid 0x4a63 / pid 0x08001038
  -> GetGraphCkv / GainStep selection
  -> dependent MSIIR 0x489e calibration
```

VLLDP's recovered endpoint-dB/postgain relation remains real, but it is an APO-generation configuration input rather than a per-slider live actuator.

Linux was previously doing too much on every slider event: it wrote VLLDP postgain and then sent final VOL_CTRL + GainStep. The production synchronizer has now been corrected so postgain is queued once per Dolby/filter-chain generation and remains frozen for ordinary volume changes and new streams within that generation.

## Exact binaries and retained evidence

Windows VLLDP binary:

```text
DolbyAPOvlldp150.dll
version 3.30704.742.0
SHA-256 A2553FF7B013B5A248E50BDCAE46D08405E393C0085073975214D035CEDF02C1
```

Windows qcadcm driver:

```text
qcadcm8380.sys
SHA-256 37F76305AC8051B0B03B6D2CE1DF7A353253DEBF546E512E447C9D95EC661429
SP7 path:
C:\Users\SurfacePro7\Documents\blobs\sp11-driverdump\qcadcm8380.inf_arm64_f5fba49e0720d715\qcadcm8380.sys
```

Raw KDNET log:

```text
C:\Users\SurfacePro7\Documents\KDNET\Codex\SP11_VOLUME_ORDER_20260814.log
bytes  69500
SHA-256 72CA9D1D2E9D10066F7A061AB948D81143F5D084BD3AFDBAB907FBC231B712D5
```

Debugger host was SP7 using classic Windows Kits `kd.exe`; target KDNET was port 50005, key `1.2.3.4` over the SP11 EEM debugger adapter. The Store-app debugger was rejected after it failed to handshake; the classic Kits debugger attached successfully.

## qcadcm static recovery

Headless Ghidra analysis of the exact driver resolved three diagnostic strings:

```text
GetGainTableStepFrmQ28Gain
failed GetGraphCkv
CKV:GainStep:%d|GslStatus:0x%x
```

All are referenced from `FUN_14006e038`, RVA `0x6e038`. Decompilation shows this function is the live qcadcm SetVolume path: it consumes endpoint Q28 gain, selects the gain-table row, applies final VOL_CTRL, calls GetGraphCkv, and applies the dependent calibration.

A separate function at RVA `0x85270` performs the same selector work during graph construction. This separates graph-start state from the true runtime volume path.

On the captured boot qcadcm loaded at:

```text
base                    0xfffff800a00f0000
SetVolume +0x6e038      0xfffff800a015e038
SET_CFG   +0x60b78      0xfffff800a0150b78
GetGraphCkv +0x91888    0xfffff800a0181888
```

The active VLLDP image loaded at `0x00007ffc46140000`. Breakpoints were placed at both the recovered wrapper postgain setter `+0x1d170` and the deeper scalar setter `+0x91480`.

## Live Windows 8% -> 17% -> 8% KDNET result

The two VLLDP postgain breakpoints recorded **zero hits** during the active endpoint-volume gestures.

The qcadcm path did fire. The definitive run recorded SetVolume, final `0x4a63/0x08001038`, then GetGraphCkv on the same live gesture.

At approximately 8%:

```text
final Q28 0x0039db88
GetGraphCkv internal index 0 -> CKV1
```

At approximately 17%:

```text
final Q28 0x00c7763f
GetGraphCkv internal index 1 -> CKV2
```

Windows updates master stereo volume as two channel updates rather than one simultaneous body. The captured final VOL_CTRL bodies show:

```text
8 -> 17:
  1. L = new, R = old; CKV remains/selects 2
  2. L = new, R = new; CKV 2

17 -> 8:
  1. L = low, R = high; CKV remains 2
  2. L = low, R = low; CKV becomes 1
```

This per-channel two-call sequencing remains one transition-level difference from Linux, which currently emits one simultaneous stereo final-volume body followed by the matching GainStep group.

## Stationary loopback proof: live slider does not alter Dolby PCM

A deterministic 45-second 48-kHz PCM16 stereo source was generated with fixed 75-Hz and 997-Hz components:

```text
stationary-75-997.wav
SHA-256 D878C1AA51728CA667C08647FFEE7362E523DDF345CCEBBBB65005B1625FA5A7
```

During one continuous playback Windows endpoint volume changed 8% -> 17% -> 8%. WASAPI speaker loopback is upstream of the physical endpoint actuator but downstream of the Dolby/userspace processing under test.

Across the two slider changes, after settling from initial startup, half-second loopback windows remained effectively constant:

```text
L RMS  about -8.776 dBFS
R RMS  about -7.898 dBFS
peak   about -2.776 dBFS
```

Thus the upstream Dolby sample transfer does not follow live endpoint volume.

## Second stationary proof: idle -> new stream also leaves VLLDP unchanged

A second experiment stopped media completely, changed endpoint volume while idle, waited, and then opened a new stationary stream. Two separate WASAPI loopback recordings gave:

```text
17% new-stream start:
  L RMS -8.77548 dBFS
  R RMS -7.89742 dBFS

8% new-stream start:
  L RMS -8.77667 dBFS
  R RMS -7.89974 dBFS
```

Differences are only about 0.0012 dB left / 0.0023 dB right. Therefore ordinary stream stop/start does not refresh VLLDP postgain while the same Windows Dolby/APO instance remains alive.

## Linux correction

`deploy/dolby/sp11_windows_volume_transaction_sync.py` now follows that lifecycle:

1. one endpoint-derived VLLDP postgain request is queued for a newly created Linux Dolby/filter-chain generation;
2. that generation is recorded in `/run/user/$UID/sp11-dolby-volume-generation`;
3. a service restart observing the same visible filter node does not rewrite VLLDP postgain;
4. ordinary slider changes do not write VLLDP postgain;
5. filter-chain recreation/new Dolby-engine generation queues the current endpoint-derived postgain once;
6. live slider changes continue to send final VOL_CTRL and the exact GainStep/MSIIR group;
7. fail-quiet host attenuation ordering is unchanged.

The generation marker is deliberately runtime-scoped and disappears with the user session.

## Linux live validation

On `7.1.5-sp11-render-parity-v4+`, with VLLDP request/ack initially `-503/-503` at a 12% visible endpoint, ordinary MP3 playback ran while the visible control changed:

```text
12% -> 8% -> 17% -> 8% -> 12%
```

At every point the VLLDP control page remained:

```text
request -503
ack     -503
```

The live qcad-equivalent transaction still moved correctly:

```text
8%  -> final_q28 0x0039db88, GainStep 1
17% -> final_q28 0x00c7763f, GainStep 2
8%  -> final_q28 0x0039db88, GainStep 1
12% -> final_q28 0x00702a69, GainStep 1
```

Both WSA8845 amplifiers produced 18 successful observer samples each during the run:

```text
PA enabled             18/18 each
current-limit code      17 each
register 0x3091         0x44 each
PA error nonzero        0/18 each
```

No XRUN, SoundWire, PA or WSA fault was logged.

Focused tests after the change: `13 passed`.
Full repository suite: `150 passed, 3 skipped, 6 subtests passed`.

## Retired theory and remaining exactness gap

The earlier theory that Linux needed to wait for a live VLLDP postgain acknowledgement before sending final VOL_CTRL/GainStep is retired. Windows does not perform that live VLLDP update at all.

The remaining volume-transition exactness gap is narrower and concrete: Windows performs two channel-ordered SetVolume transactions for a stereo master change, with GetGraphCkv/dependent calibration evaluated after each call; Linux currently applies the new stereo gain simultaneously in one transaction. That sequencing should be evaluated next rather than adding any guessed fade, EQ, limiter or Dolby retune.
