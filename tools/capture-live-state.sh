#!/usr/bin/env bash
# Capture the SP11 audio configuration without opening or changing an audio stream.

set -u

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
timestamp=$(date -u +%Y%m%dT%H%M%SZ)
output_dir=${1:-"${project_root}/artifacts/live/${timestamp}"}

mkdir -p -- "${output_dir}"

run_capture() {
    local name=$1
    shift
    {
        printf '$'
        printf ' %q' "$@"
        printf '\n\n'
        "$@"
    } >"${output_dir}/${name}" 2>&1 || true
}

read_nul_file() {
    local source=$1
    local destination=$2
    if [[ -r "${source}" ]]; then
        tr '\0' '\n' <"${source}" >"${output_dir}/${destination}"
    fi
}

run_capture uname.txt uname -a
run_capture os-release.txt cat /etc/os-release
read_nul_file /proc/device-tree/model device-tree-model.txt
read_nul_file /proc/device-tree/compatible device-tree-compatible.txt

run_capture alsa-cards.txt cat /proc/asound/cards
run_capture alsa-pcm.txt cat /proc/asound/pcm
run_capture aplay-devices.txt aplay -l
run_capture arecord-devices.txt arecord -l
run_capture alsa-controls.txt amixer -D hw:0 scontents
run_capture pipewire-version.txt pipewire --version
run_capture wireplumber-version.txt wireplumber --version
run_capture wireplumber-status.txt wpctl status

if [[ -d /sys/bus/soundwire/devices ]]; then
    run_capture soundwire-devices.txt find /sys/bus/soundwire/devices \
        -maxdepth 2 -mindepth 1 -printf '%y %p -> %l\n'
fi

if command -v rg >/dev/null 2>&1; then
    run_capture audio-modules.txt bash -c \
        "lsmod | rg '^(snd|soundwire|q6|wsa|lpass|apr|gpr)'"
    run_capture kernel-audio-log.txt bash -c \
        "journalctl -b -k --no-pager | rg -i 'asoc|alsa|audio|soundwire|wsa884|q6apm|qcom-apm|lpass'"
else
    run_capture audio-modules.txt lsmod
    run_capture kernel-audio-log.txt journalctl -b -k --no-pager
fi

topology=/lib/firmware/qcom/x1e80100/X1E80100-Microsoft-Surface-Pro-11-tplg.bin
ucm_root=/usr/share/alsa/ucm2/Qualcomm/x1e80100
hash_targets=()
[[ -f "${topology}" ]] && hash_targets+=("${topology}")
[[ -f "${ucm_root}/x1e80100.conf" ]] && hash_targets+=("${ucm_root}/x1e80100.conf")
[[ -f "${ucm_root}/MICROSOFT-Surface-Pro-11in.conf" ]] && \
    hash_targets+=("${ucm_root}/MICROSOFT-Surface-Pro-11in.conf")
[[ -f "${ucm_root}/SP11-HiFi.conf" ]] && hash_targets+=("${ucm_root}/SP11-HiFi.conf")
if ((${#hash_targets[@]})); then
    run_capture runtime-hashes.txt sha256sum "${hash_targets[@]}"
fi

kernel_build=/lib/modules/$(uname -r)/build
if [[ -e "${kernel_build}" ]]; then
    run_capture kernel-build-link.txt readlink -f "${kernel_build}"
    resolved_build=$(readlink -f "${kernel_build}")
    if git -C "${resolved_build}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        run_capture kernel-git-head.txt git -C "${resolved_build}" show \
            --no-patch --format=fuller HEAD
        run_capture kernel-git-status.txt git -C "${resolved_build}" status \
            --short --branch
    fi
fi

{
    printf 'captured_utc=%s\n' "${timestamp}"
    printf 'hostname=%s\n' "$(hostname)"
    printf 'kernel=%s\n' "$(uname -r)"
    printf 'topology=%s\n' "${topology}"
    printf 'capture_opens_audio_streams=false\n'
} >"${output_dir}/manifest.txt"

printf 'Captured read-only audio state in %s\n' "${output_dir}"
