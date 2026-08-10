#!/bin/sh
# flagsweep.sh <mode> <mcc> <bdir> <idir> <workdir> [flag|row]
#
# Coverage for the -f surface in src/mccopt.h. Four modes:
#
#   accept        every flag in the table parses in both -f and -fno- form.
#                 Catches a rename that missed a call site, a row whose name
#                 does not reach the driver, and an offset that wrapped -- all
#                 of which otherwise present as a flag that silently does
#                 nothing, which is the failure mode this whole surface is most
#                 prone to.
#
#   devgate       every MCC_OPTD_DEV row is REFUSED in the -f spelling without
#                 MCC_DEV, with a message naming the flag and MCC_DEV; accepted
#                 with MCC_DEV=1; and accepted in the -fno- spelling either way.
#                 Every other row is unaffected. Every level above the highest
#                 ungated rung and below the search level reaches only gated
#                 knobs and must be a hard ERROR naming MCC_DEV instead of
#                 quietly compiling as that rung. Silence is the failure this
#                 exists for: an unknown -f is a warning the driver exits 0 on,
#                 so a gate that merely dropped the row would turn every gated
#                 flag into a no-op nothing could tell from a typo, and a level
#                 that clamped would make every counter assertion above the rung
#                 green-but-vacuous. Both bounds are derived from src/mccopt.h,
#                 never written down here.
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
#
# The second block is earned rather than guessed: each of these is a subject a
# full-corpus sweep of all 280 runnable tests/exec programs caught a real defect
# on that the first twelve missed. Keeping them is what makes this list a
# regression net instead of a sample -- const_member_copy and libm_builtin_fold
# are the two ships-today miscompiles, the complex ones and transparent_union /
# union_byval are the replay defects the byte gate is masking, and int128 and
# weak_undef are the widest-blast-radius members of the cmp-materialize family.
# Set MCC_FLAGSWEEP_CORPUS=all to sweep every tests/exec program instead; that
# is ~25x the subjects and is how this list was derived, not a default.
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
types/const_member_copy
types/int128
features_c99_c11/libm_builtin_fold
features_c99_c11/c11_complex_convert
features_c99_c11/complex_annexg
features_c99_c11/builtin_overflow
features_c99_c11/weak_undef
structs_unions/transparent_union
structs_unions/union_byval
structs_unions/inline_sret_locrec
codegen/overflow_narrow
expressions/relational_equality
optimizer/logical_not_shortcircuit
"

# Every tests/exec program, for MCC_FLAGSWEEP_CORPUS=all. Subjects that do not
# build or run standalone at -O0 drop out on their own in corpus_run.
flagsweep_all_subjects() {
	find "$S/tests/exec" -name '*.c' | sed "s|^$S/tests/exec/||; s|\.c$||" | sort
}

# Deliberate reds, as <flag>:<on|off>:<subject>. A listed case that fails is
# XFAIL and does not fail the cell; anything unlisted fails exactly as before.
# The list is self-cleaning -- a listed case that starts PASSING fails the cell
# and says to delete the entry -- so it cannot rot into silent coverage loss.
KNOWN_RED=""
KNOWN_FLAKY_RED=""

is_known_red() {
	for kr in $KNOWN_RED; do
		[ "$kr" = "$1" ] && return 0
	done
	return 1
}

is_known_flaky_red() {
	for kr in $KNOWN_FLAKY_RED; do
		[ "$kr" = "$1" ] && return 0
	done
	return 1
}

flags_from_table() {
	sed -n 's/^	MCC_OPT_ROW([A-Z0-9_]*, *"\([a-z0-9-]*\)".*/\1/p' "$S/src/mccopt.h"
}

dev_flags_from_table() {
	sed -n 's/^	MCC_OPT_ROW([A-Z0-9_]*, *"\([a-z0-9-]*\)", *MCC_OPTD_DEV(.*/\1/p' \
		"$S/src/mccopt.h"
}

# The highest -O an ungated knob is reached by, and the search rung. Everything
# strictly between them is gate-only. Both come out of src/mccopt.h so this cell
# cannot drift from the table the driver derives its own bounds from.
top_ungated_level() {
	sed -n 's/^	MCC_OPT_ROW([A-Z0-9_]*, *"[a-z0-9-]*", *MCC_OPTD_LEVEL(\([0-9]*\)).*/\1/p' \
		"$S/src/mccopt.h" | sort -n | tail -1
}

search_level() {
	sed -n 's/^#define MCC_OPT_SEARCH_LEVEL \([0-9]*\).*/\1/p' "$S/src/mccopt.h"
}

