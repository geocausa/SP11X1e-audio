#!/usr/bin/env bash
set -euo pipefail

HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO=$(cd -- "$HERE/../.." && pwd)
PROJECT=$(cd -- "$REPO/.." && pwd)
KROOT=${SP11_KERNEL_ROOT:-$PROJECT/02-kernel}
BASE=${SP11_LINUX_BASE:-$KROOT/linux-7.1.5}
WORK=${SP11_REPRO_WORK:-$KROOT/.golden-v32-repro}
SRC=$WORK/src
OUT=$WORK/out
RESULT=$WORK/results
JOBS=${JOBS:-$(nproc)}
LOCALVERSION=+
KREL=7.1.5-sp11-render-parity-v4+

need() { command -v "$1" >/dev/null || { echo "missing tool: $1" >&2; exit 1; }; }
for x in make gcc ld patch modinfo sha256sum readelf objcopy python3 cp find; do need "$x"; done

check_srcversions() {
  local expected=$1 dir=$2 name want got f failures=0
  while read -r name want; do
    [[ -n "$name" ]] || continue
    f=$dir/$name
    [[ -f "$f" ]] || { echo "MISSING $f" >&2; failures=1; continue; }
    got=$(modinfo -F srcversion "$f")
    if [[ "$got" != "$want" ]]; then
      echo "SRCVERSION FAIL $name expected=$want got=$got" >&2; failures=1
    else
      echo "OK srcversion $name=$got"
    fi
  done < "$expected"
  return "$failures"
}

clean_scoped_dir() {
  local d=$1
  find "$d" -maxdepth 1 -type f \( \
    -name '*.o' -o -name '*.ko' -o -name '*.mod' -o -name '*.mod.c' -o \
    -name '.*.cmd' -o -name '*.d' -o -name 'Module.symvers' -o -name 'modules.order' \
  \) -delete
}

