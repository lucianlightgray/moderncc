#!/bin/sh
# T-lin-10388: detect host-local provisioning LOSS instead of degrading to a stub.
#
# For each resource in tests/provisioning.txt:
#   present now                -> auto-stamp it (record "this box has/had it"), OK
#   absent now  + stamped      -> FAIL: it was provisioned here and is now MISSING
#   absent now  + not stamped  -> skip: this box genuinely never had it
# Stamps live host-local + untracked (default vendor/.provisioned/), so they persist
# across runs on a box but are never committed -- the record is per-box, like the
# resources themselves. A named FAIL replaces the silent larger-skip-count that let
# a loss read as "never had it" and get re-recorded as a permanent fleet property.
#
# Usage:
#   provisioning-detector.sh <repo-root> [stamp-dir]
#   provisioning-detector.sh --selftest            # known-positive: prove it FAILs

set -e

# Core check. Args: ROOT MANIFEST STAMPDIR. Echoes findings; returns 1 if any loss.
check_provisioning() {
	_root=$1; _manifest=$2; _stampdir=$3
	mkdir -p "$_stampdir"
	_present=0; _never=0; _lost=0; _fail=0
	# read without a trailing-newline requirement; skip blanks + comments
	while IFS='|' read -r _name _path _desc || [ -n "$_name" ]; do
		case "$_name" in ''|\#*) continue ;; esac
		_name=$(printf '%s' "$_name" | tr -d ' ')
		_stamp="$_stampdir/$_name"
		if [ -e "$_root/$_path" ]; then
			[ -e "$_stamp" ] || : > "$_stamp"
			_present=$((_present + 1))
		elif [ -e "$_stamp" ]; then
			echo "FAIL provisioning: '$_name' ($_path) was provisioned on this box but is now MISSING -- ${_desc# }" >&2
			_lost=$((_lost + 1)); _fail=1
		else
			_never=$((_never + 1))
		fi
	done < "$_manifest"
	# Floor: a manifest with zero resource rows asserts nothing, so refuse it rather
	# than pass vacuously (the exact can't-fail-gate shape this cell exists to kill).
	if [ $((_present + _never + _lost)) -eq 0 ]; then
		echo "FAIL provisioning: manifest $_manifest lists no resources, so the detector asserts nothing" >&2
		return 1
	fi
	echo "provisioning: $_present present (stamped), $_never never provisioned here, $_lost lost"
	return $_fail
}

if [ "$1" = "--selftest" ]; then
	# Known-positive: a stamped-but-missing resource MUST make the detector go red.
	tmp=$(mktemp -d)
	trap 'rm -rf "$tmp"' EXIT
	mkdir -p "$tmp/stamps"
	: > "$tmp/stamps/ghost"                                  # stamp a resource...
	printf 'ghost|vendor/.no-such-selftest-path|selftest ghost\n' > "$tmp/manifest.txt"
	if check_provisioning "$tmp" "$tmp/manifest.txt" "$tmp/stamps" >/dev/null 2>&1; then
		echo "FAIL provisioning-selftest: a stamped-but-missing resource did NOT fail the detector" >&2
		exit 1
	fi
	# and a resource that is present-and-stamped must NOT fail
	mkdir -p "$tmp/vendor/here"
	printf 'here|vendor/here|selftest present\n' > "$tmp/manifest2.txt"
	if ! check_provisioning "$tmp" "$tmp/manifest2.txt" "$tmp/stamps" >/dev/null 2>&1; then
		echo "FAIL provisioning-selftest: a present resource wrongly failed the detector" >&2
		exit 1
	fi
	echo "provisioning-selftest: OK -- stamped-but-missing fails, present passes"
	exit 0
fi

ROOT=$1
STAMPDIR=${2:-"$ROOT/vendor/.provisioned"}
[ -n "$ROOT" ] || { echo "usage: provisioning-detector.sh <repo-root> [stamp-dir]" >&2; exit 2; }
MANIFEST="$ROOT/tests/provisioning.txt"
[ -f "$MANIFEST" ] || { echo "FAIL provisioning: manifest $MANIFEST not found" >&2; exit 1; }

if check_provisioning "$ROOT" "$MANIFEST" "$STAMPDIR"; then
	echo "provisioning: OK -- no previously-provisioned host-local resource has vanished"
	exit 0
else
	echo "provisioning: a host-local resource that WAS present on this box is gone; re-provision it or, if the loss is intentional, remove its stamp under $STAMPDIR" >&2
	exit 1
fi
