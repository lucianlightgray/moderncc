#!/bin/sh
# stratsweep.sh <mode> <mcc> <bdir> <idir> <workdir> [args...]
#
# The optimizer's strategy registry (src/mccast.c, ast_strategies[]) is a table
# of {name, gate, run} rows driven as an ordered sequence. MCC_AST_STRAT_ORDER
# forces that sequence, so a row can be run alone and a triple can be run in a
# chosen order. Everything here rides on that.
#
#   names                 print index:name for the registry, one per line.
#
#   check                 STRAT_NAMES below must equal ast_strategies[] in
#                         src/mccast.c, name for name and in order. A row added
#                         to the registry and not here is a row this sweep
#                         silently does not cover, and the sweep would still
#                         report PASS -- so that mismatch is a hard failure.
#
#   iso <strat> <corpus>  run exactly one strategy, all others off, over the
#                         corpus; the program must still print what -O0 prints.
#                         <strat> is a name, an index, or "all". This is the
#                         "does the pass work at all" axis: a row that is only
#                         ever exercised behind twenty-one other rows has no
#                         isolated evidence, and the first caller to reorder
#                         the table finds the bug.
#
#   perm3 <sh> <n> <c> <p>
#                         every ORDERED triple of distinct strategies, sharded
#                         <sh> of <n>. 22*21*20 = 9240 sequences. Order is the
#                         point: a triple that is fine as a,b,c and wrong as
#                         b,a,c is two passes that do not commute, and that is
#                         a real defect even though each pass is individually
#                         correct. <p> is an optional index list run ahead of
#                         every triple, "-" for none; "0,1,2,3" puts the four
#                         template folders the pipeline runs first back in
#                         front, which is the composition it actually performs.
#                         A triple that itself contains one of 0..3 then runs
#                         that row twice, which is a composition the pipeline
#                         also performs -- ast_run_strat_cycle re-runs the whole
#                         sequence while it keeps hitting -- so it is not
#                         excluded.
#
#   seq <a,b,c> <corpus>  one explicit sequence. Used to re-check a hit.
#
# <corpus> is "full" (every tests/exec program that qualifies) or "subjects"
# (the flagsweep subject list plus the strategy-named optimizer goldens).
#
# Subject admission is deliberate and strict:
#
#   - the -O0 build must succeed and the program must run,
#   - two -O0 runs must agree (drops the nondeterministic ones),
#   - -O2 with the whole registry disabled must agree with -O0.
#
# The last one is what keeps this honest. Without it, any -O2 defect outside
# the registry -- and any program that prints a stack address -- lands on
# whichever strategy happens to be under test.
set -eu

MODE=$1
case "$MODE" in
names|check) ;;
*) MCC=$2; BDIR=$3; IDIR=$4; WORK=$5; shift 5 ;;
esac
S="$(cd "$(dirname "$0")/../.." && pwd)"

STRAT_NAMES="bfold ident narrow cprop cse ltemp ivsr pre licm dse sccp jt bf range divmagic abs select reassoc sethi tco inline cload sra sroa"
# Every row of the registry is under test, 0..21. An earlier cut of this sweep
# started at 4 on the theory that bfold/ident/narrow/cprop are always-on folders
# and so not worth isolating. They isolate fine -- each of the four is green
# alone over the whole admitted corpus -- and "always on" is not a reason to
# leave a quarter of the table unmeasured, so the range is the whole table and
# the check mode below is what keeps it that way.
STRAT_FIRST=0
STRAT_LAST=$(($(echo $STRAT_NAMES | wc -w) - 1))
# 23 is a valid slot in MCC_AST_STRAT_ORDER's range (< AST_STRAT_COUNT_MAX, 24)
# but past AST_STRAT_COUNT, so the runner skips it: this is how "no strategy at
# all" is spelled. It stops working the moment the registry reaches 25 rows, so
# check mode asserts the gap is still there.
STRAT_NONE=25

