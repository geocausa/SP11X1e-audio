#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd -- "$HERE/../.." && pwd)
PROJECT=$(cd -- "$ROOT/.." && pwd)
KROOT=${SP11_KERNEL_ROOT:-$PROJECT/02-kernel}
WORK=${SP11_REPRO_WORK:-$KROOT/.golden-v33-repro}
SRC=$WORK/src
OUT=$WORK/out
RESULT=$WORK/results/fullio-v19c
JOBS=${JOBS:-$(nproc)}
GOLDEN_INITRD=${GOLDEN_INITRD:-/boot/sp11-7.1.5-audio-golden-v33-topcfg1-physical-vi/initrd.img-7.1.5-sp11-render-parity-v4+-golden-v33-topcfg1}
KREL=7.1.5-sp11-render-parity-v4+

need() { command -v "$1" >/dev/null || { echo "missing tool: $1" >&2; exit 2; }; }
for x in make patch python3 sha256sum modinfo zstd aarch64-linux-gnu-gcc aarch64-linux-gnu-ld; do need "$x"; done

# Exact raw module identity can be build-path-sensitive because compiler/source
# provenance may be embedded in historical module ELFs. Do not normalize or
# weaken the raw SHA gates below: a provenance mismatch must fail closed.

(
  cd "$HERE"
  sha256sum -c patches.sha256
)

# Recreate verified Golden v33 from pristine 7.1.5 + pinned overlay/patches.
SP11_KERNEL_ROOT="$KROOT" SP11_REPRO_WORK="$WORK" JOBS="$JOBS" \
  "$ROOT/repro/golden-v33/build-and-verify.sh"

# Production microphone delta only: 0072 divider + 0078 cross-macro clock owner.
patch -d "$SRC" -p1 --batch --forward < "$ROOT/patches/0072-ASoC-lpass-va-macro-SP11-match-Windows-DMIC-divider.patch"
patch -d "$SRC" -p1 --batch --forward < "$ROOT/patches/0078-ASoC-lpass-SP11-share-VA-DMIC-clock-with-TX-capture.patch"
(
  cd "$SRC"
  sha256sum -c "$HERE/expected-production-source-sha256.txt"
)

mkdir -p "$RESULT/modules"
cp "$OUT/Module.symvers" "$RESULT/golden-v33-full.symvers"

# Common provider was promoted as a true in-tree target. This regenerates the
# exact new broker CRCs and exact accepted common module bytes.
make -j"$JOBS" -C "$SRC" O="$OUT" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION=+ \
  sound/soc/codecs/snd-soc-lpass-macro-common.ko
cp "$OUT/sound/soc/codecs/snd-soc-lpass-macro-common.ko" "$RESULT/modules/"

# VA/TX source objects must also be true in-tree objects. Force them so stale
# scoped-M= objects can never leak into this replay.
rm -f "$OUT/sound/soc/codecs/lpass-va-macro.o" "$OUT/sound/soc/codecs/lpass-tx-macro.o" \
      "$OUT/sound/soc/codecs/snd-soc-lpass-va-macro.o" "$OUT/sound/soc/codecs/snd-soc-lpass-tx-macro.o"
make -j"$JOBS" -C "$SRC" O="$OUT" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- LOCALVERSION=+ \
  sound/soc/codecs/lpass-va-macro.o sound/soc/codecs/lpass-tx-macro.o \
  sound/soc/codecs/snd-soc-lpass-va-macro.o sound/soc/codecs/snd-soc-lpass-tx-macro.o

# The composite snd-soc-lpass-{va,tx}-macro.o objects are the inputs to
# MODPOST.  They must be relinked after the leaf source objects; otherwise a
# clean Golden build can leave stale pre-0078 composites even though the leaf
# objects were rebuilt successfully.
#
# single_modules MODPOST cannot resolve imports from other in-tree modules via
# Module.symvers. Build a complete symbol input, use external-symbol resolution
# to emit the correct version tables, then restore intree=Y before modfinal.
SYM=$RESULT/production-input.symvers
grep -v $'\tsound/soc/codecs/snd-soc-lpass-macro-common\t' "$RESULT/golden-v33-full.symvers" > "$SYM"
grep $'\tsound/soc/codecs/snd-soc-lpass-macro-common\t' "$OUT/Module.symvers" >> "$SYM"

cd "$OUT"
for stem in va tx; do
    b=snd-soc-lpass-$stem-macro
    obj=sound/soc/codecs/$b.o
    ./scripts/mod/modpost -M -m -b -x -a -e -i "$SYM" -o "$RESULT/$b.ext.symvers" "$obj"
    python3 - "sound/soc/codecs/$b.mod.c" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]); s=p.read_text()
if 'MODULE_INFO(intree, "Y");' not in s:
    needle='};\n\n'
    pos=s.find(needle)
    if pos < 0: raise SystemExit('module header terminator not found')
    pos += len(needle)
    s=s[:pos]+'MODULE_INFO(intree, "Y");\n\n'+s[pos:]
p.write_text(s)
PY
done

# Compile the generated VA/TX metadata with the canonical in-tree modfinal
# command derived from the already exact common target.
python3 - <<'PY'
from pathlib import Path
import re, shlex, subprocess
out=Path.cwd(); d=out/'sound/soc/codecs'
text=(d/'.snd-soc-lpass-macro-common.mod.o.cmd').read_text()
base=re.search(r':= (.*)\n',text).group(1)
for stem in ('va','tx'):
    b=f'snd-soc-lpass-{stem}-macro'
    cmd=base.replace('snd-soc-lpass-macro-common',b).replace('snd_soc_lpass_macro_common',f'snd_soc_lpass_{stem}_macro')
    subprocess.run(shlex.split(cmd),cwd=out,check=True)
PY

for stem in va tx; do
    b=snd-soc-lpass-$stem-macro
    aarch64-linux-gnu-ld -r -EL -maarch64elf -z noexecstack --no-warn-rwx-segments --build-id=sha1 \
      -T "$OUT/scripts/module.lds" -o "$OUT/sound/soc/codecs/$b.ko" \
      "$OUT/sound/soc/codecs/$b.o" "$OUT/sound/soc/codecs/$b.mod.o" "$OUT/.module-common.o"
    cp "$OUT/sound/soc/codecs/$b.ko" "$RESULT/modules/"
done

# Exact raw/source identity gates.
fail=0
while read -r name want_sha want_src; do
    [[ -n $name ]] || continue
    f=$RESULT/modules/$name
    got_sha=$(sha256sum "$f" | awk '{print $1}')
    got_src=$(modinfo -F srcversion "$f")
    if [[ $got_sha != "$want_sha" || $got_src != "$want_src" ]]; then
        echo "MODULE FAIL $name sha=$got_sha src=$got_src" >&2; fail=1
    else
        echo "OK exact module $name sha=$got_sha src=$got_src"
    fi
done < "$HERE/expected-production-modules.txt"
[[ $fail -eq 0 ]] || exit 1

# Exact initrd delta: verified Golden-v33 payload set + the three exact modules,
# with the accepted cpio header/order manifest and streamed zstd framing.
python3 "$HERE/repack-initrd.py" \
  --golden-initrd "$GOLDEN_INITRD" \
  --module-dir "$RESULT/modules" \
  --manifest "$HERE/cpio-header-manifest.tsv" \
  --output "$RESULT/initrd.img-7.1.5-sp11-fullio-v19c"

echo 'FULLIO v19c KERNEL + INITRD EXACT REPRODUCTION PASS'
echo "results=$RESULT"
