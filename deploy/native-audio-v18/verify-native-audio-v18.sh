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
    trap 'rm -f "$tmp"' EXIT
    log=$(mktemp)
    trap 'rm -f "$tmp" "$log"' EXIT
    if ! alsatplg -c X1E80100-Microsoft-Surface-Pro-11-VA-TX-AB-v16.conf -o "$tmp" 2>"$log"; then
        cat "$log" >&2
        exit 1
    fi
    cmp -s "$tmp" X1E80100-Microsoft-Surface-Pro-11-VA-TX-AB-v16-tplg.bin
    echo "topology round-trip: byte-identical"
fi

python3 - "$ROOT" <<'PY'
import json, pathlib, sys
root = pathlib.Path(sys.argv[1])
p = root / "artifacts/2026-08-26-native-mic-v18-parity/parity-summary.json"
j = json.loads(p.read_text())
a = j["acceptance"]
assert a["pass"] is True
score = float(a["parity_index_percent"])
assert 95.0 <= score <= 99.0
assert j["recordings"]["windows_raw"]["sha256"] == "62f2e77232c202a32c46bba8117c1741eaa993975ab0a6ca16b27358e0a07ba7"
assert j["recordings"]["linux_v18"]["sha256"] == "8d3926ab271c47d3de435be0d047180dac1830ff70634d4e4a5b6544da0e3f0e"
print(f"Windows/Linux microphone parity: {score:.2f}% PASS")
PY

ucm="$ROOT/deploy/ucm2/Qualcomm/x1e80100/SP11-HiFi.conf"
echo "9d36df8570b85f1dcecc385a8f85fa2d1e1058ef8efedee6ae2ce49dc259a06a  $ucm" | sha256sum -c -

if [[ ${1:-} == --live ]]; then
    grep -q 'sp11_entry=7.1.5-sp11-dmic-broker-div4-v18' /proc/cmdline
    [[ $(cat /sys/module/snd_soc_lpass_macro_common/srcversion) == 2EA7312A851E75A7C860F82 ]]
    [[ $(cat /sys/module/snd_soc_lpass_va_macro/srcversion) == DC4373218C279E16F550900 ]]
    [[ $(cat /sys/module/snd_soc_lpass_tx_macro/srcversion) == 835AF5272E94DB266E85D55 ]]
    grep -q 'MultiMedia3 Capture' /proc/asound/pcm
    if command -v wpctl >/dev/null 2>&1; then
        uid=$(id -u)
        if [[ -d /run/user/$uid ]]; then
            export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$uid}
            export PIPEWIRE_RUNTIME_DIR=${PIPEWIRE_RUNTIME_DIR:-$XDG_RUNTIME_DIR}
            export DBUS_SESSION_BUS_ADDRESS=${DBUS_SESSION_BUS_ADDRESS:-unix:path=/run/user/$uid/bus}
        fi
        status=$(wpctl status)
        grep -q 'Built-in Audio Speaker playback' <<<"$status"
        grep -q 'Built-in Audio Internal microphone array' <<<"$status"
    fi
    echo "live Native Audio v18 identity: PASS"
fi
