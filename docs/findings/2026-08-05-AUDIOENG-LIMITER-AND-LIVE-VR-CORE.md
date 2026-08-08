# AudioEng limiter + live Windows VR core recovery — 2026-08-05

## Executive result

Two independent source-of-truth gaps were closed in this pass:

1. Windows `AudioEng.dll` really instantiates a built-in `CAudioLimiter` in the
   captured `audiodg.exe` processing cluster. Its stereo limiter algorithm is
   now decoded and replayed offline. It is a real Windows stage, but applying
   it after the current VLLDP->VR output does **not** reproduce the missing
   loud 75-Hz H3/H5 behavior because the current Dolby output reaches that
   segment several dB below the limiter threshold.
2. The June-8 full `audiodg.exe` minidumps contain the actual live
   `DolbyApoVr` wrapper and core. The live core identifies the Windows profile
   as **Music**, matches all 34 stable scalar profile discriminators generated
   by the Linux replay, and directly proves Bass Enhancer / Bass Extraction are
   disabled inside the processing core itself.

This moves the parity problem upstream of AudioEng's final limiter and away
from a missing static VR profile knob. The remaining high-value target is live
runtime/history state or another upstream processing boundary that drives
Windows harder before the AudioEng limiter.

## Evidence identities

Exact Windows `AudioEng.dll` used for static RE:

```text
SHA-256
1e2cc764cae6ebfb6985d8503bb83a36022852fbbf1841c377c5ad2fa2d6795b
```

Microsoft-Windows-Audio ETW containing live AudioLimiter Start/Stop records:

```text
evidence/software_only_audio_state_20260610_232621/
  silent_audio_provider_only_events.csv

SHA-256
9622d267ea210ddaee9125bcc1f0bb4b887dd50974803729fde8fe23524e1e09
```

June `audiodg.exe` full process dumps:

```text
.../WINDOWS_LIVE_CAPTURE_20260608/
  02_process_memory_dumps_20260608_1742_audio_dolby_runtime/
    audiodg.exe_260608_174744.dmp
    audiodg.exe_260608_174832.dmp
```

Both minidumps retain normal ModuleList + Memory64 streams and the complete
VR wrapper/core pages used below.

## 1. AudioLimiter is a real AudioEng graph actor

The AudioLimiter CLSID is:

```text
{d69e0717-dd4b-4b25-997a-da813833b8ac}
```

It appears repeatedly in real `Microsoft-Windows-Audio` ETW Start/Stop events
inside `audiodg.exe`, in a coherent processing-component cluster that also
contains:

```text
Surface render MFX
  {34d30cd8-370e-4229-85be-3346c594c805}
Dolby DAX MFX
  {0ebd8606-17bb-4ae7-ad76-e86f99a425e9}
Adaptive Spatial Audio Renderer
  {5bbc2c71-dec2-4ba3-961a-36f37d1cc8a5}
AudioLimiter
  {d69e0717-dd4b-4b25-997a-da813833b8ac}
AudioFormatConvert
  {3fd7f233-a716-472e-8f2f-c25954f34e96}
```

Therefore `CAudioLimiter` is not merely class-registration metadata.

## 2. Exact CAudioLimiter state machine

The exact ARM64 `AudioEng.dll` code exposes a real look-ahead limiter.
Important recovered functions include:

```text
FUN_180045F30  limiter initialization
FUN_180045E30  look-ahead/rate-bucket setup
FUN_18000B500  stereo peak detector: max(abs(L), abs(R))
FUN_18000AD20  per-buffer delay/detector/gain pipeline
FUN_18000B200  target-gain attack/release state machine
```

Recovered constants / behavior:

```text
ceiling                0.9850000143 = about -0.131 dBFS
catastrophic guard     128.0
release constant       2.205 / sample_rate
48-kHz look-ahead      64 frames
```

Rate buckets:

```text
<= 16 kHz  -> 16 frames
<= 32 kHz  -> 32 frames
<= 64 kHz  -> 64 frames
>  64 kHz  -> 128 frames
```

