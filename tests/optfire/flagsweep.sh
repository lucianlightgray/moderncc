#!/bin/sh
# flagsweep.sh <mode> <mcc> <bdir> <idir> <workdir> [flag|row]
#
# Coverage for the -f surface in src/mccopt.h. Three modes:
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
#
#   cover <row>   the same corpus under row <row> of tests/optfire/cover3.txt,
#                 a 3-way covering array: every setting of every three flags
#                 appears in some row. One flag at a time cannot find a bug
#                 that needs two flags to agree; this can, for any bug needing
#                 three or fewer. It is not exhaustive -- see cover3.py.
set -eu

MODE=$1; MCC=$2; BDIR=$3; IDIR=$4; WORK=$5; ARG=${6:-}
S="$(cd "$(dirname "$0")/../.." && pwd)"
COVER="$(dirname "$0")/cover3.txt"
mkdir -p "$WORK"

# Subjects. These are real exec goldens, not synthetic programs, and that is
# deliberate: an earlier version of this harness used two hand-written programs
# covering loops/calls/structs/floats, and it PASSED -fjit-splice -- a pass that
# miscompiles programs/random_stuff. A sweep that misses the one bug you already
# know about is worse than no sweep, because it reads as coverage. The list
# spans threads and atomics, floats and NaN/Inf, bool and bitfields, VLA and
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

# Deliberate reds, as <flag>:<on|off>:<subject>. A listed case that fails is
# XFAIL and does not fail the cell; anything unlisted fails exactly as before.
# The list is self-cleaning -- a listed case that starts PASSING fails the cell
# and says to delete the entry -- so it cannot rot into silent coverage loss.
KNOWN_RED="jit-splice:on:random_stuff"

is_known_red() {
	for kr in $KNOWN_RED; do
		[ "$kr" = "$1" ] && return 0
	done
	return 1
}

flags_from_table() {
	sed -n 's/^	MCC_OPT_ROW([A-Z0-9_]*, *"\([a-z0-9-]*\)".*/\1/p' "$S/src/mccopt.h"
}

# Subject binaries run on one CPU. atomic_counter is 16 threads racing a
# test-and-set spinlock over 3.1M iterations: the cost is cache-line bouncing,
# not work, so spreading it over cores makes it slower, not faster. Measured on
# a 32-core host, one run: unpinned 1.97s wall / 31.0s CPU, two CPUs 0.60s /
# 1.19s, one CPU 0.02s / 0.02s. Times 113 flag cells times three runs each that
# is the difference between an 82-second suite and a 10-second one, for a sweep
# that is checking codegen, not race windows -- every atomic instruction still
# executes, and tests/exec runs these same goldens unpinned anyway. The CPU is
# picked from the cell name so concurrent ctest cells do not stack up on one
# core, and the whole thing is skipped where taskset does not exist.
PIN=""
if command -v taskset >/dev/null 2>&1; then
	NCPU=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 0)
	if [ "${NCPU:-0}" -ge 4 ]; then
		slot=$(printf '%s' "$MODE-$ARG" | cksum | cut -d' ' -f1)
		PIN="taskset -c $((slot % NCPU))"
	fi
fi

# corpus_run <label> <flagword>... -- build every subject at -O2 with the given
# -f words, run it, and diff against the same subject built at -O0 with the flag
# surface untouched. Sets `ran`, `rc` and `stale` in the caller.
ran=0
rc=0
stale=0
corpus_run() {
	label=$1
	shift
	for sub in $FLAGSWEEP_SUBJECTS; do
		src="$S/tests/exec/$sub.c"
		[ -f "$src" ] || continue
		nm=$(basename "$sub")
		"$MCC" -B"$BDIR" -I"$IDIR" -w -O0 "$src" -o "$WORK/$nm.ref" -lm -lpthread \
			>/dev/null 2>&1 || continue
		ref=$($PIN "$WORK/$nm.ref" 2>&1) || continue
		ran=$((ran + 1))
		red="$label:$nm"
		if "$MCC" -B"$BDIR" -I"$IDIR" -w -O2 "$@" "$src" -o "$WORK/$nm.t" -lm -lpthread \
			>/dev/null 2>&1; then :; else
			if is_known_red "$red"; then
				echo "XFAIL $red: build failed (deliberate red, see KNOWN_RED)"
			else
				echo "FAIL $red: build failed"
				rc=1
			fi
			continue
		fi
		if got=$($PIN "$WORK/$nm.t" 2>&1); then :; else
			crc=$?
			if is_known_red "$red"; then
				echo "XFAIL $red: run crashed (deliberate red, see KNOWN_RED)"
			else
				echo "FAIL $red: run crashed (rc=$crc)"
				rc=1
			fi
			continue
		fi
		if [ "$got" = "$ref" ]; then
			if is_known_red "$red"; then
				stale=$((stale + 1))
				echo "$red: PASSES but is listed in KNOWN_RED"
			fi
			continue
		fi
		if is_known_red "$red"; then
			echo "XFAIL $red: output differs (deliberate red, see KNOWN_RED)"
			continue
		fi
		echo "FAIL $red: output differs"
		echo "  want: $ref"
		echo "  got : $got"
		rc=1
	done
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
	[ -n "$ARG" ] || { echo "FAIL flagsweep-exec: no flag given"; exit 2; }
	corpus_run "$ARG:on" "-f$ARG"
	corpus_run "$ARG:off" "-fno-$ARG"
	[ "$ran" -gt 0 ] || { echo "SKIP flagsweep-exec $ARG: no subject built"; exit 77; }
	[ "$stale" -eq 0 ] ||
		{ echo "FAIL flagsweep-exec $ARG: $stale KNOWN_RED case(s) now pass -- drop them from KNOWN_RED in $0"; exit 1; }
	[ "$rc" -eq 0 ] || exit 1
	echo "PASS flagsweep-exec $ARG: $ran subject runs match the reference with -f and -fno-"
	;;
cover)
	[ -n "$ARG" ] || { echo "FAIL flagsweep-cover: no row given"; exit 2; }
	[ -f "$COVER" ] || { echo "SKIP flagsweep-cover: $COVER missing"; exit 77; }
	bits=$(sed -n "s/^row $ARG //p" "$COVER")
	[ -n "$bits" ] || { echo "FAIL flagsweep-cover: no row $ARG in $COVER"; exit 2; }
	names=$(flags_from_table)
	nflag=$(printf '%s\n' "$names" | wc -l)
	[ "${#bits}" -eq "$nflag" ] ||
		{ echo "FAIL flagsweep-cover row $ARG: $COVER has ${#bits} columns, src/mccopt.h has $nflag rows -- regenerate with tests/optfire/cover3.py gen"; exit 1; }
	set --
	rest=$bits
	for f in $names; do
		b=${rest%"${rest#?}"}
		rest=${rest#?}
		if [ "$b" = 1 ]; then set -- "$@" "-f$f"; else set -- "$@" "-fno-$f"; fi
	done
	corpus_run "row$ARG" "$@"
	[ "$ran" -gt 0 ] || { echo "SKIP flagsweep-cover $ARG: no subject built"; exit 77; }
	if [ "$rc" -ne 0 ]; then
		echo "FAIL flagsweep-cover row $ARG: the configuration is"
		echo "  $*"
		exit 1
	fi
	echo "PASS flagsweep-cover row $ARG: $ran subjects match the reference under $nflag forced flags"
	;;
*) echo "usage: flagsweep.sh accept|exec|cover <mcc> <bdir> <idir> <work> [flag|row]"; exit 2 ;;
esac
