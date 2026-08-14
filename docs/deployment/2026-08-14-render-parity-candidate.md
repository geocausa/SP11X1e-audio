# SP11 consolidated Windows render-parity candidate

Date: 2026-08-14 (Europe/London)

## Preboot outcome

A complete, isolated kernel candidate is built and staged as GRUB entry
`sp11-audio-render-parity`.  It has not been armed or booted.  The pre-existing
GRUB state was preserved exactly:

```text
saved_entry=sp11-audio-cps-v3
next_entry=
```

The currently running kernel therefore remains `7.1.5-sp11-softpause+`; this
document records preboot evidence, not a runtime-success claim.

## Candidate identity

| Item | Value |
|---|---|
| Kernel release | `7.1.5-sp11-render-parity+` |
| GRUB id | `sp11-audio-render-parity` |
| Boot bundle | `/boot/sp11-7.1.5-audio-render-parity` |
| Module tree | `/lib/modules/7.1.5-sp11-render-parity+` |
| Sound-card/topology model | `X1E80100-Microsoft-Surface-Pro-11-Render-Parity` |
| Kernel baseline | `f165410554d95ddd6af3c1eedc16c9703b2ce71f` |

## Consolidated functional delta

The build carries the already-tested protected CPS/VI speaker path and the
working platform closure, then consolidates the remaining evidence-backed
render changes:

1. exact Windows final `VOL_CTRL` followed by the four-frame GainStep
   transaction (`0048` + TLV capacity correction `0049`);
2. exact Windows SOFT_PAUSE/release DSP commands (`0045`);
3. removal of ordinary runtime-suspend `regcache_mark_dirty()` in WSA884x
   (`0046`), while retaining cache-only write tracking and the full restore
   after a real SoundWire UNATTACHED/ATTACHED context-loss event; and
4. the reviewed four-link DEFAULT topology, including
   `POPLESS_EQ 0x4664 <-> VOL_CTRL 0x4663` with intent `0x08001118`, selected
   through the unique Render-Parity sound-card model (`0050`).

This candidate deliberately does not guess-enable MAX34417, PBR DP4,
WaveSpeaker EQ/DRC/Bass Boost or a NOTIFICATION selector.  The available
evidence does not justify those changes for ordinary DEFAULT playback.

## Platform-integrity closure

The generic OLED DTB did not contain the external touchscreen node, so it was
not deployed directly.  The preserved Phase91 overlay (SHA-256
`14a491b170b4d4f85ed16f931b42554f4e479265eeee9d3a4dc50eea2c8083dc`)
was applied to the new DTB.  The resulting combined DTB verifies:

- sound model `X1E80100-Microsoft-Surface-Pro-11-Render-Parity`;
- `microsoft,mshw0485` at QSPI10, 40 MHz, interrupt cells `0x33 0x08`;
- QSPI and GPI DMA enabled; and
- the conflicting I2C10 node disabled.

The Phase91 `gpi`, `spi-geni-qcom` and `mshw0485_touch` modules were rebuilt
from source against the new release, stripped, signed with the same new build
key as the in-tree modules, and installed under `updates/sp11-phase91` so they
override the generic in-tree transport modules.  `modinfo -k` resolves all
three from that directory.

The first module installation accidentally retained debug sections and made
the tree 2.4 GB and the initramfs 1.03 GB.  This was caught before GRUB
publication.  The modules were reinstalled with `INSTALL_MOD_STRIP=1`, and the
external trio was clean-rebuilt, stripped, re-signed and recompressed.  Final
sizes are 151,270,832 bytes for the complete 7,886-module tree and 157,483,170
bytes for the initramfs—consistent with the known-working candidates.

## Preboot validation

- full `Image modules dtbs` build: passed;
- kernel image identity: `Linux version 7.1.5-sp11-render-parity+`;
- all 7,886 installed modules: exact vermagic, zero unsigned modules;
- critical Wi-Fi, OLED, touch, Bluetooth dependency, AudioReach, SoundWire and
  WSA modules: present in the new module tree and signed;