is_dev_flag() {
	for df in $(dev_flags_from_table); do
		[ "$df" = "$1" ] && return 0
	done
	return 1
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

# corpus_run <level> <label> <flagword>... -- build every subject at <level> with
# the given -f words, run it, and diff against the same subject built at -O0 with
# the flag surface untouched. Sets `ran`, `rc` and `stale` in the caller.
#
# The oracle is the exit status AND the output, not the output alone. An earlier
# version compared only stdout and, worse, dropped any subject whose reference
# run exited nonzero -- `ref=$(...) || continue`. That silently removed every
# program whose whole contract is its exit code from every cell that ever ran:
# codegen/overflow_inline aborts with rc=20 when the miscompile fires and was
# invisible, and types/int128 goes from rc=0 to a SIGSEGV that produces no output
# at all, so both sides compared equal as the empty string. The status is
# appended inside the same command substitution rather than read from $? after
# it, because `set -e` treats a failing substitution in an assignment as a
# failing command -- which is what the `|| continue` was really there for. The
# `set +e` is load-bearing for the same reason one level in: errexit applies
# inside the substitution's subshell too, so without it a subject that exits
# nonzero kills the subshell before the printf and the whole cell exits with the
# subject's status, silently, having checked nothing after it.
ran=0
rc=0
stale=0
corpus_run() {
	lvl=$1
	label=$2
	shift 2
	if [ "${MCC_FLAGSWEEP_CORPUS:-}" = all ]; then
		subjects=$(flagsweep_all_subjects)
	else
		subjects=$FLAGSWEEP_SUBJECTS
	fi
	for sub in $subjects; do
		src="$S/tests/exec/$sub.c"
		[ -f "$src" ] || continue
		nm=$(basename "$sub")
		"$MCC" -B"$BDIR" -I"$IDIR" -w -O0 "$src" -o "$WORK/$nm.ref" -lm -lpthread \
			>/dev/null 2>&1 || continue
		ref=$(set +e; $PIN "$WORK/$nm.ref" 2>&1; printf '[rc=%s]' "$?")
		ran=$((ran + 1))
		red="$label:$nm"
		if "$MCC" -B"$BDIR" -I"$IDIR" -w "$lvl" "$@" "$src" -o "$WORK/$nm.t" -lm -lpthread \
			>/dev/null 2>&1; then :; else
			if is_known_red "$red" || is_known_flaky_red "$red"; then
				echo "XFAIL $red: build failed at $lvl (deliberate red, see KNOWN_RED)"
			else
				echo "FAIL $red: build failed at $lvl"
				rc=1
			fi
			continue
		fi
		got=$(set +e; $PIN "$WORK/$nm.t" 2>&1; printf '[rc=%s]' "$?")
		if [ "$got" = "$ref" ]; then
			if is_known_red "$red"; then
				stale=$((stale + 1))
				echo "$red: PASSES at $lvl but is listed in KNOWN_RED"
			fi
			continue
		fi
		if is_known_red "$red" || is_known_flaky_red "$red"; then
			echo "XFAIL $red: differs at $lvl (deliberate red, see KNOWN_RED)"
			continue
		fi
		echo "FAIL $red: differs at $lvl"
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
devgate)
	printf 'int main(void){return 0;}\n' > "$WORK/a.c"
	bad=0; ngate=0; nplain=0
	dev=$(dev_flags_from_table)
	[ -n "$dev" ] || { echo "FAIL flagsweep-devgate: src/mccopt.h has no MCC_OPTD_DEV row, so this cell would agree with an ungated tree"; exit 1; }
	for f in $(flags_from_table); do
		out=$("$MCC" -B"$BDIR" -I"$IDIR" "-f$f" -c "$WORK/a.c" -o "$WORK/a.o" 2>&1) && ok=1 || ok=0
		if is_dev_flag "$f"; then
			ngate=$((ngate + 1))
			if [ "$ok" = 1 ]; then
				echo "FAIL -f$f: accepted without MCC_DEV; a gated flag must be refused"
				bad=$((bad + 1))
			else
				case "$out" in
				*"-f$f"*MCC_DEV*) ;;
				*) echo "FAIL -f$f: refused, but the message does not name both the flag and MCC_DEV: $out"
				   bad=$((bad + 1)) ;;
				esac
			fi
			out=$(MCC_DEV=1 "$MCC" -B"$BDIR" -I"$IDIR" "-f$f" -c "$WORK/a.c" -o "$WORK/a.o" 2>&1) ||
				{ echo "FAIL -f$f: still refused with MCC_DEV=1: $out"; bad=$((bad + 1)); }
			out=$("$MCC" -B"$BDIR" -I"$IDIR" "-fno-$f" -c "$WORK/a.c" -o "$WORK/a.o" 2>&1) ||
				{ echo "FAIL -fno-$f: refused without MCC_DEV; the off spelling asks for the state the gate already guarantees: $out"; bad=$((bad + 1)); }
		else
			nplain=$((nplain + 1))
			[ "$ok" = 1 ] ||
				{ echo "FAIL -f$f: refused without MCC_DEV, but it is not an MCC_OPTD_DEV row: $out"; bad=$((bad + 1)); }
		fi
	done
	# A rung whose only knobs are gated must REFUSE rather than quietly
	# compiling as the rung below it. A silent clamp is what makes a counter
	# assertion at a level nothing reaches read as green, which is the exact
	# defect filed against defstate: "off by default" indistinguishable from
	# "does not exist". Bounds derived, so consolidating or splitting a rung
	# moves this loop with the table.
	top=$(top_ungated_level); srch=$(search_level)
	[ -n "$top" ] && [ -n "$srch" ] ||
		{ echo "FAIL flagsweep-devgate: could not derive the level bounds from src/mccopt.h"; exit 1; }
	[ "$top" -lt "$srch" ] ||
		{ echo "FAIL flagsweep-devgate: the highest ungated rung -O$top is not below the search level -O$srch, so there is no gate-only band and this loop would check nothing"; exit 1; }
	nlvl=0; lvl=$((top + 1))
	while [ "$lvl" -lt "$srch" ]; do
		nlvl=$((nlvl + 1))
		out=$("$MCC" -B"$BDIR" -I"$IDIR" "-O$lvl" -c "$WORK/a.c" -o "$WORK/a.o" 2>&1) && ok=1 || ok=0
		if [ "$ok" = 1 ]; then
			echo "FAIL -O$lvl: reaches only gated knobs but compiled anyway; a level that clamps makes every assertion above -O$top vacuous"
			bad=$((bad + 1))
		else
			case "$out" in
			*MCC_DEV*) ;;
			*) echo "FAIL -O$lvl: refused, but the message does not name MCC_DEV: $out"
			   bad=$((bad + 1)) ;;
			esac
		fi
		MCC_DEV=1 "$MCC" -B"$BDIR" -I"$IDIR" "-O$lvl" -c "$WORK/a.c" -o "$WORK/a.o" >"$WORK/dg.out" 2>&1 ||
			{ echo "FAIL -O$lvl: still refused with MCC_DEV=1: $(cat "$WORK/dg.out")"; bad=$((bad + 1)); }
		lvl=$((lvl + 1))
	done
	[ "$nlvl" -gt 0 ] ||
		{ echo "FAIL flagsweep-devgate: the gate-only band is empty, so no level was checked"; exit 1; }
	# The search rung and the top ungated rung must both still compile, or the
	# refusal above would be indistinguishable from a driver that rejects every
	# numeric level.
	for lvl in "$top" "$srch"; do
		"$MCC" -B"$BDIR" -I"$IDIR" "-O$lvl" -c "$WORK/a.c" -o "$WORK/a.o" >"$WORK/dg.out" 2>&1 ||
			{ echo "FAIL -O$lvl: refused, but it is not in the gate-only band: $(cat "$WORK/dg.out")"; bad=$((bad + 1)); }
	done
	# And one past the search rung must be refused too: an unbounded parse used
	# to clamp silently at 255.
	if "$MCC" -B"$BDIR" -I"$IDIR" "-O$((srch + 1))" -c "$WORK/a.c" -o "$WORK/a.o" >"$WORK/dg.out" 2>&1; then
		echo "FAIL -O$((srch + 1)): accepted; the level parse is unbounded above the search rung"
		bad=$((bad + 1))
	fi
	[ "$bad" -eq 0 ] || { echo "FAIL flagsweep-devgate: $bad violation(s)"; exit 1; }
	echo "PASS flagsweep-devgate: $ngate gated flags refused without MCC_DEV and accepted with it, $nplain ungated flags unaffected, -O$((top + 1))..-O$((srch - 1)) ($nlvl levels) error and name the gate, -O$top and -O$srch compile, -O$((srch + 1)) is refused"
	;;
