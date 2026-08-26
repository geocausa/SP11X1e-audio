#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO=$(cd -- "$SCRIPT_DIR/../.." && pwd)
PACK=${UBIG_SP11_STAGEB_PACK:-$HOME/.local/share/ubig-private/sp11-stageb-v4.pack}
PACK_SHA=${UBIG_SP11_STAGEB_PACK_SHA:-30b9b8ce8dace4a9f5dee2c2defa7da2d9b8431cf68fb323f8d2c3e4e3c942df}
LIBDIR=${UBIG_LIBDIR:-$HOME/.local/lib/ubig}
PLUGIN=$LIBDIR/ubig-sp11.so
BINDIR=$HOME/.local/bin
CONFIG_HOME=${XDG_CONFIG_HOME:-$HOME/.config}
PWCONF=$CONFIG_HOME/pipewire/filter-chain.conf.d/98-sp11-ubig.conf
UNITDIR=$CONFIG_HOME/systemd/user
FILTER_DROPIN=$UNITDIR/filter-chain.service.d/50-ubig-production.conf
RUNTIME_DIR=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}
export XDG_RUNTIME_DIR=$RUNTIME_DIR
export DBUS_SESSION_BUS_ADDRESS=${DBUS_SESSION_BUS_ADDRESS:-unix:path=$RUNTIME_DIR/bus}

[[ -f $PACK ]] || { echo "missing private Stage-B pack: $PACK" >&2; exit 2; }
[[ $(sha256sum "$PACK"|awk '{print $1}') == $PACK_SHA ]] || { echo 'owner pack hash mismatch' >&2; exit 3; }
make -C "$REPO/ubig" candidate-ladspa
UBIG_SP11_STAGEB_PACK="$PACK" make -C "$REPO/ubig" candidate-control-check
SRC=$REPO/ubig/build/ubig-sp11-candidate.so
bad=$(readelf -d "$SRC" | awk '/NEEDED/{gsub(/\[|\]/,"",$5);print $5}' | grep -Ev '^(libm\.so\.6|libc\.so\.6|ld-linux-aarch64\.so\.1)$' || true)
[[ -z $bad ]] || { echo "unexpected runtime dependency: $bad" >&2; exit 4; }
if strings "$SRC" | grep -Eq 'DolbyAPOVR\.dll|DolbyAPOvlldp150\.dll|sp11_pe_load'; then
  echo 'refusing UbiG plugin containing vendor-loader reference' >&2; exit 5
fi

OLD_ID=$(pw-dump | python3 -c 'import sys,json;d=json.load(sys.stdin);print(next((o["id"] for o in d if o.get("info",{}).get("props",{}).get("node.name")=="effect_input.sp11_ubig"),""))')
OLD_VOL=0.10; OLD_MUTE=0
if [[ -n $OLD_ID ]]; then
  state=$(wpctl get-volume "$OLD_ID" 2>/dev/null || true)
  v=$(sed -n 's/.*Volume: \([0-9.]*\).*/\1/p' <<<"$state"); [[ -n $v ]] && OLD_VOL=$v
  grep -q '\[MUTED\]' <<<"$state" && OLD_MUTE=1 || true
fi

mkdir -p "$LIBDIR" "$BINDIR" "$(dirname "$PWCONF")" "$UNITDIR" "$(dirname "$FILTER_DROPIN")"
install -m0755 "$SRC" "$PLUGIN"
cc -O2 -Wall -Wextra -o "$LIBDIR/tlv_write" "$REPO/tools/tlv_write.c" -lasound
for f in sp11_volume_sync_dispatch.py sp11_ubig_volume_sync.py sp11_windows_volume_transaction_sync.py sp11_msiir_volume_sync.py sp11_ubig_monitor_link.py; do
  case $f in
    sp11_volume_sync_dispatch.py) out=sp11-volume-sync-dispatch;;
    sp11_ubig_volume_sync.py) out=sp11-ubig-volume-sync;;
    sp11_windows_volume_transaction_sync.py) out=sp11-windows-volume-transaction-sync;;
    sp11_msiir_volume_sync.py) out=sp11-msiir-volume-sync;;
    sp11_ubig_monitor_link.py) out=sp11-ubig-monitor-link;;
  esac
  install -m0755 "$SCRIPT_DIR/$f" "$BINDIR/$out"