- initramfs: contains Phase91 overrides, `ath12k_wifi7`, `ath12k`, `msm`, the
  complete audio dependency closure and the exact topology;
- topology binary: byte-identical to the previously boot-tested four-link
  Headroom-Test topology;
- patch `0046` strict checkpatch: 0 errors, 0 warnings, 0 checks;
- patch `0050` strict checkpatch: 0 errors, 0 warnings, 0 checks;
- repository tests: 134 passed, 3 skipped, 6 subtests passed;
- generated `/boot/grub/grub.cfg`: syntax passed and contains the isolated
  entry with all known-good Wi-Fi/touch/OLED/platform arguments; and
- persistent and one-shot GRUB state: unchanged, candidate not armed.

## Artifact hashes

| Artifact | SHA-256 |
|---|---|
| Kernel Image | `842b4af59420ce2087089e7f41e76ed12fcf54ffc6ed52c3f0ee155a0d372096` |
| System.map | `a0f7d7ee82f72a934c7867ff44254af8a1ec546558ac432a9a539921328d480f` |
| Kernel config | `e2891ef266977a8b00510e83694815ed7c6de3699bab93753e06e57b4fc8db1d` |
| Combined DTB | `ce0f424d67bca493af4d1b142424c4c91676e5f5fbfc29c8e4fbdf214fe5c0f1` |
| Initramfs | `5807d3c49ae5895eac938ddaa46e8bcaef7f95ae8dc4d9da91e934421b124f19` |
| Render-Parity topology | `1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d` |
| WSA clock-stop patch `0046` | `e8afd2ac05c6fa4ea80677eff9439f76acd2a327f446c3762ca2edf5d07ca0cc` |
| Render-Parity model patch `0050` | `5c39ea2d3639a2e45525f91a30264524b73ec76773eba14161492528466998f8` |

The machine-readable counterpart is
`artifacts/reviewed/2026-08-14-sp11-render-parity-build-manifest.json`.

## Runtime gates after deliberate one-shot boot

Do not promote the candidate merely because it boots.  Capture and verify:

1. running release, command line, sound-card model and artifact hashes;
2. Wi-Fi association, touch input and OLED/MSM display health;
3. both WSA884x amplifiers, dual VI feedback and SP/SPVI/CPS graph startup;
4. final-volume plus GainStep transactions across short and 272-byte rows;
5. SOFT_PAUSE pause/resume/STOP/reprepare lifecycle;
6. muted cold-first-play timing and WSA attach/cache behavior;
7. suspend/resume; and
8. physical YouTube seek and live-slider transients at a conservative volume.

Only those live results can promote L03b, L04 and L07 from AMBER.

## First live boot result

The one-shot entry was deliberately booted on 2026-08-14 at 08:03 BST.  It
reached the exact `7.1.5-sp11-render-parity+` release and Render-Parity sound
model.  The one-shot GRUB state was consumed while persistent
`saved_entry=sp11-audio-cps-v3` remained unchanged.

Platform and protected-audio closure passed:

- Wi-Fi associated with a valid address/default route; OLED/MSM display,
  Phase91 touch and all three external transport overrides loaded from the
  exact new release;
- the four-link Render-Parity topology opened both WSA884x amplifiers with
  playback ports 1/2/3, VI port 5 at 8 kHz and CPS port 6 at 24 kHz;
- SP, SPVI, VI and CPS configuration, endpoint calibration and graph start
  were accepted; and
- PipeWire, WirePlumber, the native Dolby chain and combined Windows volume
  synchronizer were active.  The legacy MSIIR synchronizer correctly stood
  down because the combined transaction control was present.

### Volume transaction: live transport pass

With a zero-valued local stream, the synchronizer completed
15% -> 25% -> 40% -> 25% -> 15% -> restored 31%.  This exercised GainSteps
1/3/12/3/1/7 and both 216-byte and formerly failing 272-byte rows.  Every row
reported hardware unity only after the combined transaction succeeded; no
runtime DSP error followed any update.  The recurring status-3 SET_CFG is the
already documented optional graph-start calibration frame and preceded the
runtime sweep.

