#!/bin/sh
set -eu

MCC=$1
CROSS=$2
SR=$3
SRC=$4
OUT=$5

[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
[ -d "$SR" ] || { echo "SKIP: no sysroot at $SR"; exit 77; }

exec "$MCC" -w -O2 -B "$CROSS" "--sysroot=$SR" "-I$SR/usr/include" \
	-c "$SRC" -o "$OUT"
