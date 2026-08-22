#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
PACK=${UBIG_SP11_STAGEB_PACK:-"$HOME/.local/share/ubig-private/sp11-stageb-v3.pack"}
LIBDIR=${UBIG_CANDIDATE_LIBDIR:-"$HOME/.local/lib/ubig-candidate"}
STAGEDIR=${UBIG_CANDIDATE_STAGEDIR:-"$HOME/.local/share/ubig-candidate"}
STATEDIR=${UBIG_CANDIDATE_STATEDIR:-"${XDG_STATE_HOME:-$HOME/.local/state}/ubig-candidate"}
CONFIG_HOME=${XDG_CONFIG_HOME:-"$HOME/.config"}
RUNTIME_DIR=${XDG_RUNTIME_DIR:-"/run/user/$(id -u)"}
# PiMaster/non-login maintenance shells may not inherit the graphical session
# environment even though the user manager and PipeWire session are alive.
# Resolve the standard per-user bus explicitly so activate/rollback reaches the
# same user services as the desktop session.
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-$RUNTIME_DIR}
export DBUS_SESSION_BUS_ADDRESS=${DBUS_SESSION_BUS_ADDRESS:-unix:path=$RUNTIME_DIR/bus}
PLUGIN="$LIBDIR/ubig-sp11-candidate.so"
STAGED_CONF="$STAGEDIR/98-sp11-ubig-candidate.conf"
HELPERDIR="$STAGEDIR/bin"
ACTIVE_CONF="$CONFIG_HOME/pipewire/filter-chain.conf.d/98-sp11-windows-dolby.conf"
ROLLBACK_CONF="$STATEDIR/98-sp11-windows-dolby.conf.rollback"
MARKER="$STATEDIR/active"
CONTROL="$RUNTIME_DIR/ubig-control-v2"
FILTER_DROPIN="$CONFIG_HOME/systemd/user/filter-chain.service.d/zz-ubig-candidate.conf"
VOLUME_DROPIN="$CONFIG_HOME/systemd/user/sp11-dolby-volume-sync.service.d/zz-ubig-candidate.conf"
MSIIR_DROPIN="$CONFIG_HOME/systemd/user/sp11-msiir-volume-sync.service.d/zz-ubig-candidate.conf"

systemctl_user() { systemctl --user "$@"; }

write_dropins() {
    mkdir -p "$(dirname "$FILTER_DROPIN")" "$(dirname "$VOLUME_DROPIN")" "$(dirname "$MSIIR_DROPIN")"
    cat > "$FILTER_DROPIN" <<EOD
[Service]
Environment=UBIG_PROFILE=movie
Environment=UBIG_GEQ=off
Environment=UBIG_SP11_STAGEB_PACK=$PACK
Environment=UBIG_CONTROL_PATH=$CONTROL
MemoryDenyWriteExecute=yes
EOD
    cat > "$VOLUME_DROPIN" <<EOD
[Service]
Environment=UBIG_CONTROL_PATH=$CONTROL
Environment=UBIG_CONTROL_FORMAT=ubig-v2
Environment=UBIG_VOLUME_HELPER_DIR=$HELPERDIR
ExecStart=
ExecStart=$HELPERDIR/sp11-volume-sync-dispatch
EOD
    cat > "$MSIIR_DROPIN" <<EOD
[Service]
Environment=UBIG_CONTROL_PATH=$CONTROL
Environment=UBIG_CONTROL_FORMAT=ubig-v2
Environment=UBIG_VOLUME_HELPER_DIR=$HELPERDIR
ExecStart=
ExecStart=$HELPERDIR/sp11-msiir-volume-sync
EOD
}

restart_graph() {
    systemctl_user daemon-reload
    systemctl_user stop filter-chain.service || true
    systemctl_user restart sp11-dolby-volume-sync.service || true
    systemctl_user restart sp11-msiir-volume-sync.service || true
    systemctl_user start filter-chain.service
    systemctl_user restart sp11-dolby-monitor-link.service || true
}

