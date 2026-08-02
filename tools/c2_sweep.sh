#!/bin/sh
set -e

BUILD=$1
KEY=$2
OPT=${3:--O1}
OUT=${4:-/tmp/c2sweep}

S=$(cd "$(dirname "$0")/.." && pwd)
BUILD=$(cd "$BUILD" && pwd)

case "$KEY" in
x86_64)      MCC=$BUILD/mcc;          FLAGS="" ;;
*win32|*wince)
             MCC=$BUILD/mcc-$KEY
             FLAGS="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include" ;;
*osx)        MCC=$BUILD/mcc-$KEY
             FLAGS="-B $S/runtime -I $S/runtime/include" ;;
arm)         MCC=$BUILD/mcc-arm
             FLAGS="-B $BUILD -mfloat-abi hard --sysroot=$S/vendor/gentoo-stage3-arm-glibc -I$S/vendor/gentoo-stage3-arm-glibc/usr/include" ;;
*)           MCC=$BUILD/mcc-$KEY
             FLAGS="-B $BUILD --sysroot=$S/vendor/gentoo-stage3-$KEY-glibc -I$S/vendor/gentoo-stage3-$KEY-glibc/usr/include" ;;
esac

mkdir -p "$OUT"
LOG=$OUT/$KEY$OPT.log
: > "$LOG"

SOURCE_DATE_EPOCH=1000000000
export SOURCE_DATE_EPOCH

files=$(find "$S/tests/exec" -name '*.c' | sort)
files="$files $S/tests/diff/full_language.c"

nfile=0
for f in $files; do
	nfile=$((nfile + 1))
	xflags=
	case "$f" in
	*/full_language.c) xflags="-I $S" ;;
	esac
	echo "### $f" >> "$LOG"
	MCC_REPLAY_IR=5 "$MCC" -w $OPT $FLAGS $xflags -c -o "$OUT/a-$KEY.o" "$f" >> "$LOG" 2>&1 || true
done

awk -v key="$KEY" -v opt="$OPT" -v nfile="$nfile" '
/^### / { cur = $2; next }
/^\[rir-total\]/ {
	seen++
	for (i = 1; i <= NF; i++) {
		split($i, kv, "=")
		if (kv[1] in want) tot[kv[1]] += kv[2]
	}
	next
}
BEGIN {
	split("fn faithful c2try c2skip c2ok c2bytes c2len c2err c2invalid arenafn arenahasheq", a, " ")
	for (i in a) want[a[i]] = 1
}
END {
	printf "%-14s %-4s files=%d rirfiles=%d fn=%d faithful=%d c2ok=%d/%d (bytes=%d len=%d skip=%d invalid=%d err=%d) arenahasheq=%d/%d\n",
		key, opt, nfile, seen, tot["fn"], tot["faithful"], tot["c2ok"], tot["c2try"],
		tot["c2bytes"], tot["c2len"], tot["c2skip"], tot["c2invalid"], tot["c2err"],
		tot["arenahasheq"], tot["arenafn"]
}
' "$LOG"
