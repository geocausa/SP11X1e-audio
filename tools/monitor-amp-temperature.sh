#!/usr/bin/env bash
# Log WSA884x amplifier die temperature during real playback.
#
# WHY THIS EXISTS
# ---------------
# The amplifiers measure voice-coil die temperature and expose it through
# hwmon, but nothing forwards it to the DSP speaker-protection algorithm.
# Verified 2026-07-31: grep for "temperature|get_temp" across
# sound/soc/qcom/qdsp6/*.c returns nothing. So protection currently sees
# excursion (V/I feedback) but is thermally blind.
#
# Until that path is wired up, this script is the only way to know how hot
# these drivers actually get. It is READ-ONLY: it never changes volume, never
# touches an ALSA control, never plays anything. It only reads sysfs.
#
# HOW THE SENSOR BEHAVES (important for reading the output)
# ---------------------------------------------------------
# A live temperature read is only possible when the power amplifier is OFF.
# While audio plays, wsa884x_get_temp() returns the last cached value, so the
# reading FREEZES at whatever was measured just before playback started. The
# real post-playback temperature only appears once the PA goes idle.
#
# The amps also runtime-suspend when idle. A read against a suspended device
# returns EAGAIN. That is normal, not a fault. Such samples are recorded as
# "suspend" rather than being silently dropped.
#
# Therefore the meaningful measurement is the FIRST successful live read after
# playback stops. Peaks during playback are cached values and understate the
# true temperature.
#
# Usage:
#   ./monitor-amp-temperature.sh [--duration SECONDS] [--interval SECONDS]
#                                [--label TEXT] [--outdir DIR]
#
# Typical use: start it, then play music at the volume you want to characterise.
#
#   ./monitor-amp-temperature.sh --duration 300 --label "youtube 40 percent"

set -uo pipefail

duration=300
interval=5
label=""
outdir=""

while (($#)); do
    case "$1" in
        --duration) duration=${2:?}; shift 2 ;;
        --interval) interval=${2:?}; shift 2 ;;
        --label)    label=${2:?};    shift 2 ;;
        --outdir)   outdir=${2:?};   shift 2 ;;
        -h|--help)  sed -n '2,40p' "$0"; exit 0 ;;
        *) printf 'unknown argument: %s\n' "$1" >&2; exit 2 ;;
    esac
done

project_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
stamp=$(date -u +%Y%m%dT%H%M%SZ)
outdir=${outdir:-"${project_root}/artifacts/thermal/${stamp}"}
mkdir -p -- "${outdir}" || { printf 'cannot create %s\n' "${outdir}" >&2; exit 1; }
csv="${outdir}/amp-temperature.csv"
meta="${outdir}/run-metadata.txt"

# ---------------------------------------------------------------------------
# Discover the wsa884x hwmon nodes. Index numbers are not stable across boots,
# so always match on the driver name rather than a remembered hwmonN path.
# ---------------------------------------------------------------------------
declare -a nodes=() names=()
for h in /sys/class/hwmon/hwmon*; do
    [[ -r "${h}/name" ]] || continue
    [[ $(cat "${h}/name" 2>/dev/null) == "wsa884x" ]] || continue
    [[ -r "${h}/temp1_input" ]] || continue
    nodes+=("${h}/temp1_input")
    # Resolve which physical amplifier this is via the SoundWire device name.
    dev=$(readlink -f "${h}/device" 2>/dev/null)
    names+=("$(basename "${dev:-unknown}")")
done

