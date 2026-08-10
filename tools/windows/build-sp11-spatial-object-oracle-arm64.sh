#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
OUT=${1:-/tmp/sp11-spatial-object-oracle-arm64.exe}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT INT TERM
cat >"$TMP/kernel32.def" <<'DEF'
LIBRARY KERNEL32.dll
EXPORTS
CreateEventW
CloseHandle
WaitForSingleObject
GetStdHandle
WriteFile
ExitProcess
DEF
cat >"$TMP/ole32.def" <<'DEF'
LIBRARY OLE32.dll
EXPORTS
CoInitializeEx
CoUninitialize
CoCreateInstance
CoTaskMemFree
DEF
llvm-dlltool -m arm64 -d "$TMP/kernel32.def" -l "$TMP/kernel32.lib"
llvm-dlltool -m arm64 -d "$TMP/ole32.def" -l "$TMP/ole32.lib"
clang-cl --target=aarch64-pc-windows-msvc /nologo /c /O2 /GS- /W4 \
  /clang:-fno-builtin \
  "$ROOT/tools/windows/sp11_spatial_object_oracle.c" /Fo"$TMP/oracle.obj"
lld-link /nologo /machine:arm64 /subsystem:console /entry:mainCRTStartup /nodefaultlib /timestamp:0 \
  /out:"$OUT" "$TMP/oracle.obj" "$TMP/kernel32.lib" "$TMP/ole32.lib"
printf 'built %s\n' "$OUT"
sha256sum "$OUT"
llvm-readobj --file-headers --coff-imports "$OUT"
