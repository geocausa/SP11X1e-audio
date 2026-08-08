# SurfaceAPO render modes / EFX closure — 2026-08-06

## Scope

This finding closes the remaining caveat that Microsoft `SurfaceAPO.dll` might
become an unmodeled sample-changing stage only in MEDIA, MOVIE, RAW, Spatial, or
other non-DEFAULT playback modes on the SP11.

Exact target binary:

- `SurfaceAPO.dll` version `1.216.42.0`
- SHA-256 `aa3a97e2cc7740ce3bd6b80b154354a023170d3ef29992978e36c179550a5206`
- exact binary is already preserved in `00-RE-archive/sp11-driverdump/...`

Evidence combines the board-specific `SurfaceAPO_0D.json`, historical ProcMon/ETW,
and the returned 2026-07-26 StackWalk scenario matrix.

## Board config contains only render-MFX EQ nodes

The raw `SurfaceAPO_0D.json` entity tree contains exactly these render MFX modes,
at both 48 kHz and 44.1 kHz:

- `R/MFX/DEFAULT/defaultEQ`
- `R/MFX/RAW/rawEQ`
- `R/MFX/NOTIFICATION/notifEQ`
- `R/MFX/COMMS/commEQ`
- `R/MFX/MOVIE/movieEQ`
- `R/MFX/MEDIA/mediaEQ`

There are no `R/EFX` entities in the board JSON.

For every one of the above MFX EQ nodes:

- `Enabled = false`
- `AllowBypass = false`
- `DependentBypassName = ""`
- `BypassTransitionTimeMs = 0`

DEFAULT, RAW, COMMS, MOVIE, and MEDIA coefficient arrays are unity/identity.
NOTIFICATION contains non-unity coefficients, but that node is also shipped
`Enabled=false` and is not relevant to ordinary media playback.

The dependent store contains only the runtime volume hooks.

## EFX proxy prerequisite is absent

The render EFX proxy requires endpoint property
`PKEY_SurfaceApoEfxProxyNames = {c1f75c4c-3243-11ea-850d-2e728ce88125},1`
to provide `R/EFX` pipeline entries.

Historical live ProcMon captured repeated queries of the active internal-speaker
endpoint for that exact key and returned `NAME NOT FOUND`.

Returned July registry snapshots likewise contain:

- `...,0` -> exact `SurfaceAPO_0D.json` path
- `...,7` -> MTE mode `0`

but no `...,1` EFX proxy-name value.

Thus the EFX handler can be allocated as framework machinery, but there is no
board pipeline entry connecting it to real-time render audio.

## Exact Biquad runtime dispatch

Ghidra resolves the SP11 Surface framework class
`SurfaceAudioFramework::AudioCore::BiquadEq<1>`.
Its processing vtable contains:

- slot `+0xB0` -> `FUN_1800D2100` (enable/bypass selector)
- slot `+0xB8` -> `FUN_1800A1220` (actual biquad DSP)
- slot `+0xC0` -> `FUN_1800D2190` (disabled direct-copy path)

`FUN_1800D2100` reads the block's live `Enabled` entity. If zero it calls the
`+0xC0` routine; if nonzero it calls `+0xB8`.

`FUN_1800D2190` is a direct input-to-output memcpy/copy for each connection and
copies the sample count.

`FUN_1800A1220` is the actual biquad sample loop, including filter state and
bypass-transition crossfade logic.

Therefore the board JSON's `Enabled=false` has an exact executable meaning:
the active Surface EQ object selects the direct-copy routine rather than the
sample-changing biquad routine.

## July live StackWalk validation

Returned July scenarios repeatedly sample the Biquad selector but never the
actual enabled biquad DSP region.

Observed frames in `FUN_1800D2100` selector range:

- Dolby UI bypass/shared: 3
- Enhancements off/shared: 3
- shared RAW: 4
- volume-step shared: 8
- Dolby active/shared: 5

Observed frames in actual enabled biquad routine `0xA1220..0xA16FF`:

- **0 in every scenario**

The volume-step shared scenario even samples `FUN_1800D2190`, the disabled-copy
routine, directly.

SurfaceAPO as a whole remains active as a shared MFX host in DEFAULT and RAW,
which explains its many generic `APOProcess`/framework stack frames. Those frames
do not imply that its EQ is enabled.

Exclusive playback has essentially no meaningful Surface real-time processing.

## Conclusion

For the SP11 REV_0D internal-speaker render endpoint, SurfaceAPO is not a missing
music voicing or psychoacoustic stage in DEFAULT, RAW, MEDIA, or MOVIE mode.

The remaining old caveat that MEDIA/Spatial might silently activate a Surface
render EQ/EFX path is unsupported and is now closed:

1. every board MFX EQ node is disabled;
2. the media-relevant coefficients are identity;
3. the EFX board pipeline/property prerequisite is absent;
4. live July stacks hit the disable selector and never the enabled biquad DSP;
5. a live stack samples the explicit disabled copy routine.

Do not add a Surface EQ or Surface EFX emulation to the Linux parity chain unless
new Windows evidence shows the endpoint property/config changes from this locked
REV_0D state.