copy_overlay() {
  local hash path rel dst
  while read -r hash path; do
    [[ -n "$path" ]] || continue
    rel=${path#source-overlay/}
    dst=$SRC/$rel
    mkdir -p "$(dirname "$dst")"
    cp --remove-destination -p "$HERE/$path" "$dst"
  done < "$HERE/v31-overlay.sha256"
}

runtime_gate() {
  local name want got failures=0
  while read -r name want; do
    [[ -n "$name" ]] || continue
    got=$("$HERE/runtime-module-digest.py" "$RESULT/v32/$name")
    if [[ "$got" != "$want" ]]; then
      echo "RUNTIME DIGEST FAIL $name expected=$want got=$got" >&2; failures=1
    else
      echo "OK runtime digest $name=$got"
    fi
  done < "$HERE/expected-v32-runtime-digests.txt"
  return "$failures"
}

[[ -d "$BASE" ]] || { echo "missing pristine base: $BASE" >&2; exit 1; }

# Immutable recipe inputs.
"$HERE/verify-base-tree.py" "$BASE"
(
  cd "$HERE"
  sha256sum -c config.sha256
  sha256sum -c v31-overlay.sha256
  sha256sum -c patches.sha256
)

rm -rf "$WORK"
mkdir -p "$SRC" "$OUT" "$RESULT/v31" "$RESULT/v32"
# Hardlink clone keeps the 1.6 GB pristine base cheap. Overlay copies use
# --remove-destination so the pristine base is never mutated.
cp -al "$BASE/." "$SRC/"
copy_overlay
cp "$HERE/config-7.1.5-sp11-render-parity-v4" "$OUT/.config"

make -s -C "$SRC" O="$OUT" LOCALVERSION="$LOCALVERSION" olddefconfig
got_krel=$(make -s -C "$SRC" O="$OUT" LOCALVERSION="$LOCALVERSION" kernelrelease)
[[ "$got_krel" == "$KREL" ]] || { echo "kernelrelease mismatch: $got_krel" >&2; exit 1; }
echo "OK kernelrelease=$got_krel"

# Clean v31 build. Full in-tree modules are required for the historical
# SoundWire/X1E srcversion context; macro/q6 were historically scoped M= builds.
make -j"$JOBS" -C "$SRC" O="$OUT" LOCALVERSION="$LOCALVERSION" vmlinux
make -j"$JOBS" -C "$SRC" O="$OUT" LOCALVERSION="$LOCALVERSION" modules
cp "$OUT/sound/soc/codecs/snd-soc-wsa884x.ko" "$RESULT/v31/"
cp "$OUT/drivers/soundwire/soundwire-qcom.ko" "$RESULT/v31/"
cp "$OUT/sound/soc/qcom/snd-soc-x1e80100.ko" "$RESULT/v31/"

clean_scoped_dir "$SRC/sound/soc/codecs"
make -j"$JOBS" -C "$SRC" O="$OUT" LOCALVERSION="$LOCALVERSION" \
  M="$SRC/sound/soc/codecs" snd-soc-lpass-wsa-macro.ko
cp "$SRC/sound/soc/codecs/snd-soc-lpass-wsa-macro.ko" "$RESULT/v31/"

clean_scoped_dir "$SRC/sound/soc/qcom/qdsp6"
make -j"$JOBS" -C "$SRC" O="$OUT" LOCALVERSION="$LOCALVERSION" \
  M="$SRC/sound/soc/qcom/qdsp6" snd-q6apm.ko
cp "$SRC/sound/soc/qcom/qdsp6/snd-q6apm.ko" "$RESULT/v31/"

echo '--- Golden-v31 clean reproduction ---'
check_srcversions "$HERE/expected-v31-srcversions.txt" "$RESULT/v31"

# Canonical v32 delta, and no other source mutation.
for p in \
  0069-ASoC-SP11-enable-protection-clocks-after-both-PAs-active.patch \
  0070-soundwire-qcom-SP11-use-Windows-active-Offset2-on-feedback-ports.patch \
  0071-soundwire-qcom-SP11-add-CPS-wake-and-packetization-parity.patch; do
  patch -d "$SRC" -p1 --batch --forward < "$REPO/patches/$p"
done
(
  cd "$SRC"
  sha256sum -c "$HERE/expected-v32-source-sha256.txt"
)

# Incremental in-tree build yields the promoted macro identity and preserves
# the unchanged X1E identity.
make -j"$JOBS" -C "$SRC" O="$OUT" LOCALVERSION="$LOCALVERSION" modules
cp "$OUT/sound/soc/codecs/snd-soc-lpass-wsa-macro.ko" "$RESULT/v32/"
cp "$OUT/sound/soc/qcom/snd-soc-x1e80100.ko" "$RESULT/v32/"

# Promoted WSA884x and SoundWire were scoped builds against this same O= tree.
clean_scoped_dir "$SRC/sound/soc/codecs"
make -j"$JOBS" -C "$SRC" O="$OUT" LOCALVERSION="$LOCALVERSION" \
  M="$SRC/sound/soc/codecs" snd-soc-wsa884x.ko
cp "$SRC/sound/soc/codecs/snd-soc-wsa884x.ko" "$RESULT/v32/"

clean_scoped_dir "$SRC/drivers/soundwire"
make -j"$JOBS" -C "$SRC" O="$OUT" LOCALVERSION="$LOCALVERSION" \
  M="$SRC/drivers/soundwire" soundwire-qcom.ko
cp "$SRC/drivers/soundwire/soundwire-qcom.ko" "$RESULT/v32/"

# q6apm is unchanged from v31; preserve the exact clean scoped output.
cp "$RESULT/v31/snd-q6apm.ko" "$RESULT/v32/"

echo '--- Golden-v32 clean reproduction ---'
check_srcversions "$HERE/expected-v32-srcversions.txt" "$RESULT/v32"
runtime_gate

(
  cd "$RESULT/v32"
  sha256sum *.ko > ../v32-raw-ko.sha256
)
cat > "$RESULT/BUILD-ENVIRONMENT.txt" <<EOF
kernelrelease=$KREL
base=$BASE
base_tree_sha256=7e5f8ccd76f625cb678028fe6bab2d3ef0c03878c2af21433c96f4a78b813fef
config_sha256=4fed1ee935cff7589ed2941d0bf2ddec4ddd2a03d919b9dc30ce20f5d85665ca
gcc=$(gcc --version | head -1)
ld=$(ld --version | head -1)
make=$(make --version | head -1)
jobs=$JOBS
EOF

echo "GOLDEN v32 CLEAN REPRODUCTION PASS"
echo "results=$RESULT"
