#!/usr/bin/env bash
# Install the SP11 audio runtime configuration from this repository onto the
# running system. Idempotent: safe to re-run. Every replaced file is backed up
# with a timestamp suffix, and --uninstall reverses everything.
#
# Usage:
#   sudo ./deploy/install-audio-config.sh [--dry-run] [--no-topology]
#                                             [--with-pipewire-eq]
#   sudo ./deploy/install-audio-config.sh --uninstall
#
# The PipeWire EQ is a per-user file and is installed for $SUDO_USER (or the
# invoking user when not run through sudo).

set -euo pipefail

deploy_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
stamp=$(date -u +%Y%m%dT%H%M%SZ)

ucm_root=/usr/share/alsa/ucm2
ucm_vendor_dir="${ucm_root}/Qualcomm/x1e80100"
card_conf="${ucm_vendor_dir}/x1e80100.conf"
divert_conf="${card_conf}.distrib"
fw_dir=/lib/firmware/qcom/x1e80100
topology_name=X1E80100-Microsoft-Surface-Pro-11-tplg.bin

dry_run=0
do_uninstall=0
install_topology=1
install_pipewire_eq=0
for arg in "$@"; do
    case "${arg}" in
        --dry-run)      dry_run=1 ;;
        --uninstall)    do_uninstall=1 ;;
        --no-topology)  install_topology=0 ;;
        --with-pipewire-eq) install_pipewire_eq=1 ;;
        *) printf 'unknown argument: %s\n' "${arg}" >&2; exit 2 ;;
    esac
done

if ((EUID != 0)); then
    printf 'must run as root (system paths are written)\n' >&2
    exit 1
fi

target_user=${SUDO_USER:-${USER}}
target_home=$(getent passwd "${target_user}" | cut -d: -f6)
if [[ -z "${target_home}" || ! -d "${target_home}" ]]; then
    printf 'cannot resolve home directory for user %s\n' "${target_user}" >&2
    exit 1
fi
pw_dir="${target_home}/.config/pipewire/pipewire.conf.d"
pw_conf="${pw_dir}/99-sp11-speaker-eq.conf"

run() {
    if ((dry_run)); then
        printf 'DRY-RUN:'
        printf ' %q' "$@"
        printf '\n'
    else
        "$@"
    fi
}

note() { printf '  %s\n' "$*"; }

# ---------------------------------------------------------------------------
# Preflight: refuse to run against hardware this configuration is not for.
# ---------------------------------------------------------------------------
preflight() {
    local model_file=/proc/device-tree/model model=""
    [[ -r "${model_file}" ]] && model=$(tr -d '\0' <"${model_file}")
    if [[ "${model}" != *"Surface Pro 11"* ]]; then
        printf 'refusing: device-tree model is "%s", expected a Surface Pro 11\n' \
            "${model}" >&2
        exit 1
    fi
    note "model: ${model}"

    local amps
    amps=$(find /sys/bus/soundwire/devices -maxdepth 1 -name 'sdw:*' 2>/dev/null | wc -l)
    if ((amps != 2)); then
        printf 'refusing: found %s SoundWire peripherals, this profile is for exactly 2\n' \
            "${amps}" >&2
        printf 'if this machine really has a different amp count, the UCM speaker\n' >&2
        printf 'profile includes must be revisited before installing.\n' >&2
        exit 1
    fi
    note "soundwire peripherals: ${amps}"

    # The two-speaker UCM profile this config depends on must exist upstream.
    local required="${ucm_root}/codecs/wsa884x/two-speakers/init.conf"
    if [[ ! -f "${required}" ]]; then
        printf 'refusing: missing %s\n' "${required}" >&2
        exit 1
    fi
    note "upstream two-speakers profile present"
}

backup() {
    local path=$1
    [[ -e "${path}" ]] || return 0
    run cp -a -- "${path}" "${path}.bak-${stamp}"
    note "backed up $(basename "${path}") -> $(basename "${path}").bak-${stamp}"
}

# ---------------------------------------------------------------------------
# x1e80100.conf is package-owned by alsa-ucm-conf and must gain an SP11 branch.
# Rather than shipping a full copy (which would go stale against upstream), the
# hunk is injected idempotently, and a dpkg diversion keeps upgrades from
# clobbering the result.
# ---------------------------------------------------------------------------
sp11_branch() {
    cat <<'BRANCH'

If.SURFACEPro11in {
	Condition {
		Type RegexMatch
		String "${var:DMI_info}"
		Regex "Microsoft Corporation.*Surface.*Microsoft Surface Pro, 11th Edition"
	}
	True.Include.sp11.File "/Qualcomm/x1e80100/MICROSOFT-Surface-Pro-11in.conf"
}
BRANCH
}

setup_diversion() {
    if dpkg-divert --list "${card_conf}" 2>/dev/null | grep -q .; then
        note "diversion already present for x1e80100.conf"
        return 0
    fi
    note "adding dpkg diversion: x1e80100.conf -> x1e80100.conf.distrib"
    run dpkg-divert --add --rename --divert "${divert_conf}" "${card_conf}"
    # --rename moved the live file (already SP11-patched) aside; put a copy back
    # so UCM is never left without a card conf.
    if [[ -f "${divert_conf}" ]]; then
        run cp -a -- "${divert_conf}" "${card_conf}"
    fi
}

