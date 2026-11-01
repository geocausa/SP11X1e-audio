# SP11 OLED 7.3: sound, touch and driver maintenance

Recipe **sp11-denali-oled-7.3-20261101-driver-maintenance** installs the qualified
`7.3.0-sp11-unified13` binary composition. The older `../sp11-7.3-host-integration/` and `../sp11-7.3/` recipes remain
separate recovery releases. Choose one complete release; do not mix its
modules, configuration, firmware or old 7.1 installers with this one.

The same OLED Surface Pro 11 passed two boots with normal tapping/dragging/
scrolling and clear first sound from both speakers twice after separate quiet
pauses on each boot. Both boots used the identical kernel and startup image:
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
- Source `3094f120b90e7b2df9ccb326b1193d66bcc217a5`, exact full source archive and
  47 ordered patches from pristine 7.3. AppArmor includes matching Ubuntu 7.3
  notification/network/userns integration. The DTB remains the accepted machine
  DTB; sound, touchscreen, GPI and Wi-Fi contracts are retained.
- All 18 firmware/topology inputs with the stock NVM alias preserved, six-file UCM include closure, graph/helpers,
  transaction-volume controls, both macro companders and private UBiG calibration.
- Early ath12k preload; Bluetooth patch/NVM and GPU firmware; initqueue; real kmod
  as early modprobe. Verification resolves real module dependencies inside the
  extracted image with an explicit target release and `--ignore-install`; it
  works when the installer runs under another kernel and never loads modules.
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
sudo python3 recipes/sp11-7.3-driver-maintenance/recipe.py export \
  --home /home/OWNER --private-pack /private/sp11-stageb-v4.pack \
  --kernel-source /private/kernel-source.tar.zst \
  --kernel-patches /private/kernel-integration.mbox --output /private/new-bundle
python3 recipes/sp11-7.3-driver-maintenance/recipe.py verify /private/new-bundle
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
python3 recipes/sp11-7.3-driver-maintenance/recipe.py stage /private/new-bundle \
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
sudo python3 recipes/sp11-7.3-driver-maintenance/recipe.py install /private/new-bundle \
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
sudo python3 recipes/sp11-7.3-driver-maintenance/recipe.py check-live
```

Confirm both speakers and two first sounds after separate quiet pauses physically;
repeat a successful boot before choosing it as the normal default. Software
checks cannot substitute for listening or touchscreen use.

## Maintenance scope and tests

Four ordered patches in `patches/unified13-*.patch` extend unified12. They remove
active per-frame calibration diagnostic resends, apply four upstream WSA/RX
cache/default/Compander1 fixes, move initial speaker-health scheduling to the
current per-CPU queue, and classify only intentional sync-state link refusals as
debug. Genuine failures remain errors. Protection stages, full Windows
calibration, firmware, touch transport and the accepted DTB remain intact.
The understood GET-only timing parameter still returns DSP status3 / aggregate
AR_EUNSUPPORTED. That compatibility response is retained and visible; the
release does not promise an entirely warning-free journal or fix suspend.

Primary upstream codec commits: `233d36a96009`, `246c785491bb`,
`ce7164c89b5c`, `384bd7cf4053` in the sound maintainer tree. The device-link
diagnostic change adapts the 2024 proposal
<https://patchew.org/linux/20240624-fwdevlink-probed-no-err-v1-1-d1213cd354e2%40collabora.com/>;
it is not described as an upstream-merged or suspend repair.

```sh
SP11_KERNEL_BEFORE=/path/to/unified12 SP11_KERNEL_SOURCE=/path/to/unified13 \
  python3 -m unittest discover -s recipes/sp11-7.3-driver-maintenance -p 'test_*.py' -v
```

The recipe contains 18 actual-C maintenance cases, the inherited 17 APM/HID
cases, four host-policy rollback cases, eleven portable/tamper/relocation cases
and four target-release module-loader cases. Source archive plus 47-patch series
provide rebuild inputs. To obtain the pre-maintenance source for comparisons,
reverse the four maintenance patches in a separate copy of the full source.
Never reverse patches in a running release's source or mix binary modules.

The exact Image additionally passed a diskless AppArmor enforcement VM and
eight regmap cases on a synthetic bus. The VM probe is not installed on the
Surface. Both physical boots pass all 16 loaded identities, normal touch, clear
cold sound twice, accepted speaker-protection stages and zero targeted faults
or repaired diagnostic/cache/workqueue/link warnings. Startup-capture unit
permissions were corrected between boots; no Image/initrd bytes changed.
The recipe does not install those diagnostic capture units.