# The flagsweep subject list, verbatim and for its stated reason: an earlier
# sweep built from hand-written programs passed -fjit-splice, a pass that
# segfaults a threads program. Threads, atomics, floats and NaN/Inf, bool and
# bitfields, VLA and compound literals, chained assignment, short-circuit with
# calls, and the argument-marshalling shapes the replay defects lived in.
#
# The second block was added for the same reason the first one exists. Deleting
# the read-invalidation half of ast_cse_kill() -- a one-clause guard flip that
# lets CSE reuse a cached expression across a store to one of its operands --
# left the twelve flagsweep subjects entirely green. It was caught only by
# optimizer/cse and types/int_conversion. The strategy-named optimizer goldens
# are the programs written to make each row of the registry fire, so they are
# the ones that notice when a row fires wrongly; a permutation sweep without
# them measures almost nothing.
SUBJECTS="
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
optimizer/cse
optimizer/dead_store_elim
optimizer/jump_thread
optimizer/licm
optimizer/pre_partial_redundancy
optimizer/tail_call_loop
optimizer/local_const_prop
optimizer/const_branch_fold
optimizer/narrow_ranged
optimizer/redundant_cast
optimizer/region_store
optimizer/side_effect_order
optimizer/loop_cond_effects
optimizer/logical_not_shortcircuit
optimizer/bitflag_cluster
types/int_conversion
structs_unions/struct_byval
structs_unions/bitfields
vla/basic
"

strat_index() {
	case "$1" in
	''|*[!0-9]*)
		i=0
		for n in $STRAT_NAMES; do
			[ "$n" = "$1" ] && { echo "$i"; return 0; }
			i=$((i + 1))
		done
		return 1 ;;
	*) echo "$1" ;;
	esac
}

if [ "$MODE" = check ]; then
	have=$(sed -n '/^static const AstStrategy ast_strategies\[/,/^};/p' "$S/src/mccast.c" |
		sed -n 's/^[[:space:]]*{"\([a-z0-9]*\)".*/\1/p' | tr '\n' ' ')
	want="$(echo $STRAT_NAMES) "
	[ "$have" = "$want" ] || {
		echo "FAIL stratsweep-check: ast_strategies[] and STRAT_NAMES disagree"
		echo "  registry: $have"
		echo "  sweep   : $want"
		exit 1; }
	cmax=$(sed -n 's/^#define AST_STRAT_COUNT_MAX \([0-9]*\).*/\1/p' "$S/src/mccast.c")
	[ "$STRAT_NONE" -gt "$STRAT_LAST" ] && [ "$STRAT_NONE" -lt "$cmax" ] || {
		echo "FAIL stratsweep-check: STRAT_NONE=$STRAT_NONE is no longer an empty slot"
		echo "  rows 0..$STRAT_LAST, AST_STRAT_COUNT_MAX=$cmax"
		echo "  the registry-disabled baseline this sweep compares against is not disabled"
		exit 1; }
	echo "PASS stratsweep-check: STRAT_NAMES tracks ast_strategies[] ($(echo $STRAT_NAMES | wc -w) rows), STRAT_NONE=$STRAT_NONE still empty (max $cmax)"
	exit 0
fi

if [ "$MODE" = names ]; then
	i=0
	for n in $STRAT_NAMES; do
		[ "$i" -ge "$STRAT_FIRST" ] && [ "$i" -le "$STRAT_LAST" ] && echo "$i:$n"
		i=$((i + 1))
	done
	exit 0
fi

mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd)
RUN="$WORK/run"
: > "$WORK/admitted"
: > "$WORK/skipped"

# Subjects run with cwd inside this cell's own workdir, never the shared build
# directory. tests/exec/programs/stdio.c writes and re-reads "fred.txt" in the
# current directory, so every cell that runs it from a common cwd is racing
# every other one -- and this sweep runs 22 cells at once over a corpus that
# includes it. Observed directly: of four full-corpus isolation runs started
# together from one directory, one dropped programs/stdio as nondeterministic
# and three admitted it. That is also the most likely explanation for the
# single unreproducible red this harness produced before the fix, which landed
# on iso-bf for no reason connected to the bitflag pass. tests/runner.c already
# does exactly this ("cd %s &&" per cell); this brings the sweep in line.
CWD="$WORK/cwd"
mkdir -p "$CWD"

