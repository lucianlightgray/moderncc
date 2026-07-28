#!/bin/sh
# The AST arena is supposed to be a TREE, but ast_add_child re-parents by
# overwriting parent[child] WITHOUT unlinking the node from its previous
# parent's first_child/next_sib chain. A node can therefore end up reachable
# from two child chains, and replay -- which walks the chains -- emits it once
# per chain. That is the F3a call duplication.
#
# This asserts the property that keeps that defect harmless: a violation may
# exist in a body the recorder REJECTED, but never in one it accepted. Only
# `faithful` bodies are handed to the optimizer passes, and a pass transforming
# an invalid model and re-emitting from it is the one failure mode the always-on
# replay byte-comparison cannot catch. So: zero violations in FAITHFUL bodies.
#
# Measured when this landed: 187 violations over mcc's own TU and 44 over the
# exec corpus, ALL in non-faithful bodies, at -O2/-O3/-Os alike.
#
# Usage: treecheck.sh <mcc> <corpus-dir> [olevel ...]
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
