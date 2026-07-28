#!/usr/bin/env bash
# Capture the earliest useful state from the protected-audio boot. This script
# never opens a PCM stream and never changes an ALSA control.

set -u

expected_kernel=7.1.5-sp11-audio-v3
timestamp=$(date -u +%Y%m%dT%H%M%SZ)
output_root=/var/log/sp11-audio-first-boot
output_dir="${output_root}/${timestamp}"

install -d -m 0755 -- "${output_dir}"

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

capture uname.txt uname -a
capture cmdline.txt cat /proc/cmdline
capture boot-journal.txt journalctl -b --no-pager
capture kernel-journal.txt journalctl -b -k --no-pager
capture audio-kernel-journal.txt bash -c \
	"journalctl -b -k --no-pager | rg -i 'audio|asoc|alsa|apr|gpr|q6|soundwire|wsa884|x1e80100' || true"
capture modules.txt bash -c \
	"lsmod | rg '^(snd|soundwire|q6|wsa|lpass|apr|gpr)' || true"
capture module-identities.txt bash -c \
	"printf 'loaded snd_q6apm srcversion: '; cat /sys/module/snd_q6apm/srcversion; \
	printf 'installed snd-q6apm srcversion: '; modinfo -F srcversion snd-q6apm; \
	printf 'installed snd-q6apm signer: '; modinfo -F signer snd-q6apm; \
	printf 'installed snd-q6apm file: '; modinfo -F filename snd-q6apm; \
	module_path=\$(modinfo -F filename snd-q6apm); sha256sum \"\${module_path}\""
capture alsa-cards.txt cat /proc/asound/cards
capture alsa-pcm.txt cat /proc/asound/pcm
capture aplay-devices.txt aplay -l
capture alsa-control-list.txt amixer -D hw:0 controls
capture alsa-control-contents.txt amixer -D hw:0 contents
capture soundwire-tree.txt find /sys/bus/soundwire/devices \
	-maxdepth 3 -mindepth 1 -printf '%y %p -> %l\n'

{
	for status in /sys/bus/soundwire/devices/*/status; do
		[[ -r "${status}" ]] || continue
		printf '%s = %s\n' "${status}" "$(cat "${status}")"
	done
} >"${output_dir}/soundwire-status.txt" 2>&1

capture runtime-hashes.txt sha256sum \
	/lib/firmware/qcom/x1e80100/X1E80100-Microsoft-Surface-Pro-11-tplg.bin \
	/usr/share/alsa/ucm2/Qualcomm/x1e80100/x1e80100.conf \
	/usr/share/alsa/ucm2/Qualcomm/x1e80100/MICROSOFT-Surface-Pro-11in.conf \
	/usr/share/alsa/ucm2/Qualcomm/x1e80100/SP11-HiFi.conf

kernel=$(uname -r)
{
	printf 'captured_utc=%s\n' "${timestamp}"
	printf 'expected_kernel=%s\n' "${expected_kernel}"
	printf 'observed_kernel=%s\n' "${kernel}"
	printf 'candidate_boot=%s\n' "$([[ "${kernel}" == "${expected_kernel}" ]] && printf true || printf false)"
	printf 'opens_pcm_stream=false\n'
	printf 'changes_alsa_controls=false\n'
} >"${output_dir}/manifest.txt"

ln -sfn -- "${timestamp}" "${output_root}/latest"
