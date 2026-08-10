#!/bin/sh
set -e

MCC=$1
BASE=$2
WORK=$3
SRC=$4
EXPECT=$5
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$WORK" ] && [ -n "$SRC" ] || {
	echo "usage: run-bindlocal.sh <mcc> <mccbase> <work> <srcdir> [known-positive]" >&2
	exit 2
}
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
[ -d "$SRC" ] || { echo "SKIP: no corpus at $SRC"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"
TAGS=$WORK/tags
: >"$TAGS"

UNSAFE=
[ -n "$EXPECT" ] && UNSAFE=1

rc=0
n=0
bound=0
booted=0

fail() {
	echo "FAIL [$1] $2"
	echo "$1" >>"$TAGS"
	rc=1
}

run3() {
	env $1 "$WORK/$2" >"$WORK/$2.$3.out" 2>"$WORK/$2.$3.err"
	echo "exit=$?" >>"$WORK/$2.$3.out"
}

for src in "$SRC"/*.c; do
	[ -f "$src" ] || continue
	case "$src" in *.aux.c) continue ;; esac
	name=$(basename "$src" .c)
	n=$((n + 1))
	aux=
	[ -f "$SRC/$name.aux.c" ] && aux="$SRC/$name.aux.c"

	if ! env MCC_JIT_BAKE_WHY=1 MCC_JIT_BAKE_UNSAFE_LOCALS=$UNSAFE \
		"$MCC" -B"$BASE" -O2 --embed-jit "$src" $aux -o "$WORK/$name" \
		>"$WORK/$name.why" 2>&1; then
		echo "FAIL $name: --embed-jit compile failed"
		sed 's/^/  /' "$WORK/$name.why"
		rc=1
		continue
	fi

	if grep -q 'admitted=1' "$WORK/$name.why"; then
		if grep 'admitted=1' "$WORK/$name.why" | grep -q 'static=0'; then
			fail bound-not-static "$name: a callee without VT_STATIC was admitted"
			echo "  Only a STB_LOCAL definition in this output's own .text is"
			echo "  provably the same entity the AOT code calls. A non-static"
			echo "  callee can be preempted or supplied by another object, and"
			echo "  freezing its link-time address into the blob is not sound."
			grep 'admitted=1' "$WORK/$name.why" | grep 'static=0' | sed 's/^/    /'
		fi
		if grep 'admitted=1' "$WORK/$name.why" | grep -q 'defined=0'; then
			fail bound-undefined "$name: a callee with no emitted body was admitted"
			echo "  An inline function that mcc never emitted out of line has no"
			echo "  address to bind, so admission can only fall back to resolving"
			echo "  the name at runtime -- which is the defect this guard exists"
			echo "  to prevent."
			grep 'admitted=1' "$WORK/$name.why" | grep 'defined=0' | sed 's/^/    /'
		fi
		if grep 'admitted=1' "$WORK/$name.why" | grep -q 'weak=1'; then
			fail bound-weak "$name: a weak callee was admitted"
			echo "  A weak definition may be replaced at link time by a strong one,"
			echo "  so its link-time address is not the address the program ends up"
			echo "  calling."
			grep 'admitted=1' "$WORK/$name.why" | grep 'weak=1' | sed 's/^/    /'
		fi
	fi

	set +e
	run3 "MCC_JIT=0" "$name" jit0
	run3 "MCC_JIT=1" "$name" jit1
	run3 "MCC_JIT=1 MCC_JIT_KGC=0" "$name" kgc0
	nb=$(MCC_JIT=1 MCC_JIT_VERBOSE=1 "$WORK/$name" 2>&1 >/dev/null |
		grep -c 'mccjit-bind\[')
	nk=$(MCC_JIT=1 MCC_JIT_VERBOSE=1 "$WORK/$name" 2>&1 >/dev/null |
		grep -c 'mccjit-boot\[')
	set -e
	bound=$((bound + nb))
	booted=$((booted + nk))

	if ! cmp -s "$WORK/$name.jit0.out" "$WORK/$name.kgc0.out"; then
		fail kgc0-differs "$name: MCC_JIT_KGC=0 disagrees with MCC_JIT=0"
		echo "  MCC_JIT_KGC=0 installs the recompiled variant behind a bare"
		echo "  trampoline with no differential check, so this arm is the one that"
		echo "  sees what the variant actually computes. A callee that the variant"
		echo "  resolved to a different entity than the AOT code calls shows up"
		echo "  here and nowhere else."
		echo "  jit0: $(tr '\n' '|' <"$WORK/$name.jit0.out")"
		echo "  kgc0: $(tr '\n' '|' <"$WORK/$name.kgc0.out")"
	fi
	if ! cmp -s "$WORK/$name.jit0.out" "$WORK/$name.jit1.out"; then
		fail jit1-differs "$name: MCC_JIT=1 disagrees with MCC_JIT=0"
		echo "  jit0: $(tr '\n' '|' <"$WORK/$name.jit0.out")"
		echo "  jit1: $(tr '\n' '|' <"$WORK/$name.jit1.out")"
	fi
	if [ $rc = 0 ]; then
		echo "PASS $name: jit0 == jit1 == kgc0 ($nb local bind(s), $nk boot(s))"
	fi
done

[ "$n" -gt 0 ] || { echo "FAIL: no programs found in $SRC"; exit 1; }

if [ "$booted" -lt 1 ]; then
	fail no-boot "the engine never booted in any of the $n programs, so every"
	echo "  comparison above ran the AOT path against itself"
fi
if [ "$bound" -lt 1 ]; then
	fail no-bind "not one static callee was address-bound across $n programs --"
	echo "  the widening admits a local callee only by carrying its relocated"
	echo "  address in the blob, so zero binds means either nothing was admitted"
	echo "  or admission stopped going through the bind table. Narrowing the"
	echo "  whitelist until the route stops firing is the obvious wrong fix and"
	echo "  must fail here, not pass quietly."
fi

WANT="kgc0-differs no-bind bound-not-static bound-undefined bound-weak"

if [ -z "$EXPECT" ]; then
	if [ $rc = 0 ]; then
		echo "PASS: over $n programs, MCC_JIT=1 and MCC_JIT_KGC=0 both agree with"
		echo "  MCC_JIT=0, $bound static callee(s) were bound by address across"
		echo "  $booted engine boot(s), and no callee outside the whitelist"
		echo "  (STB_LOCAL, non-weak, with an emitted body in this output) was"
		echo "  admitted"
	fi
	exit $rc
fi

if [ $rc = 0 ]; then
	echo "FAIL known-positive: the corpus passed every check with"
	echo "  MCC_JIT_BAKE_UNSAFE_LOCALS=1, which admits a local callee without"
	echo "  binding its address and leaves the runtime to resolve the name."
	echo "  A cell that cannot fail is worse than no cell."
	exit 1
fi
missing=
for t in $WANT; do
	grep -qx "$t" "$TAGS" || missing="$missing $t"
done
if [ -n "$missing" ]; then
	echo "FAIL known-positive: these checks never fired:$missing"
	echo "  each one is therefore unproven and may be inert in the real cell"
	exit 1
fi
echo "PASS known-positive: all 5 checks fired under MCC_JIT_BAKE_UNSAFE_LOCALS=1"
exit 0
