#!/bin/sh
# Gate tools/c2_sweep.sh against tests/rir/c2-sweep-floor.txt.
#
# c2_sweep.sh compiles every .c under tests/ with MCC_REPLAY_IR=5 and totals the
# per-body Replay-IR counters: how many function bodies the recorder saw (fn),
# how many replayed byte-identically (faithful), how many reached the arena
# (arenafn). It publishes those figures onto docs/TODO.md and was registered
# nowhere, so the population could shrink without anyone hearing about it.
#
# What is gated is the census half. The c2* half is behind the
# MCC_REPLAY_IR_C2 build option, OFF by default, so it is compiled out here --
# see the bank for why banking a zero floor on it would assert nothing. What IS
# asserted about it is that the tool declares which state it is in.
set -eu

TOOL=$1
BDIR=$2
BANK=$3
WORK=$4

if [ ! -f "$TOOL" ]; then
	echo "SKIP: no $TOOL"
	exit 77
fi
if [ ! -x "$BDIR/mcc" ]; then
	echo "SKIP: no mcc at $BDIR/mcc"
	exit 77
fi
if [ ! -f "$BANK" ]; then
	echo "FAIL c2-sweep: no bank at $BANK"
	exit 1
fi

rm -rf "$WORK"
mkdir -p "$WORK"
row=$WORK/row.txt
if ! sh "$TOOL" "$BDIR" x86_64 -O1 "$WORK/out" >"$row" 2>"$WORK/err.txt"; then
	echo "FAIL c2-sweep: the sweep itself did not run"
	sed -e 's/^/  /' "$WORK/err.txt" | head -20
	exit 1
fi

# Not `[ -s ]`: a lone newline is a non-empty file and would slip past, then
# fail later as a confusing pile of "no such field" messages.
if ! grep -q '[^[:space:]]' "$row"; then
	echo "FAIL c2-sweep: the sweep produced no row at all, so it measured nothing"
	sed -e 's/^/  /' "$WORK/err.txt" | head -20
	exit 1
fi
echo "  $(cat "$row")"

rc=0

# The c2 columns must be self-describing. A bare "c2ok=0/0" is ambiguous between
# "the arm ran and proved nothing" and "the arm is not in this binary"; those
# are very different pieces of news and the row has to distinguish them.
c2state=$(sed -n 's/.* c2=\([A-Za-z-]*\).*/\1/p' "$row")
case "$c2state" in
on | COMPILED-OUT) ;;
*)
	echo "FAIL c2-sweep: the row does not declare whether the C2 re-emit arm is"
	echo "  built (expected 'c2=on' or 'c2=COMPILED-OUT', got '${c2state:-<none>}')."
	echo "  Without it the c2ok=0/0 columns cannot be told apart from a measured zero."
	rc=1
	;;
esac
# If the arm IS built, its counters stop being exempt: a built arm that tries
# nothing is the vacuous case all over again.
if [ "$c2state" = on ]; then
	c2try=$(sed -n 's|.*c2ok=[0-9]*/\([0-9]*\).*|\1|p' "$row")
	if [ "${c2try:-0}" -eq 0 ]; then
		echo "FAIL c2-sweep: MCC_REPLAY_IR_C2 is ON but c2try=0, so the arm was built"
		echo "  and then attempted nothing. That is a probe reporting over an empty"
		echo "  population, not a clean result."
		rc=1
	fi
fi

# Floors.
raised=
while read -r k want; do
	case $k in
	'#'* | '') continue ;;
	esac
	got=$(sed -n "s/.*[ (]$k=\([0-9]*\).*/\1/p" "$row")
	if [ -z "$got" ]; then
		echo "FAIL c2-sweep: the row has no $k= field at all; the report format changed"
		rc=1
		continue
	fi
	if [ "$got" -lt "$want" ]; then
		echo "FAIL c2-sweep: $k fell to $got, below the banked floor of $want."
		echo "  Fewer bodies reaching the recorder means the figures this tool"
		echo "  publishes now cover less than the board says. Attribute the drop"
		echo "  rather than re-taking the floor."
		rc=1
	elif [ "$got" -gt "$want" ]; then
		raised="$raised $k=$got(was $want)"
	fi
done <"$BANK"

if [ "$rc" -ne 0 ]; then
	exit 1
fi
if [ -n "$raised" ]; then
	echo "c2-sweep: floors RISEN:$raised"
	echo "  Not a failure -- re-take tests/rir/c2-sweep-floor.txt so the floor holds the gain."
fi

echo "c2-sweep: OK (census floors hold, c2 arm state declared as $c2state)"
exit 0
