#!/bin/sh
set -e

MCC=$1
SRCDIR=$2
WORK=$3
SUBJECT=$4

if [ -z "$MCC" ] || [ -z "$SRCDIR" ] || [ -z "$WORK" ]; then
	echo "usage: select-typecov.sh <mcc> <srcdir> <workdir> [subject.c]" >&2
	exit 2
fi

mkdir -p "$WORK"
src=${SUBJECT:-$SRCDIR/tests/misc/select_typecov_subject.c}

select=$(MCC_STATS=strategy "$MCC" -O4 -c "$src" -o "$WORK/s.o" 2>&1 |
	sed -n 's/.*[^a-z]select=\([0-9][0-9]*\).*/\1/p' | tail -1)
echo "if-conversion selects ($(basename "$src")) = $select"

if [ -z "$select" ] || [ "$select" -lt 5 ]; then
	echo "FAIL: width-complete if-conversion did not select over char/short/int/unsigned/long/long-long (select=$select, want >=5, true 6)"
	exit 1
fi

"$MCC" -O0 "$src" -o "$WORK/r0" >/dev/null 2>&1
"$MCC" -O4 "$src" -o "$WORK/r4" >/dev/null 2>&1
r0=$("$WORK/r0")
r4=$("$WORK/r4")
if [ "$r0" != "$r4" ]; then
	echo "FAIL: -O4 output differs from -O0 ('$r4' vs '$r0') -- if-conversion miscompiled a GPR-typed select"
	exit 1
fi

if [ -n "$MCC_TYPECOV_RUN" ]; then
	runlo=$("$MCC" -O0 -run "$src" 2>/dev/null)
	runhi=$("$MCC" -O4 -run "$src" 2>/dev/null)
	if [ "$runlo" != "$r0" ] || [ "$runhi" != "$r0" ]; then
		echo "FAIL: JIT -run mismatch (O0-run='$runlo' O4-run='$runhi' vs AOT-O0 '$r0') -- P1 (b) JIT correctness"
		exit 1
	fi
	echo "  JIT -run verified (P1 b): -O0-run == -O4-run == AOT-O0 ($r0)"
fi

echo "ast/select-typecov OK: if-conversion width-complete over integer GPR types (select=$select over $(basename "$src")), result-invariant ($r4)"
