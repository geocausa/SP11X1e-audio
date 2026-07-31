#!/usr/bin/env bash
# Install the already-built SP11 observation kernel as an isolated GRUB entry.
# Default mode is verification only. Nothing is installed without --install.
# The script never changes the saved/default GRUB entry and never reboots.

set -euo pipefail

project_root=${SP11_PROJECT_ROOT:-/home/geoca/Documents/SP11-PROJECT}
candidate=${SP11_DIAG_CANDIDATE:-${project_root}/01-audio/artifacts/diagnostic-candidate-20260729}
release=7.1.5-sp11-audio-diag-observe
boot_dir=/boot/sp11-7.1.5-audio-diag-observe
module_dir=/lib/modules/${release}
grub_source=/etc/grub.d/48_sp11_audio_diag_observe
known_good_grub=/etc/grub.d/47_sp11_audio_vi
hook_target=/etc/initramfs-tools/hooks/sp11-audio-diag-observe-phase91
entry_id=sp11-audio-diag-observe
mode=verify

usage() {
    cat <<EOF
Usage: sudo $0 [--verify-only|--install]

--verify-only  Validate the candidate and host without changing the system.
--install      Install a separate kernel/modules/initrd/GRUB entry.

The script does not set the GRUB default, arm a one-shot boot, or reboot.
EOF
}

case ${1:---verify-only} in
--verify-only) mode=verify ;;
--install) mode=install ;;
-h|--help) usage; exit 0 ;;
*) usage >&2; exit 2 ;;
esac

fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
require_file() { [ -f "$1" ] || fail "missing required file: $1"; }

require_file "$candidate/KERNEL_RELEASE"
[ "$(cat "$candidate/KERNEL_RELEASE")" = "$release" ] || fail "candidate release mismatch"
require_file "$candidate/SHA256SUMS"
(
    cd "$candidate"
    sha256sum -c SHA256SUMS
) >/dev/null || fail "candidate SHA256 verification failed"

for file in \
 "$candidate/boot/vmlinuz-$release" \
 "$candidate/boot/System.map-$release" \
 "$candidate/boot/config-$release" \
 "$candidate/boot/x1e80100-microsoft-denali-sp11-audio-diag-observe-phase91.dtb" \
 "$candidate/rootfs/lib/modules/$release/modules.dep" \
 "$candidate/rootfs/lib/modules/$release/updates/sp11-phase91/gpi.ko.zst" \
 "$candidate/rootfs/lib/modules/$release/updates/sp11-phase91/spi-geni-qcom.ko.zst" \
 "$candidate/rootfs/lib/modules/$release/updates/sp11-phase91/mshw0485_touch.ko.zst" \
 "$project_root/01-audio/deploy/diagnostic-observe/sp11-audio-diag-observe-phase91" \
 "$known_good_grub"; do
    require_file "$file"
done

[ "$(id -u)" -eq 0 ] || {
    [ "$mode" = verify ] && printf 'Verification is non-root; privileged host checks were skipped.\n' && exit 0
    fail "--install must run as root"
}

[ ! -e "$module_dir" ] || fail "target module tree already exists: $module_dir"
[ ! -e "$boot_dir" ] || fail "target boot directory already exists: $boot_dir"
[ ! -e "$grub_source" ] || fail "target GRUB source already exists: $grub_source"
[ ! -e "$hook_target" ] || fail "target initramfs hook already exists: $hook_target"

# Verify every critical staged module belongs to the new ABI and is signed.
for module in \
 snd-q6apm snd-soc-wsa884x snd-soc-x1e80100 q6apm-dai q6apm-lpass-dais \
 gpi spi-geni-qcom mshw0485_touch; do
    path=$(find "$candidate/rootfs/lib/modules/$release" -type f -name "$module.ko.zst" -print -quit)
    [ -n "$path" ] || fail "staged module missing: $module"
    [ "$(modinfo -F vermagic "$path" | awk '{print $1}')" = "$release" ] || fail "$module vermagic mismatch"
    [ -n "$(modinfo -F signer "$path")" ] || fail "$module is unsigned"
done

printf 'Candidate verification passed.\n'
printf '  release: %s\n' "$release"
printf '  known-good entry retained: sp11-audio-vi\n'
printf '  new entry: %s\n' "$entry_id"
printf '  saved GRUB default will not be changed\n'
printf '  reboot will not be performed\n'
[ "$mode" = install ] || exit 0

work=$(mktemp -d /var/tmp/sp11-audio-diag-install.XXXXXX)
installed_modules=0
installed_boot=0
installed_hook=0
installed_grub=0
complete=0
cleanup() {
    status=$?
    if [ "$status" -ne 0 ] && [ "$complete" -ne 1 ]; then
        [ "$installed_grub" -eq 1 ] && rm -f -- "$grub_source"
        [ "$installed_hook" -eq 1 ] && rm -f -- "$hook_target"
        [ "$installed_boot" -eq 1 ] && rm -rf -- "$boot_dir"
        [ "$installed_modules" -eq 1 ] && rm -rf -- "$module_dir"
        depmod -a 2>/dev/null || true
        update-grub >/dev/null 2>&1 || true
    fi
    rm -rf -- "$work"
    exit "$status"
}
trap cleanup EXIT HUP INT TERM

