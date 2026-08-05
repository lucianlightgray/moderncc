#!/bin/sh
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
	for leg in free pinned; do
		hd=$WORK/$n.$leg.home
		rm -rf "$hd"; mkdir -p "$hd"
		if [ "$leg" = pinned ]; then pin=1; else pin=; fi
		if ! env HOME="$hd" \
				"$MCC" ${pin:+-fpromote-locals} -O13 "$src" -o "$WORK/$n.$leg" -lm >/dev/null 2>&1; then
			echo "FAIL $n: -O13 ($leg) build failed"
			rc=1
			continue 2
		fi
	done
	if [ ! -s "$WORK/$n.free" ] || [ ! -s "$WORK/$n.pinned" ]; then
		echo "FAIL $n: missing -O13 output"
		rc=1
		continue
	fi
	objcopy -O binary --only-section=.text "$WORK/$n.free" "$WORK/$n.free.text"
	objcopy -O binary --only-section=.text "$WORK/$n.pinned" "$WORK/$n.pinned.text"
	if cmp -s "$WORK/$n.free.text" "$WORK/$n.pinned.text"; then
		echo "PASS $n: -O13 keeps promotion ($(wc -c < "$WORK/$n.pinned.text") B .text)"
	else
		echo "FAIL $n: -O13 SUBTRACTED promotion -- free .text $(wc -c < "$WORK/$n.free.text") B vs pinned $(wc -c < "$WORK/$n.pinned.text") B"
		echo "  the size-scored search switched MCC_AST_PROMOTE off; see TODO 'Floor the search'"
		rc=1
	fi
done
exit $rc
