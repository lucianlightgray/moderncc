#!/bin/sh
set -e

MCC=$1
REF=$2
SRCDIR=$3
WORK=$4
[ -n "$MCC" ] && [ -n "$REF" ] && [ -n "$WORK" ] || {
	echo "usage: zero-array-fp-abi.sh <mcc> <ref-cc> <srcdir> <workdir>" >&2
	exit 2
}

[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
command -v "$REF" >/dev/null 2>&1 || { echo "SKIP: no reference cc $REF"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"

cat >"$WORK/callee.c" <<'EOF'
struct fp0 { double a, b; char pad[0]; };
double take2(struct fp0 s) { return s.a * 100.0 + s.b; }
struct fp0 make2(double a, double b) { struct fp0 r; r.a = a; r.b = b; return r; }
EOF

cat >"$WORK/caller.c" <<'EOF'
struct fp0 { double a, b; char pad[0]; };
double take2(struct fp0 s);
struct fp0 make2(double a, double b);

#ifndef DELTA
#define DELTA 0
#endif

int main(void) {
	int bad = 0;
	struct fp0 s;
	s.a = 1.5;
	s.b = 2.25;
	if (take2(s) != 152.25 + DELTA)
		bad++;
	struct fp0 r = make2(4.5, 6.75);
	if (r.a != 4.5 + DELTA || r.b != 6.75 + DELTA)
		bad++;
	return bad;
}
EOF

DELTA=0
[ -n "$ZEROARR_MUTATE" ] && DELTA=1

"$REF" -c "$WORK/callee.c" -o "$WORK/callee_ref.o" 2>"$WORK/refc.err" || {
	echo "SKIP: reference cc cannot compile the zero-length-array callee"
	sed 's/^/  /' "$WORK/refc.err" | head -3
	exit 77
}
"$REF" -c -DDELTA=$DELTA "$WORK/caller.c" -o "$WORK/caller_ref.o"

"$MCC" -c "$WORK/callee.c" -o "$WORK/callee_mcc.o" 2>"$WORK/mccc.err" || {
	echo "FAIL: mcc could not compile the zero-length-array callee:"
	sed 's/^/  /' "$WORK/mccc.err" | head -5
	exit 1
}
"$MCC" -c -DDELTA=$DELTA "$WORK/caller.c" -o "$WORK/caller_mcc.o" 2>>"$WORK/mccc.err" || {
	echo "FAIL: mcc could not compile the caller:"
	sed 's/^/  /' "$WORK/mccc.err" | head -5
	exit 1
}

# Direction 1: mcc caller <-> ref callee.  Direction 2: ref caller <-> mcc callee.
"$REF" "$WORK/caller_mcc.o" "$WORK/callee_ref.o" -o "$WORK/d1"
"$REF" "$WORK/caller_ref.o" "$WORK/callee_mcc.o" -o "$WORK/d2"

if "$WORK/d1"; then rc1=0; else rc1=$?; fi
if "$WORK/d2"; then rc2=0; else rc2=$?; fi

if [ -n "$ZEROARR_MUTATE" ]; then
	if [ "$rc1" -eq 0 ] || [ "$rc2" -eq 0 ]; then
		echo "FAIL (known-positive): a shifted expectation still passed --"
		echo "  the assertion does not observe the returned/passed FP values, so a"
		echo "  zero-length-array mis-classification would go undetected (d1=$rc1 d2=$rc2)"
		exit 1
	fi
	echo "PASS: known-positive -- a one-off expectation shift is caught in both directions"
	exit 0
fi

if [ "$rc1" -ne 0 ] || [ "$rc2" -ne 0 ]; then
	echo "FAIL: cross-module ABI mismatch on a struct with a trailing zero-length"
	echo "  array (struct{double a,b; char pad[0];}). The phantom [0] member must be"
	echo "  skipped in register classification so the aggregate stays FP (SSE/HFA),"
	echo "  matching $REF. mcc-caller<->ref-callee=$rc1 ref-caller<->mcc-callee=$rc2"
	exit 1
fi

echo "PASS: mcc<->$REF agree on the zero-length-array FP aggregate ABI"
exit 0
