#!/bin/sh
# Read-only SP11 WSA lifecycle trace.  It registers kprobes only; it does not
# read or write codec registers and does not start/stop audio services.
set -eu

TRACE=/sys/kernel/tracing
LOG=/var/log/sp11-wsa-boot-lifecycle-arm.log

exec >>"$LOG" 2>&1
printf '%s SP11TRACE arm begin\n' "$(date -Ins)"

if [ ! -d "$TRACE" ]; then
    mount -t tracefs nodev "$TRACE" 2>/dev/null || true
fi
if [ ! -w "$TRACE/kprobe_events" ]; then
    echo "tracefs kprobe_events unavailable"
    exit 1
fi

# Wait briefly for the sound modules to register their local kallsyms.  This
# service runs before the graphical session; no audio service is manipulated.
need='wsa884x_hw_params wsa884x_mute_stream wsa_macro_enable_interpolator'
i=0
while [ "$i" -lt 100 ]; do
    missing=0
    for sym in $need; do
        grep -qw "$sym" /proc/kallsyms || missing=1
    done
    [ "$missing" -eq 0 ] && break
    i=$((i + 1))
    sleep 0.1
done

for sym in $need; do
    if ! grep -qw "$sym" /proc/kallsyms; then
        echo "required symbol missing: $sym"
        exit 2
    fi
done

# Remove only our previous probe group entries if a prior diagnostic boot left
# them behind. A reboot normally clears these automatically.
for ev in \
    sp11_wsa_hw_params sp11_wsa_mute sp11_wsa_spkr_event \
    sp11_macro_interp sp11_macro_comp \
    sp11_swr_hw_params sp11_swr_suspend sp11_swr_resume \
    sp11_sdw_prepare sp11_sdw_enable sp11_sdw_disable sp11_sdw_deprepare; do
    echo "-:sp11/$ev" >> "$TRACE/kprobe_events" 2>/dev/null || true
done

add_probe() {
    name=$1
    spec=$2
    if printf 'p:sp11/%s %s\n' "$name" "$spec" >> "$TRACE/kprobe_events" 2>/dev/null; then
        echo "armed $name: $spec"
    else
        echo "not armed $name: $spec"
    fi
}

# ARM64 argument registers are logged where useful.  We intentionally avoid
# dereferencing driver structures: ordering is the discriminator and this keeps
# the diagnostic side-effect free.
add_probe sp11_wsa_hw_params 'wsa884x_hw_params substream=%x0:u64 params=%x1:u64 dai=%x2:u64'
add_probe sp11_wsa_mute 'wsa884x_mute_stream dai=%x0:u64 mute=%x1:s32 stream=%x2:s32'
add_probe sp11_wsa_spkr_event 'wsa884x_spkr_event widget=%x0:u64 event=%x2:u32'
add_probe sp11_macro_interp 'wsa_macro_enable_interpolator widget=%x0:u64 event=%x2:u32'
if grep -qw wsa_macro_config_compander /proc/kallsyms; then
    add_probe sp11_macro_comp 'wsa_macro_config_compander component=%x0:u64 interp=%x1:u32 event=%x2:u32'
else
    echo "optional symbol absent: wsa_macro_config_compander (covered by wsa_macro_enable_interpolator)"
fi

# These symbols are optional across the different protected-kernel lineages.
if grep -qw qcom_swrm_hw_params /proc/kallsyms; then
    add_probe sp11_swr_hw_params 'qcom_swrm_hw_params substream=%x0:u64 params=%x1:u64 dai=%x2:u64'
fi
if grep -qw swrm_runtime_suspend /proc/kallsyms; then
    add_probe sp11_swr_suspend 'swrm_runtime_suspend dev=%x0:u64'
fi
if grep -qw swrm_runtime_resume /proc/kallsyms; then
    add_probe sp11_swr_resume 'swrm_runtime_resume dev=%x0:u64'
fi
if grep -qw sdw_prepare_stream /proc/kallsyms; then
    add_probe sp11_sdw_prepare 'sdw_prepare_stream stream=%x0:u64'
fi
if grep -qw sdw_enable_stream /proc/kallsyms; then
    add_probe sp11_sdw_enable 'sdw_enable_stream stream=%x0:u64'
fi
if grep -qw sdw_disable_stream /proc/kallsyms; then
    add_probe sp11_sdw_disable 'sdw_disable_stream stream=%x0:u64'
fi
if grep -qw sdw_deprepare_stream /proc/kallsyms; then
    add_probe sp11_sdw_deprepare 'sdw_deprepare_stream stream=%x0:u64'
fi

# Keep only this bounded probe group enabled.
echo 0 > "$TRACE/tracing_on"
echo > "$TRACE/trace"
[ -w "$TRACE/buffer_size_kb" ] && echo 8192 > "$TRACE/buffer_size_kb" || true
if [ -d "$TRACE/events/sp11" ]; then
    echo 1 > "$TRACE/events/sp11/enable"
else
    echo "sp11 event group not created"
    exit 3
fi
echo 1 > "$TRACE/tracing_on"
printf '%s SP11TRACE armed and tracing\n' "$(date -Ins)"
