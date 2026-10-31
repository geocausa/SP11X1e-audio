# SP11 OLED: reproducible 7.3 sound/touch/Wi-Fi release

Recipe ID: **sp11-denali-oled-7.3-20261031-wifi**. This is the supported installation
entry point for the tested 7.3 sound/touch/Wi-Fi composition. The companion touch
repository uses this same lock and installer. Do not combine the old 7.1
Phase91 module installer with a 7.3 kernel.

Disconnect external audio devices during qualification; the tested internal
ALSA card is card 0.

The exact baseline passed two owner-confirmed fresh boots, including startup
tracing disabled: normal touch and clear first sound after quiet pauses. The Wi-Fi
repair was loaded on two further boots; a controlled Hub 5 test captured two
completed live channel switches, no statistics timeouts over 387.6 seconds
after the final switch, and responsive Resources. The router was restored.
Camera, microphone and whole-system suspend are not qualified by these tests.
The microphone should remain muted. The former live parent-controller restart
diagnostic is not part of this recipe.

## Supported inputs

- The same OLED Surface Pro 11 hardware unit and its private payload bundle.
- A working ARM64 Ubuntu 26.10 installation with GRUB, ext4 root, and `/boot`
  on that root filesystem. Its stock kernel/default are retained.
- Package versions in `release-lock.json`. Different versions are reported as
  unqualified before installation; the script does not silently downgrade them.
- The exact `7.3.0-sp11-unified10` kernel/config/DTB and full module set. The
  name stays unchanged because the repaired GPI module uses that closed ABI.
  Image source is e8ceea84e; repaired GPI is cedc5a624 and the accepted
  Wi-Fi integration is e9e5dd622. There are 39 integration commits after the
  pristine 7.3 source snapshot c03c1a145.

The lock pins the kernel/DTB/config, **all 8711 modules**, module metadata,
ten firmware/topology inputs (including the four-file WCN7850 closure),
the six-file UCM include closure,
helpers/control CLI, UBiG engine/private calibration, graph and endpoint policy.
The SP11 UCM explicitly enables both macro companders before PCM probing.
The dispatcher waits for late transaction-control registration before choosing
its actuator. Root modules and the generated initrd must agree. The ath12k core
counts only
initial vdev starts, not channel restarts. `rd.driver.pre=ath12k` loads the correct
core before switch-root; inclusion alone previously allowed the old root copy
to load. Actual loaded identities are checked, including both Wi-Fi modules.

This is a **binary-release installation recipe**. It never invokes a compiler,
`make`, playback, recording, suspend, controller unbind/rebind or reboot.
Its complete source archive and ordered patch series retain the corresponding
source for review or rebuilding; a newly compiled/different kernel is a new build requiring qualification.

## Prepare a private release bundle on the accepted machine

Private firmware/calibration and per-device identity are intentionally excluded
from Git. Keep the bundle on owner-controlled storage; do not add it to either
repository. The exporter verifies current installed bytes and renders no live
configuration. Supply the matching StageB pack; it is not downloaded.

```sh
mkdir -m700 /path/to/private-release
git --git-dir=/path/to/linux.git archive --format=tar --prefix=linux-sp11/ \
  e9e5dd622ee88f33fdc7e275963d0cb9e42aa40a | \
  zstd -q -T2 -3 -o /path/to/private-release/kernel-source.tar.zst
git --git-dir=/path/to/linux.git format-patch --stdout \
  c03c1a14507dfaf8b856c2608f9ac216021fc222..e9e5dd622ee88f33fdc7e275963d0cb9e42aa40a \
  > /path/to/private-release/kernel-integration.mbox

sudo python3 recipes/sp11-7.3/recipe.py export \
  --home /home/OWNER \
  --private-pack /path/to/sp11-stageb-v4.pack \
  --kernel-source /path/to/private-release/kernel-source.tar.zst \
  --kernel-patches /path/to/private-release/kernel-integration.mbox \
  --output /path/to/private-release/bundle
python3 recipes/sp11-7.3/recipe.py verify /path/to/private-release/bundle
```

The source artifacts are hash-pinned in the lock. Use the provided release
copies when re-exporting; different compression/tool output is not silently
accepted. The original kernel Git repository is shallow: a Git bundle alone
was found insufficient on a clean clone. The complete archive avoids that
hidden dependency. To inspect the source later:

