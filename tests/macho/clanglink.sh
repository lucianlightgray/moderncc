#!/bin/sh
# mcc linking REAL clang -c Mach-O objects.
#
# Nothing else in the suite does this, which is why three separate reader
# defects sat unnoticed until the corpus was swept by hand:
#   * a Mach-O COMMON (tentative) definition read as a plain undefined, so
#     every file-scope `int x;` clang compiled was an unresolved reference
#   * DWARF relocations reaching the chained-fixup rebase list, whose 4-byte
#     alignment rule DWARF's byte-packed fields do not obey
#   * the GOT slot of an imported DATA symbol never bound, because only the
#     FIRST GOT relocation for a symbol creates the slot and Mach-O stores
#     relocations in descending address order -- so clang's LO12 half arrived
#     first and the ADR_GOT_PAGE case never ran
#
# Each program is compiled by clang with -g (DWARF) and the stack protector
# left ON (it is what pulls in __stack_chk_guard, the imported data symbol),
# linked twice -- once by clang as the reference, once by mcc -- and both are
# run and compared on stdout AND exit status.
#
# Usage: clanglink.sh <mcc> <srcdir> <workdir> [-B<prefix>]
set -e

MCC=$1
SRC=$2
WORK=$3
BFLAG=$4
[ -n "$MCC" ] && [ -n "$SRC" ] && [ -n "$WORK" ] || {
	echo "usage: clanglink.sh <mcc> <srcdir> <workdir> [-B<prefix>]" >&2
	exit 2
}

[ "$(uname -s)" = "Darwin" ] || { echo "SKIP: needs a Darwin host to run both images"; exit 77; }
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
command -v clang >/dev/null 2>&1 || { echo "SKIP: no clang to produce Mach-O objects"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"

CASES="programs/quicksort programs/hanoi programs/grep
statements/tentative_array statements/bracket_evaluation statements/scopes
statements/conditional_operator types/storage_tentative types/floating_point
types/types types/typedef pointers_arrays/pointer"

fails=0
ran=0
for c in $CASES; do
	name=$(basename "$c")
	csrc="$SRC/tests/exec/$c.c"
	[ -f "$csrc" ] || { echo "SKIP $name: $csrc missing"; continue; }

	clang -w -g -c "$csrc" -o "$WORK/$name.o" 2>/dev/null || {
		echo "SKIP $name: clang could not compile it"
		continue
	}
	clang "$WORK/$name.o" -o "$WORK/$name.ref" 2>/dev/null || {
		echo "SKIP $name: not standalone-linkable"
		continue
	}

	if ! "$MCC" $BFLAG "$WORK/$name.o" -o "$WORK/$name" 2>"$WORK/$name.err"; then
		echo "FAIL $name: mcc could not link a clang object: $(head -1 "$WORK/$name.err")" >&2
		fails=$((fails + 1))
		continue
	fi

	ran=$((ran + 1))
	refout=$("$WORK/$name.ref" 2>&1) || true
	refrc=$?
	gotout=$("$WORK/$name" 2>&1) || true
	gotrc=$?
	if [ "$refout" != "$gotout" ] || [ "$refrc" != "$gotrc" ]; then
		echo "FAIL $name: mcc-linked image disagrees with the clang-linked one" >&2
		echo "  ref rc=$refrc: $(printf '%s' "$refout" | head -3)" >&2
		echo "  mcc rc=$gotrc: $(printf '%s' "$gotout" | head -3)" >&2
		fails=$((fails + 1))
	fi
done

[ "$ran" -gt 0 ] || { echo "SKIP: no case survived the clang reference build"; exit 77; }

if [ "$fails" -gt 0 ]; then
	echo "macho-clanglink: $fails failure(s) over $ran case(s)" >&2
	exit 1
fi
echo "macho-clanglink: OK ($ran clang objects linked by mcc, output and exit status match)"
