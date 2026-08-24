#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
PACK=${UBIG_SP11_STAGEB_PACK:-"$HOME/.local/share/ubig-private/sp11-stageb-v4.pack"}
EXPECTED_PACK_SHA=${UBIG_SP11_STAGEB_PACK_SHA:-30b9b8ce8dace4a9f5dee2c2defa7da2d9b8431cf68fb323f8d2c3e4e3c942df}
LIBDIR=${UBIG_CANDIDATE_LIBDIR:-"$HOME/.local/lib/ubig-candidate"}
STAGEDIR=${UBIG_CANDIDATE_STAGEDIR:-"$HOME/.local/share/ubig-candidate"}
PLUGIN="$LIBDIR/ubig-sp11-candidate.so"
CONF="$STAGEDIR/98-sp11-ubig-candidate.conf"
HELPERDIR="$STAGEDIR/bin"

[ -f "$PACK" ] || { echo "missing private Stage-B pack: $PACK" >&2; exit 2; }
actual_pack_sha=$(sha256sum "$PACK" | awk '{print $1}')
[ "$actual_pack_sha" = "$EXPECTED_PACK_SHA" ] || {
    echo "refusing unpinned Stage-B pack" >&2
    echo " expected $EXPECTED_PACK_SHA" >&2
    echo " actual   $actual_pack_sha" >&2
    exit 3
}

make -C "$ROOT/ubig" candidate-ladspa
UBIG_SP11_STAGEB_PACK="$PACK" make -C "$ROOT/ubig" candidate-control-check

mkdir -p "$LIBDIR" "$STAGEDIR" "$HELPERDIR"
install -m 0755 "$ROOT/ubig/build/ubig-sp11-candidate.so" "$PLUGIN"
install -m 0755 "$ROOT/deploy/ubig/sp11_volume_sync_dispatch.py" "$HELPERDIR/sp11-volume-sync-dispatch"
install -m 0755 "$ROOT/deploy/ubig/sp11_ubig_volume_sync.py" "$HELPERDIR/sp11-ubig-volume-sync"
install -m 0755 "$ROOT/deploy/ubig/sp11_windows_volume_transaction_sync.py" "$HELPERDIR/sp11-windows-volume-transaction-sync"
install -m 0755 "$ROOT/deploy/ubig/sp11_msiir_volume_sync.py" "$HELPERDIR/sp11-msiir-volume-sync"
python3 - "$SCRIPT_DIR/98-sp11-ubig-candidate.conf.in" "$CONF" "$PLUGIN" <<'PY'
from pathlib import Path
import sys
src,dst,plugin=map(Path,sys.argv[1:])
text=src.read_text().replace('@PLUGIN@',str(plugin))
if '@PLUGIN@' in text:
    raise SystemExit('candidate PipeWire template expansion failed')
dst.write_text(text)
PY
chmod 0644 "$CONF"

bad_needed=$(readelf -d "$PLUGIN" | awk '/NEEDED/{gsub(/\[|\]/,"",$5); print $5}' | grep -Ev '^(libm\.so\.6|libc\.so\.6|ld-linux-aarch64\.so\.1)$' || true)
[ -z "$bad_needed" ] || { echo "unexpected candidate runtime dependency: $bad_needed" >&2; exit 4; }
if strings "$PLUGIN" | grep -Eq 'DolbyAPOVR\.dll|DolbyAPOvlldp150\.dll|sp11_pe_load'; then
    echo "refusing candidate containing a vendor-loader reference" >&2
    exit 5
fi

plugin_sha=$(sha256sum "$PLUGIN" | awk '{print $1}')
cat > "$STAGEDIR/manifest" <<MANIFEST
plugin=$PLUGIN
plugin_sha256=$plugin_sha
pack=$PACK
pack_sha256=$actual_pack_sha
pipewire_conf=$CONF
helper_dir=$HELPERDIR
control_path=${XDG_RUNTIME_DIR:-/run/user/$(id -u)}/ubig-control-v2
MANIFEST

printf 'Prepared SP11 UbiG candidate (not activated):\n'
printf '  plugin %s\n' "$PLUGIN"
printf '  sha256 %s\n' "$plugin_sha"
printf '  pack   %s\n' "$PACK"
printf '  config %s\n' "$CONF"
printf '  helpers %s\n' "$HELPERDIR"
printf 'Activate only through: %s activate\n' "$SCRIPT_DIR/switch.sh"
