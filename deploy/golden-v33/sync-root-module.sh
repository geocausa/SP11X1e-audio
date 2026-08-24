#!/usr/bin/env bash
set -euo pipefail
[[ $EUID -eq 0 ]] || { echo 'run with sudo' >&2; exit 1; }
KREL=7.1.5-sp11-render-parity-v4+
EXPECTED=39674078b0781323464b3de647caf9db0b25cde51d447e8e0253630de91d3f2d
ROOTMOD=/lib/modules/$KREL/kernel/sound/soc/codecs/snd-soc-lpass-wsa-macro.ko.zst
SRC=${1:-/usr/lib/sp11-audio-golden-v33/snd-soc-lpass-wsa-macro.ko.zst}
BACK=/usr/local/lib/sp11-audio/v32-root-modules-backup-20260824
[[ -f $SRC ]] || { echo "missing v33 module: $SRC" >&2; exit 2; }
[[ $(sha256sum "$SRC"|awk '{print $1}') == $EXPECTED ]] || { echo 'v33 source hash mismatch' >&2; exit 3; }
mkdir -p "$BACK"
if [[ -f $ROOTMOD ]]; then old=$(sha256sum "$ROOTMOD"|awk '{print $1}'); if [[ $old != $EXPECTED ]]; then cp -a --update=none "$ROOTMOD" "$BACK/snd-soc-lpass-wsa-macro.ko.zst.$old"; fi; fi
install -m0644 "$SRC" "$ROOTMOD"
depmod "$KREL"
[[ $(sha256sum "$ROOTMOD"|awk '{print $1}') == $EXPECTED ]] || exit 4
echo "v33 root module synchronized: $EXPECTED"
