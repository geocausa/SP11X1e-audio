#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO=$(cd -- "$HERE/../.." && pwd)
VERSION=${VERSION:-33.0-1}
ARCH=${ARCH:-$(dpkg --print-architecture)}
MODULE=${V33_MODULE_ZST:-/home/geoca/Documents/SP11-PROJECT/02-kernel/golden/v33-topcfg1-physical-vi/snd-soc-lpass-wsa-macro.v33.reference-signed.ko.zst}
EXPECTED=39674078b0781323464b3de647caf9db0b25cde51d447e8e0253630de91d3f2d
OUT=${OUT_DIR:-$REPO/dist}
SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH:-1787535600}
export SOURCE_DATE_EPOCH
PKG=sp11x1e-audio-golden-v33
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
ROOT=$WORK/root
[[ $(sha256sum "$MODULE" | awk '{print $1}') == "$EXPECTED" ]] || { echo 'v33 module hash mismatch' >&2; exit 2; }
mkdir -p "$ROOT/DEBIAN" "$ROOT/usr/lib/sp11-audio-golden-v33" "$ROOT/usr/local/sbin" \
  "$ROOT/usr/share/doc/$PKG" "$ROOT/etc/initramfs-tools/hooks" "$ROOT/lib/systemd/system" "$OUT"
install -m0644 "$MODULE" "$ROOT/usr/lib/sp11-audio-golden-v33/snd-soc-lpass-wsa-macro.ko.zst"
install -m0644 "$REPO/deploy/golden-v33/manifest.json" "$ROOT/usr/lib/sp11-audio-golden-v33/manifest.json"
install -m0755 "$REPO/deploy/golden-v33/verify-golden-v33.sh" "$ROOT/usr/local/sbin/sp11-audio-golden-v33-verify"
install -m0644 "$REPO/deploy/golden-v33/README.md" "$ROOT/usr/share/doc/$PKG/README.md"
cat > "$ROOT/DEBIAN/control" <<CTL
Package: $PKG
Version: $VERSION
Architecture: $ARCH
Maintainer: SP11 Audio Project
Depends: kmod, initramfs-tools
Section: kernel
Priority: optional
Description: Surface Pro 11 X1E Golden v33 WSA physical-VI hardening
 Hash-pins the promoted Golden-v33 WSA macro, synchronizes the root module tree,
 and installs an initramfs/boot-identity guard. It contains no private vendor DSP,
 ACDB, firmware, or UbiG owner-only tuning payload.
CTL
cat > "$ROOT/DEBIAN/postinst" <<'POST'
#!/bin/sh
set -eu
KREL=7.1.5-sp11-render-parity-v4+
EXPECTED=39674078b0781323464b3de647caf9db0b25cde51d447e8e0253630de91d3f2d
SRC=/usr/lib/sp11-audio-golden-v33/snd-soc-lpass-wsa-macro.ko.zst
DST=/lib/modules/$KREL/kernel/sound/soc/codecs/snd-soc-lpass-wsa-macro.ko.zst
BACK=/usr/local/lib/sp11-audio/v32-root-modules-backup-20260824
[ "$(sha256sum "$SRC" | awk '{print $1}')" = "$EXPECTED" ] || exit 10
mkdir -p "$BACK"
if [ -f "$DST" ]; then
  old=$(sha256sum "$DST" | awk '{print $1}')
  if [ "$old" != "$EXPECTED" ]; then cp -a --update=none "$DST" "$BACK/snd-soc-lpass-wsa-macro.ko.zst.$old"; fi
fi
install -m0644 "$SRC" "$DST"
depmod "$KREL"
systemctl daemon-reload >/dev/null 2>&1 || true
systemctl enable sp11-audio-v33-verify.service >/dev/null 2>&1 || true
exit 0
POST
cat > "$ROOT/etc/initramfs-tools/hooks/sp11-audio-golden-v33" <<'HOOK'
#!/bin/sh
set -eu
PREREQ=''
prereqs(){ echo "$PREREQ"; }
case "${1:-}" in prereqs) prereqs; exit 0;; esac
KREL=7.1.5-sp11-render-parity-v4+
[ "${version:-}" = "$KREL" ] || exit 0
EXPECTED=39674078b0781323464b3de647caf9db0b25cde51d447e8e0253630de91d3f2d
SRC=/usr/lib/sp11-audio-golden-v33/snd-soc-lpass-wsa-macro.ko.zst
[ "$(sha256sum "$SRC" | awk '{print $1}')" = "$EXPECTED" ] || { echo 'Golden v33 payload hash mismatch' >&2; exit 1; }
DST="$DESTDIR/usr/lib/modules/$KREL/kernel/sound/soc/codecs/snd-soc-lpass-wsa-macro.ko.zst"
mkdir -p "$(dirname "$DST")"
cp -a "$SRC" "$DST"
HOOK
cat > "$ROOT/lib/systemd/system/sp11-audio-v33-verify.service" <<'UNIT'
[Unit]
Description=Verify SP11 Audio Golden v33 identity
After=local-fs.target
ConditionKernelCommandLine=sp11_entry=7.1.5-sp11-golden-v33-topcfg1-physical-vi
[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/local/sbin/sp11-audio-golden-v33-verify
[Install]
WantedBy=multi-user.target
UNIT
chmod 0755 "$ROOT/DEBIAN/postinst" "$ROOT/etc/initramfs-tools/hooks/sp11-audio-golden-v33"
find "$ROOT" -exec touch -h -d '@1787535600' {} +
dpkg-deb --root-owner-group --build "$ROOT" "$OUT/${PKG}_${VERSION}_${ARCH}.deb"
sha256sum "$OUT/${PKG}_${VERSION}_${ARCH}.deb" > "$OUT/${PKG}_${VERSION}_${ARCH}.deb.sha256"
echo "$OUT/${PKG}_${VERSION}_${ARCH}.deb"
