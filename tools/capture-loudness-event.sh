#!/usr/bin/env bash
# Observe a heard loudness event without changing mixer or codec state.

set -u

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
duration=120
record_sink=false
output_dir=

usage() {
    cat <<'EOF'
Usage: capture-loudness-event.sh [options]

Options:
  --duration SECONDS  capture duration (default: 120)
  --record-sink       record the PipeWire default-sink monitor as 48 kHz s16 WAV
  --output DIR        output directory (default: artifacts/live/loudness-TIMESTAMP)
  -h, --help          show this help

The script never changes volume or ALSA controls. While it runs, press Enter
whenever a loudness jump is heard; the timestamp is recorded in heard-events.tsv.
EOF
}

while (($#)); do
    case $1 in
        --duration)
            [[ $# -ge 2 ]] || { printf 'error: --duration needs a value\n' >&2; exit 2; }
            duration=$2
            shift 2
            ;;
        --record-sink)
            record_sink=true
            shift
            ;;
        --output)
            [[ $# -ge 2 ]] || { printf 'error: --output needs a value\n' >&2; exit 2; }
            output_dir=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'error: unknown option: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

[[ ${duration} =~ ^[1-9][0-9]*$ ]] || {
    printf 'error: duration must be a positive integer\n' >&2
    exit 2
}

timestamp=$(date -u +%Y%m%dT%H%M%SZ)
output_dir=${output_dir:-"${project_root}/artifacts/live/loudness-${timestamp}"}
mkdir -p -- "${output_dir}"

snapshot() {
    local label=$1
    wpctl get-volume @DEFAULT_AUDIO_SINK@ >"${output_dir}/${label}-volume.txt" 2>&1 || true
    wpctl inspect @DEFAULT_AUDIO_SINK@ >"${output_dir}/${label}-sink.txt" 2>&1 || true
    amixer -D hw:0 scontents >"${output_dir}/${label}-alsa-controls.txt" 2>&1 || true
    pw-dump >"${output_dir}/${label}-pipewire.json" 2>&1 || true
}

snapshot before
start_realtime=$(date --iso-8601=ns)
start_epoch=$(date +%s.%N)
start_epoch_seconds=$(date +%s)

{
    printf 'start_realtime=%s\n' "${start_realtime}"
    printf 'start_epoch=%s\n' "${start_epoch}"
    printf 'duration_seconds=%s\n' "${duration}"
    printf 'record_sink=%s\n' "${record_sink}"
    printf 'kernel=%s\n' "$(uname -r)"
} >"${output_dir}/manifest.txt"

timeout --signal=INT "${duration}s" stdbuf -oL -eL \
    alsactl monitor hw:0 >"${output_dir}/alsa-control-events.txt" 2>&1 &
alsa_monitor_pid=$!

timeout --signal=INT "${duration}s" stdbuf -oL -eL \
    pw-mon -N -o -a >"${output_dir}/pipewire-events.txt" 2>&1 &
pipewire_monitor_pid=$!

(
    end=$((SECONDS + duration))
    while ((SECONDS < end)); do
        printf '%s\t' "$(date +%s.%N)"
        wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>&1 || true
        sleep 0.1
    done
) >"${output_dir}/pipewire-volume.tsv" &
volume_monitor_pid=$!

record_pid=
if [[ ${record_sink} == true ]]; then
    sink_serial=$(wpctl inspect @DEFAULT_AUDIO_SINK@ 2>/dev/null |
        sed -n 's/^[[:space:]]*\*\{0,1\}[[:space:]]*object.serial = "\([0-9][0-9]*\)"/\1/p' |
        head -1)
    if [[ -n ${sink_serial} ]]; then
        timeout --signal=INT "${duration}s" pw-record \
            --target "${sink_serial}" --rate 48000 --channels 2 --format s16 \
            "${output_dir}/sink-monitor.wav" \
            >"${output_dir}/sink-monitor-record.log" 2>&1 &
        record_pid=$!
    else
        printf 'Could not resolve default sink serial.\n' \
            >"${output_dir}/sink-monitor-record.log"
    fi
fi

printf 'Capturing for %s seconds in %s\n' "${duration}" "${output_dir}"
if [[ -t 0 ]]; then
    printf 'Press Enter whenever you hear a loudness jump. Do not adjust volume.\n'
    end=$((SECONDS + duration))
    while ((SECONDS < end)); do
        if read -r -t 1; then
            printf '%s\tHEARD_LOUDNESS_JUMP\n' "$(date +%s.%N)" |
                tee -a "${output_dir}/heard-events.tsv"
        fi
    done
else
    sleep "${duration}"
fi

wait "${alsa_monitor_pid}" 2>/dev/null || true
wait "${pipewire_monitor_pid}" 2>/dev/null || true
wait "${volume_monitor_pid}" 2>/dev/null || true
if [[ -n ${record_pid} ]]; then
    wait "${record_pid}" 2>/dev/null || true
fi

snapshot after
journalctl -b -k --since "@${start_epoch_seconds}" --no-pager \
    >"${output_dir}/kernel-log.txt" 2>&1 || true
diff -u "${output_dir}/before-alsa-controls.txt" \
    "${output_dir}/after-alsa-controls.txt" \
    >"${output_dir}/alsa-controls.diff" || true
diff -u "${output_dir}/before-volume.txt" \
    "${output_dir}/after-volume.txt" \
    >"${output_dir}/pipewire-volume.diff" || true

printf 'Capture complete: %s\n' "${output_dir}"
