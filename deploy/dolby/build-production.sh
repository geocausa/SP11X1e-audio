#!/bin/sh
set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)
BUNDLE=${SP11_DOLBY_BUNDLE:-"$HOME/.local/lib/sp11-dolby"}
VL="$BUNDLE/DolbyAPOvlldp150.dll"
VR="$BUNDLE/DolbyAPOVR.dll"
OUT=${1:-"$ROOT/dolby-port/sp11_dolby_windows_chain.production.so"}
CC=${CC:-gcc}

VL_SHA=a2553ff7b013b5a248e50bdcae46d08405e393c0085073975214d035cedf02c1
VR_SHA=1d74477ea0dae66961a21bf6bc3ce0d8062836fc4dd96b59c14de11257f5eecc

check_file() {
  path=$1 expected=$2
  [ -f "$path" ] || { echo "Missing required private DLL: $path" >&2; exit 2; }
  actual=$(sha256sum "$path" | awk '{print $1}')
  [ "$actual" = "$expected" ] || {
    echo "Refusing unexpected DLL: $path" >&2
    echo " expected $expected" >&2
    echo " actual   $actual" >&2
    exit 3
  }
}

check_file "$VL" "$VL_SHA"
check_file "$VR" "$VR_SHA"

mkdir -p "$(dirname -- "$OUT")"
"$CC" -shared -fPIC -O2 -Wall -Wextra \
  -DSP11_CHAIN_DEFAULT_VLLDP_DLL="\"$VL\"" \
  -DSP11_CHAIN_DEFAULT_VR_DLL="\"$VR\"" \
  -o "$OUT" "$ROOT/dolby-port/sp11_dolby_windows_chain_ladspa.c" -ldl -lm

printf 'built %s\n' "$OUT"
printf 'sha256 '
sha256sum "$OUT" | awk '{print $1}'
printf 'VLLDP %s\nVR    %s\n' "$VL" "$VR"