done
install -m0755 "$SCRIPT_DIR/sp11-ubig" "$BINDIR/sp11-ubig"
install -m0644 "$SCRIPT_DIR/sp11-ubig-volume-sync.service" "$UNITDIR/sp11-ubig-volume-sync.service"
install -m0644 "$SCRIPT_DIR/sp11-ubig-monitor-link.service" "$UNITDIR/sp11-ubig-monitor-link.service"
install -m0644 "$SCRIPT_DIR/sp11-msiir-volume-sync.service" "$UNITDIR/sp11-msiir-volume-sync.service"
python3 - "$SCRIPT_DIR/98-sp11-ubig.conf.in" "$PWCONF" "$PLUGIN" <<'PY'
from pathlib import Path
import sys
src,dst,plugin=sys.argv[1:]
s=Path(src).read_text().replace('@PLUGIN@',plugin)
if '@PLUGIN@' in s: raise SystemExit('UbiG config expansion failed')
Path(dst).write_text(s)
PY
cat > "$FILTER_DROPIN" <<EOD
[Service]
Environment=UBIG_PROFILE=movie
Environment=UBIG_GEQ=off
Environment=UBIG_SP11_STAGEB_PACK=$PACK
Environment=UBIG_CONTROL_PATH=$RUNTIME_DIR/ubig-control-v2
MemoryDenyWriteExecute=yes
EOD

# A diagnostic mic/parity session may deliberately mask the dedicated filter
# host and leave only the transparent bypass. A production install must retire
# that mask explicitly; otherwise the controller can update a valid mmap page
# with no DSP consumer behind it.
[[ -f /usr/lib/systemd/user/filter-chain.service || -f /lib/systemd/user/filter-chain.service ]] || {
  echo 'system filter-chain.service is missing' >&2; exit 6;
}
systemctl --user unmask filter-chain.service >/dev/null
systemctl --user enable filter-chain.service sp11-ubig-volume-sync.service sp11-ubig-monitor-link.service >/dev/null

# Retire candidate-only overrides only after the production files exist.
rm -f "$UNITDIR/filter-chain.service.d/zz-ubig-candidate.conf"
rm -f "$UNITDIR/sp11-ubig-volume-sync.service.d/zz-ubig-candidate.conf"
rm -f "$UNITDIR/sp11-msiir-volume-sync.service.d/zz-ubig-candidate.conf"

systemctl --user daemon-reload
systemctl --user stop sp11-ubig-monitor-link.service sp11-msiir-volume-sync.service sp11-ubig-volume-sync.service filter-chain.service || true
systemctl --user start sp11-ubig-volume-sync.service
systemctl --user start filter-chain.service
systemctl --user start sp11-ubig-monitor-link.service sp11-msiir-volume-sync.service
sleep 3
NEW_ID=$(pw-dump | python3 -c 'import sys,json;d=json.load(sys.stdin);print(next((o["id"] for o in d if o.get("info",{}).get("props",{}).get("node.name")=="effect_input.sp11_ubig"),""))')
[[ -n $NEW_ID ]] || { echo 'production UbiG sink missing after restart' >&2; exit 20; }
wpctl set-volume "$NEW_ID" "$OLD_VOL"
wpctl set-mute "$NEW_ID" "$OLD_MUTE"
wpctl set-default "$NEW_ID"
sleep 1
DEFAULT_LINE=$(wpctl status -n | awk '/Default Configured Devices:/{p=1;next} p && /Audio\/Sink/{print;exit}')
grep -q 'effect_input.sp11_ubig' <<<"$DEFAULT_LINE" || { echo "UbiG was not persisted as default: $DEFAULT_LINE" >&2; exit 25; }
grep -q 'effect_input.sp11_ubig_bypass' <<<"$DEFAULT_LINE" && { echo 'transparent bypass remained default after production install' >&2; exit 26; }
PID=$(systemctl --user show filter-chain.service -p MainPID --value)
MAP=$(grep -F "$PLUGIN" "/proc/$PID/maps" || true)
[[ -n $MAP ]] || { echo 'production UbiG plugin not mapped' >&2; exit 21; }
grep -q '(deleted)' <<<"$MAP" && { echo 'production plugin mapping is deleted' >&2; exit 22; }
for u in filter-chain.service sp11-ubig-volume-sync.service sp11-ubig-monitor-link.service; do
  [[ $(systemctl --user is-active "$u") == active ]] || { echo "$u not active" >&2; exit 23; }
done
ms_state=$(systemctl --user is-active sp11-msiir-volume-sync.service 2>/dev/null || true)
if [[ $ms_state != active ]]; then
  ms_status=$(systemctl --user show sp11-msiir-volume-sync.service -p ExecMainStatus --value 2>/dev/null || echo 1)
  ms_result=$(systemctl --user show sp11-msiir-volume-sync.service -p Result --value 2>/dev/null || echo failed)
  [[ $ms_status == 0 && $ms_result == success ]] || { echo "sp11-msiir-volume-sync.service failed status=$ms_status result=$ms_result" >&2; exit 24; }
fi
printf 'UbiG production deployment PASS\nplugin=%s\nplugin_sha256=%s\nsink=%s\n' "$PLUGIN" "$(sha256sum "$PLUGIN"|awk '{print $1}')" "$NEW_ID"
