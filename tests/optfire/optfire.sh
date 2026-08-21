#!/bin/sh
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

norun=${OPTFIRE_NORUN:-0}
MCCFLAGS=${OPTFIRE_MCCFLAGS:-}
RUN=${OPTFIRE_RUN:-}
EXECVIA=${OPTFIRE_EXECVIA:-aot}
case $mode in
counter) LDF=$8 ;;
differ)  LDF=$9 ;;
level)   LDF=$8 ;;
defstate) LDF= ;;
cdelta)  LDF= ;;
esac
[ "$LDF" = "-" ] && LDF=
refout=""
# A failing cell has to explain itself in one run. optfire.sh used to send every
# compiler stderr to /dev/null, so a build-failure mode reported only that a
# build failed -- which on a nightly-only target costs another whole cycle to
# learn why. Echo what was captured, and on DID NOT FIRE echo the sizes too,
# since "byte-identical" and "both empty" look the same from a distance.
ofdiag() {
	if [ -s "$1" ]; then
		echo "  --- compiler output ---"
		sed -e 's/^/  /' "$1" | head -40
	else
		echo "  (compiler produced no output)"
	fi
}
ofsize() {
	echo "  objects: $(wc -c <"$1") B off, $(wc -c <"$2") B on"
}

[ "$mode" = "level" ] && norun=1
[ "$mode" = "defstate" ] && norun=1
[ "$mode" = "cdelta" ] && norun=1
if [ "$norun" != "1" ]; then
	if [ "$EXECVIA" = "run" ]; then
		refout=$($RUN "$MCC" $MCCFLAGS -O0 -run "$SRC" $LDF 2>&1) ||
			{ echo "FAIL $NAME: -O0 -run reference failed"; echo "  output: $refout"; exit 1; }
	else
		$RUN "$MCC" $MCCFLAGS -O0 "$SRC" -o "$ref" $LDF >"$WORK/$NAME.ref.err" 2>&1 ||
			{ echo "FAIL $NAME: -O0 reference build failed"; ofdiag "$WORK/$NAME.ref.err"; exit 1; }
		refout=$("$ref" 2>&1) || { echo "FAIL $NAME: -O0 reference run failed"; echo "  output: $refout"; exit 1; }
	fi
fi

case $mode in
counter)
	COUNTER=$7
	[ -n "$COUNTER" ] || { echo "FAIL $NAME: no counter name given"; exit 2; }
	$RUN "$MCC" $MCCFLAGS "$OLEVEL" --stats=4 -c "$SRC" -o "$WORK/$NAME.o" >"$WORK/$NAME.stats" 2>&1 ||
		{ echo "FAIL $NAME: $OLEVEL --stats compile failed"; ofdiag "$WORK/$NAME.stats"; exit 1; }
	got=$(strip_ansi <"$WORK/$NAME.stats" | grep -oE "(^| )$COUNTER +[0-9]+" | grep -oE '[0-9]+$' | head -1)
	[ -n "$got" ] || { echo "FAIL $NAME: counter '$COUNTER' absent from --stats output"; ofdiag "$WORK/$NAME.stats"; exit 1; }
	[ "$got" -gt 0 ] 2>/dev/null || {
		echo "FAIL $NAME: pass DID NOT FIRE ($COUNTER=$got at $OLEVEL)"
		exit 1
	}
	if [ "$norun" = "1" ]; then
		echo "PASS $NAME: $COUNTER=$got at $OLEVEL (norun: fired-only)"
		exit 0
	fi
	if [ "$EXECVIA" = "run" ]; then
		optout=$($RUN "$MCC" $MCCFLAGS "$OLEVEL" -run "$SRC" $LDF 2>&1) ||
			{ echo "FAIL $NAME: $OLEVEL -run failed"; echo "  output: $optout"; exit 1; }
	else
		$RUN "$MCC" $MCCFLAGS "$OLEVEL" "$SRC" -o "$opt" $LDF >"$WORK/$NAME.opt.err" 2>&1 ||
			{ echo "FAIL $NAME: $OLEVEL build failed"; ofdiag "$WORK/$NAME.opt.err"; exit 1; }
		optout=$("$opt" 2>&1) || { echo "FAIL $NAME: $OLEVEL run failed"; echo "  output: $optout"; exit 1; }
	fi
	[ "$optout" = "$refout" ] || {
		echo "FAIL $NAME: output changed under $OLEVEL"
		echo "  -O0: $refout"
		echo "  $OLEVEL: $optout"
		exit 1
	}
	echo "PASS $NAME: $COUNTER=$got at $OLEVEL, output matches -O0"
	;;
