#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH='' cd -- "$SCRIPT_DIR/../.." && pwd)
VERSION=${1:-0.1.0}
ARCH=$(dpkg --print-architecture)
DEST=${UBIG_DEB_DEST:-"$ROOT/dist"}
STAGE=$(mktemp -d)
PACKAGE="$DEST/ubig-control_${VERSION}_${ARCH}.deb"
BUILD_EPOCH=${SOURCE_DATE_EPOCH:-$(sed -n '1p' "$SCRIPT_DIR/source-date-epoch")}
export SOURCE_DATE_EPOCH="$BUILD_EPOCH"
trap 'rm -rf "$STAGE"' EXIT HUP INT TERM

make -C "$ROOT/ubig" build/ubigctl

mkdir -p "$STAGE/DEBIAN" \
    "$STAGE/usr/bin" \
    "$STAGE/usr/lib/ubig-control" \
    "$STAGE/usr/share/applications" \
    "$STAGE/usr/share/icons/hicolor/scalable/apps" \
    "$STAGE/usr/share/doc/ubig-control" \
    "$DEST"

sed -e "s/@VERSION@/$VERSION/g" -e "s/@ARCH@/$ARCH/g" \
    "$SCRIPT_DIR/control.in" > "$STAGE/DEBIAN/control"
install -m 0755 "$ROOT/ubig/build/ubigctl" "$STAGE/usr/bin/ubigctl"
install -m 0755 "$SCRIPT_DIR/ubig-geq" "$STAGE/usr/bin/ubig-geq"
install -m 0644 "$ROOT/ubig/app/ubig_control.py" "$STAGE/usr/lib/ubig-control/ubig_control.py"
install -m 0644 "$ROOT/ubig/app/ubig_geq.py" "$STAGE/usr/lib/ubig-control/ubig_geq.py"
install -m 0644 "$SCRIPT_DIR/io.github.geocausa.UbiG.desktop" "$STAGE/usr/share/applications/io.github.geocausa.UbiG.desktop"
install -m 0644 "$SCRIPT_DIR/io.github.geocausa.UbiG.svg" "$STAGE/usr/share/icons/hicolor/scalable/apps/io.github.geocausa.UbiG.svg"
install -m 0644 "$ROOT/ubig/docs/CONTROL-APP.md" "$STAGE/usr/share/doc/ubig-control/README.md"
strip --strip-unneeded "$STAGE/usr/bin/ubigctl"
find "$STAGE" -type d -exec chmod 0755 {} +
find "$STAGE" -exec touch -h -d "@$BUILD_EPOCH" {} +

dpkg-deb --build --root-owner-group "$STAGE" "$PACKAGE"
printf '%s\n' "$PACKAGE"
