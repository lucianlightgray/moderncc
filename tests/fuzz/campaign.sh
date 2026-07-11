#!/bin/sh
# Nightly differential-fuzz campaign driver (TODO §0 "CI integration & budget").
# Loops the fuzz runner over fresh seed batches until a wall-clock budget is
# exhausted or K consecutive batches surface no new miscompile *class*
# (attribution = the -O level + MCC_AST_* gate the runner blames). Found repros
# are reduced+saved into the corpus by the runner; this driver dedups their
# attribution classes and enforces the stop rule.
#
# usage: campaign.sh <mcc> <bdir> <idir> <gcc> <clang> <corpus> <work> \
#                    [budget_secs] [batch] [stop_k]
set -eu

runner=${FUZZ_RUNNER:-}
mcc=$1 bdir=$2 idir=$3 gcc=$4 clang=$5 corpus=$6 work=$7
budget=${8:-3600}
batch=${9:-50}
stop_k=${10:-20}

if [ -z "$runner" ]; then
	echo "campaign.sh: set FUZZ_RUNNER to the fuzz_runner binary" >&2
	exit 2
fi

mkdir -p "$corpus" "$work"
classes="$work/seen-classes.txt"
: > "$classes"

start=$(date +%s)
seed=${MCC_FUZZ_SEED:-1000}
empty_streak=0
total_fail=0
round=0

while :; do
	now=$(date +%s)
	elapsed=$((now - start))
	[ "$elapsed" -ge "$budget" ] && { echo "campaign: budget ${budget}s reached"; break; }
	[ "$empty_streak" -ge "$stop_k" ] && {
		echo "campaign: $stop_k consecutive batches with no new class -> converged"
		break
	}

	round=$((round + 1))
	log="$work/round-$round.log"
	rc=0
	"$runner" "$mcc" "$bdir" "$idir" "$work/r$round" "$gcc" "$clang" \
		--seed "$seed" --count "$batch" --gates --corpus "$corpus" >"$log" 2>&1 || rc=$?

	# A miscompile prints "attribution: <opt> <gate>"; the class is that pair.
	new=0
	while IFS= read -r line; do
		key=$(printf '%s\n' "$line" | sed -n 's/^  attribution: //p')
		[ -z "$key" ] && continue
		total_fail=$((total_fail + 1))
		if ! grep -qxF "$key" "$classes"; then
			printf '%s\n' "$key" >> "$classes"
			new=$((new + 1))
			echo "campaign: NEW miscompile class [$key] at round $round (seed base $seed)"
		fi
	done < "$log"

	if [ "$new" -gt 0 ]; then
		empty_streak=0
	else
		empty_streak=$((empty_streak + 1))
	fi
	echo "campaign: round $round seeds ${seed}..$((seed + batch - 1)) rc=$rc new=$new streak=$empty_streak elapsed=${elapsed}s"
	seed=$((seed + batch))
done

nclass=$(wc -l < "$classes" | tr -d ' ')
echo "campaign: done rounds=$round miscompiles=$total_fail distinct-classes=$nclass"
# Fail the job only if a brand-new class (not already in the committed corpus)
# was found, so a nightly run turns red exactly when there is a new bug to fix.
[ "$nclass" -gt 0 ] && exit 1 || exit 0
