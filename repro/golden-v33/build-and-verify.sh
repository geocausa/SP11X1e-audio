#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
V32=$(cd -- "$HERE/../golden-v32" && pwd)
REPO=$(cd -- "$HERE/../.." && pwd)
PROJECT=$(cd -- "$REPO/.." && pwd)
KROOT=${SP11_KERNEL_ROOT:-$PROJECT/02-kernel}
WORK=${SP11_REPRO_WORK:-$KROOT/.golden-v33-repro}
SRC=$WORK/src
OUT=$WORK/out
RESULT=$WORK/results
JOBS=${JOBS:-$(nproc)}

export SP11_REPRO_WORK="$WORK"
"$V32/build-and-verify.sh"
(
 cd "$HERE"
 sha256sum -c patches.sha256
)
patch -d "$SRC" -p1 --batch --forward < "$REPO/patches/0072-ASoC-lpass-wsa-macro-SP11-materialize-Windows-TOP-CFG1.patch"
(
 cd "$SRC"
 sha256sum -c "$HERE/expected-v33-source-sha256.txt"
)

mkdir -p "$RESULT/v33"
# Promoted v33 macro was a scoped codec build against the verified v32 O= tree.
find "$SRC/sound/soc/codecs" -maxdepth 1 -type f \( -name '*.o' -o -name '*.ko' -o -name '*.mod' -o -name '*.mod.c' -o -name '.*.cmd' -o -name '*.d' -o -name 'Module.symvers' -o -name 'modules.order' \) -delete
make -j"$JOBS" -C "$SRC" O="$OUT" LOCALVERSION=+ M="$SRC/sound/soc/codecs" snd-soc-lpass-wsa-macro.ko
cp "$SRC/sound/soc/codecs/snd-soc-lpass-wsa-macro.ko" "$RESULT/v33/"
for f in snd-soc-wsa884x.ko soundwire-qcom.ko snd-soc-x1e80100.ko snd-q6apm.ko; do cp "$RESULT/v32/$f" "$RESULT/v33/"; done

fail=0
while read -r name want; do
 [[ -n $name ]] || continue
 got=$(modinfo -F srcversion "$RESULT/v33/$name")
 if [[ $got != $want ]]; then echo "SRCVERSION FAIL $name expected=$want got=$got" >&2; fail=1; else echo "OK srcversion $name=$got"; fi
done < "$HERE/expected-v33-srcversions.txt"
while read -r name want; do
 [[ -n $name ]] || continue
 got=$("$V32/runtime-module-digest.py" "$RESULT/v33/$name")
 if [[ $got != $want ]]; then echo "RUNTIME DIGEST FAIL $name expected=$want got=$got" >&2; fail=1; else echo "OK runtime digest $name=$got"; fi
done < "$HERE/expected-v33-runtime-digests.txt"
[[ $fail -eq 0 ]] || exit 1
(
 cd "$RESULT/v33"
 sha256sum *.ko > ../v33-raw-ko.sha256
)
echo 'GOLDEN v33 CLEAN REPRODUCTION PASS'
echo "results=$RESULT/v33"
