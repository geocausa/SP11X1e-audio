#!/usr/bin/env bash
set -euo pipefail
[[ $EUID -eq 0 ]] || { echo 'run with sudo' >&2; exit 1; }
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
RUN_USER=${SUDO_USER:-root}
RUN_UID=$(id -u "$RUN_USER")

# Promotion is intentionally conservative: only promote from the already tested
# v19c runtime, never merely because files exist on disk.
grep -q 'sp11_entry=7.1.5-sp11-fullio-v19c' /proc/cmdline || {
    echo 'refusing promotion: current boot is not accepted FullIO v19c' >&2
    exit 2
}
sudo -u "$RUN_USER" XDG_RUNTIME_DIR="/run/user/$RUN_UID" \
    DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$RUN_UID/bus" \
    "$HERE/verify-native-audio-v19c.sh" --live

ROOT_DEV=$(findmnt -n -o SOURCE /)
ROOT_UUID=$(blkid -s UUID -o value "$ROOT_DEV")
[[ -n $ROOT_UUID ]] || { echo 'could not resolve root UUID' >&2; exit 2; }
D=/boot/sp11-7.1.5-audio-fullio-v19c
OUT=/etc/grub.d/76_sp11_audio_fullio_v19c
[[ -f $D/vmlinuz-7.1.5-sp11-render-parity-v4+ ]]
[[ -f $D/initrd.img-7.1.5-sp11-fullio-v19c ]]
[[ -f $D/x1e80100-microsoft-denali-sp11-fullio-v19c.dtb ]]

cat >"$OUT" <<GRUB
#!/bin/sh
exec tail -n +3 \$0
menuentry 'SP11 Audio FullIO v19c — collision-free Golden + MicArray' --id 'sp11-audio-fullio-v19c' --class ubuntu --class gnu-linux --class gnu --class os {
 load_video
 set gfxpayload=keep
 insmod gzio
 insmod part_gpt
 insmod ext2
 insmod fdt
 search --no-floppy --fs-uuid --set=root $ROOT_UUID
 devicetree $D/x1e80100-microsoft-denali-sp11-fullio-v19c.dtb
 linux $D/vmlinuz-7.1.5-sp11-render-parity-v4+ root=UUID=$ROOT_UUID ro clk_ignore_unused pd_ignore_unused cma=128M efi=noruntime quiet splash console=tty0 crashkernel=2G-4G:320M,4G-32G:512M,32G-64G:1024M,64G-128G:2048M,128G-:4096M mshw0485_touch.windows_init_parity=1 mshw0485_touch.parity_linux_power=1 mshw0485_touch.windows_read_cadence=1 mshw0485_touch.parity_display_bitmap=1 mshw0485_touch.parity_stitching_flag=0 mshw0485_touch.parity_hinge_angle=400 mshw0485_touch.parity_fast_host_id=400 mshw0485_touch.parity_report56_identity=0xbc,0xe6,0x4a,0x2e,0x86,0x78 mshw0485_touch.parity_report56_flag=0 mshw0485_touch.parity_cfu_inventory=1 mshw0485_touch.parity_cfu_offer=0x00,0x00,0x12,0x00,0x89,0x14,0x00,0x3f,0xff,0xff,0xff,0xff,0x04,0x04,0x75,0x00 mshw0485_touch.parity_heat_input=1 mshw0485_touch.behavior_v2=1 mshw0485_touch.host_fault_recovery=1 mshw0485_touch.ready_quiesce=1 sp11_cps_parity_v2=1 sp11_cps_v3=1 sp11_volume_transaction=1 sp11_softpause=1 sp11_headroom_link=1 sp11_wsa_clockstop=1 sp11_visense_parity=1 sp11_wsa_windows_init=1 sp11_wsa_macro0db_oracle=1 sp11_wsa_winproducer_nohd2_v3=1 sp11_wsa_csren0_v4=1 sp11_wsa_csren0_v5_idlegated=1 sp11_wsa_windows_3state_v26=1 sp11_wsa_windows_3state_retain_v27=1 sp11_wsa_dp2_offsetctrl2_v28=1 soundwire_qcom.sp11_feedback_active_offset2_zero=1 soundwire_qcom.sp11_cps_pcm_route_105c=1 sp11_entry=7.1.5-sp11-fullio-v19c
 initrd $D/initrd.img-7.1.5-sp11-fullio-v19c
}
GRUB
chmod 0755 "$OUT"
update-grub
grub-set-default sp11-audio-fullio-v19c

echo 'Installed FullIO v19c GRUB entry and saved it as default. No reboot performed.'
echo 'Rollback entries retained: sp11-audio-dmic-broker-div4-v18 and sp11-audio-golden-v33-topcfg1-physical-vi.'