For stereo, the detector computes the per-frame peak across both channels. On
an over-threshold peak, the target gain is `0.985 / peak`; the limiter ramps
down across the look-ahead interval, handles a more severe target arriving
mid-attack, and releases exponentially once the envelope has fallen far enough.
The delayed audio is multiplied by the common linked-channel gain.

This is a conventional linked stereo look-ahead limiter, **not** the decoded
Dolby z/z^2/z^3 harmonic virtual-bass synthesizer.

A tracked offline oracle now lives at:

```text
dolby-port/sp11_audioeng_limiter_oracle.py
```

It is intentionally analysis-only and is not part of production deployment.

## 3. Exact limiter replay does not close the 75-Hz residual

The current byte-identical installed VLLDP->VR production host was used to
produce Dynamic/Movie/Music candidates, then the decoded AudioEng limiter was
applied offline.

Representative whole-waveform result against the recovered May Windows WASAPI
loopback:

```text
Movie    raw ~0.9641752  -> limiter ~0.9641963
Music    raw ~0.9615454  -> limiter ~0.9615644
Dynamic  raw ~0.9628353  -> limiter ~0.9628410
```

The limiter engages lightly elsewhere in the 29.45-s stimulus, but at the two
critical loud 75-Hz steps the current Dolby candidate remains below threshold:

```text
Movie -6 dBFS step: fundamental about -3.20 dBFS, peak about 0.722
Movie -3 dBFS step: fundamental about -2.00 dBFS, peak about 0.825
```

Thus the exact AudioEng limiter does not alter those segments and cannot create
the missing Windows H3/H5 onset there.

Windows instead drives those same bass steps close to the limiter ceiling
(about -0.4 dBFS fundamental) before the strong odd-harmonic signature appears.
The unexplained gain/nonlinearity is therefore **upstream of the final AudioEng
limiter**.

## 4. Live June DolbyApoVr object recovered from audiodg memory

The June minidumps load the expected exact modules at stable bases, including:

```text
AudioEng.dll             0x00007FFD16300000
DolbyDax3Apo.dll         0x00007FFD16680000
SurfaceAPO.dll           0x00007FFD08E70000
DolbyAPOvlldp150.dll     0x00007FFD07D80000
DolbyApoVr.dll           0x00007FFD07A60000
```

The known primary `LibWrapperVr` vtable RVA is `0x1D8AE0`. Searching for the
relocated vtable pointer produces exactly one convincing runtime object in each
dump:

```text
LibWrapperVr object      0x000002453913C2F0
config pointer           0x000002453913C0F0
core pointer             0x00000245391DD808
block geometry           512 / 256
fill                      96 -> 160 across the two dumps
```

The changing fill proves the object is live processing state, not an inert
allocation.

## 5. Direct core proof: ordinary bass controls are OFF

Direct reads from the live Windows core at `0x245391DD808` give:

```text
core+0x0000 sample rate              48000
core+0x061C postgain                 0
core+0x0C90 bass-enhancer-enable     0
core+0x0C98 bass-enhancer-boost      0
core+0x0CA0 bass-enhancer-cutoff   200
core+0x0CA8 bass-enhancer-width      16
core+0x0D60 bass-extraction-enable   0
core+0x0D68 bass-extraction-cutoff 200
core+0x1278 dirty flag               0
```

This is stronger than XML, RPC, or DAX-service-map evidence: the actual
`audiodg` processing core itself has Bass Enhancer and Bass Extraction OFF.

## 6. Live VR profile is Music and Linux configuration matches it

Cold post-profile core snapshots were generated from the current original-code
Linux replay for all seven recovered profiles. Across the u32 offsets whose
values differ among profiles, the live Windows core scores:

```text
stable scalar profile discriminators
Music          34 / 34
Movie          28 / 34
Dynamic         4 / 34
others         lower
```

The larger discriminating set also ranks Music first. The IEQ/curve region
around `core+0x704` matches the Movie/Music family while additional scalar
fields distinguish **Music** specifically.

Therefore the static Music configuration in the Linux replay is not merely
plausible: it reproduces every stable scalar profile discriminator recovered
from this real Windows VR core.

## 7. Windows VR history evolves in compact regions

