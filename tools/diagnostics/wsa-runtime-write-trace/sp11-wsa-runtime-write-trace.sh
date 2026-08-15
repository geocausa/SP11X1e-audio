#!/bin/bash
set -euo pipefail

TRACE=/sys/kernel/tracing
OUTDIR=/var/lib/sp11-audio/diagnostics
OUT="$OUTDIR/wsa-runtime-write-boot.trace"
META="$OUTDIR/wsa-runtime-write-boot.meta"

mkdir -p "$OUTDIR"

cleanup() {
    echo 0 > "$TRACE/tracing_on" 2>/dev/null || true
    echo 0 > "$TRACE/events/kprobes/sp11_wsa_cupd/enable" 2>/dev/null || true
    echo 0 > "$TRACE/events/kprobes/sp11_wsa_cwrite/enable" 2>/dev/null || true
    echo 0 > "$TRACE/options/stacktrace" 2>/dev/null || true
    echo '-:sp11_wsa_cupd' >> "$TRACE/kprobe_events" 2>/dev/null || true
    echo '-:sp11_wsa_cwrite' >> "$TRACE/kprobe_events" 2>/dev/null || true
}
trap cleanup EXIT

# Do not alter audio state. Wait only for the ASoC core symbol used as a passive
# observation boundary.
for _ in $(seq 1 100); do
    if grep -qw snd_soc_component_update_bits /proc/kallsyms; then
        break
    fi
    sleep 0.1
done
grep -qw snd_soc_component_update_bits /proc/kallsyms

echo 0 > "$TRACE/tracing_on"
echo > "$TRACE/trace"
echo 0 > "$TRACE/options/stacktrace" 2>/dev/null || true

# Remove stale probes from an interrupted previous diagnostic boot, if any.
echo '-:sp11_wsa_cupd' >> "$TRACE/kprobe_events" 2>/dev/null || true
echo '-:sp11_wsa_cwrite' >> "$TRACE/kprobe_events" 2>/dev/null || true

echo 'p:sp11_wsa_cupd snd_soc_component_update_bits component=%x0 reg=%x1:u32 mask=%x2:u32 val=%x3:u32' >> "$TRACE/kprobe_events"
echo 'p:sp11_wsa_cwrite snd_soc_component_write component=%x0 reg=%x1:u32 val=%x2:u32' >> "$TRACE/kprobe_events"

# WSA macro aperture is 0x000..0xbff. Exclude zero/very-low generic registers
# to reduce unrelated codec traffic; stack traces identify the actual WSA macro
# caller/component without dereferencing device memory.
echo 'reg >= 128 && reg <= 3071' > "$TRACE/events/kprobes/sp11_wsa_cupd/filter"
echo 'reg >= 128 && reg <= 3071' > "$TRACE/events/kprobes/sp11_wsa_cwrite/filter"
echo 1 > "$TRACE/events/kprobes/sp11_wsa_cupd/enable"
echo 1 > "$TRACE/events/kprobes/sp11_wsa_cwrite/enable"
echo 1 > "$TRACE/options/stacktrace"
echo 1 > "$TRACE/tracing_on"

{
    echo "boot_id=$(cat /proc/sys/kernel/random/boot_id)"
    echo "kernel=$(uname -r)"
    echo "cmdline=$(cat /proc/cmdline)"
    echo "started_monotonic=$(cut -d' ' -f1 /proc/uptime)"
} > "$META"

# Normal boot/login graph construction reaches the WSA speaker DAPM path well
# inside this bounded window. No playback is initiated by this diagnostic.
sleep 35

echo 0 > "$TRACE/tracing_on"
cat "$TRACE/trace" > "$OUT"
echo "stopped_monotonic=$(cut -d' ' -f1 /proc/uptime)" >> "$META"
