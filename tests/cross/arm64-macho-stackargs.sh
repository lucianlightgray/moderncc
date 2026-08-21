#!/bin/sh
set -e

MCC=$1
REF=$2
SRCDIR=$3
WORK=$4
[ -n "$MCC" ] && [ -n "$REF" ] && [ -n "$WORK" ] || {
	echo "usage: arm64-macho-stackargs.sh <mcc> <ref-cc> <srcdir> <workdir>" >&2
	exit 2
}

[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
command -v "$REF" >/dev/null 2>&1 || { echo "SKIP: no reference cc $REF"; exit 77; }
"$MCC" -v 2>&1 | head -1 | grep -qF '(AArch64 Darwin)' || {
	echo "SKIP: mcc does not target AArch64 Darwin"
	exit 77
}
[ "$(uname -s)" = "Darwin" ] && [ "$(uname -m)" = "arm64" ] || {
	echo "SKIP: the linked image must run on a native arm64 Darwin host"
	exit 77
}

rm -rf "$WORK"
mkdir -p "$WORK"

cat >"$WORK/callee.c" <<'EOF'
long mix(long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8,
				 char c, short s, int i, char c2, long l) {
	return (long)c + (long)s * 10 + (long)i * 100 + (long)c2 * 1000 + l * 10000;
}
long ints(long a1, long a2, long a3, long a4, long a5, long a6, long a7, long a8,
					int i, int j, int k, int m, int n) {
	return i + j * 10 + k * 100 + m * 1000 + (long)n * 10000;
}
double flts(double d1, double d2, double d3, double d4, double d5, double d6,
						double d7, double d8, float g, double h, float k) {
	return g * 1.0 + h * 10.0 + k * 100.0;
}
long shorts(long a1, long a2, long a3, long a4, long a5, long a6, long a7,
						long a8, short s1, short s2, short s3, short s4) {
	return s1 + s2 * 10 + s3 * 100 + (long)s4 * 1000;
}
long chars(long a1, long a2, long a3, long a4, long a5, long a6, long a7,
					 long a8, char c1, char c2, char c3, char c4, int t) {
	return (long)c1 + (long)c2 * 10 + (long)c3 * 100 + (long)c4 * 1000 +
				 (long)t * 10000;
}
EOF

cat >"$WORK/caller.c" <<'EOF'
long mix(long, long, long, long, long, long, long, long, char, short, int, char, long);
long ints(long, long, long, long, long, long, long, long, int, int, int, int, int);
double flts(double, double, double, double, double, double, double, double, float, double, float);
long shorts(long, long, long, long, long, long, long, long, short, short, short, short);
long chars(long, long, long, long, long, long, long, long, char, char, char, char, int);

#ifndef DELTA
#define DELTA 0
#endif

int main(void) {
	int bad = 0;

	long r1 = mix(1, 2, 3, 4, 5, 6, 7, 8, (char)3, (short)4, 5, (char)6, 7L);
	bad += (r1 != 76543 + DELTA);

	long r2 = ints(1, 2, 3, 4, 5, 6, 7, 8, 9, 8, 7, 6, 5);
	bad += (r2 != 56789 + DELTA);

	double r3 = flts(1, 2, 3, 4, 5, 6, 7, 8, 2.0f, 3.0, 4.0f);
	bad += (r3 != 432.0 + DELTA);

	long r4 = shorts(1, 2, 3, 4, 5, 6, 7, 8, (short)2, (short)3, (short)4, (short)5);
	bad += (r4 != 5432 + DELTA);

	long r5 = chars(1, 2, 3, 4, 5, 6, 7, 8, (char)2, (char)3, (char)4, (char)5, 6);
	bad += (r5 != 65432 + DELTA);

	return bad;
}
EOF

DELTA=0
[ -n "$STACKABI_MUTATE" ] && DELTA=1

"$REF" -c "$WORK/callee.c" -o "$WORK/callee.o" 2>"$WORK/refc.err" || {
	echo "SKIP: reference cc cannot compile the Mach-O callee"
	sed 's/^/  /' "$WORK/refc.err" | head -3
	exit 77
}

"$MCC" -DDELTA=$DELTA "$WORK/caller.c" "$WORK/callee.o" -o "$WORK/out" 2>"$WORK/link.err" || {
	echo "FAIL: mcc failed to compile+link the caller against a $REF callee:"
	sed 's/^/  /' "$WORK/link.err" | head -5
	exit 1
}

if "$WORK/out"; then
	rc=0
else
	rc=$?
fi

if [ -n "$STACKABI_MUTATE" ]; then
	if [ "$rc" -eq 0 ]; then
		echo "FAIL (known-positive): the shifted expectation still passed --"
		echo "  the assertion does not observe the stack-argument values, so a"
		echo "  packing regression would go undetected"
		exit 1
	fi
	echo "PASS: known-positive -- a one-off expectation shift is caught"
	exit 0
fi

if [ "$rc" -ne 0 ]; then
	echo "FAIL: an mcc caller passing sub-8-byte scalars on the stack to a $REF"
	echo "  callee read the wrong values (Apple packs named stack args to natural"
	echo "  size/alignment; mcc must not round each to an 8-byte slot). exit=$rc"
	exit 1
fi

echo "PASS: mcc<->$REF cross-module packed stack arguments agree"
exit 0
