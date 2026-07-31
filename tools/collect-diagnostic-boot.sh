#!/usr/bin/env bash
# Read-only post-boot evidence collector for the SP11 audio observation kernel.
# It does not open a PCM stream, change an ALSA control, write debugfs, or alter
# the installed system. Run as root after boot for complete journal/debugfs data.

set -u

expected_kernel=${EXPECTED_KERNEL:-7.1.5-sp11-audio-diag-observe}
project_root=${SP11_PROJECT_ROOT:-/home/geoca/Documents/SP11-PROJECT}
timestamp=$(date -u +%Y%m%dT%H%M%SZ)
output_root=${OUTPUT_ROOT:-${project_root}/01-audio/artifacts/diagnostic-boots}
output_dir=${output_root}/${timestamp}
mkdir -p -- "$output_dir"

capture() {
    local name=$1
    shift
    {
        printf '$'
        printf ' %q' "$@"
        printf '\n\n'
        "$@"
    } >"${output_dir}/${name}" 2>&1 || true
}

capture_shell() {
    local name=$1
    local command=$2
    capture "$name" bash -c "$command"
}

# Wait only for device enumeration; never start audio playback.
for _ in $(seq 1 25); do
    grep -q 'X1E80100' /proc/asound/cards 2>/dev/null && break
    sleep 1
done

capture uname.txt uname -a
capture cmdline.txt cat /proc/cmdline
capture uptime.txt cat /proc/uptime
capture os-release.txt cat /etc/os-release
capture kernel-journal.txt journalctl -b -k --no-pager -o short-monotonic
capture diagnostic-kernel-lines.txt bash -c \
    "journalctl -b -k --no-pager -o short-monotonic | grep -Eai 'SP11 GET_CFG|sp11-getcfg:|SP11 SoundWire|SP11 .*stream|SP11 stage|SP11.*protection|SPVI|VISENSE|PBR|CPS|wsa884|soundwire|q6apm|audioreach' || true"
capture boot-errors.txt bash -c \
    "journalctl -b -k --no-pager -p warning..alert -o short-monotonic || true"
capture modules.txt bash -c \
    "lsmod | grep -E '^(snd|soundwire|q6|wsa|lpass|apr|gpr)' || true"
capture proc-asound-cards.txt cat /proc/asound/cards
capture proc-asound-pcm.txt cat /proc/asound/pcm
capture aplay-list.txt aplay -l
capture pactl-info.txt pactl info
capture pactl-sinks.txt pactl list sinks
capture wpctl-status.txt wpctl status
capture alsa-controls.txt bash -c \
    "for card in /proc/asound/card[0-9]*; do [ -d \"\$card\" ] || continue; n=\${card##*card}; printf '\n===== card %s =====\n' \"\$n\"; amixer -c \"\$n\" contents; done"
capture smart-amp-controls.txt bash -c \
    "for card in /proc/asound/card[0-9]*; do [ -d \"\$card\" ] || continue; n=\${card##*card}; amixer -c \"\$n\" contents 2>/dev/null | grep -EA5 -B2 'Spkr(Left|Right) (PBR|VISENSE|CPS) Switch|PA Volume|WSA MODE' || true; done"
capture soundwire-sysfs.txt bash -c \
    "find /sys/bus/soundwire/devices -maxdepth 5 -mindepth 1 -printf '%y %p -> %l\n' 2>/dev/null; for f in /sys/bus/soundwire/devices/*/{status,modalias,uevent}; do [ -r \"\$f\" ] || continue; printf '\n===== %s =====\n' \"\$f\"; cat \"\$f\"; done"
capture soundwire-debugfs.txt bash -c \
    "find /sys/kernel/debug/soundwire -maxdepth 6 -type f -print 2>/dev/null | while read -r f; do printf '\n===== %s =====\n' \"\$f\"; cat \"\$f\" 2>/dev/null || true; done"
capture asoc-debugfs.txt bash -c \
    "find /sys/kernel/debug/asoc -maxdepth 6 -type f -print 2>/dev/null | while read -r f; do printf '\n===== %s =====\n' \"\$f\"; cat \"\$f\" 2>/dev/null || true; done"
capture pcm-runtime.txt bash -c \
    "for f in /proc/asound/card*/pcm*/sub*/{hw_params,status,info}; do [ -r \"\$f\" ] || continue; printf '\n===== %s =====\n' \"\$f\"; cat \"\$f\"; done"

# Preserve full read-only WSA regmaps and a compact target-register view.
capture wsa-regmaps-full.txt bash -c \
    "for f in /sys/kernel/debug/regmap/*0217:0204*/registers; do [ -r \"\$f\" ] || continue; printf '\n===== %s =====\n' \"\$f\"; cat \"\$f\"; done"
capture wsa-register-focus.txt bash -c \
    "for f in /sys/kernel/debug/regmap/*0217:0204*/registers; do [ -r \"\$f\" ] || continue; printf '\n===== %s =====\n' \"\$f\"; grep -Ei '^(3020|3021|304c|3091|3468|34e0|34e1|34e2|34e3|34e4|34e5|34e6|34e7|34e8|34e9|34ea|34eb|34ec|34ed|34ee|34ef):' \"\$f\" || true; done"

capture module-identities.txt bash -c '
mods="snd_q6apm snd_soc_wsa884x snd_soc_x1e80100"
for mod in $mods; do
    printf "\n===== %s =====\n" "$mod"
    modinfo "$mod" 2>&1 || true
    path=$(modinfo -n "$mod" 2>/dev/null || true)
    [ -n "$path" ] && [ -r "$path" ] && sha256sum "$path"
    [ -r "/sys/module/$mod/srcversion" ] && printf "loaded-srcversion=" && cat "/sys/module/$mod/srcversion"
done'

capture runtime-audio-hashes.txt bash -c '
for f in \
 /lib/firmware/qcom/x1e80100/X1E80100-Microsoft-Surface-Pro-11-tplg.bin \
 /usr/share/alsa/ucm2/Qualcomm/x1e80100/x1e80100.conf \
 /usr/share/alsa/ucm2/Qualcomm/x1e80100/MICROSOFT-Surface-Pro-11in.conf \
 /usr/share/alsa/ucm2/Qualcomm/x1e80100/SP11-HiFi.conf; do
    [ -r "$f" ] && sha256sum "$f"
done'

observed_kernel=$(uname -r)
{
    printf 'captured_utc=%s\n' "$timestamp"
    printf 'expected_kernel=%s\n' "$expected_kernel"
    printf 'observed_kernel=%s\n' "$observed_kernel"
    printf 'candidate_boot=%s\n' "$([[ "$observed_kernel" == "$expected_kernel" ]] && printf true || printf false)"
    printf 'opens_pcm_stream=false\n'
    printf 'changes_alsa_controls=false\n'
    printf 'writes_debugfs=false\n'
    printf 'output_dir=%s\n' "$output_dir"
} >"${output_dir}/manifest.txt"

(
    cd "$output_dir"
    sha256sum -- * > SHA256SUMS
)
ln -sfn -- "$timestamp" "$output_root/latest"
printf '%s\n' "$output_dir"
