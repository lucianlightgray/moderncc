#!/bin/sh
# flagsweep.sh <mode> <mcc> <bdir> <idir> <workdir> [flag]
#
# Coverage for the -f surface in src/mccopt.h. Two modes:
#
#   accept        every flag in the table parses in both -f and -fno- form.
#                 Catches a rename that missed a call site, a row whose name
#                 does not reach the driver, and an offset that wrapped -- all
#                 of which otherwise present as a flag that silently does
#                 nothing, which is the failure mode this whole surface is most
#                 prone to.
#
#   exec <flag>   turn the flag on, and off, and check the program still
#                 computes the right answer. -O0 with the flag untouched is the
#                 reference. This is the jit-splice detector: that pass was off
#                 by default, exercised by nothing, and miscompiled the moment
#                 a level turned it on. A flag with no coverage is a flag whose
#                 first user finds the bug.
set -eu

MODE=$1; MCC=$2; BDIR=$3; IDIR=$4; WORK=$5; FLAG=${6:-}
S="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$WORK"

# Subjects. These are real exec goldens, not synthetic programs, and that is
# deliberate: an earlier version of this harness used two hand-written programs
# covering loops/calls/structs/floats, and it PASSED -fjit-splice -- a pass that
# segfaults a threads program. A sweep that misses the one bug you already know
# about is worse than no sweep, because it reads as coverage. The list spans
# threads and atomics, floats and NaN/Inf, bool and bitfields, VLA and
# compound literals, chained assignment, short-circuit with calls, and the
# argument-marshalling shapes that the replay defects lived in.
FLAGSWEEP_SUBJECTS="
features_c99_c11/c11_threads
features_c99_c11/atomic_counter
programs/random_stuff
types/floating_point
types/bool
types/builtin_inf_nan
statements/void_expr
statements/chained_assign
optimizer/assign_value_effects
expressions/precedence
functions_abi/func_name
codegen/overflow_inline
"
flags_from_table() {
	sed -n 's/.*MCC_OPT_ROW([A-Z0-9_]*, *"\([a-z0-9-]*\)".*/\1/p' "$S/src/mccopt.h"
}

case "$MODE" in
accept)
	printf 'int main(void){return 0;}\n' > "$WORK/a.c"
	bad=0; n=0
	for f in $(flags_from_table); do
		n=$((n + 1))
		for spell in "-f$f" "-fno-$f"; do
			out=$("$MCC" -B"$BDIR" -I"$IDIR" "$spell" -c "$WORK/a.c" -o "$WORK/a.o" 2>&1) || true
			case "$out" in
			*"unsupported option"*|*"invalid option"*)
				echo "FAIL $spell: not accepted by the driver"; bad=$((bad + 1)) ;;
			esac
		done
	done
	[ "$bad" -eq 0 ] || { echo "FAIL flagsweep-accept: $bad spelling(s) unreachable"; exit 1; }
	echo "PASS flagsweep-accept: $n flags accept -f and -fno-"
	;;
exec)
	[ -n "$FLAG" ] || { echo "FAIL flagsweep-exec: no flag given"; exit 2; }
	rc=0; ran=0
	for sub in $FLAGSWEEP_SUBJECTS; do
		src="$S/tests/exec/$sub.c"
		[ -f "$src" ] || continue
		nm=$(basename "$sub")
		"$MCC" -B"$BDIR" -I"$IDIR" -w -O0 "$src" -o "$WORK/$nm.ref" -lm -lpthread >/dev/null 2>&1 || continue
		ref=$("$WORK/$nm.ref" 2>&1) || continue
		ran=$((ran + 1))
		for spell in "-f$FLAG" "-fno-$FLAG"; do
			# -O2 so level-gated passes are live; the flag then forces its own state
			"$MCC" -B"$BDIR" -I"$IDIR" -w -O2 "$spell" "$src" -o "$WORK/$nm.t" -lm -lpthread \
				>/dev/null 2>&1 || {
				echo "FAIL $FLAG/$nm: build failed with $spell"; rc=1; continue; }
			got=$("$WORK/$nm.t" 2>&1) || {
				echo "FAIL $FLAG/$nm: run crashed with $spell (rc=$?)"; rc=1; continue; }
			[ "$got" = "$ref" ] || {
				echo "FAIL $FLAG/$nm: output differs with $spell"
				echo "  want: $ref"
				echo "  got : $got"
				rc=1; }
		done
	done
	[ "$ran" -gt 0 ] || { echo "SKIP flagsweep-exec $FLAG: no subject built"; exit 77; }
	[ "$rc" -eq 0 ] || exit 1
	echo "PASS flagsweep-exec $FLAG: $ran subjects match the reference with -f and -fno-"
	;;
*) echo "usage: flagsweep.sh accept|exec <mcc> <bdir> <idir> <work> [flag]"; exit 2 ;;
esac
