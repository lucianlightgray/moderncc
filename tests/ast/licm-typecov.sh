#!/bin/sh
# ast/licm-typecov -- loop-invariant motion is type-complete (float/double).
#
# ast_ltemp_scan/ast_ltemp_materialize (TREE_LOOP_IM, -O4) gated the hoisted
# invariant expression on ast_ident_intt(et), so a loop-invariant float/double
# computation was never hoisted out of a loop. Both gates now use ast_cse_scalar
# (int/float/double/ptr, <=8B, matching the shared width-8 ltemp slot); the temp
# is typed via ast_set_type(tref, et, er), so a double invariant is stored/loaded
# as a double. Hoisting a loop-invariant is value-safe for any type. See
# DETAILS.md#t-lin-10441.
#
# The subject hoists a reused invariant (a*b) from a double loop and an int loop
# -- 2 ltemp hoists at -O4. Anti-vacuity: integer-only LICM hoists only 1; with
# the float extension it hoists 2. Correctness: -O4 output must equal -O0.
set -e

MCC=$1
SRCDIR=$2
WORK=$3

if [ -z "$MCC" ] || [ -z "$SRCDIR" ] || [ -z "$WORK" ]; then
	echo "usage: licm-typecov.sh <mcc> <srcdir> <workdir>" >&2
	exit 2
fi

mkdir -p "$WORK"
src=$SRCDIR/tests/misc/licm_typecov_subject.c

lt=$(MCC_STATS=strategy "$MCC" -O4 -c "$src" -o "$WORK/l.o" 2>&1 |
	sed -n 's/.*[^a-z]ltemp=\([0-9][0-9]*\).*/\1/p' | tail -1)
echo "ltemp hoists (double+int+long-double+__int128 loops) = $lt"

if [ -z "$lt" ] || [ "$lt" -lt 4 ]; then
	echo "FAIL: type-complete LICM did not hoist every-width loop invariant (ltemp=$lt, want >=4)"
	exit 1
fi

"$MCC" -O0 "$src" -o "$WORK/r0" >/dev/null 2>&1
"$MCC" -O4 "$src" -o "$WORK/r4" >/dev/null 2>&1
r0=$("$WORK/r0")
r4=$("$WORK/r4")
if [ "$r0" != "$r4" ]; then
	echo "FAIL: -O4 output differs from -O0 ('$r4' vs '$r0') -- LICM hoisted a non-invariant"
	exit 1
fi

echo "ast/licm-typecov OK: LICM type-complete (ltemp=$lt over double+int+long-double+__int128), result-invariant ($r4)"
