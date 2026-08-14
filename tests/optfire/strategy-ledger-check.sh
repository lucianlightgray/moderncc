#!/bin/sh
# Gate tools/strategy-ledger.sh against tests/optfire/strategy-ledger.txt.
#
# The tool has published "which strategies fire" since it was written and was
# registered nowhere, so a strategy that stopped firing said nothing. This is
# the ratchet: every banked FIRES row must still fire.
#
# One-directional. A banked strategy going dark fails. A dark strategy starting
# to fire does not -- that is the direction the project wants -- but it is
# reported, because the floor should then be re-taken.
set -eu

LEDGER=$1
BDIR=$2
BANK=$3
WORK=$4

if [ ! -x "$LEDGER" ] && [ ! -f "$LEDGER" ]; then
	echo "SKIP: no $LEDGER"
	exit 77
fi
if [ ! -f "$BANK" ]; then
	echo "FAIL strategy-ledger: no bank at $BANK"
	exit 1
fi

mkdir -p "$WORK"
out=$WORK/ledger.txt
if ! sh "$LEDGER" "$BDIR" -O2 -O3 >"$out" 2>&1; then
	echo "FAIL strategy-ledger: the ledger itself did not run"
	sed -e 's/^/  /' "$out" | head -20
	exit 1
fi

# Anti-vacuity: the tool must have produced a partition at all. An empty run
# would otherwise satisfy every "must still fire" check by vacuous truth in the
# other direction -- no, worse: it would fail them all. Either way, say so.
nfire=$(grep -c '^  FIRES ' "$out" || true)
if [ "${nfire:-0}" -lt 1 ]; then
	echo "FAIL strategy-ledger: the ledger reported no firing strategy at all,"
	echo "  so it measured nothing. Output:"
	sed -e 's/^/  /' "$out" | head -20
	exit 1
fi

lvl=
rc=0
missing=
# Re-emit the run as "<level> <strategy>" lines so the bank can be compared
# without caring about column widths.
: >"$WORK/now.txt"
while IFS= read -r line; do
	case $line in
	"=== -O"*) lvl=${line#=== -} ;;
	"  FIRES "*)
		set -- $line
		printf '%s %s\n' "$lvl" "$2" >>"$WORK/now.txt"
		;;
	esac
done <"$out"

while IFS= read -r want; do
	case $want in
	'#'* | '') continue ;;
	esac
	if ! grep -qx "$want" "$WORK/now.txt"; then
		missing="$missing $want"
		rc=1
	fi
done <"$BANK"

if [ "$rc" -ne 0 ]; then
	echo "FAIL strategy-ledger: banked strategy/ies no longer fire:$missing"
	echo "  A strategy that used to fire and now does not is either an"
	echo "  optimizer regression or a corpus that stopped reaching it. Both"
	echo "  want a diagnosis rather than a re-bank."
	exit 1
fi

# New arrivals are reported, never failed.
extra=
while IFS= read -r got; do
	if ! grep -qx "$got" "$BANK"; then
		extra="$extra $got"
	fi
done <"$WORK/now.txt"
if [ -n "$extra" ]; then
	echo "strategy-ledger: NEW strategy/ies now fire:$extra"
	echo "  Not a failure -- re-take the bank so the floor includes them."
fi

echo "strategy-ledger: OK ($(wc -l <"$WORK/now.txt") firing row(s) over -O2 and -O3, all banked rows still fire)"
exit 0
