# SP11 native Windows Dolby chain — live Linux completion checkpoint (2026-08-05)

## Result

The two Dolby stages proven hardware-hot in the Windows SP11 render graph now run
natively on ARM64 Ubuntu as one persistent Linux LADSPA processor:

```text
Linux/PipeWire stereo stream
  -> original DolbyAPOvlldp150.dll
       Windows outer scheduler / state machine (432-domain)
       -> original 256-frame VLLDP accumulator/core/orchestrator
  -> original DolbyApoVr.dll
       original outer APO transition wrapper
       -> LibWrapperVr / dap_vr_state_s
  -> physical SP11 speaker sink
```

The production processor is not a fitted EQ or a mathematical reimplementation.
It maps and executes the shipped ARM64 PE DSP code. Linux shims only the small
Windows runtime / allocation / resource / logging surface needed to host it.

The proven Windows per-cycle order from the 2026-08-04 KDNET session remains:

```text
DolbyDax3Apo -> DolbyAPOvlldp150 -> DolbyApoVr
```

## Binaries used

```text
DolbyAPOvlldp150.dll
SHA256 a2553ff7b013b5a248e50bdcae46d08405e393c0085073975214d035cedf02c1

DolbyAPOVR.dll
SHA256 1d74477ea0dae66961a21bf6bc3ce0d8062836fc4dd96b59c14de11257f5eecc
```

The live user bundle is intentionally private/local at:

```text
~/.local/lib/sp11-dolby/
```

Do not publish the proprietary DLLs into Git.

## VLLDP wrapper/core reconstruction

Windows block domains are now executable as written by Dolby:

```text
480-frame Windows host callback
  -> outer scheduler limit 432, RVA 0x0ED348
  -> inner rate adapter / accumulator, RVA 0x033640
       persistent fill inner+0x20
       fixed block inner+0x3c = 256
  -> descriptor shim
  -> orchestrator FUN_18001f7a8
```

The full original 432 -> 256 path is host-buffer invariant in Linux. Tests with
1, 64, 127/353, 480, 1024 and chaotic mixed host chunks produced bit-identical
results for the same persistent initial state.

## VLLDP Dynamic profile: live-state byte match

The Dynamic-family SP11 `tuning-vlldp` values were applied through the original
VLLDP150 setters rather than by editing state bytes.

Against the fresh 2026-08-04 Windows orchestrator state snapshot, all checked
static profile regions now match byte-for-byte:

```text
regulator high thresholds : 0 differing bytes / 80
regulator low thresholds  : 0 / 80
isolated-band flags        : 0 / 80
stress                     : 0 / 32
audio-optimizer region     : 0 / 160
compressor/PID17 region    : 0 / 96
stress/PID22 checked region: 0 / 68
sliding-bass/PID31 region  : 0 / 20
```

Also byte-matched at their known scalar locations:

```text
audio optimizer enable
regulator distortion slope
regulator overdrive
regulator timbre
regulator speaker-distortion enable
system gain
peak level
target power
noise-gate enable
noise-gate threshold
```

This corrects the earlier Linux prototype, which had initialized several direct
controls but had left the VLLDP regulator/optimizer tables at constructor
defaults.

## VR reconstruction

The supposed external/missing VR processor was disproven. RTTI and live-vtable
matching identify the in-DLL class chain:

```text
IAudioProcessor -> LibWrapperBase -> LibWrapperDap2 -> LibWrapperVr
```

The real `LibWrapperVr` constructor/initializer now succeeds on Linux and builds
`dap_vr_state_s`. The original outer APO object is also constructed, and its
live deep-path callback is executed rather than bypassed.

Important addresses in this build include:

```text
LibWrapperVr ctor             0x1800DB270
LibWrapperVr InitLibrary      0x1800DBE30
LibWrapperVr process          0x1800F65E0
live outer callback RVA       0x001D10C8
live callback thunk RVA       0x001D1220
LibWrapperVr vtable           0x1801D8AE0
```

The original VR processing callback performs zero heap allocations, frees or
reallocations during audio processing in the tested path.

## Dynamic CP configuration

The 39-property VR dispatch table was recovered from the DLL. Exact scalar
Dynamic-profile values are applied through the original handlers. Structured
settings use the lower-level Dolby calls reached by the same configuration
path, avoiding only Microsoft C++ diagnostic/string plumbing.

Applied Dynamic CP families include:

- volume leveler: enabled, amount 5, DRC enabled, in/out target -320;
- dialog enhancer: enabled, amount 5;
- IEQ: enabled, amount 10 plus the 20 exact center/target pairs;
- regulator scalar controls and 20-band CP tables;
- MI steering controls;
- surround decoder / boost;
- speaker virtualizer angles;
- VolMax boost;
- output mode 11 with the exact 8x2 Q14 matrix.

