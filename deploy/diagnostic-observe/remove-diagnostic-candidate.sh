#!/usr/bin/env bash
# Remove only the isolated observation candidate. Refuses while it is running.
set -euo pipefail
release=7.1.5-sp11-audio-diag-observe
[ "$(id -u)" -eq 0 ] || { echo 'Run as root.' >&2; exit 1; }
[ "$(uname -r)" != "$release" ] || { echo 'Refusing to remove the running kernel.' >&2; exit 1; }
rm -f /etc/grub.d/48_sp11_audio_diag_observe
rm -f /etc/initramfs-tools/hooks/sp11-audio-diag-observe-phase91
rm -rf /boot/sp11-7.1.5-audio-diag-observe
rm -rf /lib/modules/$release
depmod -a
update-grub
echo 'Removed the isolated diagnostic candidate; known-good entries were untouched.'
