#!/bin/sh
set -e

BUILD=$1
KEY=$2
OPT=${3:--O1}
OUT=${4:-/tmp/c2sweep}

S=$(cd "$(dirname "$0")/.." && pwd)
BUILD=$(cd "$BUILD" && pwd)

# The native compiler is $BUILD/mcc and carries no sysroot flags, but which key
# that IS depends on the host: x86_64 on an x86_64 Linux host, arm64-osx on an
# arm64 mac, arm64 in a linux/arm64 container. Selecting $BUILD/mcc for a key
# the host is not silently measures the wrong target -- rc stays 0 and the row
# still prints. Take an explicit mcc-<key> whenever one was built, and fall
# back to the native compiler, with no sysroot flags, only when there is none.
case "$KEY" in
*win32|*wince)
             FLAGS="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include" ;;
*osx)        FLAGS="-B $S/runtime -I $S/runtime/include" ;;
arm)         FLAGS="-B $BUILD -mfloat-abi hard -I $S/runtime/include --sysroot=$S/vendor/gentoo-stage3-arm-glibc -I$S/vendor/gentoo-stage3-arm-glibc/usr/include" ;;
*)           FLAGS="-B $BUILD -I $S/runtime/include --sysroot=$S/vendor/gentoo-stage3-$KEY-glibc -I$S/vendor/gentoo-stage3-$KEY-glibc/usr/include" ;;
esac
if [ -x "$BUILD/mcc-$KEY" ]; then
	MCC=$BUILD/mcc-$KEY
else
	MCC=$BUILD/mcc
	FLAGS=
	native=1
fi

mkdir -p "$OUT"
LOG=$OUT/$KEY$OPT.log
: > "$LOG"

SOURCE_DATE_EPOCH=1000000000
export SOURCE_DATE_EPOCH

# ast_replay_env needs optimize >= 1, so a plain -O0 run journals nothing at
# all and would read as a perfect board over an empty population. C2_FORCE=1
# turns capture back on and, with it, every pass gated on "o4 || optimize >= 1"
# -- the same derivation rir_parity.cmake does, by regex over the sources, so a
# renamed gate cannot silently drop out of the set.
FORCEENV=
if [ -n "$C2_FORCE" ]; then
	gates=$(grep -hoE 'ast_env_gate\("MCC_AST_[A-Z0-9_]+", *o4 \|\| s1->optimize >= 1\)' \
			"$S"/src/*.c | sed -E 's/.*"(MCC_AST_[A-Z0-9_]+)".*/\1/' | sort -u)
	ngate=$(printf '%s\n' $gates | grep -c . || true)
	if [ "$ngate" -eq 0 ]; then
		echo "c2_sweep: C2_FORCE derived 0 gates from $S/src -- the ast_env_gate" >&2
		echo "  spelling changed, so this run would measure -O0 with every pass" >&2
		echo "  off and call it parity" >&2
		exit 1
	fi
	FORCEENV="MCC_FORCE_REPLAY=1"
	for g in $gates; do FORCEENV="$FORCEENV $g=1"; done
	echo "c2_sweep: C2_FORCE -- $ngate optimize>=1 gate(s) forced on" >&2
fi

# The corpus. C2_CORPUS names a subtree of tests/ ("exec" for the historical
# exec-only board, "all" -- the default -- for every .c under tests/). Files
# that do not compile are excluded by the rc check below and reported as the
# difference between files= and ok=, so a broader corpus can only add
# population, never partial bodies.
CORPUS=${C2_CORPUS:-all}
case "$CORPUS" in
all) root=$S/tests ;;
/*)  root=$CORPUS ;;
*)   root=$S/tests/$CORPUS ;;
esac
files=$(find "$root" -name '*.c' | sort)
# full_language.c needs its own two flags to compile at all and used to be the
# only file outside tests/exec in the sweep; C2_NO_EXTRA=1 drops it so an
# exec-only run stays comparable with the historical board.
if [ -n "$C2_NO_EXTRA" ]; then
	files=$(printf '%s\n' $files | grep -v '/full_language\.c$')
fi

nfile=0
nok=0
for f in $files; do
	nfile=$((nfile + 1))
	xflags="-I $(dirname "$f")"
	case "$f" in
	*/full_language.c) xflags="$xflags -I $S -DCC_NAME=CC_gcc" ;;
	esac
	if env $FORCEENV MCC_REPLAY_IR=5 "$MCC" -w $OPT $FLAGS $xflags \
			-c -o "$OUT/a-$KEY.o" "$f" > "$OUT/f.log" 2>&1; then
		nok=$((nok + 1))
		echo "### $f" >> "$LOG"
		grep -E '^\[rir-(total|c2|c2op|c2len|c2byte|c2part)' "$OUT/f.log" >> "$LOG" \
			|| true
	else
		echo "!!! rc!=0 $f" >> "$LOG"
	fi
done

awk -v key="$KEY" -v opt="$OPT" -v nfile="$nfile" -v nok="$nok" -v corpus="$CORPUS" -v native="$native" '
/^### / { cur = $2; if (cur ~ /full_language\.c$/) extra = 1; next }
/^\[rir-total\]/ {
	seen++
	for (i = 1; i <= NF; i++) {
		split($i, kv, "=")
		if (kv[1] in want) tot[kv[1]] += kv[2]
	}
	next
}
BEGIN {
	split("fn faithful c2try c2skip c2ok c2bytes c2len c2err c2invalid c2equiv c2unproven arenafn", a, " ")
	for (i in a) want[a[i]] = 1
}
END {
	printf "%-14s %-4s %-5s%s files=%d ok=%d extra=%d rirfiles=%d fn=%d faithful=%d c2ok=%d/%d (bytes=%d len=%d skip=%d invalid=%d err=%d) equiv=%d/%d arenafn=%d\n",
		key, opt, corpus, native ? " NATIVE" : "", nfile, nok, extra + 0, seen, tot["fn"], tot["faithful"], tot["c2ok"], tot["c2try"],
		tot["c2bytes"], tot["c2len"], tot["c2skip"], tot["c2invalid"], tot["c2err"],
		tot["c2equiv"], tot["c2equiv"] + tot["c2unproven"],
		tot["arenafn"]
}
' "$LOG"
