#!/bin/sh
set -e

MCC=$1
SRC=$2
WORK=$3
NAME=$4
[ -n "$MCC" ] && [ -n "$SRC" ] && [ -n "$WORK" ] && [ -n "$NAME" ] || {
	echo "usage: run.sh <mcc> <srcdir> <workdir> <name> [-B<prefix>] [ld args]" >&2
	exit 2
}
shift 4

[ "$(uname -s)" = "Darwin" ] || { echo "SKIP: needs a Darwin host"; exit 77; }
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }

CSRC="$SRC/tests/darwin/$NAME.c"
[ -f "$CSRC" ] || { echo "SKIP: no $CSRC"; exit 77; }

mkdir -p "$WORK"
EXE="$WORK/$NAME"
rm -f "$EXE"

"$MCC" "$@" "$CSRC" -o "$EXE"
[ -x "$EXE" ] || { echo "FAIL: mcc produced no $EXE" >&2; exit 1; }

"$EXE"
