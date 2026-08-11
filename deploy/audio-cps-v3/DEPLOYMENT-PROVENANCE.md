# SP11 Audio CPS V3 deployment provenance

Built and staged on 2026-08-11 for a one-shot runtime test. This candidate is
isolated from the accepted `7.1.5-sp11-audio-clean+` installation by its kernel
release, module directory, initramfs, DTB, `/boot` bundle, and GRUB entry ID.

## Identity

- Kernel release: `7.1.5-sp11-cps-v3+`
- Kernel source branch: `agent/cps-windows-parity-v2-20260811`
- Kernel transport base commit: `826091400f088c1df0709f78b1d7e2b2d8d1fea7`
- Kernel runtime-fix commit: `4c5c85f` (published as patch `0041`)
- Kernel live-observer commit: `223f3f1` (published as patch `0042`)
- Kernel Wi-Fi DT commit: `11d875d` (published as patch `0044`)
- Source worktree: `02-kernel/sp11-audio-powerlab-src-20260810`
- Build directory: `02-kernel/build-cps-v3-20260811`
- GRUB entry ID: `sp11-audio-cps-v3`
- Persistent GRUB default: `sp11-audio-cps-v3` (changed from Windows at the
  operator's explicit request after the accepted runtime boots)

The build config differs from the accepted audio-clean config only in
`CONFIG_LOCALVERSION`, changed from `-sp11-audio-clean` to `-sp11-cps-v3`.

## Artifact SHA-256

- Raw kernel `Image`:
  `164bc92d88c724ac4e7872212405c8149cd52a8ee1553d54e11d56581751fc48`
- Audio-only OLED/CPS base DTB:
  `341dfb733ef07d9b1bee02ee016e120fd6fb50a34762aaed4dac5131142953a6`
- Phase91/Wi-Fi platform overlay source:
  `b2b065caf2301b7b67daaa82718743d1fc73ab117c6399a9e9bb73e135ca8a70`
- Compiled Phase91/Wi-Fi platform overlay:
  `1f333a13c0d59d1f0ca1de31ae75534c350adab3f7caa03d5aec4f45265f44ab`
- Final composite OLED/Phase91/Wi-Fi/Bluetooth/CPS DTB:
  `7cd5fdd8ef59c46ca9a3661adacce0444893a6c26fca71c97eaa3070a88aab84`
- Pre-Bluetooth-address composite DTB retained as rollback:
  `126911f321badad1b33c8bad50ad460b4f6e03f8f9851ce4b532988bed1e2241`
- Fresh kernel-built OLED/CPS base DTB with baked-in Wi-Fi property:
  `d10b36c73c3d1653bf5cb5ff1ac05c6763d0e0e3b3c510d136e389894d07bcbb`
- Current live-observer initramfs:
  `8504076a0f40926eee09233451b078d32da1fbb72bb52d4556bec528bd6e6153`
- Force-on initramfs retained as pre-live-observer rollback:
  `4a48cc2aea277954e3712480b31a15316c928659c90c54dce0650d63664a9928`
- OF-property-fix initramfs retained as pre-force-on rollback:
  `265f4ba7897d663108b616bdfdf28a39f41eedf0040f6c818adf6e6e0c51a177`
- Initial initramfs retained as pre-OF-fix evidence:
  `40b842d26c5857cfc3228b14eaab5b940c1d585e3d2d9f47cdfe75d368d7f7f6`
- Kernel config:
  `f2e5113d24b20b915035cdbc57e91534c40b8a1dce64427b69d114c2d9d30543`
- System.map:
  `b8975af9f2f33b4d798d5133fd42ac95f9bac5c4c16af3300dbbe4ecce7a257d`
- Reviewed CPS topology:
  `f385a5d83127cf8f83dab0cbc86f418514f9c8839f2da6aac97e3e2ee782d121`
- Current live-observer signed `snd-soc-wsa884x`:
  `ccc9a4d1a3e0cc34e4761a0b4ddaebbbc152b822bb4f579dd512374e9fe4251e`
- Force-on `snd-soc-wsa884x` retained as pre-live-observer rollback:
  `039588e2b0033057bfa8b95af408c7504b29ec8ea4fcfc9925a664356449917f`
- OF-property-fix `snd-soc-wsa884x` retained as pre-force-on rollback:
  `04c850bc3e6917472b02ea1a244e17329569db93fd53249f6864d7bb209610e5`
- Initial `snd-soc-wsa884x` retained as pre-OF-fix evidence:
  `89c4df0860848e970a27d79c052184f1046ea326527a281cb232487d81e6e111`

## Offline gates passed

- Full kernel, modules, and DTBs built successfully with one ABI.
- Staged tree: 7,886 modules and zero missing symbols or CRC mismatches across
  327,478 versioned imports.
- Extracted initramfs: 2,910 modules and zero missing symbols or CRC mismatches
  across 128,312 versioned imports.
- The former mixed-ABI failure symbols `sdw_nread_no_pm` and
  `sdw_nwrite_no_pm` match the new `Module.symvers` ledger.
- The initramfs contains only the `7.1.5-sp11-cps-v3+` module release.
- Embedded Phase91 touch modules and CPS topology match the installed files
  byte-for-byte.
- The final DTB contains the accepted Phase91 GPI/SPI/touch nodes and Wi-Fi
  RF-kill bypass together with the reviewed per-speaker CPS offsets and link.
- Corrected extracted initramfs: 2,910 modules and zero missing symbols or CRC
  mismatches across 128,311 versioned imports.
- Force-on extracted initramfs: 2,910 modules and zero missing symbols or CRC
  mismatches across 128,311 versioned imports. Its embedded WSA884x module is
  byte-for-byte identical to the installed signed module.
- CPS review suite: 99 tests, 96 passed and 3 skipped only because the private
  Windows capture bundle is not present.

## Mandatory Wi-Fi bake gate

Wi-Fi continuity is a deployment prerequisite because losing the network also
cuts off the development session. Every fresh kernel source must include both:

1. patch `0043-wifi-ath12k-honor-DT-disable-rfkill.patch`, which makes
   ath12k honor the OF boolean; and
2. patch `0044-arm64-dts-qcom-disable-broken-SP11-WiFi-rfkill.patch`, which
   places that boolean on the SP11 WCN7850 node.

The platform overlay deliberately repeats the DT property, so a composite DTB
also retains it when the audio-only base is used. Before installation, run:

```sh
./tools/verify_sp11_kernel_bake.py /path/to/linux-source \
  --dtb /path/to/final-sp11.dtb
```

The gate rejects a missing driver hook, missing board property or final DTB
without `/soc@0/pci@1c08000/pcie@0/wifi@0/disable-rfkill`. A DT-only fix is
not sufficient: the ath12k driver half is equally mandatory.

## Included platform paths

- Phase91 touchscreen GPI/GENI overrides
- WCN7850 ath12k Wi-Fi and Qualcomm Bluetooth
- MSM/OLED display path
- Complete Qualcomm SoundWire, WSA884x, AudioReach, PBR/VI/CPS path
- Reviewed Windows-derived two-speaker CPS topology

MAX34417 is deliberately not force-loaded. No speaker-rail device responded in
the earlier hardware scan, and this full config has no `max34417` module. It is
an optional lab observer, not a dependency of the CPS audio candidate.

## Runtime corrections

The first boot proved the full kernel/module ABI but exposed that the audio-only
CPS DTB omitted the accepted Phase91 touchscreen and Wi-Fi nodes. The final DTB
is now built by applying `sp11-cps-v3-platform.dtso` to that reviewed CPS base.
The second boot confirmed Wi-Fi connected, Phase91 touch bound and initialized,
and OLED remained connected.

The second boot also proved that the CPS properties were present on both live
codec OF nodes but were missed by `device_property_read_*()`: SoundWire supplies
the slave device with a different primary firmware node. WSA884x now reads the
CPS boolean and offsets from `dev->of_node`, matching the driver's existing
`qcom,port-mapping` lookup. The targeted module was rebuilt, signed, compressed,
and passed the full 7,886-module CRC closure gate before initramfs replacement.

The third boot confirmed both codec OF properties and per-speaker offsets, but
also exposed a stale `/var/lib/alsa/asound.state`: both CPS mixer switches were
stored as `false`, so `alsa-restore` disabled port 6 after probe. Enabling those
two controls in the live boot made a silent PCM stream remain open and produced
successful left/right SoundWire selections for speaker playback, VI feedback,
and CPS feedback. The graph reported that the SP/SPVI stage was enabled with
VI+CPS feedback accepted. WSA884x now treats DT-enabled CPS as fixed board
wiring and does not permit a stale userspace control restore to disable it. The
replacement module and initramfs passed the full installed and extracted CRC
closure gates.

The next boot loaded the bounded live-observer module with srcversion
`E084BC31719EE85BB8DEABD`. Its default-zero parameter leaves normal playback
unchanged. A deliberately armed 12-sample playback captured 24/24 successful
register sets across the two amplifiers: both PAs active, no fault or interrupt,
distinct changing raw ADC/temperature/VBAT words, and `CURRENT_LIMIT=0x44` on
both sides. That register proves the recovered PBR-enabled 2-cell policy was
live. `CPS_CTL` remained zero while CPS DP6/DSP transport was accepted. Later
static review found no required Windows or Linux HLOS write to that register,
so zero is not treated as a deployment gap. See
`docs/findings/2026-08-11-linux-cps-v3-live-wsa-observation.md`.

That boot also exposed a pre-existing Bluetooth service race. The userspace
workaround ran before the WCN7850's second firmware setup, saw Qualcomm's
temporary `00:00:00:00:5A:AD` sentinel in the configured list, and exited. A
manual daemon/fix/daemon sequence recovered the established public address
`02:99:3A:42:EA:30` and powered the controller normally. The clean documented
fix is now in the composite DTB as little-endian
`local-bd-address = [30 ea 42 3a 99 02]`. Offline decompilation proved this is
the only difference from the accepted composite DTB. The pre-address DTB is
retained beside it for rollback; the next boot must confirm that the existing
userspace service becomes a no-op before it is disabled.

Boot ID `1a37e1b4-93b7-4239-8aee-5e048119bbba` then confirmed the composite
DTB again with Wi-Fi connected to `GEOCA`, no soft or hard RF-kill, both CPS
switches on, left/right offsets 0/25, and the 48/8/24 kHz render/VI/CPS graph
accepted. Bluetooth remains outside the audio acceptance gate at the
operator's request; its existing workaround is left untouched.

A direct `pw-play /usr/share/sounds/alsa/Front_Center.wav` at the existing 11%
default-sink volume returned zero at 23:21 BST on the same boot. Render, VI and
CPS selected their 48/8/24 kHz ports on both amplifiers; SP/SPVI, endpoint
calibration, VI+CPS enable and `GRAPH_START` were accepted with no XRUN, PA
fault, SoundWire bus clash or DAI2-selection failure.

A second bounded playback at 23:26 BST temporarily raised the Windows-Dolby
default sink from 0.11 to 0.30, returned zero, and restored 0.11 afterward.
All 24 observer reads succeeded; both PAs remained active; all fault and
interrupt bytes stayed zero; ADC/VBAT words changed; and `CURRENT_LIMIT=0x44`
remained live on both amplifiers. The same 48/8/24 kHz render/VI/CPS graph and
DP6 mask `0x03` on both speakers were accepted without XRUN, PA fault or bus
clash. A Ghidra scan of the shipped `qcaucd8380.sys` found no decoded scalar
for WSA8845 `CPS_CTL` address `0x3468`, and every reviewed Linux WSA884x source
copy only defines its reset default `0x00`. The zero live value is therefore
not treated as a missing Linux write or deployment blocker; its undocumented
semantics and actual limiter intervention remain evidence questions.

## Rollback model

The operator made `sp11-audio-cps-v3` the persistent GRUB default after its
runtime acceptance. Windows and the accepted Linux audio-clean entry remain
available as independent menu entries. The observer is default-off and its
pre-observer module and initramfs are retained in the V3 boot bundle.
