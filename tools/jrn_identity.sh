#!/bin/sh
# Operation-journal inertness check.
#
# The journal is an oracle, not a code path: turning it on must never move a
# byte of the object it is observing. This compiles every corpus source twice
# with the same compiler -- once plain, once under MCC_JOURNAL=1 -- and fails on
# any difference. It is the runtime half of the inertness property; the
# compile-time half (a build with MCC_JOURNAL_HOOKS against one built with
# -DMCC_JOURNAL_HOOKS=0) needs two build trees and is checked by hand.
#
# Usage: jrn_identity.sh <mcc> <corpus-dir> <extra-source> <tmpdir> [opt]
# Exits 77 (ctest SKIP) when the build has no MCC_JOURNAL_HOOKS.
set -e
MCC=$1
CORPUS=$2
EXTRA=$3
TMP=$4
OPT=${5:--O1}
mkdir -p "$TMP"

printf 'int jrn_probe(int a, int b) { return a * b + 1; }\n' >"$TMP/probe.c"
probe=$(MCC_JOURNAL=1 "$MCC" -w "$OPT" -c -o "$TMP/probe.o" "$TMP/probe.c" 2>&1 >/dev/null |
	grep -c '^\[jrn-verify\]' || true)
if [ "$probe" -eq 0 ]; then
	echo "jrn_identity: no [jrn-verify] output — build has no MCC_JOURNAL_HOOKS; SKIP"
	exit 77
fi

n=0
bad=0
for f in $(find "$CORPUS" -name '*.c' | sort) "$EXTRA"; do
	"$MCC" -w "$OPT" -c -o "$TMP/a.o" "$f" 2>/dev/null || continue
	MCC_JOURNAL=1 MCC_JOURNAL_OUT="$TMP/j.log" \
		"$MCC" -w "$OPT" -c -o "$TMP/b.o" "$f" 2>/dev/null || continue
	n=$((n + 1))
	if ! cmp -s "$TMP/a.o" "$TMP/b.o"; then
		bad=$((bad + 1))
		echo "DIFF $f"
	fi
done
echo "jrn_identity: $OPT compared=$n differing=$bad"
[ "$n" -gt 0 ] || { echo "jrn_identity: compiled nothing"; exit 1; }
[ "$bad" -eq 0 ] || exit 1
