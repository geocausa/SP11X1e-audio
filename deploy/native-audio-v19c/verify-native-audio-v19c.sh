#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
if ROOT=$(git -C "$HERE" rev-parse --show-toplevel 2>/dev/null); then
    :
else
    ROOT=$(cd "$HERE/../.." && pwd)
fi
cd "$HERE"
sha256sum -c SHA256SUMS

if command -v alsatplg >/dev/null 2>&1; then
    tmp=$(mktemp)
    log=$(mktemp)
    trap 'rm -f "$tmp" "$log"' EXIT
    if ! alsatplg -c X1E80100-Microsoft-Surface-Pro-11-FullIO-v19c0.conf -o "$tmp" 2>"$log"; then
        cat "$log" >&2
        exit 1
    fi
    cmp -s "$tmp" X1E80100-Microsoft-Surface-Pro-11-FullIO-v19c0-tplg.bin
    echo 'FullIO v19c topology round-trip: byte-identical'
fi

echo '1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d  '"$ROOT"'/deploy/render-parity/X1E80100-Microsoft-Surface-Pro-11-Render-Parity-tplg.bin' | sha256sum -c -
echo '9d36df8570b85f1dcecc385a8f85fa2d1e1058ef8efedee6ae2ce49dc259a06a  '"$ROOT"'/deploy/ucm2/Qualcomm/x1e80100/SP11-HiFi.conf' | sha256sum -c -
python3 -m py_compile "$ROOT/tools/build_sp11_native_audio_topology.py"

if [[ ${1:-} == --live ]]; then
    grep -q 'sp11_entry=7.1.5-sp11-fullio-v19c' /proc/cmdline
    [[ $(cat /sys/module/snd_soc_lpass_macro_common/srcversion) == 2EA7312A851E75A7C860F82 ]]
    [[ $(cat /sys/module/snd_soc_lpass_va_macro/srcversion) == DC4373218C279E16F550900 ]]
    [[ $(cat /sys/module/snd_soc_lpass_tx_macro/srcversion) == 835AF5272E94DB266E85D55 ]]

    boot=/boot/sp11-7.1.5-audio-fullio-v19c
    echo 'bca0a336c15d2995c61b8df9d449afb9df5fc8776a3da1ad034616f917bb428a  '"$boot"'/vmlinuz-7.1.5-sp11-render-parity-v4+' | sha256sum -c -
    echo 'ac3ba64bd1c6bd6b8c0dc01b9836fb7466128fcc687903673b6fd598ebefb66d  '"$boot"'/initrd.img-7.1.5-sp11-fullio-v19c' | sha256sum -c -
    echo '2fcfa738c229b32764ff2722847cf4056b3153c64a12f8490429309f29df6d00  '"$boot"'/x1e80100-microsoft-denali-sp11-fullio-v19c.dtb' | sha256sum -c -
    echo 'e7bb06a03e7bd9b869825a51775355a6743477d1579d78eb09fad5881cfb20f0  /lib/firmware/qcom/x1e80100/X1E80100-Microsoft-Surface-Pro-11-FullIO-v19c0-tplg.bin' | sha256sum -c -

    model=$(tr -d '\0' </sys/firmware/devicetree/base/sound/model)
    [[ $model == X1E80100-Microsoft-Surface-Pro-11-FullIO-v19c0 ]]
    grep -q 'MultiMedia3 Capture' /proc/asound/pcm
    amixer -D hw:0 controls | grep -q "SP11 Windows Volume Transaction"
    amixer -D hw:0 controls | grep -q "SP11 Windows Endpoint Mute"

    uid=$(id -u)
    export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$uid}
    export PIPEWIRE_RUNTIME_DIR=${PIPEWIRE_RUNTIME_DIR:-$XDG_RUNTIME_DIR}
    export DBUS_SESSION_BUS_ADDRESS=${DBUS_SESSION_BUS_ADDRESS:-unix:path=$XDG_RUNTIME_DIR/bus}
    status=$(wpctl status -n)
    grep -q 'alsa_output.platform-sound.HiFi__Speaker__sink' <<<"$status"
    grep -q 'alsa_input.platform-sound.HiFi__Mic__source' <<<"$status"
    grep -q 'effect_input.sp11_ubig' <<<"$status"
    pw-dump | python3 -c '
import json,sys
d=json.load(sys.stdin)
props=[o.get("info",{}).get("props",{}) for o in d]
raw=next((p for p in props if p.get("node.name")=="alsa_output.platform-sound.HiFi__Speaker__sink"),None)
if raw is None: raise SystemExit("physical speaker backend missing")
if raw.get("node.hidden") not in (True,"true"): raise SystemExit("physical speaker backend is not hidden")
if str(raw.get("priority.session")) != "0": raise SystemExit("physical speaker backend priority is not zero")
if any(p.get("node.name")=="effect_input.sp11_ubig_bypass" for p in props): raise SystemExit("diagnostic bypass unexpectedly active")
'

    if command -v ubigctl >/dev/null 2>&1 && [[ -e $XDG_RUNTIME_DIR/ubig-control-v2 ]]; then
        ctl=$(UBIG_CONTROL_PATH="$XDG_RUNTIME_DIR/ubig-control-v2" ubigctl status)
        req=$(sed -n 's/^request_generation=//p' <<<"$ctl")
        ack=$(sed -n 's/^ack_generation=//p' <<<"$ctl")
        err=$(sed -n 's/^last_error=//p' <<<"$ctl")
        [[ $req == "$ack" ]]
        [[ $err == 0 ]]
    fi

    if [[ -x $HOME/.local/bin/sp11-ubig ]]; then
        cmp -s "$ROOT/deploy/ubig/sp11-ubig" "$HOME/.local/bin/sp11-ubig"
    fi
    echo 'live FullIO v19c identity and desktop path: PASS'
fi
