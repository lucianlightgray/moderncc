#!/bin/sh
# ast/sroa-typecov -- scalar-replacement-of-aggregates is type-complete.
#
# ast_sroa (TREE_SROA, -O4) splits a never-address-taken local struct into
# per-member scalars. It must handle a struct whose members are
# double/float/pointer/long long, not only int. The subject is DOMINATED by
# non-int-member structs (4 of 5), so an SROA that only decomposed int members
# would fire ~4 and miss the rest; the type-complete pass fires 20. The >=16
# floor is the anti-vacuity gate (well above the int-only ~4, at/below the true
# 20 with margin). The count is an AST-level property so it is arch-independent
# across the 64-bit native legs. Correctness: -O4 output must equal -O0, and
# (P1 req b) the JIT -run result must equal the AOT -O0 result.
set -e

MCC=$1
SRCDIR=$2
WORK=$3
SUBJECT=$4

if [ -z "$MCC" ] || [ -z "$SRCDIR" ] || [ -z "$WORK" ]; then
	echo "usage: sroa-typecov.sh <mcc> <srcdir> <workdir> [subject.c]" >&2
	exit 2
fi

mkdir -p "$WORK"
src=${SUBJECT:-$SRCDIR/tests/misc/sroa_typecov_subject.c}

sroa=$(MCC_STATS=strategy "$MCC" -O4 -c "$src" -o "$WORK/s.o" 2>&1 |
	sed -n 's/.*[^a-z]sroa=\([0-9][0-9]*\).*/\1/p' | tail -1)
echo "sroa splits ($(basename "$src")) = $sroa"

if [ -z "$sroa" ] || [ "$sroa" -lt 16 ]; then
	echo "FAIL: type-complete SROA did not split the double+float+pointer+long-long structs (sroa=$sroa, want >=16)"
	exit 1
fi

"$MCC" -O0 "$src" -o "$WORK/r0" >/dev/null 2>&1
"$MCC" -O4 "$src" -o "$WORK/r4" >/dev/null 2>&1
r0=$("$WORK/r0")
r4=$("$WORK/r4")
if [ "$r0" != "$r4" ]; then
	echo "FAIL: -O4 output differs from -O0 ('$r4' vs '$r0') -- SROA broke a member"
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

echo "ast/sroa-typecov OK: SROA type-complete (sroa=$sroa over $(basename "$src")), result-invariant ($r4)"