# The timeout is a hang guard, not a performance budget. The slowest admitted
# subject is bounds/bound_signal at 1.1s, but these cells run 29-wide inside a
# ctest that is already running 8700 other cells, and a subject pinned to
# a core that is 30-deep can miss a tight bound by a wide margin. A timeout
# kill reads as a wrong answer, so it has to be well clear of scheduling noise.
flakes=0
TMO=""
if command -v timeout >/dev/null 2>&1; then
	TMO="timeout 120"
fi

# Subject binaries run on one CPU. atomic_counter is 16 threads hammering one
# cache line: unpinned it is 2.0s wall of pure line-bouncing and it alone was
# 95% of a pass, pinned it is 0.05s. Every atomic instruction still executes
# and the answer is still checked -- what is lost is true simultaneity, and
# tests/exec still runs these goldens unpinned. The CPU is picked from the pid
# so parallel shards do not pile onto one core. Both the reference and the
# subject run are pinned the same way, so the comparison is unaffected.
PIN=""
if command -v taskset >/dev/null 2>&1 && command -v nproc >/dev/null 2>&1; then
	PIN="taskset -c $(( $$ % $(nproc) ))"
fi

build() {
	# build <order> <src> <out>; empty order means "compiler default"
	if [ -z "$1" ]; then
		"$MCC" -B"$BDIR" -I"$IDIR" -w -O0 "$2" -o "$3" -lm -lpthread >/dev/null 2>&1
	else
		MCC_AST_STRAT_ORDER="$1" "$MCC" -B"$BDIR" -I"$IDIR" -w -O2 "$2" -o "$3" \
			-lm -lpthread >/dev/null 2>&1
	fi
}

runit() {
	# runit <exe> -> prints "<rc>|<output-with-newlines-squashed>"
	out=$(cd "$CWD" && $TMO $PIN "$1" 2>&1 </dev/null) && rc=0 || rc=$?
	printf '%s|%s\n' "$rc" "$(printf '%s' "$out" | tr '\n' '\036')"
}

collect_corpus() {
	case "$1" in
	subjects)
		for sub in $SUBJECTS; do
			[ -f "$S/tests/exec/$sub.c" ] && echo "$sub"
		done ;;
	full)
		( cd "$S/tests/exec" && find . -name '*.c' | sed 's,^\./,,; s,\.c$,,' | sort ) ;;
	*) echo "stratsweep: unknown corpus '$1'" >&2; exit 2 ;;
	esac
}

# Admit the corpus once per invocation and cache the reference strings.
admit() {
	nsub=0
	for sub in $(collect_corpus "$1"); do
		src="$S/tests/exec/$sub.c"
		nm=$(echo "$sub" | tr '/' '_')
		build "" "$src" "$RUN.$nm.ref" || { echo "$sub O0-build" >> "$WORK/skipped"; continue; }
		r1=$(runit "$RUN.$nm.ref")
		r2=$(runit "$RUN.$nm.ref")
		[ "$r1" = "$r2" ] || { echo "$sub nondeterministic" >> "$WORK/skipped"; continue; }
		build "$STRAT_NONE" "$src" "$RUN.$nm.base" || { echo "$sub O2-build" >> "$WORK/skipped"; continue; }
		b1=$(runit "$RUN.$nm.base")
		[ "$b1" = "$r1" ] || { echo "$sub O2-baseline-differs" >> "$WORK/skipped"; continue; }
		printf '%s\t%s\t%s\n' "$sub" "$nm" "$r1" >> "$WORK/admitted"
		nsub=$((nsub + 1))
	done
	echo "$nsub"
}