# Stage then atomically publish the module tree.
cp -a "$candidate/rootfs/lib/modules/$release" "$work/modules"
install -d -m 0755 /lib/modules
mv "$work/modules" "$module_dir"
installed_modules=1
depmod -a "$release"

install -d -m 0755 "$boot_dir"
installed_boot=1
install -m 0644 "$candidate/boot/vmlinuz-$release" "$boot_dir/vmlinuz-$release"
install -m 0644 "$candidate/boot/System.map-$release" "$boot_dir/System.map-$release"
install -m 0644 "$candidate/boot/config-$release" "$boot_dir/config-$release"
install -m 0644 "$candidate/boot/x1e80100-microsoft-denali-sp11-audio-diag-observe-phase91.dtb" \
    "$boot_dir/x1e80100-microsoft-denali-sp11-audio-diag-observe-phase91.dtb"

install -m 0755 "$project_root/01-audio/deploy/diagnostic-observe/sp11-audio-diag-observe-phase91" "$hook_target"
installed_hook=1

# Build in a private initramfs configuration copy. The candidate-specific hook
# selects the Phase91 overrides; old version-gated SP11 hooks remain inert.
cp -a /etc/initramfs-tools "$work/initramfs-tools"
mkinitramfs -d "$work/initramfs-tools" -o "$work/initrd.img-$release" "$release"
unmkinitramfs "$work/initrd.img-$release" "$work/unpacked"
# 2026-07-30: snd-q6apm and snd-soc-wsa884x removed from this check. They are
# never added by the candidate hook and are absent from the known-good
# audio-vi initramfs too (verified: 0 hits). The audio stack loads from the
# root filesystem after boot. Requiring them here made --install unreachable.
# Adding them to the hook instead would change probe ordering versus the
# baseline boot and contaminate the GET_CFG/port-mask comparison.
for module in gpi spi-geni-qcom mshw0485_touch; do
    embedded=$(find "$work/unpacked" -type f -name "$module.ko*" -print -quit)
    [ -n "$embedded" ] || fail "generated initramfs is missing $module"
    [ "$(modinfo -F vermagic "$embedded" | awk '{print $1}')" = "$release" ] || fail "initramfs $module vermagic mismatch"
done
install -m 0644 "$work/initrd.img-$release" "$boot_dir/initrd.img-$release"

# Derive the new isolated entry from the known-good AUDIO VI entry so every
# validated root/touch/platform argument remains identical.
python3 - "$known_good_grub" "$work/grub" <<'PY'
from pathlib import Path
import sys
src=Path(sys.argv[1]).read_text()
repl={
"Ubuntu SP11 7.1.5 AUDIO VI (complete WSA feedback, protected fallback)":"Ubuntu SP11 7.1.5 AUDIO DIAGNOSTIC (observation only)",
"--id 'sp11-audio-vi'":"--id 'sp11-audio-diag-observe'",
"/boot/sp11-7.1.5-audio-vi/x1e80100-microsoft-denali-sp11-audio-vi-phase91.dtb":"/boot/sp11-7.1.5-audio-diag-observe/x1e80100-microsoft-denali-sp11-audio-diag-observe-phase91.dtb",
"/boot/sp11-7.1.5-audio-vi/vmlinuz-7.1.5-sp11-audio-vi":"/boot/sp11-7.1.5-audio-diag-observe/vmlinuz-7.1.5-sp11-audio-diag-observe",
"sp11_entry=7.1.5-sp11-audio-vi":"sp11_entry=7.1.5-sp11-audio-diag-observe",
"/boot/sp11-7.1.5-audio-vi/initrd.img-7.1.5-sp11-audio-vi":"/boot/sp11-7.1.5-audio-diag-observe/initrd.img-7.1.5-sp11-audio-diag-observe",
}
for old,new in repl.items():
    if old not in src: raise SystemExit(f'missing expected GRUB token: {old}')
    src=src.replace(old,new)
Path(sys.argv[2]).write_text(src)
PY
install -m 0755 "$work/grub" "$grub_source"
installed_grub=1
update-grub
grub-script-check /boot/grub/grub.cfg
for token in "$entry_id" "$release" 'sp11_entry=7.1.5-sp11-audio-diag-observe'; do
    grep -q -- "$token" /boot/grub/grub.cfg || fail "generated GRUB config lacks $token"
done

{
    printf 'installed=%s\n' "$(date --iso-8601=seconds)"
    printf 'kernel_release=%s\n' "$release"
    printf 'entry_id=%s\n' "$entry_id"
    printf 'known_good_entry=sp11-audio-vi\n'
    printf 'saved_grub_default_changed=false\n'
    printf 'one_shot_boot_armed=false\n'
    printf 'reboot_performed=false\n'
    sha256sum "$boot_dir"/*
} > "$boot_dir/DEPLOYMENT-MANIFEST.txt"

complete=1
printf '\nInstalled isolated diagnostic entry: %s\n' "$entry_id"
printf 'No default was changed and no reboot was requested.\n'
printf 'Select the entry manually when ready.\n'