Comparing the two live cores 48 seconds apart changes 1203 of 6144 u32 words,
but those changes cluster into only eight regions. The largest are:

```text
core+0x3A98..0x4398   576 changing float words
core+0x43B8..0x4CB8   576 changing float words
```

Smaller dynamic regions include `+0x654..0x668`, `+0x990`, and a state block
around `+0x11D0..0x1270`.

These are strong candidates for analyzer/envelope/history/audio state rather
than profile configuration. This is now a better explanation class for the
remaining parity residual than an undiscovered static Music knob.

## 8. Two apparent configuration gaps were tested and eliminated

### Dormant `core+0xB48..0xC38` table

The live Windows core contains a stable frequency-like table in this region
that differs substantially from constructor defaults. Thirty-eight clearly
scalar/table words were copied exactly into a temporary Linux Music core while
all enable gates remained unchanged.

Result:

```text
short deterministic plugin hash: unchanged
full 29.45-s f32 SHA-256:         unchanged
output:                            bit-identical
```

Therefore this Windows-only table is dormant in the captured disabled state and
does not explain the missing bass behavior.

### VR InitLibrary property keys

`LibWrapperVr::InitLibrary` queries four endpoint `PROPERTYKEY`s under:

```text
{f112024a-fe30-42a8-80ab-8dd825a06f78}
PIDs 60, 61, 62, 63
```

The real SP11 Windows SOFTWARE hive stores all four as integer zero. The Linux
replay's zero/default construction therefore matches those constructor inputs;
there is no hidden media/Virtual-Bass enablement in these four keys.

## Revised highest-value target

The current evidence points away from:

- named Bass Enhancer / Bass Extraction / Virtual Bass being secretly enabled;
- the final AudioEng limiter being the sole missing actor;
- an omitted static Music scalar/profile setting;
- the stable `+0xB48..0xC38` tuning table;
- the four low-level InitLibrary endpoint keys.

The next target is the small set of **live runtime/state fields and history
regions that differ between a real Windows Music core and the exact Linux Music
core after processing**, especially the compact state around `core+0x120..0x13C`
and the dynamic state near `core+0x5E0`, before moving deeper into the large
history arrays.

## 9. Output-mode state discrepancy is real but dormant for Music

A stable structural mismatch around `core+0x120..0x13C` was traced to the
persistent VR `output-mode` complex setter (`VR_OUT_MODE_VA 0x180032F70`).
Applying that setter is solely responsible for changing the replay core from
the constructor-zero state to:

```text
core+0x120  2
core+0x128  1
core+0x130  1.0f
core+0x13C  1.0f
```

The live June Music core retains zeros in those locations. Blindly zeroing the
fields after configuration is invalid and can silence/crash processing because
it leaves the associated structure internally inconsistent.

The correct lifecycle test is to omit the output-mode setter entirely. For the
Music profile, doing so produces **bit-identical output** to the normal replay
for the full 29.45-second deterministic stimulus. Thus this structural
lifecycle difference is dormant for the captured Music case and does not
explain the missing loud-bass behavior.

Dynamic/Movie do change when this setter is omitted, so the conclusion is
profile-specific and must not be generalized beyond Music.

## 10. Live Windows VR history transplant does not restore the missing drive

The two largest regions changing between the live Windows VR snapshots are
float/history arrays:

```text
core+0x3A98..0x4398   576 float words
core+0x43B8..0x4CB8   576 float words
```

They contain finite small floats and no pointer-like qwords. These arrays were
copied exactly from the first live Windows Music core into an otherwise clean
Linux Music core before processing. A second test also copied the small live
float state at `+0x654..0x668` and `+0x990`.

Result:

```text
base Music RMS             0.3233544
Windows-large-history RMS  0.2936351
base waveform corr         ~0.960598
history-seeded corr        ~0.959223
```

At the loud 75-Hz steps the transplanted history makes the candidate slightly
**quieter**, not louder; H3/H5 remain around the same weak levels. The small
state transplant alone is bit-identical to baseline.

