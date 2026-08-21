#!/usr/bin/env bash
set -euo pipefail
D=/boot/sp11-7.1.5-audio-v32-feedback-exact-golden
KREL=7.1.5-sp11-render-parity-v4+
TOPO=/usr/lib/firmware/qcom/x1e80100/X1E80100-Microsoft-Surface-Pro-11-Render-Parity-tplg.bin
check() {
  local expected=$1 file=$2 got
  [[ -f "$file" ]] || { echo "MISSING $file" >&2; return 1; }
  got=$(sha256sum "$file" | awk '{print $1}')
  [[ "$got" == "$expected" ]] || { echo "HASH FAIL $file expected=$expected got=$got" >&2; return 1; }
  echo "OK $file"
}
check bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a "$D/vmlinuz-$KREL"
check 227a5f1531077306dfdcd244d7e28ae9734858ccf9cd6129203e3bb5a943769d "$D/initrd.img-$KREL-feedback-v32-exact-golden"
check 3530e3426c500d664be6ed3ef066d1b548025ba8286a5810e8b98c591b6555ca "$D/x1e80100-microsoft-denali-sp11-audio-rpv4-macro84-winproducer-nohd2-wsa-windows-3state-retain-dp2offset2-v28-idlegated.dtb"
check 1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d "$TOPO"
check 864e13cef93da7213609ae8858748d1eb6faffa011efcf61c00692bdf8ded318 "/lib/modules/$KREL/kernel/sound/soc/codecs/snd-soc-lpass-wsa-macro.ko.zst"
check 7ce1a39f867bc6914e73f71508512ff94b028e9bf243c434c1302c82c81b0c6b "/lib/modules/$KREL/kernel/sound/soc/codecs/snd-soc-wsa884x.ko.zst"
check 6c580955cb3e977298a0f5a8fb6c60ce13fe365e28ec9c22d5961b08e5959e88 "/lib/modules/$KREL/kernel/drivers/soundwire/soundwire-qcom.ko.zst"
check_live() {
  local module=$1 expected=$2 got
  [[ -r /sys/module/$module/srcversion ]] || { echo "LIVE MODULE MISSING $module" >&2; return 1; }
  got=$(cat /sys/module/$module/srcversion)
  [[ "$got" == "$expected" ]] || { echo "LIVE SRCVERSION FAIL $module expected=$expected got=$got" >&2; return 1; }
  echo "OK live srcversion $module=$got"
}
if grep -qw 'sp11_entry=7.1.5-sp11-v32-feedback-exact-golden' /proc/cmdline; then
  check_live snd_soc_lpass_wsa_macro F32C7A03F713D1B20F0BF78
  check_live snd_soc_wsa884x 5859E70AFD0A1D420E8ADD4
  check_live snd_soc_x1e80100 13326073E27DFA035180C56
  check_live soundwire_qcom D008A3D6B585C11BE023992
  check_live snd_q6apm 687B16CF9C43B43E90C0746
  grep -qw 'soundwire_qcom.sp11_feedback_active_offset2_zero=1' /proc/cmdline
  grep -qw 'soundwire_qcom.sp11_cps_pcm_route_105c=1' /proc/cmdline
  echo "OK v32 feedback parameters active"
fi
echo "GOLDEN v32 verification PASS"
