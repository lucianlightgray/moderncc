#!/bin/sh
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
types/types types/typedef pointers_arrays/pointer
features_c99_c11/constructor functions_abi/func_name lexical/string_literals
features_c99_c11/complex features_c99_c11/complex_annexg
features_c99_c11/c11_complex_convert features_c99_c11/c11_complex_decls
programs/random_stuff
structs_unions/struct_byval structs_unions/union_byval
structs_unions/bitfields functions_abi/variadic_promotions
codegen/bswap_inline codegen/popcount_inline codegen/signbit_inline
expressions/integer_promotion expressions/div_mod_shift
optimizer/loop_fusion optimizer/loop_interchange
types/bool"

fails=0
ran=0
for c in $CASES; do
	base=$(basename "$c")
	csrc="$SRC/tests/exec/$c.c"
	[ -f "$csrc" ] || { echo "SKIP $base: $csrc missing"; continue; }

	for opt in -O0 -O2; do
		name="$base$opt"

		clang -w -g $opt -c "$csrc" -o "$WORK/$name.o" 2>/dev/null || {
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
done

[ "$ran" -gt 0 ] || { echo "SKIP: no case survived the clang reference build"; exit 77; }

if [ "$fails" -gt 0 ]; then
	echo "macho-clanglink: $fails failure(s) over $ran case(s)" >&2
	exit 1
fi
echo "macho-clanglink: OK ($ran clang objects linked by mcc, output and exit status match)"
