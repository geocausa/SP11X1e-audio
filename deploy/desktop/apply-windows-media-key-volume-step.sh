#!/usr/bin/env bash
# Match the native SP11 Windows hardware-volume key step without disabling
# notification/event sounds. Windows changes 12% -> 14% -> 12% per key pair;
# the Ubuntu GNOME default on this machine was 6% per key.
set -euo pipefail
# PiSlave/non-interactive shells do not necessarily inherit the desktop DBus
# environment. Resolve the standard per-user bus explicitly when available.
: "${XDG_RUNTIME_DIR:=/run/user/$(id -u)}"
export XDG_RUNTIME_DIR
if [[ -z "${DBUS_SESSION_BUS_ADDRESS:-}" && -S "$XDG_RUNTIME_DIR/bus" ]]; then
  export DBUS_SESSION_BUS_ADDRESS="unix:path=$XDG_RUNTIME_DIR/bus"
fi
SCHEMA=org.gnome.settings-daemon.plugins.media-keys
KEY=volume-step
WANT=2
command -v gsettings >/dev/null 2>&1 || { echo "gsettings not found" >&2; exit 1; }
gsettings writable "$SCHEMA" "$KEY" | grep -qx true || {
  echo "$SCHEMA $KEY is not writable" >&2
  exit 2
}
before=$(gsettings get "$SCHEMA" "$KEY")
gsettings set "$SCHEMA" "$KEY" "$WANT"
after=$(gsettings get "$SCHEMA" "$KEY")
[[ "$after" == "$WANT" ]] || { echo "failed to set $SCHEMA $KEY=$WANT (got $after)" >&2; exit 3; }
echo "Windows media-key volume step applied: $before -> $after"
