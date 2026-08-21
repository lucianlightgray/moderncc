#!/bin/sh
# ast/cse-typecov -- common-subexpression elimination is type-complete.
#
# ast_cse_block/ast_cse_stmts gated both the store lval (ast_cprop_is_local)
# and the value expression (ast_ident_intt(et)) to integers, and ast_cse_regpure
# rejected float/pointer Ref leaves -- so a redundant double or pointer
# computation was never CSE'd. The foundation made ast_ident_common/etype
# float-capable; this pass adds ast_cse_scalar (int/float/double/ptr, all <=8B so
# the shared width-8 ltemp slot stays safe) + ast_cse_is_local, so a repeated
# float or pointer expression is reused. CSE reuses the first store's existing
# local (no new temp), so it is value-safe for any scalar. See DETAILS#t-lin-10441.
#
# The subject repeats double, float, pointer, int, ldouble, __int128 exprs -- 6 CSE
# folds. Anti-vacuity: an integer-only CSE would fold only 1 (icse); with the
# float type extension it folds 6. Correctness: -O2 output must equal -O0.
# (ldouble CSE works (reuses the existing local; no width-8 temp) via ast_cse_wide;
# pointer CSE now fires too via the pointee-ref relaxation.)
set -e

MCC=$1
SRCDIR=$2
WORK=$3
SUBJECT=$4

if [ -z "$MCC" ] || [ -z "$SRCDIR" ] || [ -z "$WORK" ]; then
	echo "usage: cse-typecov.sh <mcc> <srcdir> <workdir> [subject.c]" >&2
	exit 2
fi

mkdir -p "$WORK"
src=${SUBJECT:-$SRCDIR/tests/misc/cse_typecov_subject.c}

cse=$(MCC_STATS=strategy "$MCC" -O2 -c "$src" -o "$WORK/c.o" 2>&1 |
	sed -n 's/.*[^a-z]cse=\([0-9][0-9]*\).*/\1/p' | tail -1)
echo "cse folds ($(basename "$src")) = $cse"

if [ -z "$cse" ] || [ "$cse" -lt 6 ]; then
	echo "FAIL: type-complete CSE did not fire on the double+float+pointer+ldouble expressions (cse=$cse, want >=6)"
	exit 1
fi

"$MCC" -O0 "$src" -o "$WORK/r0" >/dev/null 2>&1
"$MCC" -O2 "$src" -o "$WORK/r2" >/dev/null 2>&1
r0=$("$WORK/r0")
r2=$("$WORK/r2")
if [ "$r0" != "$r2" ]; then
	echo "FAIL: -O2 output differs from -O0 ('$r2' vs '$r0') -- CSE reused a stale value"
	exit 1
fi

echo "ast/cse-typecov OK: CSE type-complete (cse=$cse over $(basename "$src")), result-invariant ($r2)"
