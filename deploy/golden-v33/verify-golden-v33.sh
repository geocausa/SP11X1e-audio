#!/usr/bin/env bash
set -euo pipefail
D=/boot/sp11-7.1.5-audio-golden-v33-topcfg1-physical-vi
KREL=7.1.5-sp11-render-parity-v4+
TOPO=/usr/lib/firmware/qcom/x1e80100/X1E80100-Microsoft-Surface-Pro-11-Render-Parity-tplg.bin
check(){ local e=$1 f=$2 g; [[ -f $f ]] || { echo "MISSING $f" >&2; exit 1; }; g=$(sha256sum "$f"|awk '{print $1}'); [[ $g == $e ]] || { echo "HASH FAIL $f expected=$e got=$g" >&2; exit 1; }; echo "OK $f"; }
check_live(){ local m=$1 e=$2 g; g=$(cat "/sys/module/$m/srcversion"); [[ $g == $e ]] || { echo "LIVE SRCVERSION FAIL $m expected=$e got=$g" >&2; exit 1; }; echo "OK live $m=$g"; }
check bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a "$D/vmlinuz-$KREL"
check 19db416046a363821f1d0887a43562d69c3593f6df85b7b16017adcc6bc59a44 "$D/initrd.img-$KREL-golden-v33-topcfg1"
check 3530e3426c500d664be6ed3ef066d1b548025ba8286a5810e8b98c591b6555ca "$D/x1e80100-microsoft-denali-sp11-audio-rpv4-macro84-winproducer-nohd2-wsa-windows-3state-retain-dp2offset2-v28-idlegated.dtb"
check 1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d "$TOPO"
check 39674078b0781323464b3de647caf9db0b25cde51d447e8e0253630de91d3f2d "/lib/modules/$KREL/kernel/sound/soc/codecs/snd-soc-lpass-wsa-macro.ko.zst"
check 7ce1a39f867bc6914e73f71508512ff94b028e9bf243c434c1302c82c81b0c6b "/lib/modules/$KREL/kernel/sound/soc/codecs/snd-soc-wsa884x.ko.zst"
check 6c580955cb3e977298a0f5a8fb6c60ce13fe365e28ec9c22d5961b08e5959e88 "/lib/modules/$KREL/kernel/drivers/soundwire/soundwire-qcom.ko.zst"
if grep -qw sp11_entry=7.1.5-sp11-golden-v33-topcfg1-physical-vi /proc/cmdline; then
 check_live snd_soc_lpass_wsa_macro 3FAA616CDE10DDBF9D90D6F
 check_live snd_soc_wsa884x 5859E70AFD0A1D420E8ADD4
 check_live snd_soc_x1e80100 13326073E27DFA035180C56
 check_live soundwire_qcom D008A3D6B585C11BE023992
 check_live snd_q6apm 687B16CF9C43B43E90C0746
fi
echo 'GOLDEN v33 verification PASS'
