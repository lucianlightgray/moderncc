#!/bin/sh
# ast/reemit-cfi -- every re-emitted function must carry its own .eh_frame FDE.
#
# Sweep row 17 called the forward-inline re-emit "not a correctness bug" because
# it only leaves dead bytes behind.  It leaves something else behind too:
# ast_reemit appends the new body at the end of .text and re-points the symbol
# at it, but never called mcc_debug_funcend, so the FDE still described the
# ORPHAN.  The unwinder then stops dead at the first re-emitted frame -- the
# same program with the two definitions reordered unwound fully.
#
# The check is structural rather than a backtrace, so it needs no glibc and no
# execution: every FUNC symbol in .text must have an FDE whose initial location
# is that symbol's st_value.  The orphans keep their own stale FDEs on purpose
# (under --embed-jit the dispatch slot still points into that range), so this
# asserts coverage, never equality of counts.
set -e

MCC=$1
SRCDIR=$2
WORK=$3
READELF=${READELF:-readelf}

if [ -z "$MCC" ] || [ -z "$SRCDIR" ] || [ -z "$WORK" ]; then
	echo "usage: reemit-cfi.sh <mcc> <srcdir> <workdir>" >&2
	exit 2
fi
if ! command -v "$READELF" >/dev/null 2>&1; then
	echo "SKIP: no $READELF"
	exit 77
fi

mkdir -p "$WORK"
obj=$WORK/reemit-cfi.o
src=$SRCDIR/tests/ast/reemit_cfi_subject.c

# MCC_INV=1 reports ast.orphan_fn, which is the anti-vacuity floor: if the
# subject stops triggering the forward-inline re-emit this cell would pass by
# testing nothing, which is exactly the failure mode row 17 is filed under.
inv=$(MCC_INV=1 "$MCC" -O3 -c "$src" -o "$obj" 2>&1 >/dev/null |
	tr ' ' '\n' | sed -n 's/^ast\.orphan_fn=//p' | tail -1)
inv=${inv:-0}
if [ "$inv" -lt 1 ]; then
	echo "FAIL reemit-cfi: the subject produced ast.orphan_fn=$inv, so no body was"
	echo "  re-emitted and this cell checked nothing. Row 17's re-emit path is"
	echo "  what it exists to cover -- fix the subject, do not relax the floor."
	exit 1
fi

"$READELF" -sW "$obj" |
	awk '$4 == "FUNC" && $8 != "UND" { printf "%s %s\n", strtonum("0x" $2), $8 }' |
	sort -u >"$WORK/syms.txt"

"$READELF" -wf "$obj" |
	sed -n 's/.*FDE .*pc=\([0-9a-f][0-9a-f]*\)\.\..*/\1/p' |
	while read -r a; do printf '%s\n' "$((0x$a))"; done |
	sort -u >"$WORK/fdes.txt"

nsym=$(wc -l <"$WORK/syms.txt")
nfde=$(wc -l <"$WORK/fdes.txt")
if [ "$nfde" -lt 1 ]; then
	echo "SKIP: $READELF reported no FDEs; this build has no .eh_frame to check"
	exit 77
fi

missing=0
while read -r addr name; do
	if ! grep -qx "$addr" "$WORK/fdes.txt"; then
		echo "FAIL reemit-cfi: $name at .text+$addr has no FDE at its own address"
		missing=$((missing + 1))
	fi
done <"$WORK/syms.txt"

if [ "$missing" -gt 0 ]; then
	echo "FAIL reemit-cfi: $missing of $nsym function(s) ship with no CFI."
	echo "  ast_reemit re-points the symbol at a new body without emitting an FDE"
	echo "  for it, so an unwinder stops at the first such frame."
	exit 1
fi

echo "reemit-cfi: OK ($nsym function(s), $nfde FDE(s), ast.orphan_fn=$inv re-emitted)"
exit 0
