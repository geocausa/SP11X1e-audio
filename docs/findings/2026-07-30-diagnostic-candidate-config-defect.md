# Diagnostic observation candidate: config defect and installer defect

Date: 2026-07-30

Root: `/home/geoca/Documents/SP11-PROJECT`

Status: **the `7.1.5-sp11-audio-diag-observe` candidate as staged on 2026-07-29
is not usable.** It was installed, booted, and produced no sound card at all.
Two separate defects were found. Neither is in the kernel source, the
observation patch, the device tree, or the audio work.

The running system was returned to `7.1.5-sp11-audio-vi` and is healthy.

## 1. Installer defect: unreachable initramfs validation

`deploy/diagnostic-observe/install-diagnostic-candidate.sh --install` aborted:

```text
ERROR: generated initramfs is missing snd-q6apm
```

The transactional installer rolled back cleanly. No boot directory, module
tree, or GRUB entry was left behind.

Root cause: the installer validated that five modules were embedded in the
generated initramfs:

```text
for module in gpi spi-geni-qcom mshw0485_touch snd-q6apm snd-soc-wsa884x; do
```

but the candidate hook `sp11-audio-diag-observe-phase91` only adds three:

```text
manual_add_modules gpi
manual_add_modules spi-geni-qcom
manual_add_modules mshw0485_touch
```

The two audio modules are never added by the hook, and are **absent from the
known-good `audio-vi` initramfs as well**. Verified by counting `.ko` entries
in the live initramfs:

```text
gpi                 2
spi-geni-qcom       2
mshw0485_touch      1
snd-q6apm           0
snd-soc-wsa884x     0
snd-soc-x1e80100    0
```

The audio stack loads from the root filesystem after boot
(`/lib/modules/<release>/kernel/sound/soc/qcom/qdsp6/*.ko.zst`). Requiring it
in the initramfs made `--install` impossible to satisfy.

### Correction applied

The two audio modules were removed from the validation list; the vermagic
check for the three Phase91 modules is retained. Original script preserved as
`install-diagnostic-candidate.sh.bak-before-initramfs-fix-20260730`.

The alternative fix — adding the audio modules to the hook — was **rejected
deliberately**. Early-loading the audio stack from initramfs instead of the
root filesystem changes probe ordering relative to the `audio-vi` baseline, and
would contaminate the GET_CFG and port-mask comparison the boot exists to make.

Note this defect also means `--install` was never exercised offline before the
handoff was written. Only `--verify-only` was run.

## 2. Config defect: the candidate is not a controlled variant of audio-vi

After the installer fix, the candidate installed and booted. The kernel came
up, wifi, bluetooth and touchscreen worked, but:

```text
$ cat /proc/asound/cards
--- no soundcards ---
```

### Failure chain

From `/sys/kernel/debug/devices_deferred`:

```text
reset.gpio.0            reset_gpio: Could not get reset gpios          <-- root
sdw:1:0:0217:0204:00:0  wsa884x-codec: Failed to get reset
sdw:1:0:0217:0204:00:1  wsa884x-codec: Failed to get reset
sound                   snd-x1e80100: WSA Playback: codec dai not found
```

Both amplifiers fail to obtain their reset, so no codec DAI is registered and
the card is never created.

The speaker reset is a GPIO reset. Resolving the `reset-gpios` phandle
(`0x000000e0`) from `speaker@0,0` gives:

```text
OWNER NODE: /proc/device-tree/soc@0/pinctrl@6e80000
COMPATIBLE: qcom,x1e80100-lpass-lpi-pinctrl qcom,sm8550-lpass-lpi-pinctrl
```

That pinctrl driver is present, loaded, bound, and **does** register its
gpiochip — `gpiochip10: 23 GPIOs, parent: platform/6e80000.pinctrl`. So the
GPIO provider works; `reset.gpio.0` simply probed before it existed and the
deferred retry never succeeded. Reloading `reset_gpio` did not recover it.

### Root cause

The candidate was built with its own heavily reduced kernel configuration:

| | `=y` built-in | `=m` modules | config lines |
|---|---:|---:|---:|
| `audio-vi` (working) | 4,061 | **7,651** | 15,516 |
| `audio-diag-observe` | 3,107 | **159** | 10,237 |

7,480 options that are modules in `audio-vi` are missing or off in the
candidate, and ~954 fewer options are built in. Module trees on disk:
7,888 files versus 159.

