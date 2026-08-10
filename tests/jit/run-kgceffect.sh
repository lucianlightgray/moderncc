#!/bin/sh
set -e

MCC=$1
BASE=$2
WORK=$3
SRC=$4
EXPECT=$5
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$WORK" ] && [ -n "$SRC" ] || {
	echo "usage: run-kgceffect.sh <mcc> <mccbase> <work> <srcdir> [known-positive]" >&2
	exit 2
}
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
[ -d "$SRC" ] || { echo "SKIP: no corpus at $SRC"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"
TAGS=$WORK/tags
: >"$TAGS"

rc=0
n=0
routed=0

fail() {
	echo "FAIL [$1] $2"
	echo "$1" >>"$TAGS"
	rc=1
}

run3() {
	env $1 "$WORK/$2" >"$WORK/$2.$3.out" 2>/dev/null
	echo "exit=$?" >>"$WORK/$2.$3.out"
}

for src in "$SRC"/*.c; do
	[ -f "$src" ] || continue
	name=$(basename "$src" .c)
	n=$((n + 1))

	if ! "$MCC" -B"$BASE" -O2 --embed-jit "$src" -o "$WORK/$name" >"$WORK/$name.cc" 2>&1; then
		echo "FAIL $name: --embed-jit compile failed"
		sed 's/^/  /' "$WORK/$name.cc"
		rc=1
		continue
	fi

	set +e
	run3 "MCC_JIT=0" "$name" jit0
	run3 "MCC_JIT=1" "$name" jit1
	run3 "MCC_JIT=1 MCC_JIT_KGC=0" "$name" kgc0
	nk=$(MCC_JIT=1 MCC_JIT_VERBOSE=1 "$WORK/$name" 2>&1 >/dev/null |
		grep -c 'route=kgc.*swapped')
	set -e
	routed=$((routed + nk))

	if ! cmp -s "$WORK/$name.jit0.out" "$WORK/$name.kgc0.out"; then
		fail kgc0-differs "$name: MCC_JIT_KGC=0 disagrees with MCC_JIT=0, so the"
		echo "  divergence is not the known-good-constant route at all -- triage"
		echo "  the recompiled variant itself before reading the next check."
		echo "  jit0: $(tr '\n' '|' <"$WORK/$name.jit0.out")"
		echo "  kgc0: $(tr '\n' '|' <"$WORK/$name.kgc0.out")"
		continue
	fi
	if ! cmp -s "$WORK/$name.jit0.out" "$WORK/$name.jit1.out"; then
		fail jit1-differs "$name: the KGC route changed an observable side effect"
		echo "  jit0: $(tr '\n' '|' <"$WORK/$name.jit0.out")"
		echo "  jit1: $(tr '\n' '|' <"$WORK/$name.jit1.out")"
		echo "  MCC_JIT_KGC=0 agrees with MCC_JIT=0, so the KGC stub is at fault."
		echo "  The route calls the variant AND the baseline to compare them, and"
		echo "  under memoize_ok it serves a repeated argument tuple from the"
		echo "  near-match correction cache. Both are sound only for a callee that"
		echo "  is a function of its arguments. Admission is mccjit_last_kgc_ok in"
		echo "  src/mccjit_embed.c over ast_fn_purity in src/mccast.c: a callee"
		echo "  that writes memory must classify AST_PURITY_IMPURE, and one that"
		echo "  reads mutable memory must not reach AST_PURITY_TIER0."
		continue
	fi
	echo "PASS $name: jit0 == kgc0 == jit1 ($nk kgc-routed swap(s))"
done

[ "$n" -gt 0 ] || { echo "FAIL: no programs found in $SRC"; exit 1; }

if [ "$routed" -lt 1 ]; then
	fail no-route "no program in $SRC took the kgc route across $n programs --"
	echo "  this cell would then compare a route that was never entered against"
	echo "  itself. Narrowing admission until the route stops firing is the"
	echo "  obvious wrong fix to the side-effect defect and must fail here."
fi

WANT="jit1-differs kgc0-differs no-route"

if [ -z "$EXPECT" ]; then
	if [ $rc = 0 ]; then
		echo "PASS: over $n programs with observable side effects, MCC_JIT=1 agrees"
		echo "  with MCC_JIT=0 and with MCC_JIT_KGC=0, and $routed callee(s) still"
		echo "  took the kgc route, so admission was narrowed to the effect-free"
		echo "  callees rather than switched off"
	fi
	exit $rc
fi

if [ $rc = 0 ]; then
	echo "FAIL known-positive: the deliberately-broken corpus $SRC passed every"
	echo "  check. A cell that cannot fail is worse than no cell."
	exit 1
fi
missing=
for t in $WANT; do
	grep -qx "$t" "$TAGS" || missing="$missing $t"
done
if [ -n "$missing" ]; then
	echo "FAIL known-positive: these checks never fired on the broken corpus:$missing"
	echo "  each one is therefore unproven and may be inert in the real cell"
	exit 1
fi
echo "PASS known-positive: all 3 checks fired on $SRC"
exit 0