differ)
	GATE=$7
	EXTRA=$8
	[ -n "$GATE" ] || { echo "FAIL $NAME: no gate flag given"; exit 2; }
	[ "$EXTRA" = "-" ] && EXTRA=
	# EXTRA carries both compiler flags (leading -) and the few remaining
	# environment knobs (NAME=VALUE). Gates are flags now, so they must reach
	# the command line; anything still spelled NAME=VALUE stays in the env.
	EFLAGS=; EENV=
	for _x in $EXTRA; do
		case "$_x" in
		-*) EFLAGS="$EFLAGS $_x" ;;
		*)  EENV="$EENV $_x" ;;
		esac
	done
	env $EENV $RUN "$MCC" $MCCFLAGS "$OLEVEL" $EFLAGS "-fno-$GATE" -c "$SRC" -o "$WORK/$NAME.off.o" >"$WORK/$NAME.off.err" 2>&1 ||
		{ echo "FAIL $NAME: gate-off compile failed"; ofdiag "$WORK/$NAME.off.err"; exit 1; }
	env $EENV $RUN "$MCC" $MCCFLAGS "$OLEVEL" $EFLAGS "-f$GATE" -c "$SRC" -o "$WORK/$NAME.on.o" >"$WORK/$NAME.on.err" 2>&1 ||
		{ echo "FAIL $NAME: gate-on compile failed"; ofdiag "$WORK/$NAME.on.err"; exit 1; }
	if cmp -s "$WORK/$NAME.off.o" "$WORK/$NAME.on.o"; then
		echo "FAIL $NAME: pass DID NOT FIRE (-fno-$GATE and -f$GATE objects are byte-identical at $OLEVEL)"
		ofsize "$WORK/$NAME.off.o" "$WORK/$NAME.on.o"
		exit 1
	fi
	if [ "$norun" = "1" ]; then
		echo "PASS $NAME: objects differ with -f/-fno-$GATE at $OLEVEL (norun: fired-only)"
		exit 0
	fi
	for v in 0 1; do
		if [ "$v" = 1 ]; then gflag="-f$GATE"; else gflag="-fno-$GATE"; fi
		if [ "$EXECVIA" = "run" ]; then
			out=$(env $EENV $RUN "$MCC" $MCCFLAGS "$OLEVEL" $EFLAGS "$gflag" -run "$SRC" $LDF 2>&1) ||
				{ echo "FAIL $NAME: $gflag -run failed"; echo "  output: $out"; exit 1; }
		else
			env $EENV $RUN "$MCC" $MCCFLAGS "$OLEVEL" $EFLAGS "$gflag" "$SRC" -o "$opt.$v" $LDF \
				>"$WORK/$NAME.$v.err" 2>&1 ||
				{ echo "FAIL $NAME: $gflag build failed"; ofdiag "$WORK/$NAME.$v.err"; exit 1; }
			out=$("$opt.$v" 2>&1) || { echo "FAIL $NAME: $gflag run failed"; echo "  output: $out"; exit 1; }
		fi
		[ "$out" = "$refout" ] || {
			echo "FAIL $NAME: $gflag output differs from -O0"
			echo "  -O0: $refout"
			echo "  on : $out"
			exit 1
		}
	done
	echo "PASS $NAME: objects differ with -f/-fno-$GATE at $OLEVEL, both outputs match -O0"
	;;