The handoff records "The candidate contains 159 signed compressed modules" as a
neutral fact. It is in fact the defect. The reduced set changes udev coldplug
and driver registration ordering, which exposes the reset/gpio race.

This is not a marginal difference. The candidate is a materially different
kernel, not `audio-vi` plus an observation patch. Even if the card had
registered, observations from it would not have been safely comparable to the
`audio-vi` baseline.

## 3. What is NOT wrong

Verified good, so these need no rework:

- **Patch 0023 is present and compiled in.** Patch SHA-256 matches the handoff
  (`72ef6a8f...`). Marker strings verified in both source and built binaries:

  ```text
  source   wsa884x.c  "SP11 SoundWire"  x2
  source   q6apm.c    "SP11 GET_CFG"    x2,  "sp11-getcfg" x1
  binary   snd-q6apm.ko        x3 markers
  binary   snd-soc-wsa884x.ko  x2 markers
  ```

  The patch touches `sound/soc/codecs/wsa884x.c` and
  `sound/soc/qcom/qdsp6/q6apm.c` — **not** `audioreach.c`. An earlier grep of
  `audioreach.c` returned nothing and was briefly misread as the patch being
  absent. It is present.

- **The absence of `sp11-getcfg:` lines in the failed boot is fully explained.**
  No sound card was created, so no graph was configured and that code never
  ran. It is not evidence of a patch problem.

- **The device tree is byte-identical** to the working one:
  `5f7de091ec19cc874f401001d1e3aa984faf889921abb382df900c5d8fcd5d8a` for both
  `...-audio-vi-phase91.dtb` and `...-audio-diag-observe-phase91.dtb`.

- **The source lineage is correct.** `patches/README.md` states 0023 stacks on
  the reconstructed `audio-vi` source, above series 0001-0022.

- **The failure messages appear nowhere in the `audio-vi` baseline capture**
  (`artifacts/collector-selftest-current-kernel/20260729T224958Z/`), confirming
  the candidate introduced them.

- **The collector is intact and read-only.** SHA-256 matches the handoff
  (`25a2de9c...`). It ran on the failed boot without altering audio state.

## 4. Required fix

Rebuild the candidate from the existing diagnostic source tree, replacing only
the configuration:

1. Use `/boot/sp11-7.1.5-audio-vi/config-7.1.5-sp11-audio-vi` as `.config`.
2. Keep `CONFIG_LOCALVERSION="-sp11-audio-diag-observe"` so the release string,
   module tree and GRUB entry stay isolated from the working kernel.
3. Rebuild Image, modules and DTBs; stage and sign as before.

Prerequisites confirmed available:

```text
Secure Boot            disabled (platform in Setup Mode)
CONFIG_MODULE_SIG_ALL  y  (build generates its own key; no external key needed)
free space on /        87 GB
```

Cost: this is a 7,651-module build, on the order of 1-3 hours. Run it detached
so a dropped session cannot kill it.

Not yet independently verified: that the diagnostic source tree is
byte-identical to the `audio-vi` source apart from 0023. `patches/README.md`
asserts that comparison was made, but no surviving `audio-vi` source tree was
located to diff against. Reconstructing `linux-7.1.5` + patches 0001-0022 and
diffing would close this.

## 5. Artefacts

Failure-state capture from the diagnostic boot, taken before anything was
perturbed:

```text
01-audio/artifacts/diagnostic-boots/20260730T070105Z/
```

This documents why the candidate is unusable and is a valid negative result,
not a wasted boot.

## 6. Process note

The handoff's "Validation already completed" list was accurate but
**self-referential**. Every check proved the candidate internally consistent
with itself:

```text
full ARM64 build:              pass
Image/modules/DTBs:            pass
module signing:                pass
staged depmod resolution:      pass
root installer verify-only:    pass
```

Nothing compared the candidate against the working reference. Two defects
passed through as a direct result: the config divergence was never detected
because config parity with `audio-vi` was never checked, and the impossible
initramfs assertion survived because `--install` was never run.

Suggested additions to future candidate validation:

- assert `=y` and `=m` counts against the reference kernel config, and fail on
  material divergence;
- run the real `--install` path in a disposable environment, not only
  `--verify-only`;
- assert the candidate's module count is within a defined tolerance of the
  reference module tree.

The handoff's own warning was correct and fired exactly as written: *"Absence
of these lines on the candidate kernel means the intended modules did not load
and must be treated as a packaging/provenance failure before any audio test."*
That instruction was followed.
