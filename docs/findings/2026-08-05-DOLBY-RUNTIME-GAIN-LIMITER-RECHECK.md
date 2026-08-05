# Dolby runtime gain / limiter recheck — 2026-08-05

## Why this exists

The static SP11 tuning says Virtual Bass, Bass Enhancer, Bass Extraction,
Sliding Bass and Volume Modeler are disabled. That is true as a statement about
the named controls, but it is not enough to certify the running Windows audio
path. The Windows DAX service rebuilds final content/device processing maps at
runtime, and the recovered digital loopback proves a remaining level-dependent
nonlinearity that the current Linux bridge does not reproduce exactly.

## Live DAX Music content map from process memory

The June-8 main DAX3API service dump is a standard Memory64 minidump. Rebuilding
MSVC `std::map<std::wstring,std::wstring>` nodes recovers a complete live Music
content-processing map. Important values include:

```text
virtual_bass_process_enable = 0
virtual-bass-mode           = 0
virtual-bass-overall-gain   = 0
bass-enhancer-enable        = 0
bass-extraction-enable      = 0
volume-modeler-enable       = 0
volume-leveler-enable       = 1
volume-leveler-drc-enable   = 1
regulator-enable            = 1
volmax-boost                = 96
postgain                    = 0       (persistent/base map)
system-gain                 = 0       (persistent/base map)
```

The separate device/VLLDP map carries Audio Optimizer enabled plus the exact
Movie/Music compressor/deviation/slow-gain tuning.

Thus the explicit harmonic/virtual-bass gate really is off in this live service
state. The active loudness machinery is instead Leveler + DRC + Regulator +
Volume Maximizer plus VLLDP device processing.

## Runtime DAX feedback is not static XML

`DolbyEndpointControl::UpdateDapParameters` (`DAX3API.exe` `0x140088EA8`)
rebuilds final maps while Windows is running.

For the SP11 speaker content/device-APO arrangement it:

1. obtains current content and device tuning maps;
2. reads cached endpoint master-volume dB;
3. computes `postgain = round(master_volume_dB * 16)`;
4. injects `postgain` into the content map;
5. obtains the separate `VlldpSystemGain` runtime value;
6. injects `system-gain` into the device map;
7. pushes the rebuilt maps into the live APO path.

SP11 operator settings confirm `volume_feedback_enable = true` while
`compensate_volume = false` and `override_device_volume = false`.

A real June endpoint-control object was recovered from DAX3API process memory.
Its endpoint volume range/state is approximately:

```text
min         -75 dB
max           0 dB
step          0.5 dB
scalar        0.04
master level -46.506 dB   (that snapshot)
```

Therefore authentic SP11 postgain is never positive. Positive postgain probes
were useful sensitivity tests but are not valid normal Windows operating
states. `VlldpSystemGain` defaults to zero when its device-info key is absent;
no nonzero SP11 value has been recovered so far.

The original VR postgain handler is `0x18003C590`. It stores the normalized
runtime control at core `+0x61C` and raises the dirty flag. A diagnostic build
proved this setter path works, but realistic negative postgain values alone do
not reproduce the missing Windows loud-level nonlinearity.

## DAX3 wrapper notifications — corrected

The DAX3 outer wrapper subscribes to endpoint-volume notifications and forwards
notifications to the inner APO `IAudioProcessingObjectNotifications` interface.
RTTI recovery identifies the concrete secondary interface at object offset
`0x38`.

Exact notification vtables/handlers:

```text
VLLDP CComObject notification vtable RVA 0x1094E0
  registration +0x18 -> 0x18002B830
  HandleNotification +0x20 -> 0x18002CD10

VR CComObject notification vtable RVA 0x1D5B58
  registration +0x18 -> 0x1800ED480
  HandleNotification +0x20 -> 0x1800EE960
```

Both registration functions request notification type `2`, and both handlers
act only when `*notification_type == 2`. Type-1 endpoint-volume notifications
are ignored by the inner VLLDP/VR objects. Therefore the inner notification
interface is **not** a hidden master-volume modulation route.