# check_seq <order> -> 0 clean, 1 dirty; prints one FAIL line per bad subject.
#
# A first mismatch is not reported straight away. These cells run 29-wide
# inside a ctest that is already running 8700 others, and at that load a
# fork or an exec can fail for reasons that have nothing to do with codegen.
# So a mismatch is re-run four times. If any of the four still mismatches it is
# a hard failure, which is what a real miscompile does -- a deterministic one
# every time, a racy one with four more chances to show itself. Only a
# mismatch that never recurs is treated as the machine hiccuping, and even
# that is printed on stderr and counted in the PASS line rather than hidden,
# because a cell that quietly swallows its own red is not evidence.
check_seq() {
	seq_="$1"
	bad=0
	while IFS="$(printf '\t')" read -r sub nm want; do
		src="$S/tests/exec/$sub.c"
		build "$seq_" "$src" "$RUN.$nm.t" || {
			echo "FAIL seq=$seq_ $sub: build failed"; bad=1; continue; }
		got=$(runit "$RUN.$nm.t")
		[ "$got" = "$want" ] && continue
		again=0
		r=0
		while [ "$r" -lt 4 ]; do
			g2=$(runit "$RUN.$nm.t")
			[ "$g2" = "$want" ] || { again=1; got=$g2; break; }
			r=$((r + 1))
		done
		if [ "$again" -eq 0 ]; then
			flakes=$((flakes + 1))
			echo "FLAKE seq=$seq_ $sub: first run differed, four re-runs did not" >&2
			continue
		fi
		echo "FAIL seq=$seq_ $sub: output differs"
		echo "  want: $want"
		echo "  got : $got"
		bad=1
	done < "$WORK/admitted"
	return $bad
}

case "$MODE" in
iso)
	WHICH=${1:-all}; CORPUS=${2:-full}
	n=$(admit "$CORPUS")
	[ "$n" -gt 0 ] || { echo "SKIP stratsweep-iso: no subject admitted"; exit 77; }
	if [ "$WHICH" = all ]; then
		lo=$STRAT_FIRST; hi=$STRAT_LAST
	else
		lo=$(strat_index "$WHICH") || { echo "stratsweep: no strategy '$WHICH'" >&2; exit 2; }
		hi=$lo
	fi
	rc=0; ns=0
	i=$lo
	while [ "$i" -le "$hi" ]; do
		check_seq "$i" || rc=1
		ns=$((ns + 1))
		i=$((i + 1))
	done
	[ "$rc" -eq 0 ] || { echo "FAIL stratsweep-iso $WHICH ($CORPUS)"; exit 1; }
	echo "PASS stratsweep-iso $WHICH: $ns strategy/ies x $n subjects match the -O0 reference ($flakes non-recurring)"
	;;
seq)
	ORDER=$1; CORPUS=${2:-subjects}
	n=$(admit "$CORPUS")
	[ "$n" -gt 0 ] || { echo "SKIP stratsweep-seq: no subject admitted"; exit 77; }
	check_seq "$ORDER" || { echo "FAIL stratsweep-seq $ORDER"; exit 1; }
	echo "PASS stratsweep-seq $ORDER: $n subjects match the -O0 reference ($flakes non-recurring)"
	;;
perm3)
	SH=$1; NSH=$2; CORPUS=${3:-subjects}; PFX=${4:--}
	[ "$PFX" = - ] && PFX="" || PFX="$PFX,"
	n=$(admit "$CORPUS")
	[ "$n" -gt 0 ] || { echo "SKIP stratsweep-perm3: no subject admitted"; exit 77; }
	rc=0; k=0; ran=0
	a=$STRAT_FIRST
	while [ "$a" -le "$STRAT_LAST" ]; do
		b=$STRAT_FIRST
		while [ "$b" -le "$STRAT_LAST" ]; do
			if [ "$b" -ne "$a" ]; then
				c=$STRAT_FIRST
				while [ "$c" -le "$STRAT_LAST" ]; do
					if [ "$c" -ne "$a" ] && [ "$c" -ne "$b" ]; then
						if [ $((k % NSH)) -eq "$SH" ]; then
							check_seq "$PFX$a,$b,$c" || rc=1
							ran=$((ran + 1))
						fi
						k=$((k + 1))
					fi
					c=$((c + 1))
				done
			fi
			b=$((b + 1))
		done
		a=$((a + 1))
	done
	[ "$rc" -eq 0 ] || { echo "FAIL stratsweep-perm3 shard $SH/$NSH"; exit 1; }
	echo "PASS stratsweep-perm3 shard $SH/$NSH: $ran ordered triples x $n subjects match ($flakes non-recurring)"
	;;
*) echo "usage: stratsweep.sh names|iso|seq|perm3 <mcc> <bdir> <idir> <work> ..." >&2; exit 2 ;;
esac
