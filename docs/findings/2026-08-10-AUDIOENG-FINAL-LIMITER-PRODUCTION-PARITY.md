# Windows AudioEng final limiter — exact production parity

Date: 2026-08-10

## Result

The last non-transparent mature stage missing from the Linux stereo production chain was the
Microsoft Audio Engine `CAudioLimiter` that follows the Dolby MFX/VLLDP edge in the frozen Windows
graph.  The production bridge now contains a streaming 48-kHz stereo implementation translated
from the exact Microsoft `AudioEng.dll` implementation and independently checked bit-for-bit against
the existing Python oracle.

No proprietary Microsoft or Dolby binary is committed by this change.

## Windows graph position

The state-pinned Aug-8 full-memory graph reconstruction proves the downstream order:

```text
Dolby DAX MFX / VLLDP  h11 -> h12
Surface render MFX     h12 -> h13
AudioLimiter           h13 -> h14
AudioFormatConvert     h14 -> h15
```

For the low-level 75-Hz and 997-Hz oracle blocks, Surface render MFX and AudioLimiter are transparent
because the signal does not cross the limiter threshold.  Older loud-level Windows captures show
that `CAudioLimiter` becomes causal near full scale and creates the characteristic Windows endpoint
ceiling.

## Exact Microsoft implementation

Binary analyzed privately:

```text
Windows/System32/AudioEng.dll
PDB: AUDIOENG.pdb
CodeView GUID: {7BE86E96-435E-7B7E-55CA-5EE93ED4FFA0}, age 1
```

Private Microsoft symbols identify:

```text
CAudioLimiter::APOProcess                       0x18000AD20
CAudioLimiter::ComputeMaxFrameValsInBuffer      0x18000B190
CAudioLimiter::ProcessLimiterBuffer             0x18000B200
CAudioLimiter::ComputeMaxFrameValsInBuffer_Stereo_NEON 0x18000B500
CAudioLimiter::AllocateDelayLine                0x180045E30
CAudioLimiter::Initialize                       0x180045F30
```

Recovered constants and 48-kHz contract:

```text
ceiling                   0.9850000143051147
catastrophic-input guard  128.0
look-ahead @ 48 kHz       64 frames
rate buckets              16 / 32 / 64 / 128 frames
bucket thresholds         16 / 32 / 64 kHz
release constant          2.205 / sample_rate
release multipliers       exp(+/-2.205 / 48000)
```

The limiter is not a clipper.  It is a linked-channel look-ahead gain envelope: it detects the
maximum absolute sample across channels per frame, delays the audio, ramps gain down before a peak
arrives, and releases exponentially afterwards.

## Production implementation

Added:

```text
dolby-port/sp11_audioeng_limiter.h
```

The SP11 production endpoint is fixed stereo / 48 kHz, so the implementation keeps the exact
64-frame Windows delay line directly inside `ChainInst`.  There are no allocations, filesystem
operations, locks, or system calls in the audio callback.  Limiter state survives PipeWire
activate/idle transitions along with the existing VR/VLLDP long-memory state.

The stage is applied after original-Dolby VR -> original-Dolby VLLDP and before LADSPA output,
matching the frozen Windows graph position.

## Independent C <-> Python parity

The pre-existing independent analysis oracle remains:

```text
dolby-port/sp11_audioeng_limiter_oracle.py
```

A permanent C harness and parity test were added:

```text
dolby-port/sp11_audioeng_limiter_cli.c
tools/dolby/test_audioeng_limiter_c_parity.py
make -C dolby-port windows-audioeng-limiter-check
```

The test stimulus contains quiet program material, asymmetric stereo, deterministic noise and two
heavy over-ceiling regions.  Across 144,000 input stereo frames plus the exact 64-frame flush:

```text
C vs Python exact floats  288128 / 288128
max difference            0
RMSE                      0
SHA-256 both              1a95080c0473c8dcdce63f6f3215bb3f785a278f3b6db827e4acce5f1d03ec8a
```

The result is identical for host chunk patterns:

```text
1
64
480
1024
127,353
31,257,509,17,1024,3,480,65
```

## Production regression gates

With the limiter integrated:

```text
windows-chain-profile-lifecycle-check   PASS
windows-chain-postgain-control-check    PASS
analyseplugin                            PASS
```

The actual production LADSPA build was then run for 1,000,000 frames with all established host
chunk patterns.  Every result was bit-identical to the one-frame reference:

```text
64        diff=0
480       diff=0
1024      diff=0
127/353   diff=0
mixed     diff=0
PLUGIN_RESULT PASS
```

Offline candidate build SHA-256 at this checkpoint:

```text
ef4d995216b3ba5ae55189a7d5032a402968e308f18e2f780959788e21179d31
```

Compiled artifacts remain outside Git.

## Historical Windows known-input A/B

The preserved May-18 deterministic Windows known-input capture was re-rendered with the same
historical state used for the previous production closure:

```text
profile   Movie
postgain  -385
```

Using the same alignment/scoring method:

```text
previous deployed build, without AudioEng limiter
  lag                33664 samples
  fitted gain        1.007924546
  correlation        0.999652987
  residual SNR       31.58699 dB
  peak               0.999900222

new candidate, exact AudioEng limiter
  lag                33600 samples
  fitted gain        1.010267317
  correlation        0.999689694
  residual SNR       32.07248 dB
  peak               0.985905945

preserved Windows peak
  0.985870361
```

The 64-sample lag change is exactly the recovered 48-kHz limiter look-ahead.  The candidate both
improves waveform correlation/residual SNR and moves the endpoint peak from the VLLDP ~0.9999 ceiling
to essentially the exact Windows AudioEng ~0.9859 ceiling.

This makes the limiter a supported production-parity stage rather than a cosmetic safety clamp.

## Live deployment

The exact limiter build was deployed to the user-space SP11 PipeWire Dolby bundle only after the
native `wpctl` Audio/Streams section confirmed zero active playback streams.  No playback/test sound
was opened for deployment validation.

Rollback snapshot:

```text
~/.local/state/sp11-dolby/backups/20260810T031912+0100-audioeng-limiter/
```

The snapshot contains the prior plugin, PipeWire config, helper, volume-sync executable/service and
shared control page.  The prior plugin SHA-256 is:

```text
f8c21ddc66af748310caef86f5087f3c991c6de5899cab571906d77375fe9b3b
```

Installed production plugin:

```text
~/.local/lib/sp11-dolby/sp11_dolby_windows_chain.so
SHA-256 ef4d995216b3ba5ae55189a7d5032a402968e308f18e2f780959788e21179d31
inode   11059250
```

`filter-chain.service` PID 35898 was verified to map that exact inode after restart.
Post-deployment health:

```text
filter-chain.service             active / enabled
sp11-dolby-volume-sync.service  active / enabled
pipewire.service                 active
pipewire-pulse.service           active
wireplumber.service              active
profile                          dynamic
sink UI volume                   0.06
VLLDP postgain request           -1150 (-71.875 dB)
VLLDP postgain ack               -1150
Audio/Streams                    empty
```

The endpoint volume and control-page request/ack survived the restart unchanged.  The hardened
volume-sync service produced no transient unity/postgain-0 event during this deployment.
