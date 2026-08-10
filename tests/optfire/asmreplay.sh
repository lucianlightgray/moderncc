#!/bin/sh
set -u

MCC=${1:-}
BDIR=${2:-}
IDIR=${3:-}
WORK=${4:-}
SRC=${5:-}
MODE=${6:-}

[ -n "$MCC" ] && [ -n "$BDIR" ] && [ -n "$IDIR" ] && [ -n "$WORK" ] && [ -n "$SRC" ] || {
	echo "usage: asmreplay.sh <mcc> <bdir> <idir> <work> <src.c> [known-positive]" >&2
	exit 2
}
[ -x "$MCC" ] || { echo "SKIP asmreplay: no mcc at $MCC"; exit 77; }
[ -f "$SRC" ] || { echo "SKIP asmreplay: no subject at $SRC"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"

KPDEF=-DASMREPLAY_KP=0
if [ "$MODE" = known-positive ]; then
	KPDEF=-DASMREPLAY_KP=1
fi

if ! "$MCC" -B"$BDIR" -I"$IDIR" -w -O0 -DASMREPLAY_KP=0 "$SRC" -o "$WORK/ref" \
		>"$WORK/ref.log" 2>&1; then
	echo "FAIL asmreplay: the -O0 reference did not build"
	exit 1
fi
ref=$(set +e; "$WORK/ref" 2>&1; printf '[rc=%s]' "$?")
case "$ref" in
*"skip: not an x86 target"*)
	echo "SKIP asmreplay: the subject's asm is x86-only"
	exit 77
	;;
*"[rc=0]") ;;
*)
	echo "FAIL asmreplay: the -O0 reference does not compute its own answer"
	echo "  got: $ref"
	exit 1
	;;
esac

WANT=7
ran=0
diffs=0
rc=0

check() {
	lbl=$1
	shift
	ran=$((ran + 1))
	if ! env $ENVWORDS "$MCC" -B"$BDIR" -I"$IDIR" -w "$@" $KPDEF "$SRC" -o "$WORK/t$ran" \
			>"$WORK/build$ran.log" 2>&1; then
		echo "FAIL asmreplay [$lbl]: the subject did not build"
		grep -v '^\[rir-' "$WORK/build$ran.log" | head -4 | sed 's/^/  /'
		rc=1
		return
	fi
	got=$(set +e; "$WORK/t$ran" 2>&1; printf '[rc=%s]' "$?")
	[ "$got" = "$ref" ] && return
	diffs=$((diffs + 1))
	echo "DIFF asmreplay [$lbl]"
	echo "  want: $ref"
	echo "  got : $got"
}

ENVWORDS=""
check -O1 -O1
check -O2 -O2
check -O3 -O3
check -Os -Os

ENVWORDS="MCC_REPLAY_IR=1 MCC_RIR_FORCE=1"
check "O0+replay" -O0
check "O1+replay" -O1
ENVWORDS="MCC_REPLAY_IR=3 MCC_RIR_FORCE=1"
check "O0+shiftprobe" -O0
ENVWORDS=""

[ "$ran" -eq "$WANT" ] || {
	echo "FAIL asmreplay: $ran of $WANT configurations ran"
	exit 1
}

if [ "$MODE" = known-positive ]; then
	[ "$diffs" -gt 0 ] || {
		echo "FAIL asmreplay-known-positive: all $ran configurations matched the -O0"
		echo "  reference while the subject was built with $KPDEF, which changes what"
		echo "  it prints -- this cell is comparing nothing"
		exit 1
	}
	echo "PASS asmreplay-known-positive: the skewed subject differs in $diffs of $ran configurations"
	exit 0
fi

[ "$rc" -eq 0 ] || exit 1
[ "$diffs" -eq 0 ] || {
	echo "FAIL asmreplay: $diffs of $ran configurations disagree with -O0"
	exit 1
}
echo "PASS asmreplay: $ran configurations compute the -O0 answer"
