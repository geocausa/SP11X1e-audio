#!/bin/bash
set -euo pipefail
T=/sys/kernel/tracing
OUTDIR=/var/lib/sp11-audio/diagnostics
mkdir -p "$OUTDIR"
BOOT_ID="$(cat /proc/sys/kernel/random/boot_id)"
OUT="$OUTDIR/wsa884x-write-${BOOT_ID}.trace"
META="$OUTDIR/wsa884x-write-${BOOT_ID}.meta"
cleanup(){
  echo 0 > "$T/tracing_on" 2>/dev/null || true
  echo 0 > "$T/events/kprobes/sp11_wsa884x_write/enable" 2>/dev/null || true
  echo 0 > "$T/options/stacktrace" 2>/dev/null || true
  echo '-:sp11_wsa884x_write' >> "$T/kprobe_events" 2>/dev/null || true
}
trap cleanup EXIT
for _ in $(seq 1 120); do grep -qw _regmap_write /proc/kallsyms && break; sleep 0.1; done
grep -qw _regmap_write /proc/kallsyms
echo 0 > "$T/tracing_on"
echo > "$T/trace"
echo '-:sp11_wsa884x_write' >> "$T/kprobe_events" 2>/dev/null || true
echo 'p:sp11_wsa884x_write _regmap_write map=%x0 reg=%x1:u32 val=%x2:u32' >> "$T/kprobe_events"
echo 'reg >= 12288 && reg <= 13823' > "$T/events/kprobes/sp11_wsa884x_write/filter"
echo 1 > "$T/events/kprobes/sp11_wsa884x_write/enable"
echo 1 > "$T/options/stacktrace"
echo 1 > "$T/tracing_on"
{
 echo "boot_id=$BOOT_ID"
 echo "kernel=$(uname -r)"
 echo "cmdline=$(cat /proc/cmdline)"
 echo "started_monotonic=$(cut -d' ' -f1 /proc/uptime)"
} > "$META"
sleep 40
echo 0 > "$T/tracing_on"
cat "$T/trace" > "$OUT"
echo "stopped_monotonic=$(cut -d' ' -f1 /proc/uptime)" >> "$META"
