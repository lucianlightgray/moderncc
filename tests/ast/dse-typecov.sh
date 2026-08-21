#!/bin/sh
# ast/dse-typecov -- dead-store elimination is type-complete AND width-safe.
#
# ast_dse_run (TREE_DSE, -O4 / -ftree-dse) used to gate stores through
# ast_cprop_is_local -> ast_ident_intt, so only BOOL/BYTE/SHORT/INT/LLONG
# locals were candidates: a dead double or pointer store was never eliminated.
# ast_dse_local now admits every scalar local (float/double/ldouble/ptr/128/
# _BitInt), and the elimination is width-aware -- a prior store is poisoned only
# when the later store's width covers it, so a union punned narrow-over-wide
# (u.l then u.i) is NOT wrongly killed. See DETAILS.md#t-lin-10441.
#
# Anti-vacuity: the subject has a dead double store and a dead pointer store and
# NO dead integer store, so with the type extension dse fires >=2; a revert to
# the integer-only gate drops it to 0 and this cell fails. Correctness: -O2
# -ftree-dse output must equal -O0 (the union result proves width-safety).
set -e

MCC=$1
SRCDIR=$2
WORK=$3

if [ -z "$MCC" ] || [ -z "$SRCDIR" ] || [ -z "$WORK" ]; then
	echo "usage: dse-typecov.sh <mcc> <srcdir> <workdir>" >&2
	exit 2
fi

mkdir -p "$WORK"
src=$SRCDIR/tests/misc/dse_typecov_subject.c

dse=$(MCC_STATS=strategy "$MCC" -O2 -ftree-dse -c "$src" -o "$WORK/d.o" 2>&1 |
	sed -n 's/.*[^a-z]dse=\([0-9][0-9]*\).*/\1/p' | tail -1)
echo "dse folds (float+ptr, -ftree-dse) = $dse"

if [ -z "$dse" ] || [ "$dse" -lt 2 ]; then
	echo "FAIL: type-complete DSE did not fire on the float+pointer dead stores (dse=$dse, want >=2)"
	exit 1
fi

"$MCC" -O0 "$src" -o "$WORK/r0" >/dev/null 2>&1
"$MCC" -O2 -ftree-dse "$src" -o "$WORK/r2" >/dev/null 2>&1
r0=$("$WORK/r0")
r2=$("$WORK/r2")
if [ "$r0" != "$r2" ]; then
	echo "FAIL: -O2 -ftree-dse output differs from -O0 ('$r2' vs '$r0') -- a live store was eliminated"
	exit 1
fi

if [ -n "$MCC_TYPECOV_RUN" ]; then
	runlo=$("$MCC" -O0 -run "$src" 2>/dev/null)
	runhi=$("$MCC" -O2 -ftree-dse -run "$src" 2>/dev/null)
	if [ "$runlo" != "$r0" ] || [ "$runhi" != "$r0" ]; then
		echo "FAIL: JIT -run mismatch (O0-run='$runlo' O2dse-run='$runhi' vs AOT-O0 '$r0') -- P1 (b) JIT correctness"
		exit 1
	fi
	echo "  JIT -run verified (P1 b): -O0-run == -O2-ftree-dse-run == AOT-O0 ($r0)"
fi

echo "ast/dse-typecov OK: DSE type-complete (dse=$dse on float+ptr), width-safe, result-invariant ($r2)"
