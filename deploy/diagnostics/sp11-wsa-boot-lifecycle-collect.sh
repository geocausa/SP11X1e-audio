#!/bin/sh
# Collect and stop the read-only SP11 WSA lifecycle trace.
set -eu
TRACE=/sys/kernel/tracing
OUT=${1:-/var/log/sp11-wsa-boot-lifecycle.trace}

if [ ! -d "$TRACE" ]; then
    echo "tracefs unavailable" >&2
    exit 1
fi

echo 0 > "$TRACE/tracing_on"
cat "$TRACE/trace" > "$OUT"
if [ -d "$TRACE/events/sp11" ]; then
    echo 0 > "$TRACE/events/sp11/enable" || true
fi
printf '%s\n' "$OUT"
