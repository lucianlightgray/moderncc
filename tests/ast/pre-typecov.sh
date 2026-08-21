#!/bin/sh
# ast/pre-typecov -- partial-redundancy elimination is type-complete (float/double).
#
# ast_pre_run (TREE_PRE, -O4) gated the hoisted expression on ast_ident_intt(et),
# so a partially-redundant float/double expression (computed on one branch arm
# and again after the merge) was never PRE'd. The gate now uses ast_cse_scalar
# (int/float/double/ptr, <=8B for the shared width-8 ltemp slot); the temp is
# typed via ast_set_type(tref,et,er). Hoisting a redundant computation is value-
# safe for any type. See DETAILS.md#t-lin-10441.
#
# Subject: a+b redundant across a branch, once double once int -> pre>=2 at -O4.
# Anti-vacuity: integer-only PRE fires 1; the float extension makes it 2.
# Correctness: -O4 output must equal -O0.
set -e
MCC=$1; SRCDIR=$2; WORK=$3; SUBJECT=$4
if [ -z "$MCC" ] || [ -z "$SRCDIR" ] || [ -z "$WORK" ]; then
	echo "usage: pre-typecov.sh <mcc> <srcdir> <workdir> [subject.c]" >&2; exit 2
fi
mkdir -p "$WORK"
src=${SUBJECT:-$SRCDIR/tests/misc/pre_typecov_subject.c}
pre=$(MCC_STATS=strategy "$MCC" -O4 -c "$src" -o "$WORK/p.o" 2>&1 |
	sed -n 's/.*[^a-z]pre=\([0-9][0-9]*\).*/\1/p' | tail -1)
echo "pre folds ($(basename "$src")) = $pre"
if [ -z "$pre" ] || [ "$pre" -lt 4 ]; then
	echo "FAIL: type-complete PRE did not fire on every-width redundancy (pre=$pre, want >=4)"; exit 1
fi
"$MCC" -O0 "$src" -o "$WORK/r0" >/dev/null 2>&1
"$MCC" -O4 "$src" -o "$WORK/r4" >/dev/null 2>&1
r0=$("$WORK/r0"); r4=$("$WORK/r4")
if [ "$r0" != "$r4" ]; then
	echo "FAIL: -O4 output differs from -O0 ('$r4' vs '$r0')"; exit 1
fi
echo "ast/pre-typecov OK: PRE type-complete (pre=$pre over $(basename "$src")), result-invariant ($r4)"