A first attempt to reuse the high-level runtime-config wrapper crashed in its
Microsoft C++ logging/string path, not DSP. Directly calling the lower-level
Dolby setters removed that host-runtime dependency; output-mode, IEQ and
regulator configuration then passed separately and together.

## Stress / determinism

The combined original-code VLLDP -> VR LADSPA chain passed:

```text
1,000,000 frames
0 non-finite samples
64-frame chunks      bit-identical
480-frame chunks     bit-identical
1024-frame chunks    bit-identical
127/353 alternation  bit-identical
mixed chunks         bit-identical
```

The production local bundle passed the same million-frame test after descriptor
and diagnostic cleanup.

## Live PipeWire integration

The main PipeWire service uses:

```text
MemoryDenyWriteExecute=yes
NoNewPrivileges=yes
```

The PE bridge legitimately needs an executable relocated image mapping, so a
first attempt to host Dolby directly in the main PipeWire process was rejected
with `Permission denied` at LADSPA instantiation. The daemon did not crash.

The final design keeps the main daemon's W^X policy intact. Dolby runs in the
separate stock `filter-chain.service`, with a user override that changes only:

```text
MemoryDenyWriteExecute=no
```

Other sandboxing remains, including `NoNewPrivileges=yes` and
`RestrictNamespaces=yes`.

The transparent pre-existing boundary remains available as a fallback:

```text
effect_input.sp11_dolby_bypass
```

The production Dolby sink is:

```text
effect_input.sp11_windows_dolby
SP11 Windows Dolby (Dynamic)
```

The Dolby sink is now the configured/default output. It was deliberately left
at volume 0.10 after validation; the bypass retains its previous 0.18 volume.

## Live proof

A calibrated low-level stereo tone was run through both the transparent bypass
and the native Dolby sink at the same PipeWire volume. Output was tapped from
the corresponding filter output nodes.

```text
bypass RMS  : -114.7465 dBFS
Dolby RMS   : -104.3662 dBFS
Delta       : +10.3803 dB
```

Both captures contained zero NaN/Inf samples. This proves the live path was
processing, not silently passing through.

A subsequent ~30-second multi-level known-input stress stream, explicitly sent
through the Dolby sink at conservative volume, completed with:

```text
main pipewire PID unchanged
filter-chain PID unchanged
no PipeWire/filter-chain error
no xrun/underrun/overrun report
no coredump
filter host roughly <1% average CPU in ps sampling
RSS about 28.5 MB while active
```

Finally, an untargeted `pw-play` stream was observed linked to:

```text
SP11 Windows Dolby (Dynamic):playback_FL/FR
```

and a simultaneous tap of the Dolby output contained finite non-zero audio.
This verifies normal default-device routing, not only explicit test routing.

## Profile provenance correction

Do not use the old 2026-05-18 SoundPlayer loopback as an exact Dynamic oracle.
It remains useful as a transfer fingerprint, but profile selection for that run
was not proven Dynamic.

The Microsoft operator file says automatic application profile switching is
disabled by default; Movie is the spatial default and Music the non-spatial
default. The browser->Dynamic mapping table exists but is not automatically
active by default.

The 2026-08-04 live VLLDP state nevertheless rules out Movie, Music and Off:
its full VLLDP tuning subtree matches the Dynamic/Game/Voice/Personalize family.
June DAX RPC captures separately prove `active_profile=5` means Dynamic.

The deployed Linux endpoint is therefore deliberately labelled **Dynamic**, not
claimed to be a universal reconstruction of every Windows UI profile.

## User-level fallback

Installed helper:

```text
~/.local/bin/sp11-dolby status
~/.local/bin/sp11-dolby on
~/.local/bin/sp11-dolby off
~/.local/bin/sp11-dolby restart
```

`off` merely selects the preserved transparent bypass; it does not delete the
Dolby host. Both the PipeWire and filter-chain services are enabled for the user
session, so the configuration is persistent across normal restarts/logins.

## Remaining scope

The persistent Windows-hot speaker chain is live on Linux and the seven static
SP11 speaker profile families (Dynamic, Movie, Music, Game, Voice, Online
Course, Personalize) are selectable through the original Windows DSP code. The
profile implementation and live-switch validation are recorded in
`2026-08-05-DOLBY-NATIVE-PROFILES.md`. Remaining work is parity validation and
optional feature completion rather than a blocker for the speaker path:

1. obtain or recover a purpose-built, proven-profile Windows loopback using the
   same deterministic stimulus for final waveform/transfer-function parity;
2. bind the still-unproven Movie/Music `speaker-peq-enable` and partial
   virtualizer-enable controls only if evidence shows they materially alter this
   SP11 speaker path;
3. add the user-custom GEQ layer for the three Personalize slots if desired;
4. continue investigating modern ASAR/AIDE only for modes where hardware traps
   prove those stages execute; do not assume them into the steady-state speaker
   chain without new live evidence.
