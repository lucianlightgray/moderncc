#!/bin/sh
set -e

MCC=$1
BASE=$2
SRC=$3
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$SRC" ] || {
	echo "usage: run-kgcroute.sh <mcc> <mccbase> <srcdir>" >&2
	exit 2
}

rc=0
n=0
routed=0
for src in "$SRC"/*.c; do
	[ -f "$src" ] || continue
	name=$(basename "$src" .c)
	n=$((n + 1))

	ref=$(MCC_JIT=0 "$MCC" -B"$BASE" -O2 -run "$src" 2>/dev/null) || {
		echo "FAIL $name: reference run (MCC_JIT=0) failed"
		rc=1
		continue
	}
	off=$(MCC_JIT=1 MCC_JIT_KGC=0 "$MCC" -B"$BASE" -O2 -run "$src" 2>/dev/null) || {
		echo "FAIL $name: MCC_JIT_KGC=0 run failed"
		rc=1
		continue
	}
	on=$(MCC_JIT=1 MCC_JIT_KGC=1 "$MCC" -B"$BASE" -O2 -run "$src" 2>/dev/null) || {
		echo "FAIL $name: MCC_JIT_KGC=1 run failed"
		rc=1
		continue
	}
	nk=$(MCC_JIT=1 MCC_JIT_KGC=1 MCC_JIT_VERBOSE=1 "$MCC" -B"$BASE" -O2 -run "$src" 2>&1 |
		grep -c 'route=kgc.*swapped' || true)
	routed=$((routed + nk))

	if [ "$ref" != "$off" ]; then
		echo "FAIL $name: MCC_JIT_KGC=0 output differs from MCC_JIT=0"
		echo "  jit0: $ref"
		echo "  kgc0: $off"
		rc=1
		continue
	fi
	if [ "$ref" != "$on" ]; then
		echo "FAIL $name: the known-good-constant route changed the answer"
		echo "  jit0: $ref"
		echo "  kgc1: $on"
		echo "  MCC_JIT_KGC=0 agrees, so the divergence is the KGC stub itself:"
		echo "  the 64-bit value it hands back is not the one the AOT callee"
		echo "  would have left in the return register. A sub-64-bit GP return"
		echo "  occupies the low half only and the high half is zero; the caller"
		echo "  re-extends signed results itself and assumes zero for unsigned"
		echo "  ones (src/mccjit_embed.c, mccjit_invoke)."
		rc=1
		continue
	fi
	echo "PASS $name: jit0 == kgc0 == kgc1 ($nk kgc-routed swap(s))"
done

[ "$n" -gt 0 ] || { echo "FAIL: no programs found in $SRC"; exit 1; }
if [ "$routed" -lt 1 ]; then
	echo "FAIL: no program in $SRC took the kgc route across $n programs -- this"
	echo "  cell compares a KGC route that was never entered against itself, so"
	echo "  it cannot see a KGC defect. Fix the corpus or the admission rule"
	echo "  before trusting a green result here."
	rc=1
fi
exit $rc
