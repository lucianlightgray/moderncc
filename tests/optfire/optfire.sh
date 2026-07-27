#!/bin/sh
# optfire: assert an optimizer pass FIRED, not merely that output was correct.
#
#   counter <mcc> <src.c> <work> <name> <olevel> <counter>
#   differ  <mcc> <src.c> <work> <name> <olevel> <env> <extra-env|->
#   level   <mcc> <src.c> <work> <name> <unused> <counter> <spec>
#
# OPTFIRE_NORUN=1 asserts only that the pass FIRED and skips the -O0 oracle and
# the program runs. That is the cross-compiler mode: a cross mcc cannot link a
# runnable host binary, but the differ assertion is still meaningful there.
#
# level mode locks the -O curation: `spec` is -O1:on,-O2:on,-O3:on,-Os:off and
# each listed level must have the counter nonzero (on) or zero (off) with NO env
# set. It catches a default-on condition silently drifting between -O levels,
# which no other test in the suite would notice.
#
# Every mode also re-checks correctness by requiring the optimized program's
# stdout to equal the -O0 build's stdout, so a pass that fires but miscompiles
# fails here rather than silently passing a fired-only check.
set -e

mode=$1 MCC=$2 SRC=$3 WORK=$4 NAME=$5 OLEVEL=$6
[ -n "$mode" ] && [ -n "$MCC" ] && [ -n "$SRC" ] && [ -n "$WORK" ] || {
	echo "usage: optfire.sh <counter|differ> <mcc> <src> <work> <name> <olevel> ..." >&2
	exit 2
}
mkdir -p "$WORK"
ref=$WORK/$NAME.ref
opt=$WORK/$NAME.opt

strip_ansi() { sed 's/\x1b\[[0-9;]*[A-Za-z]//g'; }

# -O0 reference behaviour: the oracle every mode compares against.
norun=${OPTFIRE_NORUN:-0}
MCCFLAGS=${OPTFIRE_MCCFLAGS:-}
case $mode in
counter) LDF=$8 ;;
differ)  LDF=$9 ;;
level)   LDF=$8 ;;
esac
[ "$LDF" = "-" ] && LDF=
refout=""
if [ "$norun" != "1" ]; then
	# shellcheck disable=SC2086
	"$MCC" $MCCFLAGS -O0 "$SRC" -o "$ref" $LDF >/dev/null 2>&1 || { echo "FAIL $NAME: -O0 reference build failed"; exit 1; }
	refout=$("$ref" 2>&1) || { echo "FAIL $NAME: -O0 reference run failed"; exit 1; }
fi

case $mode in
counter)
	COUNTER=$7
	[ -n "$COUNTER" ] || { echo "FAIL $NAME: no counter name given"; exit 2; }
	got=$("$MCC" $MCCFLAGS "$OLEVEL" --stats=4 -c "$SRC" -o "$WORK/$NAME.o" 2>&1 |
		strip_ansi | grep -oE "(^| )$COUNTER +[0-9]+" | grep -oE '[0-9]+$' | head -1)
	[ -n "$got" ] || { echo "FAIL $NAME: counter '$COUNTER' absent from --stats output"; exit 1; }
	[ "$got" -gt 0 ] 2>/dev/null || {
		echo "FAIL $NAME: pass DID NOT FIRE ($COUNTER=$got at $OLEVEL)"
		exit 1
	}
	# shellcheck disable=SC2086
	if [ "$norun" = "1" ]; then
		echo "PASS $NAME: $COUNTER=$got at $OLEVEL (norun: fired-only)"
		exit 0
	fi
	"$MCC" $MCCFLAGS "$OLEVEL" "$SRC" -o "$opt" $LDF >/dev/null 2>&1 || { echo "FAIL $NAME: $OLEVEL build failed"; exit 1; }
	optout=$("$opt" 2>&1) || { echo "FAIL $NAME: $OLEVEL run failed"; exit 1; }
	[ "$optout" = "$refout" ] || {
		echo "FAIL $NAME: output changed under $OLEVEL"
		echo "  -O0: $refout"
		echo "  $OLEVEL: $optout"
		exit 1
	}
	echo "PASS $NAME: $COUNTER=$got at $OLEVEL, output matches -O0"
	;;
