#!/usr/bin/env bash
set -euo pipefail
HERE=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
ROOT=$(cd -- "$HERE/../.." && pwd)
DEPLOY="$ROOT/deploy/native-audio-v19c"
SRC="$DEPLOY/X1E80100-Microsoft-Surface-Pro-11-FullIO-v19c0.conf"
WANT="$DEPLOY/X1E80100-Microsoft-Surface-Pro-11-FullIO-v19c0-tplg.bin"

command -v alsatplg >/dev/null 2>&1 || { echo 'alsatplg is required' >&2; exit 2; }
"$DEPLOY/verify-native-audio-v19c.sh"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
alsatplg -c "$SRC" -o "$tmp/fullio-v19c.bin"
cmp -s "$tmp/fullio-v19c.bin" "$WANT"
echo 'FullIO v19c clean-clone topology build: byte-identical PASS'

python3 - "$ROOT/tools/build_sp11_native_audio_topology.py" <<'PY'
import importlib.util, pathlib, sys
p = pathlib.Path(sys.argv[1])
spec = importlib.util.spec_from_file_location('v19c_builder', p)
m = importlib.util.module_from_spec(spec)
assert spec.loader
spec.loader.exec_module(m)
assert m.GOLDEN_SHA256 == '1b0c7217fc67bb11da002b06563dd8c411b0f0e35ac40778bff3d65093061c9d'
assert m.CAPTURE_FE_SUBGRAPH == 0xB0000203
assert m.CAPTURE_FE_CONTAINER == 0xE0000203
assert m.CAPTURE_BE_SUBGRAPH == 0xB0000209
assert m.CAPTURE_BE_CONTAINER == 0xE0000209
print('FullIO v19c builder constants/collision namespace: PASS')
PY

if command -v pytest >/dev/null 2>&1; then
    (cd "$ROOT" && PYTHONPATH=. pytest -q tests/test_sp11_ubig_helper.py tests/test_ubig_control_app.py)
fi
