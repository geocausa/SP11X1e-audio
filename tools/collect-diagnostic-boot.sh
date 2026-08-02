#!/usr/bin/env bash
# Read-only post-boot evidence collector for the SP11 audio observation kernel.
# It does not open a PCM stream, change an ALSA control, write debugfs, or alter
# the installed system. Run as root after boot for complete journal/debugfs data.

set -u

expected_kernel=${EXPECTED_KERNEL:-7.1.5-sp11-audio-diag-observe}
expected_topology=${EXPECTED_TOPOLOGY:-X1E80100-Microsoft-Surface-Pro-11-tplg.bin}
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
capture device-tree-model.txt bash -c "tr '\0' '\n' </proc/device-tree/sound/model"
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
capture soundwire-debugfs.txt sudo bash -c \
    "find /sys/kernel/debug/soundwire -maxdepth 6 -type f -print 2>/dev/null | while read -r f; do printf '\n===== %s =====\n' \"\$f\"; cat \"\$f\" 2>/dev/null || true; done"
capture asoc-debugfs.txt sudo bash -c \
    "find /sys/kernel/debug/asoc -maxdepth 6 -type f -print 2>/dev/null | while read -r f; do printf '\n===== %s =====\n' \"\$f\"; cat \"\$f\" 2>/dev/null || true; done"
capture pcm-runtime.txt bash -c \
    "for f in /proc/asound/card*/pcm*/sub*/{hw_params,status,info}; do [ -r \"\$f\" ] || continue; printf '\n===== %s =====\n' \"\$f\"; cat \"\$f\"; done"

# A full regmap sweep issues tens of thousands of SoundWire reads and can add
# substantial traffic while a peripheral is reattaching.  Keep it opt-in;
# MapDiag's driver-side health snapshots are the safe default for live failures.
capture_full_wsa_regmap=${SP11_CAPTURE_FULL_WSA_REGMAP:-0}
if [[ "$capture_full_wsa_regmap" == 1 ]]; then
    capture wsa-regmaps-full.txt sudo bash -c \
        "for f in /sys/kernel/debug/regmap/*0217:0204*/registers; do [ -r \"\$f\" ] || continue; printf '\n===== %s =====\n' \"\$f\"; cat \"\$f\"; done"
fi
# Do not pipe the debugfs registers file through grep for a "focused" view.
# It is a generated file, so grep must read every register before filtering and
# is therefore just as invasive as saving a full dump.  MapDiag reports the
# required live health registers from the driver without walking the regmap.
capture wsa-register-focus.txt bash -c \
    "printf '%s\n' 'disabled: use MapDiag kernel health snapshots; debugfs filtering still walks the full regmap'"

capture sp11-mapdiag-controls.txt bash -c '
for f in \
 /sys/module/snd_soc_wsa884x/parameters/sp11_recover_sta0 \
 /sys/module/snd_soc_wsa884x/parameters/sp11_recover_unique_id \
 /sys/module/snd_q6apm/parameters/sp11_swap_vi_speakers \
 /sys/module/snd_q6apm/parameters/sp11_park_protection; do
    [ -r "$f" ] || continue
    printf "%s = " "$f"
    cat "$f"
done'
capture sp11-mapdiag-gpios.txt sudo bash -c '
for f in /sys/kernel/debug/pinctrl/*/pinmux-pins; do
    [ -r "$f" ] || continue
    printf "\n===== %s =====\n" "$f"
    grep -E "(^|[[:space:]])pin (202|204|205) " "$f" || true
done
printf "\n===== gpio consumers =====\n"
cat /sys/kernel/debug/gpio 2>/dev/null || true'

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
expected_topology=$1
for f in \
 "/lib/firmware/qcom/x1e80100/${expected_topology}" \
 /usr/share/alsa/ucm2/Qualcomm/x1e80100/x1e80100.conf \
 /usr/share/alsa/ucm2/Qualcomm/x1e80100/MICROSOFT-Surface-Pro-11in.conf \
 /usr/share/alsa/ucm2/Qualcomm/x1e80100/SP11-HiFi.conf; do
    [ -r "$f" ] && sha256sum "$f"
done' _ "$expected_topology"

observed_kernel=$(uname -r)
{
    printf 'captured_utc=%s\n' "$timestamp"
    printf 'expected_kernel=%s\n' "$expected_kernel"
    printf 'observed_kernel=%s\n' "$observed_kernel"
    printf 'expected_topology=%s\n' "$expected_topology"
    printf 'candidate_boot=%s\n' "$([[ "$observed_kernel" == "$expected_kernel" ]] && printf true || printf false)"
    printf 'opens_pcm_stream=false\n'
    printf 'changes_alsa_controls=false\n'
    printf 'writes_debugfs=false\n'
    printf 'captures_full_wsa_regmap=%s\n' "$capture_full_wsa_regmap"
    printf 'captures_focused_wsa_regmap=false\n'
    printf 'output_dir=%s\n' "$output_dir"
} >"${output_dir}/manifest.txt"

(
    cd "$output_dir"
    sha256sum -- * > SHA256SUMS
)
ln -sfn -- "$timestamp" "$output_root/latest"
printf '%s\n' "$output_dir"
