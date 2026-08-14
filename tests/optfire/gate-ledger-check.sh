#!/bin/sh
# Gate tools/gate-ledger.sh against tests/optfire/gate-ledger.txt.
#
# The tool measures, for every MCC_OPT_ROW in src/mccopt.h, whether toggling
# that gate changes the recorded AST, changes only the emitted object, or
# changes nothing at all across tests/exec. It has published that partition
# since it was written and was registered nowhere, so a gate that quietly
# stopped doing anything said nothing.
#
# The banked invariant is the union of FIRES and OBJONLY -- "this gate still
# does something". FIRES vs OBJONLY is not banked separately on purpose: a gate
# legitimately moves between those two classes when what the AST hash covers
# changes, and failing on that would be a flaky cell rather than a finding.
#
# One-directional. A banked gate going dark fails. A dark gate lighting up does
# not -- that is the direction the project wants -- but it is reported so the
# floor can be re-taken.
set -eu

LEDGER=$1
BDIR=$2
BANK=$3
WORK=$4

if [ ! -f "$LEDGER" ]; then
	echo "SKIP: no $LEDGER"
	exit 77
fi
if [ ! -x "$BDIR/mcc" ]; then
	echo "SKIP: no mcc at $BDIR/mcc"
	exit 77
fi
if [ ! -f "$BANK" ]; then
	echo "FAIL gate-ledger: no bank at $BANK"
	exit 1
fi

mkdir -p "$WORK"
out=$WORK/ledger.txt
if ! MCC="$BDIR/mcc" sh "$LEDGER" >"$out" 2>&1; then
	echo "FAIL gate-ledger: the ledger itself did not run"
	sed -e 's/^/  /' "$out" | head -20
	exit 1
fi

grep -E '^(FIRES|OBJONLY)	' "$out" | awk '{print $1" "$2}' | sort -k2 >"$WORK/now.txt"

# Anti-vacuity. An empty or near-empty run would fail every banked row below and
# read as a catastrophic regression, when the real story is that the ledger
# measured nothing. Distinguish the two.
nnow=$(wc -l <"$WORK/now.txt" | tr -d ' ')
if [ "$nnow" -lt 1 ]; then
	echo "FAIL gate-ledger: the ledger reported no active gate at all, so it"
	echo "  measured nothing. Output:"
	sed -e 's/^/  /' "$out" | head -20
	exit 1
fi

# A gate the compiler refused is a gate that was not measured. If a banked gate
# lands there, the honest report is "unmeasurable", not "stopped firing".
nref=$(grep -c '^REFUSED' "$out" || true)
if [ "${nref:-0}" -gt 0 ]; then
	echo "FAIL gate-ledger: $nref gate(s) were refused by the compiler and so"
	echo "  could not be measured. A refused flag compiles nothing, and scoring"
	echo "  that as a difference is exactly the miscount this ledger was fixed"
	echo "  for. Diagnose the refusal rather than re-banking around it:"
	grep '^REFUSED' "$out" | cut -f2- | sed -e 's/^/    /'
	exit 1
fi

rc=0
missing=
while IFS= read -r want; do
	case $want in
	'#'* | '') continue ;;
	esac
	g=${want#* }
	if ! grep -q " $g\$" "$WORK/now.txt"; then
		missing="$missing $g"
		rc=1
	fi
done <"$BANK"

if [ "$rc" -ne 0 ]; then
	echo "FAIL gate-ledger: banked gate(s) no longer change anything:$missing"
	echo "  A gate that used to change the AST or the object and now changes"
	echo "  neither is either an optimization that stopped running or a corpus"
	echo "  that stopped reaching it. Both want a diagnosis, not a re-bank."
	exit 1
fi

extra=
while IFS= read -r got; do
	g=${got#* }
	if ! grep -q " $g\$" "$BANK"; then
		extra="$extra $g"
	fi
done <"$WORK/now.txt"
if [ -n "$extra" ]; then
	echo "gate-ledger: NEW gate(s) now active:$extra"
	echo "  Not a failure -- re-take the bank so the floor includes them."
fi

echo "gate-ledger: OK ($nnow active gate(s) at -O1, all $(grep -c '^[A-Z]' "$BANK") banked gate(s) still active)"
exit 0
