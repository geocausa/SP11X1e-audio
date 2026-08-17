#!/usr/bin/env bash
set -euo pipefail
D=/boot/sp11-7.1.5-audio-golden-v30-mute-dp1-dp3
check() {
  local expected=$1 file=$2 got
  [[ -f "$file" ]] || { echo "MISSING $file" >&2; return 1; }
  got=$(sha256sum "$file" | awk '{print $1}')
  [[ "$got" == "$expected" ]] || { echo "HASH FAIL $file expected=$expected got=$got" >&2; return 1; }
  echo "OK $file"
}
check bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a "$D/vmlinuz-7.1.5-sp11-render-parity-v4+"
check 51e17bd4952d785ef511b925f8835a415d7243ff95a7dcb97670e0879d5f0944 "$D/initrd.img-7.1.5-sp11-render-parity-v4+"
check 3530e3426c500d664be6ed3ef066d1b548025ba8286a5810e8b98c591b6555ca "$D/x1e80100-microsoft-denali-sp11-audio-rpv4-macro84-winproducer-nohd2-wsa-windows-3state-retain-dp2offset2-v28-idlegated.dtb"
check_live() {
  local module=$1 expected=$2
  if [[ -r /sys/module/$module/srcversion ]]; then
    local got; got=$(cat /sys/module/$module/srcversion)
    [[ "$got" == "$expected" ]] || { echo "LIVE SRCVERSION FAIL $module expected=$expected got=$got" >&2; return 1; }
    echo "OK live srcversion $module=$got"
  fi
}
if grep -qw 'sp11_entry=7.1.5-sp11-golden-v30-mute-dp1-dp3' /proc/cmdline; then
  check_live snd_q6apm F50BA24BDA6FAC8AE991A54
  check_live snd_soc_wsa884x A4F2E38C5C27D13E327887B
  check_live snd_soc_lpass_wsa_macro 4AF6F542C17BA6DD46586DA
  check_live soundwire_qcom 406975A3ED60935B31491BF
  amixer -D hw:0 controls | grep -F "SP11 Windows Endpoint Mute" >/dev/null
  echo "OK exact endpoint mute control present"
fi