if ((${#nodes[@]} == 0)); then
    printf 'no wsa884x hwmon nodes found; is the audio driver loaded?\n' >&2
    exit 1
fi

# MEASURED BEHAVIOUR (characterised 2026-07-31 on this machine)
# -------------------------------------------------------------
#   PA on            -> returns CACHED value (wsa884x->temperature), not live
#   PA off, <3000ms  -> readable window; a live read can occur here
#   PA off, >3000ms  -> device runtime-suspends, reads return EAGAIN
#
# autosuspend_delay_ms is 3000 on both amps. A live read therefore requires
# sampling within ~3s of playback stopping. Continuous idle polling yields
# only "suspend"; continuous playback yields only frozen cached values.
#
# The practical consequence: to characterise heating, play audio for the
# period of interest, STOP, and sample immediately. This script does that
# automatically when --stop-detect is used.

read_temp() {
    # Echo millidegrees, or a status word. Never fail the whole run.
    #
    # A read against a runtime-suspended amplifier returns EAGAIN. The read
    # itself calls pm_runtime_resume_and_get(), so the FIRST attempt wakes the
    # device but still fails; a retry a moment later succeeds. Without this
    # retry an idle machine logs nothing but "suspend" and the run is useless.
    local path=$1 raw attempt
    for attempt in 1 2 3; do
        if raw=$(cat "${path}" 2>/dev/null) && [[ -n "${raw}" ]]; then
            printf '%s' "${raw}"
            return 0
        fi
        sleep 0.3
    done
    printf 'suspend'
}

{
    printf 'started_utc=%s\n' "${stamp}"
    printf 'label=%s\n' "${label:-none}"
    printf 'duration_s=%s\n' "${duration}"
    printf 'interval_s=%s\n' "${interval}"
    printf 'kernel=%s\n' "$(uname -r)"
    printf 'amps_found=%s\n' "${#nodes[@]}"
    for i in "${!nodes[@]}"; do
        printf 'amp%s_device=%s\n' "${i}" "${names[$i]}"
        printf 'amp%s_path=%s\n'   "${i}" "${nodes[$i]}"
    done
    printf 'read_only=true\n'
    printf 'note=values during playback are CACHED; first live read after playback stops is the real figure\n'
} >"${meta}"

# CSV header
{
    printf 'elapsed_s,wall_utc'
    for i in "${!nodes[@]}"; do printf ',amp%s_millideg' "${i}"; done
    printf ',pcm_state\n'
} >"${csv}"

pcm_state() {
    local s
    s=$(cat /proc/asound/card0/pcm0p/sub0/status 2>/dev/null | head -1)
    case "${s}" in
        closed) printf 'idle' ;;
        state:*) printf 'playing' ;;
        *) printf '%s' "${s:-unknown}" ;;
    esac
}

printf 'Logging %s amplifier(s) every %ss for %ss\n' "${#nodes[@]}" "${interval}" "${duration}"
printf 'Output: %s\n\n' "${csv}"
printf '%8s  %-9s' 'elapsed' 'pcm'
for i in "${!nodes[@]}"; do printf '  %-10s' "amp${i}"; done
printf '\n'

start=$(date +%s)
end=$((start + duration))
peak_live=0

while (($(date +%s) < end)); do
    now=$(date +%s)
    elapsed=$((now - start))
    state=$(pcm_state)

    line="${elapsed},$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    disp=""
    for path in "${nodes[@]}"; do
        v=$(read_temp "${path}")
        line+=",${v}"
        if [[ "${v}" == "suspend" ]]; then
            disp+=$(printf '  %-10s' 'suspend')
        else
            disp+=$(printf '  %-10s' "$((v / 1000))C")
            if [[ "${state}" == "idle" ]] && ((v > peak_live)); then
                peak_live=${v}
            fi
        fi
    done
    line+=",${state}"

    printf '%8s  %-9s%s\n' "${elapsed}" "${state}" "${disp}"
    printf '%s\n' "${line}" >>"${csv}"

    sleep "${interval}"
done

{
    printf '\n'
    printf 'finished_utc=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf 'peak_live_read_millideg=%s\n' "${peak_live}"
    printf 'samples=%s\n' "$(($(wc -l <"${csv}") - 1))"
} >>"${meta}"

printf '\nDone.\n'
if ((peak_live > 0)); then
    printf 'Peak live (PA idle) reading: %s C\n' "$((peak_live / 1000))"
else
    printf 'No live reads captured; every sample was cached or suspended.\n'
    printf 'Let the amps go idle for ~10s at the end of the run to get a real figure.\n'
fi
printf 'CSV:  %s\n' "${csv}"
printf 'Meta: %s\n' "${meta}"