Therefore these recovered VR history arrays are acoustically active, but the
specific June snapshot does not explain the Windows oracle's extra 2--3 dB of
pre-limiter drive or its strong odd-harmonic onset. This significantly lowers
"missing VR warm history" as the primary explanation for that residual.

## 11. Live AudioEng limiter state recovered from the same June process

The exact limiter initialization signature was used instead of the earlier
incorrect absolute-detector-pointer heuristic. The unique live limiter state is
identified by:

```text
channels             2
look-ahead            64 frames
1/look-ahead          0.015625
sample rate           48000.0
release-up            exp(+2.205/48000)
release-down          exp(-2.205/48000)
ceiling/envelope      0.9850000143
```

At both captured instants the live limiter reports:

```text
current gain          1.0
attack frames left    0
release enabled       1
```

The stored target/attack-step values retain evidence of prior peak events, but
current gain is unity and the attack countdown is zero. Therefore the Windows
AudioEng limiter is fully instantiated yet **not actively attenuating at either
snapshot instant**. This is consistent with its role as a downstream safety
ceiling rather than the source of the normal Music loudness/bass character.

## 12. Full live VR outer allocation replays at original Windows addresses

The June `audiodg.exe` dump retains the complete contiguous `DolbyApoVr` outer
allocation:

```text
outer base          0x0000024539010000
outer size          0x003C0430 (3,933,232 bytes)
inner LibWrapperVr  outer+0x12C2F0 = 0x000002453913C2F0
embedded arena      outer+0x12C430, size 0x294000
live core           0x00000245391DD808
DLL runtime base    0x00007FFD07A60000
```

There are no minidump gaps across the outer allocation. A replay maps that
allocation at the exact Windows heap VA, loads the exact VR PE at its captured
ASLR base, patches only the already-established Windows runtime plumbing, and
calls the original `LibWrapperVr` process function.

With a continuous-phase 997-Hz stereo tone, the captured Windows Music state
and a fresh reconstructed Music state settle to **different** long-term levels:

```text
                         RMS after ~22 s     peak
fresh reconstructed      ~0.15354            ~0.21544
captured Windows Music    ~0.12005            ~0.16843
```

This difference survives thousands of blocks and is therefore not FIFO phase,
startup ring-out, or a short warm-history transient. Static Music profile
scalars already match the live core 34/34, so a separate VR lifecycle/state
component remains.

## 13. Exact-address hybrid localization

Because fresh and captured allocations can occupy the same heap VA and use the
same DLL base, captured chunks can be transplanted into a fresh deterministic
object without pointer-relocation ambiguity.

At 4096 continuous-phase blocks:

```text
fresh object                         ~0.153542 RMS
captured core only (0x6000 bytes)    ~0.125866 RMS
captured inner wrapper only          ~0.153653 RMS
captured inner + core                ~0.125889 RMS
captured entire embedded arena       ~0.120015 RMS
captured complete outer allocation   ~0.120047 RMS
```

Thus most of the persistent difference is inside the VR core, with a smaller
additional contribution from another arena subobject. DAX outer wrapper/FIFO
state is not the cause.

A 0x20000 arena scan localizes the primary contribution to:

```text
outer+0x1CC430 .. outer+0x1EC430
```

which contains the live core. Pairing that captured core window with each
other arena window shows only one dependent region matters:

```text
outer+0x1EC430 .. outer+0x20C430
```

Further subdivision localizes the dependency to one 8-KiB region:

```text
outer+0x1F0430 .. outer+0x1F2430
```

and then to two 1-KiB blocks with opposing effects when paired with the live
core:

```text
outer+0x1F0830 .. 0x1F0C30  -> raises hybrid toward fresh (~0.15409 RMS)
outer+0x1F1430 .. 0x1F1830  -> lowers hybrid to captured (~0.11990 RMS)
```

The dominant downward block is at absolute Windows address approximately:

```text
0x0000024539201430
```

or `core + 0x23C28`. The opposing block begins at `core + 0x23028`.

This is the current highest-value VR target: identify the object/fields in the
`core+0x23C28` dependent block and the core state that makes it active. This is
a concrete localized lifecycle/state discrepancy, not evidence that the named
Virtual Bass switch is enabled.