inject_branch() {
    if grep -q 'If.SURFACEPro11in' "${card_conf}" 2>/dev/null; then
        note "SP11 branch already present in x1e80100.conf"
        return 0
    fi
    if ! grep -q 'Define.DMI_info' "${card_conf}"; then
        printf 'refusing: no Define.DMI_info anchor in %s\n' "${card_conf}" >&2
        exit 1
    fi
    note "injecting SP11 branch into x1e80100.conf"
    if ((dry_run)); then
        printf 'DRY-RUN: inject SP11 branch after Define.DMI_info\n'
        return 0
    fi
    local tmp
    tmp=$(mktemp)
    awk -v branch="$(sp11_branch)" '
        { print }
        /Define.DMI_info/ && !done { print branch; done = 1 }
    ' "${card_conf}" >"${tmp}"
    cat -- "${tmp}" >"${card_conf}"
    rm -f -- "${tmp}"
}

install_ucm() {
    local src
    for src in "${deploy_root}"/ucm2/Qualcomm/x1e80100/*.conf; do
        [[ -f "${src}" ]] || continue
        local dst
        dst="${ucm_vendor_dir}/$(basename "${src}")"
        backup "${dst}"
        run install -m 0644 -o root -g root -- "${src}" "${dst}"
        note "installed $(basename "${src}")"
    done
}

install_pipewire() {
    local src="${deploy_root}/pipewire/99-sp11-speaker-eq.conf"
    [[ -f "${src}" ]] || { note "no PipeWire EQ in repo, skipping"; return 0; }
    run install -d -m 0755 -o "${target_user}" -g "${target_user}" -- "${pw_dir}"
    backup "${pw_conf}"
    run install -m 0644 -o "${target_user}" -g "${target_user}" -- "${src}" "${pw_conf}"
    note "installed PipeWire EQ for ${target_user}"
}

disable_pipewire_eq() {
    if [[ -f "${pw_conf}" ]]; then
        backup "${pw_conf}"
        run rm -f -- "${pw_conf}"
        note "removed external PipeWire EQ from the protected baseline"
    else
        note "external PipeWire EQ is already absent"
    fi
}

install_tplg() {
    local src="${deploy_root}/firmware/${topology_name}"
    [[ -f "${src}" ]] || { note "no topology in repo, skipping"; return 0; }
    local dst="${fw_dir}/${topology_name}"
    if [[ -f "${dst}" ]] && cmp -s -- "${src}" "${dst}"; then
        note "topology already matches repo copy"
        return 0
    fi
    backup "${dst}"
    run install -m 0644 -o root -g root -- "${src}" "${dst}"
    note "installed topology (reboot or module reload required)"
}

verify() {
    ((dry_run)) && return 0
    printf '\nVerification:\n'
    local verbs
    if verbs=$(alsaucm -c hw:0 list _verbs 2>&1); then
        note "UCM verbs: $(printf '%s' "${verbs}" | tr '\n' ' ')"
    else
        printf '  WARNING: alsaucm could not list verbs:\n%s\n' "${verbs}"
    fi
    sha256sum "${ucm_vendor_dir}/MICROSOFT-Surface-Pro-11in.conf" \
              "${ucm_vendor_dir}/SP11-HiFi.conf" \
              "${card_conf}" 2>/dev/null | sed 's/^/  /'
}

uninstall() {
    printf 'Reversing SP11 audio configuration:\n'
    run rm -f -- "${ucm_vendor_dir}/MICROSOFT-Surface-Pro-11in.conf"
    run rm -f -- "${ucm_vendor_dir}/SP11-HiFi.conf"
    note "removed SP11 UCM profile files"
    if dpkg-divert --list "${card_conf}" 2>/dev/null | grep -q .; then
        run rm -f -- "${card_conf}"
        run dpkg-divert --remove --rename "${card_conf}"
        note "removed diversion, restored packaged x1e80100.conf"
    fi
    run rm -f -- "${pw_conf}"
    note "removed PipeWire EQ"
    printf '\nRestart audio: systemctl --user restart pipewire pipewire-pulse wireplumber\n'
    printf 'Timestamped .bak-* files were left in place deliberately.\n'
}

if ((do_uninstall)); then
    uninstall
    exit 0
fi

printf 'SP11 audio configuration install (stamp %s)\n\nPreflight:\n' "${stamp}"
preflight
printf '\nInstalling:\n'
setup_diversion
inject_branch
install_ucm
if ((install_pipewire_eq)); then
    install_pipewire
else
    disable_pipewire_eq
fi
((install_topology)) && install_tplg
verify

printf '\nApply: systemctl --user restart pipewire pipewire-pulse wireplumber\n'
printf 'Reverse: sudo %s --uninstall\n' "${BASH_SOURCE[0]}"
