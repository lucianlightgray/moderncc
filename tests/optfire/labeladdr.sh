#!/bin/sh
set -u

MCC=${1:-}
BDIR=${2:-}
IDIR=${3:-}
WORK=${4:-}
SRC=${5:-}
MODE=${6:-}

[ -n "$MCC" ] && [ -n "$BDIR" ] && [ -n "$IDIR" ] && [ -n "$WORK" ] && [ -n "$SRC" ] || {
	echo "usage: labeladdr.sh <mcc> <bdir> <idir> <work> <src.c> [known-positive]" >&2
	exit 2
}
[ -x "$MCC" ] || { echo "SKIP labeladdr: no mcc at $MCC"; exit 77; }
[ -f "$SRC" ] || { echo "SKIP labeladdr: no subject at $SRC"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"

KPDEF=-DLABELADDR_KP=0
if [ "$MODE" = known-positive ]; then
	KPDEF=-DLABELADDR_KP=1
fi

if ! "$MCC" -B"$BDIR" -I"$IDIR" -w -O0 -DLABELADDR_KP=0 "$SRC" -o "$WORK/ref"; then
	echo "FAIL labeladdr: the -O0 reference did not build"
	exit 1
fi
ref=$(set +e; "$WORK/ref" 2>&1; printf '[rc=%s]' "$?")
case "$ref" in
*"[rc=0]") ;;
*)
	echo "FAIL labeladdr: the -O0 reference does not compute its own answer"
	echo "  got: $ref"
	exit 1
	;;
esac

WANT=8
ran=0
diffs=0
rc=0

check() {
	ran=$((ran + 1))
	if ! "$MCC" -B"$BDIR" -I"$IDIR" -w "$@" $KPDEF "$SRC" -o "$WORK/t$ran"; then
		echo "FAIL labeladdr [$*]: the subject did not build"
		rc=1
		return
	fi
	got=$(set +e; "$WORK/t$ran" 2>&1; printf '[rc=%s]' "$?")
	[ "$got" = "$ref" ] && return
	diffs=$((diffs + 1))
	echo "DIFF labeladdr [$*]"
	echo "  want: $ref"
	echo "  got : $got"
}

check -O1
check -O2
check -O3
check -Os
check -O2 -fjit-splice
check -O3 -fjit-splice
check -O2 -fno-inline
check -O2 -fjit-splice -fno-inline

[ "$ran" -eq "$WANT" ] || {
	echo "FAIL labeladdr: $ran of $WANT configurations ran"
	exit 1
}

if [ "$MODE" = known-positive ]; then
	[ "$diffs" -gt 0 ] || {
		echo "FAIL labeladdr-known-positive: all $ran configurations matched the -O0"
		echo "  reference while the subject was built with $KPDEF, which changes what"
		echo "  it prints -- this cell is comparing nothing"
		exit 1
	}
	echo "PASS labeladdr-known-positive: the skewed subject differs in $diffs of $ran configurations"
	exit 0
fi

[ "$rc" -eq 0 ] || exit 1
[ "$diffs" -eq 0 ] || {
	echo "FAIL labeladdr: $diffs of $ran optimized configurations disagree with -O0"
	exit 1
}
echo "PASS labeladdr: $ran optimized configurations compute the -O0 answer"
