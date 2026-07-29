#!/bin/bash
#
# Refuse to deploy a kernel module built from an object older than its source.
# This catches the split-output-tree mistake that deployed a stale WSA884x
# object during the 2026-07-29 audio-vi follow-up.

set -euo pipefail

if [[ $# -lt 3 || $# -gt 5 ]]; then
	echo "usage: $0 SOURCE OBJECT MODULE [EXPECTED_RELEASE] [REQUIRED_MARKER]" >&2
	exit 2
fi

source_file=$1
object_file=$2
module_file=$3
expected_release=${4:-}
required_marker=${5:-}

for path in "$source_file" "$object_file" "$module_file"; do
	if [[ ! -f "$path" ]]; then
		echo "missing required build artifact: $path" >&2
		exit 1
	fi
done

if [[ ! "$object_file" -nt "$source_file" ]]; then
	echo "stale object: $object_file is not newer than $source_file" >&2
	exit 1
fi

if [[ ! "$module_file" -nt "$object_file" ]]; then
	echo "stale module: $module_file is not newer than $object_file" >&2
	exit 1
fi

if [[ -n "$expected_release" ]]; then
	actual_release=$(modinfo -F vermagic "$module_file" | cut -d' ' -f1)
	if [[ "$actual_release" != "$expected_release" ]]; then
		echo "vermagic release mismatch: expected $expected_release, got $actual_release" >&2
		exit 1
	fi
fi

if [[ -n "$required_marker" ]] &&
   ! grep -aFq -- "$required_marker" "$module_file"; then
	echo "required marker is absent from module: $required_marker" >&2
	exit 1
fi

printf 'source_mtime=%s\n' "$(stat -c %y "$source_file")"
printf 'object_mtime=%s\n' "$(stat -c %y "$object_file")"
printf 'module_mtime=%s\n' "$(stat -c %y "$module_file")"
printf 'module_sha256=%s\n' "$(sha256sum "$module_file" | cut -d' ' -f1)"
printf 'module_vermagic=%s\n' "$(modinfo -F vermagic "$module_file")"
printf 'module_signer=%s\n' "$(modinfo -F signer "$module_file")"
