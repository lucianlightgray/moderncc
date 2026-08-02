#!/bin/sh
set -e

MCC=$1
BASE=$2
SRC=$3
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$SRC" ] || {
	echo "usage: run-parity.sh <mcc> <mccbase> <srcdir>" >&2
	exit 2
}

WIDE="MCC_JIT_PURITY_NOESCAPE=1 MCC_JIT_LAZY=1 MCC_JIT_SEARCH=1"
rc=0
n=0
installs=0
for src in "$SRC"/*.c; do
	[ -f "$src" ] || continue
	name=$(basename "$src" .c)
	n=$((n + 1))

	ref=$(MCC_JIT=0 "$MCC" -B"$BASE" -O2 -run "$src" 2>/dev/null) || {
		echo "FAIL $name: reference run (MCC_JIT=0) failed"
		rc=1
		continue
	}
	got=$(MCC_JIT=1 "$MCC" -B"$BASE" -O2 -run "$src" 2>/dev/null) || {
		echo "FAIL $name: MCC_JIT=1 run failed"
		rc=1
		continue
	}
	if [ "$ref" != "$got" ]; then
		echo "FAIL $name: MCC_JIT=1 output differs from MCC_JIT=0"
		echo "  jit0: $ref"
		echo "  jit1: $got"
		rc=1
		continue
	fi

	wide=$(env $WIDE MCC_JIT=1 "$MCC" -B"$BASE" -O2 -run "$src" 2>/dev/null) || {
		echo "FAIL $name: widened-admission run failed"
		rc=1
		continue
	}
	if [ "$ref" != "$wide" ]; then
		echo "FAIL $name: widened admission changed the output"
		echo "  jit0: $ref"
		echo "  wide: $wide"
		echo "  this is the MCC_JIT_NEARMATCH parity hazard; see TODO KGC section"
		rc=1
		continue
	fi

	nv=$(env $WIDE MCC_JIT=1 MCC_JIT_VERBOSE=1 "$MCC" -B"$BASE" -O2 -run "$src" 2>&1 |
		grep -c 'mccjit-lazy\[install\]' || true)
	installs=$((installs + nv))
	echo "PASS $name: jit0 == jit1 == widened ($nv variant(s) installed)"
done

[ "$n" -gt 0 ] || { echo "FAIL: no programs found in $SRC"; exit 1; }
if [ "$installs" -lt 1 ]; then
	echo "FAIL: the widened leg installed NO variants across $n programs -- this"
	echo "  cell is comparing a JIT that swapped nothing against no JIT at all."
	echo "  Admission narrowed, or MCC_JIT_LAZY stopped installing; fix before"
	echo "  trusting a green result here."
	rc=1
fi
exit $rc