exec)
	[ -n "$ARG" ] || { echo "FAIL flagsweep-exec: no flag given"; exit 2; }
	# All three levels, because a flag's effect is relative to the level's own
	# default and one level cannot see the others. inline defaults off at -O1 and
	# on at -O3, inline-functions off at -O1 and on at -O2: the state (inline=1,
	# inline-functions=0) that miscompiles structs_unions/union_byval is reached
	# by -finline at -O1 and by -fno-inline-functions at -O3, and by no single
	# flip at -O2. A sweep at one level calls two of those three green.
	for lvl in -O1 -O2 -O3; do
		corpus_run "$lvl" "$ARG:on" "-f$ARG"
		corpus_run "$lvl" "$ARG:off" "-fno-$ARG"
	done
	[ "$ran" -gt 0 ] || { echo "SKIP flagsweep-exec $ARG: no subject built"; exit 77; }
	[ "$stale" -eq 0 ] ||
		{ echo "FAIL flagsweep-exec $ARG: $stale KNOWN_RED case(s) now pass -- drop them from KNOWN_RED in $0"; exit 1; }
	[ "$rc" -eq 0 ] || exit 1
	echo "PASS flagsweep-exec $ARG: $ran subject runs match the reference with -f and -fno- at -O1 -O2 -O3"
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
	for lvl in -O1 -O2 -O3; do
		corpus_run "$lvl" "row$ARG" "$@"
	done
	[ "$ran" -gt 0 ] || { echo "SKIP flagsweep-cover $ARG: no subject built"; exit 77; }
	if [ "$rc" -ne 0 ]; then
		echo "FAIL flagsweep-cover row $ARG: the configuration is"
		echo "  $*"
		exit 1
	fi
	echo "PASS flagsweep-cover row $ARG: $ran subject runs match the reference under $nflag forced flags at -O1 -O2 -O3"
	;;
*) echo "usage: flagsweep.sh accept|devgate|exec|cover <mcc> <bdir> <idir> <work> [flag|row]"; exit 2 ;;
esac
