#!/bin/sh
set -e

MCC=$1
CORPUS=$2
shift 2
[ -n "$MCC" ] && [ -n "$CORPUS" ] || {
	echo "usage: treecheck.sh <mcc> <corpus> [olevel ...]" >&2
	exit 2
}
[ $# -gt 0 ] || set -- -O2

rc=0
for opt in "$@"; do
	bad=0
	files=0
	for src in $(find "$CORPUS" -name '*.c' ! -name runner.c | sort); do
		out=$(MCC_AST_TREECHK=1 "$MCC" -w "$opt" -c "$src" -o /dev/null 2>&1 |
			sed 's/\x1b\[[0-9;]*[A-Za-z]//g' | grep '(FAITHFUL)' || true)
		[ -z "$out" ] && continue
		files=$((files + 1))
		bad=$((bad + $(printf '%s\n' "$out" | wc -l)))
		echo "$out" | sed 's/^/  /'
	done
	if [ "$bad" -ne 0 ]; then
		echo "FAIL $opt: $bad tree violations in FAITHFUL bodies across $files files"
		echo "  a pass may now be transforming an invalid model; see TODO F3a"
		rc=1
	else
		echo "PASS $opt: no tree violations in any FAITHFUL body"
	fi
done
exit $rc