```sh
mkdir source-review
tar --zstd -xf /path/to/private-release/bundle/kernel-source.tar.zst -C source-review
# source-review/linux-sp11 contains the full accepted integration tree.
```

Offline rehearsal reconstructs its Git tree and checks it against the pinned
integration tree hash. The mbox contains the 39 ordered changes from the
pristine 7.3 snapshot, including the audio and touch transport fixes.

The bundle binds to a hash of this unit's firmware DMI UUID and carries its
private touch identity. Another unit needs its own payload/identity and hardware
qualification. A different Linux username or root filesystem UUID is supported.

## Offline rehearsal first

This writes only a new, empty staging directory. It runs no host installation
commands or user services. It renders a stable private-pack path under the new
user's `.local/share/ubig-private`, never a project evidence-folder path.

```sh
python3 recipes/sp11-7.3/recipe.py stage /path/to/private-release/bundle \
  --root /path/to/empty-new-root --home /home/NEWOWNER --uid 2000 \
  --root-uuid 11111111-2222-3333-4444-555555555555
```

The shipped tests cover tampering, missing private identity, wrong release,
path escape and staging into a different home/UID/root UUID. Offline rehearsal
also constructs/checks a new initrd from the staged modules/firmware. It is not
a claim that a destructive clean OS reinstall was performed.

## Install on a fresh, supported installation

Review the verified bundle and lock first. The firmware must be able to boot
the unsigned custom kernel. Existing recipe modules/assets or SP11 user audio
configuration are refused; this is not an in-place upgrade script.

```sh
sudo python3 recipes/sp11-7.3/recipe.py install /path/to/private-release/bundle \
  --user NEWOWNER --unsigned-boot-ready
```

Installation backs up replaced system files in `/var/backups/sp11-baseline73`,
installs the matching binaries/configuration, enables the user graph/helpers for
the next login, runs depmod and builds a fresh initrd **without an embedded old
host root UUID**, then checks its modules/firmware. The new GRUB entry is
`sp11-baseline73`. The existing default and one-shot environment are preserved.
No services are restarted and no reboot is initiated.

On failure the script restores replaced system files, removes this release's
new modules/entry/user enablement and restores the previous generated menu.
The backup is retained for inspection. Do not blindly rerun after a failure.

After reviewing the new entry, select one boot when ready:

```sh
sudo grub-reboot sp11-baseline73
# Save work and reboot through the desktop when ready.
```

## Validate that boot

First touch/drag/scroll. Before playback, as the desktop user, select the
processed output and keep input muted:

```sh
wpctl status -n
# Use the ID for effect_input.sp11_ubig, not its internal engine sibling:
wpctl set-default VISIBLE_UBIG_NODE_ID
wpctl set-mute @DEFAULT_AUDIO_SOURCE@ 1
sudo python3 recipes/sp11-7.3/recipe.py check-live
```

The verifier checks installed bytes, actual GPI/entry identity, touch contacts,
zero targeted kernel/transport faults, both Wi-Fi module identities, both
SoundWire flags and both macro
companders. It never generates sound or records input. Then the owner checks
both speakers and two first playbacks after separate 20-second quiet pauses.
Repeat a fresh boot and check again before changing the persistent default.
For Wi-Fi qualification, capture a completed live AP channel switch while
associated; a router reboot/reassociation alone does not exercise the original
statistics defect. Confirm prompt status queries, plausible signal, no new
firmware-statistics timeout stream and normal data exchange afterwards.

If a gate fails, preserve `journalctl -b -1`, current boot logs and contact
counters, then use the retained stock/recovery entry. Do not use live parent
controller restarts to force a passing result. Existing 7.1 recovery remains
usable if already installed; this bundle does not install a 7.1 recovery image.

## Newer kernels and other package versions

This recipe accepts the tested composition only. A newer stock kernel may
remain installed as recovery, but installing this release still boots the
pinned 7.3 kernel. Supporting a newer kernel requires porting/reviewing the
shared integration, producing a complete matching artifact set and repeating
boot/contact/cold-audio gates. Neither a successful compile nor a version
number alone establishes compatibility.

The previous recipe `sp11-denali-oled-7.3-20261030` remains reproducible from its
older pinned commits and separate private bundle. Its Wi-Fi module predates this
repair. `wifi-vdev-count.patch` preserves the small upstream-proposed source
delta; the complete 39-commit integration is in this release's source artifacts.