### WSA clock-stop correction: live latency pass

Two zero-valued starts began with the SoundWire manager and both WSA slaves in
`runtime_status=suspended`, `power/control=auto`.  ALSA reached `RUNNING` in
208 ms and 83 ms respectively, versus the prior reproducible 18.627/18.464 s
baseline.  No multi-second regcache replay occurred.  After each stopped
stream all three devices returned to automatic runtime suspend.  Suspend-to-RAM
and forced detach/re-attach recovery remain separate gates.

### SOFT_PAUSE: live failure and bounded correction

A direct, zero-valued ALSA `PAUSE_PUSH` reached the new lifecycle path, but
`PAUSE_COMPLETE` missed the 50 ms waiter and ALSA returned `-ETIMEDOUT`.
After the trigger stall ended the kernel logged both correct DSP module events
from iid `0x466b`: pause `0x0800103f` and resume `0x08001043`.  The graph then
closed normally, all services remained active and all SoundWire devices
returned to suspend.

This localizes the failure to Linux host callback ordering rather than the
recovered Windows parameter/event identities.  ALSA calls `.trigger()` with
the PCM stream lock held.  Leaving the pull stream marked RUNNING permits a
watermark on the same serialized GPR receive path to enter
`snd_pcm_period_elapsed()` and block before the completion event can be
delivered.  Patch `0051` enters STOPPED/Windows PAUSE-state 3 before issuing
the pause command, suppresses watermark period callbacks during the wait, and
restores RUNNING if the command itself is rejected.  It passes reverse
applicability, strict checkpatch, focused tests and a `W=1` GCC compile of the
exact `q6apm-dai.o` target.  A new isolated boot candidate is required before
L04 can be promoted.

Physical seek/slider listening was not performed with the zero-valued probes.
The current candidate is therefore a partial live pass, not a promoted
default.

## Corrected Render-Parity v2 deployment

Patch `0051` was consolidated into a new full release rather than replacing a
live module in place. `7.1.5-sp11-render-parity-v2+` completed a full
`Image modules dtbs` build and was installed under isolated module and boot
paths. All 7,886 modules have exact v2 vermagic and build-key signatures; the
Phase91 trio was rebuilt from source against the exact v2 ABI. The combined
DTB and reviewed topology remain byte-identical to the first Render-Parity
candidate, and the initramfs contains the complete Wi-Fi, touch, OLED,
AudioReach, SoundWire, WSA and topology closure.

GRUB syntax and asset checks passed. The one-shot state is armed as
`next_entry=sp11-audio-render-parity-v2`, while persistent
`saved_entry=sp11-audio-cps-v3` remains unchanged. See
`deploy/render-parity-v2/README.md` and
`artifacts/reviewed/2026-08-14-sp11-render-parity-v2-build-manifest.json`.

The v2 one-shot subsequently booted successfully. Platform, Wi-Fi, touch,
display, Dolby, both amplifiers, VI/CPS protection and the complete graph
startup passed. The formerly failing zero-valued direct-ALSA lifecycle now
received pause-complete `0x0800103f`, resume-complete `0x08001043`, and a
second resume-complete during STOP-while-paused, with no timeout. ALSA moved
through `RUNNING -> PAUSED -> RUNNING`, then closed cleanly. A cold start from
all three SoundWire nodes suspended reached RUNNING in 86 ms, and a 272-byte
GainStep-3 volume transaction passed. This promotes L04's implementation and
live lifecycle gate to GREEN; physical YouTube seek/slider judgment remains a
separate L03/L03b gate.

The user subsequently repeated the physical paused-YouTube volume-slider
notification test and reports that the stale-media "ghost" remains gone on
v2. This is a physical no-regression pass for the already closed Dolby
pause-drain fix (L03c), distinct from the still-pending in-stream seek test.