activate() {
    [ -x "$PLUGIN" ] || { echo "candidate not prepared: $PLUGIN" >&2; exit 2; }
    [ -f "$STAGED_CONF" ] || { echo "candidate config not prepared: $STAGED_CONF" >&2; exit 2; }
    [ -x "$HELPERDIR/sp11-volume-sync-dispatch" ] || { echo "candidate helpers not prepared: $HELPERDIR" >&2; exit 2; }
    [ -x "$HELPERDIR/sp11-windows-volume-transaction-sync" ] || { echo "candidate transaction helper missing: $HELPERDIR" >&2; exit 2; }
    [ -x "$HELPERDIR/sp11-msiir-volume-sync" ] || { echo "candidate MSIIR helper missing: $HELPERDIR" >&2; exit 2; }
    [ -f "$PACK" ] || { echo "private pack missing: $PACK" >&2; exit 2; }
    [ -f "$ACTIVE_CONF" ] || { echo "active Golden PipeWire config missing: $ACTIVE_CONF" >&2; exit 3; }
    if [ -e "$MARKER" ]; then
        echo "candidate already marked active; rollback first" >&2
        exit 4
    fi
    if grep -q 'label  = ubig_sp11_candidate' "$ACTIVE_CONF"; then
        echo "candidate config already active without rollback marker; refusing" >&2
        exit 5
    fi

    mkdir -p "$STATEDIR" "$(dirname "$ACTIVE_CONF")" "$RUNTIME_DIR"
    cp -a "$ACTIVE_CONF" "$ROLLBACK_CONF"
    install -m 0644 "$STAGED_CONF" "$ACTIVE_CONF"
    write_dropins
    rm -f "$CONTROL"
    cat > "$MARKER" <<EOD
activated=$(date -u +%Y-%m-%dT%H:%M:%SZ)
plugin=$PLUGIN
pack=$PACK
control=$CONTROL
rollback=$ROLLBACK_CONF
EOD
    restart_graph
    echo "SP11 UbiG candidate activated. Golden config saved at $ROLLBACK_CONF"
}

rollback() {
    [ -e "$MARKER" ] || { echo "candidate is not marked active" >&2; exit 2; }
    [ -f "$ROLLBACK_CONF" ] || { echo "rollback config missing: $ROLLBACK_CONF" >&2; exit 3; }
    systemctl_user stop filter-chain.service || true
    install -m 0644 "$ROLLBACK_CONF" "$ACTIVE_CONF"
    rm -f "$FILTER_DROPIN" "$VOLUME_DROPIN" "$MSIIR_DROPIN" "$CONTROL"
    rm -f "$MARKER"
    restart_graph
    echo "SP11 Golden Windows bridge restored. Candidate files remain staged for inspection."
}

status() {
    if [ -f "$ACTIVE_CONF" ] && grep -q 'label  = ubig_sp11_candidate' "$ACTIVE_CONF"; then
        echo "graph=candidate"
    else
        echo "graph=golden-or-other"
    fi
    [ -e "$MARKER" ] && echo "marker=active" || echo "marker=inactive"
    [ -x "$PLUGIN" ] && sha256sum "$PLUGIN" | sed 's/^/plugin=/' || echo "plugin=missing"
    [ -f "$PACK" ] && sha256sum "$PACK" | sed 's/^/pack=/' || echo "pack=missing"
    [ -f "$CONTROL" ] && echo "control=$CONTROL" || echo "control=absent"
    systemctl_user is-active filter-chain.service 2>/dev/null | sed 's/^/filter-chain=/' || true
}

case "${1:-}" in
    activate) activate ;;
    rollback) rollback ;;
    status) status ;;
    *) echo "usage: $0 {activate|rollback|status}" >&2; exit 2 ;;
esac