level)
	COUNTER=$7
	SPEC=$8
	[ -n "$COUNTER" ] && [ -n "$SPEC" ] || { echo "FAIL $NAME: level mode needs <counter> <spec>"; exit 2; }
	LDF=
	bad=0
	echo "$SPEC" | tr ',' '\n' | while IFS=: read -r lvl want; do
		$RUN "$MCC" $MCCFLAGS "$lvl" --stats=4 -c "$SRC" -o "$WORK/$NAME.$lvl.o" >"$WORK/$NAME.$lvl.stats" 2>&1 ||
			{ echo "FAIL $NAME: $lvl --stats compile failed"; ofdiag "$WORK/$NAME.$lvl.stats"; exit 1; }
		got=$(strip_ansi <"$WORK/$NAME.$lvl.stats" | grep -oE "(^| )$COUNTER +[0-9]+" | grep -oE '[0-9]+$' | head -1)
		[ -n "$got" ] || got=0
		case $want in
		on)  [ "$got" -gt 0 ] || { echo "FAIL $NAME: expected $COUNTER ON at $lvl, got $got"; ofdiag "$WORK/$NAME.$lvl.stats"; exit 1; } ;;
		off) [ "$got" -eq 0 ] || { echo "FAIL $NAME: expected $COUNTER OFF at $lvl, got $got"; exit 1; } ;;
		*)   echo "FAIL $NAME: bad spec token '$lvl:$want'"; exit 2 ;;
		esac
	done || bad=1
	[ "$bad" -eq 0 ] || exit 1
	echo "PASS $NAME: $COUNTER level map matches $SPEC"
	;;
cdelta)
	COUNTER=$7
	GATE=$8
	EXTRA=$9
	[ "$EXTRA" = "-" ] && EXTRA=
	[ -n "$COUNTER" ] && [ -n "$GATE" ] || { echo "FAIL $NAME: cdelta needs <counter> <flag>"; exit 2; }
	gflag_for() { [ "$1" = 1 ] && echo "-f$GATE" || echo "-fno-$GATE"; }
	cd_read() {
		$RUN "$MCC" $MCCFLAGS "$OLEVEL" $EXTRA "$(gflag_for $1)" --stats=4 -c "$SRC" -o "$WORK/$NAME.$1.o" >"$WORK/$NAME.$1.stats" 2>&1 || return 1
		strip_ansi <"$WORK/$NAME.$1.stats" | grep -oE "(^| )$COUNTER +[0-9]+" | grep -oE '[0-9]+$' | head -1
		return 0
	}
	c0=$(cd_read 0) || { echo "FAIL $NAME: $COUNTER -fno-$GATE compile failed"; ofdiag "$WORK/$NAME.0.stats"; exit 1; }
	c1=$(cd_read 1) || { echo "FAIL $NAME: $COUNTER -f$GATE compile failed"; ofdiag "$WORK/$NAME.1.stats"; exit 1; }
	[ -n "$c0" ] || c0=0
	[ -n "$c1" ] || c1=0
	[ "$c0" -eq 0 ] 2>/dev/null || {
		echo "FAIL $NAME: $COUNTER should be 0 with -fno-$GATE, got $c0"
		exit 1
	}
	[ "$c1" -gt 0 ] 2>/dev/null || {
		echo "FAIL $NAME: pass DID NOT FIRE ($COUNTER=$c1 with -f$GATE) at $OLEVEL"
		exit 1
	}
	echo "PASS $NAME: $COUNTER 0 -> $c1 when -f$GATE is given at $OLEVEL"
	;;
defstate)
	GATE=$7
	WANT=$8
	DENV=${9:-}
	[ "$DENV" = "-" ] && DENV=
	[ -n "$GATE" ] && [ -n "$WANT" ] || { echo "FAIL $NAME: defstate needs <flag> <on|off>"; exit 2; }
	env $DENV $RUN "$MCC" $MCCFLAGS "$OLEVEL" -c "$SRC" -o "$WORK/$NAME.def.o" \
		>"$WORK/$NAME.def.err" 2>&1 ||
		{ echo "FAIL $NAME: default compile failed"; ofdiag "$WORK/$NAME.def.err"; exit 1; }
	env $DENV $RUN "$MCC" $MCCFLAGS "$OLEVEL" "-fno-$GATE" -c "$SRC" -o "$WORK/$NAME.zero.o" \
		>"$WORK/$NAME.zero.err" 2>&1 ||
		{ echo "FAIL $NAME: -fno-$GATE compile failed"; ofdiag "$WORK/$NAME.zero.err"; exit 1; }
	if cmp -s "$WORK/$NAME.def.o" "$WORK/$NAME.zero.o"; then
		got=off
	else
		got=on
	fi
	[ "$got" = "$WANT" ] || {
		echo "FAIL $NAME: -f$GATE is default-$got at $OLEVEL, expected default-$WANT"
		exit 1
	}
	echo "PASS $NAME: -f$GATE is default-$got at $OLEVEL"
	;;
*)
	echo "unknown mode: $mode" >&2
	exit 2
	;;
esac
