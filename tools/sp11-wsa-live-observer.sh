#!/usr/bin/env bash
# Read-only WSA884x register observer for controlled SP11 playback tests.
#
# This script never writes a codec register, changes a mixer control, or starts
# audio.  It samples the two regmap debugfs files that the running kernel
# already exposes and records raw values plus a few unambiguous bit decodes.
# A debugfs register dump is not atomic and can take seconds while the amp is
# runtime-suspended, so every amplifier row receives its own real timestamp.
#
# Usage:
#   sudo ./tools/sp11-wsa-live-observer.sh --duration 30 --interval 0.25
#   sudo ./tools/sp11-wsa-live-observer.sh --output /tmp/wsa.tsv

set -uo pipefail

duration=30
interval=0.25
output=""

usage() {
    sed -n '2,11p' "$0"
}

while (($#)); do
    case "$1" in
        --duration) duration=${2:?}; shift 2 ;;
        --interval) interval=${2:?}; shift 2 ;;
        --output)   output=${2:?};   shift 2 ;;
        -h|--help)  usage; exit 0 ;;
        *) printf 'unknown argument: %s\n' "$1" >&2; exit 2 ;;
    esac
done

if ! awk -v v="${duration}" 'BEGIN { exit !(v > 0) }' </dev/null ||
   ! awk -v v="${interval}" 'BEGIN { exit !(v > 0) }' </dev/null; then
    printf 'duration and interval must be positive numbers\n' >&2
    exit 2
fi

shopt -s nullglob
declare -a maps=(/sys/kernel/debug/regmap/sdw:*:0217:0204:*)
shopt -u nullglob

if ((${#maps[@]} == 0)); then
    printf 'no WSA884x SoundWire regmaps found under /sys/kernel/debug/regmap\n' >&2
    printf 'mount debugfs and run this observer as root\n' >&2
    exit 1
fi

for map in "${maps[@]}"; do
    if [[ ! -r "${map}/registers" ]]; then
        printf 'cannot read %s/registers; run as root\n' "${map}" >&2
        exit 1
    fi
done

declare -a addresses=(
    3015 3016 3034
    3076 3077 3078 3079 307a 307b
    3091
    3430 3435 3436 3438 3439
    3450 3452 3453 3454 3455
    345a 345b 345c 345d 3464 3468
    34d8 34e0
)

read_registers() {
    local file=$1
    awk '
        function emit(    order, count, keys, i) {
            order="3015 3016 3034 3076 3077 3078 3079 307a 307b 3091 3430 3435 3436 3438 3439 3450 3452 3453 3454 3455 345a 345b 345c 345d 3464 3468 34d8 34e0";
            count=split(order, keys, " ");
            for (i=1; i<=count; i++)
                printf "%s%s", (i == 1 ? "" : " "), (keys[i] in value ? value[keys[i]] : "NA");
            printf "\n";
        }
        BEGIN {
            wanted["3015"]; wanted["3016"]; wanted["3034"];
            wanted["3076"]; wanted["3077"]; wanted["3078"];
            wanted["3079"]; wanted["307a"]; wanted["307b"];
            wanted["3091"];
            wanted["3430"]; wanted["3435"]; wanted["3436"];
            wanted["3438"]; wanted["3439"];
            wanted["3450"]; wanted["3452"]; wanted["3453"];
            wanted["3454"]; wanted["3455"];
            wanted["345a"]; wanted["345b"]; wanted["345c"];
            wanted["345d"]; wanted["3464"]; wanted["3468"];
            wanted["34d8"]; wanted["34e0"];
        }
        {
            key=$1; sub(/:$/, "", key);
            if (key in wanted) value[key]=$2;
            if (key == "34e0") {
                emit(); emitted=1; exit;
            }
        }
        END {
            if (!emitted) emit();
        }
    ' "$file"
}

decode_hex() {
    local value=${1:-NA}
    if [[ "$value" =~ ^[[:xdigit:]]+$ ]]; then
        printf '%d' "$((16#${value}))"
    else
        printf '%s' -1
    fi
}

emit_header() {
    printf 'elapsed_s\tutc\tamp'
    for address in "${addresses[@]}"; do printf '\tr_%s' "$address"; done
    printf '\tpa_enabled\tcurrent_limit_override\tcurrent_limit_code\tpa_error_nonzero\tcps_ctl_nonzero\n'
}

emit_sample() {
    local map=$1 raw current pa_en err0 err1 cps now_ns elapsed
    local current_dec pa_dec err0_dec err1_dec cps_dec
    raw=$(read_registers "${map}/registers")
    read -r -a values <<<"${raw}"

    now_ns=$(date +%s%N)
    elapsed=$(awk -v v="$((now_ns - start_ns))" 'BEGIN { printf "%.3f", v / 1000000000 }')

    current=${values[9]:-NA}
    pa_en=${values[10]:-NA}
    err0=${values[13]:-NA}
    err1=${values[14]:-NA}
    cps=${values[25]:-NA}
    current_dec=$(decode_hex "$current")
    pa_dec=$(decode_hex "$pa_en")
    err0_dec=$(decode_hex "$err0")
    err1_dec=$(decode_hex "$err1")
    cps_dec=$(decode_hex "$cps")

    printf '%s\t%s\t%s' "$elapsed" "$(date -u +%Y-%m-%dT%H:%M:%S.%3NZ)" "$(basename "$map")"
    for value in "${values[@]}"; do printf '\t%s' "$value"; done
    printf '\t%s\t%s\t%s\t%s\t%s\n' \
        "$((pa_dec >= 0 ? pa_dec & 1 : -1))" \
        "$((current_dec >= 0 ? (current_dec >> 7) & 1 : -1))" \
        "$((current_dec >= 0 ? (current_dec & 0x7c) >> 2 : -1))" \
        "$((err0_dec > 0 || err1_dec > 0 ? 1 : 0))" \
        "$((cps_dec > 0 ? 1 : 0))"
}

if [[ -n "$output" ]]; then
    mkdir -p -- "$(dirname -- "$output")"
    exec > >(tee -- "$output")
fi

emit_header
start_ns=$(date +%s%N)
duration_ns=$(awk -v v="$duration" 'BEGIN { printf "%.0f", v * 1000000000 }')

while :; do
    now_ns=$(date +%s%N)
    elapsed_ns=$((now_ns - start_ns))
    ((elapsed_ns <= duration_ns)) || break
    for map in "${maps[@]}"; do emit_sample "$map"; done
    sleep "$interval"
done
