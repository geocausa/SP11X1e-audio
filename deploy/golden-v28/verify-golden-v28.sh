#!/usr/bin/env bash
set -euo pipefail
D=/boot/sp11-7.1.5-audio-rpv4-macro84-winproducer-nohd2-wsa-windows-3state-retain-dp2offset2-v28-idlegated
check() {
  local expected=$1 file=$2
  [[ -f "$file" ]] || { echo "MISSING $file" >&2; return 1; }
  local got
  got=$(sha256sum "$file" | awk '{print $1}')
  [[ "$got" == "$expected" ]] || { echo "HASH FAIL $file" >&2; echo " expected $expected" >&2; echo " got      $got" >&2; return 1; }
  echo "OK $file"
}
check bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a "$D/vmlinuz-7.1.5-sp11-render-parity-v4+"
check 94f6716ea210c0b3d82eb3403f08102de90be13ea526c1fb4f273324db9f754d "$D/initrd.img-7.1.5-sp11-render-parity-v4+"
check 3530e3426c500d664be6ed3ef066d1b548025ba8286a5810e8b98c591b6555ca "$D/x1e80100-microsoft-denali-sp11-audio-rpv4-macro84-winproducer-nohd2-wsa-windows-3state-retain-dp2offset2-v28-idlegated.dtb"
if [[ -f "$HOME/.local/lib/sp11-dolby/sp11_dolby_windows_chain.so" ]]; then
  check ee02ff299146b0ed8387fda1da820a8ed7c9612fc4a5946ed921e5c0dca715d9 "$HOME/.local/lib/sp11-dolby/sp11_dolby_windows_chain.so"
fi
if [[ -f "$HOME/.local/bin/sp11-dolby-volume-sync" ]]; then
  check c1207fd5ffc134650276905ecbca22ceddfd282a261db30cc4b8d0386fe213c5 "$HOME/.local/bin/sp11-dolby-volume-sync"
fi
if [[ -f "$HOME/.local/bin/sp11-windows-volume-transaction-sync" ]]; then
  check 19ec64ba0275e6c2cead1fde6d097eac428a80fca34bfece87380d3229455f4b "$HOME/.local/bin/sp11-windows-volume-transaction-sync"
fi
if grep -qw 'sp11_wsa_dp2_offsetctrl2_v28=1' /proc/cmdline; then
  echo "LIVE: v28 DP2 prerequisite present"
fi
