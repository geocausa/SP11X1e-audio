# SP11 OLED 7.3: sound, touch and host integration

Recipe **sp11-denali-oled-7.3-20261101-host-integration** installs the qualified
`7.3.0-sp11-unified12` binary composition. The older `../sp11-7.3/` recipe remains
an immutable recovery release. Choose one complete release; do not mix its
modules, configuration, firmware or old 7.1 installers with this one.

The same OLED Surface Pro 11 passed two boots with normal tapping/dragging/
scrolling and clear first sound from both speakers twice after separate quiet
pauses on each boot. The corrected startup image passed on the second boot:
zero early module-hook errors, initial SPF timeouts or targeted kernel/SPI/Wi-Fi
faults; full/strict Snap support; successful Bluetooth firmware setup; and one
correct battery report. Bluetooth remains soft-blocked: pairing was not tested.
The previously tested live-channel-switch repair is retained; no new channel
switch was performed for this full build. Camera, microphone and whole-system
suspend remain unqualified. Keep the microphone muted.

## Supported installation

Same tested hardware unit/private identity, ARM64 Ubuntu 26.10, ext4 root and
`/boot` on that root, GRUB, and exact versions in `release-lock.json`. Stock
`upower=1.91.3-1` and its matching library must be installed initially. The recipe
installs the pinned local UPower package; it keeps the library ABI and charging
settings. It refuses different versions or existing local SP11 overrides rather
than silently replacing an unknown installation. This is a fresh-install recipe,
not an in-place updater for an already installed SP11 release.

Newer kernels or distribution packages require rebasing/testing and new locks;
these checks prevent accidental reuse as a claim that every future release works.
No kernel is compiled and no automatic reboot, playback, recording, suspend,
firmware flashing or parent-controller restart is performed. Package configuration
may restart **UPower only**; audio/radio/kernel drivers are not restarted.

## Complete closure

- Full kernel/config/DTB/System.map, all 8,711 modules and generated metadata.
- Source `2fa94dc1319aea59e4ce9c0d8a1a4db1b4617ff9`, exact full source archive and
  43 ordered patches from pristine 7.3. AppArmor includes matching Ubuntu 7.3
  notification/network/userns integration. The DTB remains the accepted machine
  DTB; sound, touchscreen, GPI and Wi-Fi contracts are retained.
- All 18 firmware/topology inputs with the stock NVM alias preserved, six-file UCM include closure, graph/helpers,
  transaction-volume controls, both macro companders and private UBiG calibration.
- Early ath12k preload; Bluetooth patch/NVM and GPU firmware; initqueue; real kmod
  as early modprobe. Verification executes a real **dry-run** inside the extracted
  image to catch BusyBox's unsupported `--ignore-install` before boot.
- Guarded single-pack UPower policy, before-UPower startup ordering, stock/custom
  package rollback copies, original UPower source archives and source patch.
- Reviewed mapper/brltty/ALSA/retry-policy files. SSSD responder symlinks are
  removed only when domain configuration is absent; PAM/NSS are not changed.

`host-files/` contains the public source of each installed host override, pinned
against the lock. Private firmware/calibration/device identity stay outside Git.
The battery policy keeps both kernel drivers and all kernel charging controls;
UPower presents the authoritative Surface object. UPower upgrades need the patch
rebased/requalified. The recipe does not hold back unrelated system updates.

## Export a private bundle on the qualified machine

Provide the exact source archive/patch series recorded in the lock and this
unit's StageB pack. The exporter verifies actual running module identities and
installed bytes. The archive is complete; it does not depend on a shallow Git
bundle or USB reverse-engineering directory.

```sh
sudo python3 recipes/sp11-7.3-host-integration/recipe.py export \
  --home /home/OWNER --private-pack /private/sp11-stageb-v4.pack \
  --kernel-source /private/kernel-source.tar.zst \
  --kernel-patches /private/kernel-integration.mbox --output /private/new-bundle
python3 recipes/sp11-7.3-host-integration/recipe.py verify /private/new-bundle
```

The initial source-package inputs are retained under the owner's source reference
and `/var/lib/sp11-power-reporting/20261031` paths pinned in the lock. Export is
from the qualified reference machine; the resulting bundle is self-contained
for installation and supports a different Linux username, UID and root UUID on
this same hardware unit. Copy this entire recipe directory alongside the bundle
for offline use, including host_policy.py, host-files, templates, config and maps.
Do not publish the bundle or copy it into either Git checkout.

## Rehearse without installing

```sh
python3 recipes/sp11-7.3-host-integration/recipe.py stage /private/new-bundle \
  --root /scratch/empty-root --home /home/NEWOWNER --uid 2000 \
  --root-uuid 11111111-2222-3333-4444-555555555555
```

Staging writes only the new empty directory. It renders the relocated private
pack/home/UID/root UUID and stages local DEBs without configuring packages or
contacting system/user services. The release rehearsal additionally runs depmod
and constructs/checks a fresh initrd from the staged modules/firmware, extracts
the packaged daemon and reconstructs the source tree. This is not a claim of a
physical destructive clean OS reinstall.

## Install and verify

Review the supported package versions/lock and firmware boot readiness first.
Existing defaults, stock recovery and one-shot selections are preserved.

```sh
sudo python3 recipes/sp11-7.3-host-integration/recipe.py install /private/new-bundle \
  --user NEWOWNER --unsigned-boot-ready
sudo grub-reboot sp11-baseline73
# Save work and reboot through the desktop when ready.
```

The installer backs up replaced files and the stock package under
`/var/backups/sp11-baseline73`, installs matching binary/policy inputs, builds
and verifies a new initrd without an embedded old root UUID, and checks GRUB.
It rolls back host files, new assets/modules, SSSD enablement and package on
failure. Inspect preserved evidence before retrying a failed installation.

After boot, touch/drag/scroll, select the visible `effect_input.sp11_ubig` processed
output using `wpctl status -n`, keep input muted, then:

```sh
wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 1
sudo python3 recipes/sp11-7.3-host-integration/recipe.py check-live
```

Confirm both speakers and two first sounds after separate quiet pauses physically;
repeat a successful boot before choosing it as the normal default. Software
checks cannot substitute for listening or touchscreen use.

## Source regression tests

```sh
SP11_KERNEL_SOURCE=/path/to/pre-fix/kernel \
  python3 -m unittest discover -s recipes/sp11-7.3-host-integration -p 'test_*.py' -v
python3 -m unittest discover -s tests -p test_sp11_recipe73_host_integration.py -v
```

The 17 actual-C cases reproduce old APM reply-order/HID descriptor failures and
preserve valid behavior. Four host-policy cases protect configured domains and
exercise partial package failure recovery; eleven portable cases cover tampering,
wrong kernels/private identity, path escape and relocated staging. Source review
and rebuilds are possible from the full archive; the binary installer never bakes.