differ)
	ENVV=$7
	EXTRA=$8
	[ -n "$ENVV" ] || { echo "FAIL $NAME: no gate env given"; exit 2; }
	[ "$EXTRA" = "-" ] && EXTRA=
	# shellcheck disable=SC2086
	env $EXTRA "$ENVV=0" "$MCC" $MCCFLAGS "$OLEVEL" -c "$SRC" -o "$WORK/$NAME.off.o" >/dev/null 2>&1 ||
		{ echo "FAIL $NAME: gate-off compile failed"; exit 1; }
	# shellcheck disable=SC2086
	env $EXTRA "$ENVV=1" "$MCC" $MCCFLAGS "$OLEVEL" -c "$SRC" -o "$WORK/$NAME.on.o" >/dev/null 2>&1 ||
		{ echo "FAIL $NAME: gate-on compile failed"; exit 1; }
	if cmp -s "$WORK/$NAME.off.o" "$WORK/$NAME.on.o"; then
		echo "FAIL $NAME: pass DID NOT FIRE ($ENVV=0 and $ENVV=1 objects are byte-identical at $OLEVEL)"
		exit 1
	fi
	if [ "$norun" = "1" ]; then
		echo "PASS $NAME: objects differ with $ENVV toggled at $OLEVEL (norun: fired-only)"
		exit 0
	fi
	for v in 0 1; do
		# shellcheck disable=SC2086
		env $EXTRA "$ENVV=$v" "$MCC" $MCCFLAGS "$OLEVEL" "$SRC" -o "$opt.$v" $LDF >/dev/null 2>&1 ||
			{ echo "FAIL $NAME: $ENVV=$v build failed"; exit 1; }
		out=$("$opt.$v" 2>&1) || { echo "FAIL $NAME: $ENVV=$v run failed"; exit 1; }
		[ "$out" = "$refout" ] || {
			echo "FAIL $NAME: $ENVV=$v output differs from -O0"
			echo "  -O0: $refout"
			echo "  on : $out"
			exit 1
		}
	done
	echo "PASS $NAME: objects differ with $ENVV toggled at $OLEVEL, both outputs match -O0"
	;;
level)
	COUNTER=$7
	SPEC=$8
	[ -n "$COUNTER" ] && [ -n "$SPEC" ] || { echo "FAIL $NAME: level mode needs <counter> <spec>"; exit 2; }
	LDF=
	bad=0
	echo "$SPEC" | tr ',' '\n' | while IFS=: read -r lvl want; do
		got=$("$MCC" $MCCFLAGS "$lvl" --stats=4 -c "$SRC" -o "$WORK/$NAME.$lvl.o" 2>&1 |
			strip_ansi | grep -oE "(^| )$COUNTER +[0-9]+" | grep -oE '[0-9]+$' | head -1)
		[ -n "$got" ] || got=0
		case $want in
		on)  [ "$got" -gt 0 ] || { echo "FAIL $NAME: expected $COUNTER ON at $lvl, got $got"; exit 1; } ;;
		off) [ "$got" -eq 0 ] || { echo "FAIL $NAME: expected $COUNTER OFF at $lvl, got $got"; exit 1; } ;;
		*)   echo "FAIL $NAME: bad spec token '$lvl:$want'"; exit 2 ;;
		esac
	done || bad=1
	[ "$bad" -eq 0 ] || exit 1
	echo "PASS $NAME: $COUNTER level map matches $SPEC"
	;;
*)
	echo "unknown mode: $mode" >&2
	exit 2
	;;
esac
