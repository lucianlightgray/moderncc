#!/bin/sh
set -eu

ARCH=$1
MCC=$2
CROSS=$3
SR=$4
CORPUS=$5
shift 5
OPTS="$*"
[ -n "$OPTS" ] || OPTS="-O2"

[ -x "$MCC" ] || { echo "SKIP: no $ARCH mcc at $MCC"; exit 77; }
[ -d "$CORPUS" ] || { echo "SKIP: no corpus at $CORPUS"; exit 77; }

EXTRA=""
case "$ARCH" in
arm|arm-win32) EXTRA="-mfloat-abi hard" ;;
esac
case "$ARCH" in
*-win32)
	R=$(dirname "$CROSS")
	EXTRA="$EXTRA -I$R/runtime/include -I$R/runtime/win32/include -I$R/runtime/win32/include/winapi"
	;;
esac
if [ "$SR" != "-" ]; then
	[ -d "$SR" ] || { echo "SKIP: no $ARCH sysroot at $SR"; exit 77; }
	EXTRA="$EXTRA --sysroot=$SR -I$SR/usr/include"
fi

OBJ=$(mktemp)
trap 'rm -f "$OBJ"' EXIT

n=0
crashed=0
for o in $OPTS; do
	for f in $(find "$CORPUS" -name '*.c' ! -name 'runner.c' | sort); do
		n=$((n + 1))
		rc=0
		"$MCC" -w "$o" -B "$CROSS" $EXTRA -c "$f" -o "$OBJ" >/dev/null 2>&1 || rc=$?
		if [ "$rc" -ge 128 ]; then
			crashed=$((crashed + 1))
			echo "CRASH rc=$rc  $MCC $o $f"
		fi
	done
done

if [ "$n" -eq 0 ]; then
	echo "FAIL: no files were compiled; this check is vacuous"
	exit 1
fi
if [ "$crashed" -ne 0 ]; then
	echo "FAIL: $ARCH crashed the compiler on $crashed of $n compile(s) over $OPTS"
	exit 1
fi
echo "PASS: $ARCH did not crash on any of $n compile(s) over $OPTS"