## DAX3 wrapper SRC path

`CDolbyAPOWrapper::APOProcess` uses a direct inner-APO call when requested and
inner sample rates are equal. `LockForProcess` stores requested rate at wrapper
byte `+0x120` and inner rate at `+0x198`.

`GetInnerApoSampleRate` has a fast path for standard APO positions:

```text
APO_SFX / APO_MFX / APO_EFX / APO_OSFX / APO_OMFX
```

For those positions it returns the requested rate unchanged. The persistent
SP11 Dolby stages are standard APO effects in the 48-kHz graph, so the DAX3
wrapper's SRC path is not currently supported as the missing steady-state
nonlinearity.

## Exact disabled-scalar parity probes

The fresh VR constructor leaves some dormant values different from the live DAX
map (for example Bass Enhancer boost starts at 192 while the live map says 0).
This was tested rather than assumed harmless.

Original VR handlers were used to force the complete known disabled/zero block:

```text
audio optimizer enable       0
bass enhancer                 0
bass boost                    0
bass cutoff                 200
bass width                   16
bass extraction               0
bass extraction cutoff      200
calibration boost             0
height-filter mode            0
pregain                       0
volume modeler enable         0
volume modeler calibration    0
```

Dynamic, Movie and Music outputs remained **bit-identical** to baseline. Thus
these dormant constructor differences do not explain the parity residual.

## Digital Windows nonlinear evidence

The recovered May-18 controlled input/output pair uses the same VLLDP/VR/DAX3
DLL hashes as the later July/August system. After delay alignment and one global
gain fit, the current original-code chain reaches roughly 0.96 waveform
correlation with the Windows WASAPI loopback, with Movie slightly best.

The deliberately designed 75-Hz staircase exposes a missing loud-level effect.
At the two loudest steps Windows shows approximately:

```text
input -6 dBFS: fundamental -0.39 dBFS, H3 -34.2 dBc, H5 -44.2 dBc
input -3 dBFS: fundamental -0.37 dBFS, H3 -34.1 dBc, H5 -42.4 dBc
```

H2 remains around -77 to -79 dBc. The current VLLDP->VR chain stays several dB
lower and produces H3 only around -58..-61 dBc. The dominant odd-harmonic onset
near a repeatable ceiling looks more like a symmetric limiter/maximizer/soft
saturation regime than the decoded modern z/z^2/z^3 Virtual Bass synthesizer.

The loopback recorder receives WASAPI float and clamps only outside +/-1 before
writing 16-bit PCM. The observed repeatable ceiling is below +/-1, so this is
not explained by the recorder's int16 conversion clamp.

Simple post-clipping/tanh models improve the whole-file match only slightly;
the missing behavior is not just `clip(current_output)`.

## SurfaceAPO check

The generic SurfaceAPO binary is capable of real sample processing and receives
volume callbacks, but the exact SP11 REV_0D render JSON defines DEFAULT, RAW,
NOTIFICATION, COMMS, MOVIE and MEDIA paths whose render EQ nodes are disabled.
MEDIA/MOVIE/default coefficients are identity. Runtime override has not been
hardware-proved impossible, but the shipped SP11 media configuration does not
look like the missing maximizer.

## July Firefox/YouTube dump

The July WinDbg dump directly retains a Firefox -> internal-speaker audio-session
record and one genuine live VLLDP runtime page. Named compressor controls in
that page are:

```text
channel deviation   96
slow gain enable     1
slow mix            103
peak level            0
target power        -80
```

The first three are an exact Movie/Music-family signature, not Dynamic. The
retained page cannot distinguish Movie from Music because those profiles share
the relevant VLLDP state. The VR page containing other profile controls did not
survive.

## Current highest-value suspect

The remaining unexplained Windows ceiling should be investigated as a real
limiter/maximizer state problem, not by blindly enabling the named Virtual Bass
flag. The next target is the DAX/VLLDP multiband-compressor limiter path,
including the `mb_compressor_limiter_gain` control/name and the already decoded
frequency-aware VLLDP limiter whose threshold is near full scale.
