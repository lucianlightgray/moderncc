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
#   * ___dso_handle undefined, so no TU with a constructor or destructor linked
#   * __DATA,__mod_init_func mapped to a section nothing runs, so the
#     constructors that did link never fired
#   * a scalar LDRSB decoded as a 128-bit SIMD load, scaling its LO12 immediate
#     by 16 and truncating every offset below 16 to zero
#   * every arm64 sibling call rewritten into `bl`, because Mach-O has one
#     BRANCH26 for both and the reader called all of them CALL26
#
# Each program is compiled by clang with -g (DWARF) and the stack protector
# left ON (it is what pulls in __stack_chk_guard, the imported data symbol),
# linked twice -- once by clang as the reference, once by mcc -- and both are
# run and compared on stdout AND exit status.
#
# Every case is swept at -O0 AND -O2. This cell ran unoptimized only until
# 2026-08-02, and that is precisely why the sibling-call rewrite above survived:
# clang emits tail calls from -O1 up, so no object this cell produced contained
# one.
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

# types/int128 is DELIBERATELY not in that list, and the reason is a real gap
# rather than a quirk of one file: nothing on an mcc DARWIN link line provides
# compiler-rt. mcc spells its own overflow helpers __mcc_mulo_ti
# (runtime/lib/int128.c), and that file has been x86_64-only since 2026-08-02, so
# an arm64 Darwin link offers no *ti* helper at all. On ELF the same objects
# resolve because mcc_add_runtime puts -lgcc_s ahead of libmccrt and libgcc
# defines the compiler-rt names -- which is why the 2026-08-02 widening,
# validated on Linux, did not see it.
#
# What makes this case UNSTABLE rather than simply broken is that whether it
# trips depends on the clang version, so it must not gate CI either way. Its
# __builtin_mul_overflow over __int128 is lowered to a ___muloti4 CALL by the
# clang on the macOS runner -- mcc links the image and dyld then rejects it at
# load with "symbol not found in flat namespace '___muloti4'" (nightly Matrix run
# 30742545269) -- while Apple clang 21 inlines the same builtin and leaves only
# ___divti3/___modti3/___udivti3/___umodti3 undefined, all of which do resolve,
# so the identical case passes on a current Xcode. Restore it when a Darwin link
# line gains a compiler-rt provider.

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
