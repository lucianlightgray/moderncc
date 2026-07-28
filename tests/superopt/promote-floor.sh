#!/bin/sh
# The -O4 superopt driver scores candidates by size. Promotion trades size
# (prologue saves) for speed (fewer spills), so a size-scored search will
# happily switch it off -- which is how -O4 came to be SLOWER than -O3 on every
# plb kernel while the -O4 default config was the fastest thing measured.
#
# so_setenv_cfg now treats MCC_AST_PROMOTE as add-only: it may set the gate ON,
# but when the gate bit is clear it restores the compiler's own default instead
# of forcing 0. This asserts that floor holds, deterministically and without
# timing anything: -O4 output must be byte-identical to -O4 with promotion
# pinned on. If the search ever subtracts promotion again, these differ.
#
# Usage: promote-floor.sh <mcc> <workdir> <src.c> [more.c ...]
set -e

MCC=$1
WORK=$2
shift 2
[ -n "$MCC" ] && [ -n "$WORK" ] && [ $# -gt 0 ] || {
	echo "usage: promote-floor.sh <mcc> <work> <src.c> [...]" >&2
	exit 2
}
mkdir -p "$WORK"

rc=0
for src in "$@"; do
	[ -f "$src" ] || { echo "SKIP: $src not present"; continue; }
	n=$(basename "$src" .c)
	# -O4 drives an out-of-process search whose result is cached per input in the
	# user cache dir; a stale checkpoint would make this assert nothing, so both
	# legs run with a private HOME and the cache cleared.
	for leg in free pinned; do
		hd=$WORK/$n.$leg.home
		rm -rf "$hd"; mkdir -p "$hd"
		if [ "$leg" = pinned ]; then pin=1; else pin=; fi
		if ! env HOME="$hd" ${pin:+MCC_AST_PROMOTE=1} \
				"$MCC" -O4 "$src" -o "$WORK/$n.$leg" -lm >/dev/null 2>&1; then
			echo "FAIL $n: -O4 ($leg) build failed"
			rc=1
			continue 2
		fi
	done
	if [ ! -s "$WORK/$n.free" ] || [ ! -s "$WORK/$n.pinned" ]; then
		echo "FAIL $n: missing -O4 output"
		rc=1
		continue
	fi
	# Compare .text only: the two links are otherwise identical, and this is the
	# section the search actually chooses between.
	objcopy -O binary --only-section=.text "$WORK/$n.free" "$WORK/$n.free.text"
	objcopy -O binary --only-section=.text "$WORK/$n.pinned" "$WORK/$n.pinned.text"
	if cmp -s "$WORK/$n.free.text" "$WORK/$n.pinned.text"; then
		echo "PASS $n: -O4 keeps promotion ($(wc -c < "$WORK/$n.pinned.text") B .text)"
	else
		echo "FAIL $n: -O4 SUBTRACTED promotion -- free .text $(wc -c < "$WORK/$n.free.text") B vs pinned $(wc -c < "$WORK/$n.pinned.text") B"
		echo "  the size-scored search switched MCC_AST_PROMOTE off; see TODO 'Floor the search'"
		rc=1
	fi
done
exit $rc
