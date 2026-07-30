#!/bin/sh
set -e
MCC=$1
NOJRN=$2
CORPUS=$3
EXTRA=$4
TMP=$5
OPT=${6:--O1}
mkdir -p "$TMP"

if [ ! -x "$NOJRN" ]; then
	echo "jrn_hooks_identity: $NOJRN missing; SKIP"
	exit 77
fi

printf 'int jrn_probe(int a, int b) { return a * b + 1; }\n' >"$TMP/probe.c"
probe=$(MCC_JOURNAL=1 "$MCC" -w "$OPT" -c -o "$TMP/probe.o" "$TMP/probe.c" 2>&1 >/dev/null |
	grep -c '^\[jrn-verify\]' || true)
if [ "$probe" -eq 0 ]; then
	echo "jrn_hooks_identity: no [jrn-verify] output — reference build has no MCC_JOURNAL_HOOKS; SKIP"
	exit 77
fi
anti=$(MCC_JOURNAL=1 "$NOJRN" -w "$OPT" -c -o "$TMP/probe.o" "$TMP/probe.c" 2>&1 >/dev/null |
	grep -c '^\[jrn-verify\]' || true)
if [ "$anti" -ne 0 ]; then
	echo "jrn_hooks_identity: the -DMCC_JOURNAL_HOOKS=0 build still emits [jrn-verify]"
	exit 1
fi

n=0
bad=0
for f in $(find "$CORPUS" -name '*.c' | sort) "$EXTRA"; do
	"$MCC" -w "$OPT" -c -o "$TMP/a.o" "$f" 2>/dev/null || continue
	"$NOJRN" -w "$OPT" -c -o "$TMP/b.o" "$f" 2>/dev/null || continue
	n=$((n + 1))
	if ! cmp -s "$TMP/a.o" "$TMP/b.o"; then
		bad=$((bad + 1))
		echo "DIFF $f"
	fi
done
echo "jrn_hooks_identity: $OPT compared=$n differing=$bad"
[ "$n" -gt 0 ] || { echo "jrn_hooks_identity: compiled nothing"; exit 1; }
[ "$bad" -eq 0 ] || exit 1
