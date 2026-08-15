#!/bin/sh
# pe/ms-bitfield-sizing (T-win-50015 gap a): mcc -mms-bitfields must lay out
# structs/unions byte-for-byte with a native Windows compiler. mingw-gcc
# defaults to the MS bit-field layout on a Windows target, so its sizeof output
# is the oracle; we diff mcc's against it. Catches mcc's ms-mode over-sizing a
# zero-width-only union (`union {int :0;}` -> 4 instead of 0).
#
# usage: pe-msbitfield-sizing.sh <mcc> <gcc> <src> <work>
set -u
MCC="$1"; GCC="$2"; SRC="$3"; WORK="$4"

if [ ! -f "$MCC" ] && [ ! -x "$MCC" ]; then echo "ms-bitfield-sizing: no mcc: $MCC" >&2; exit 77; fi
if [ ! -f "$GCC" ]; then echo "ms-bitfield-sizing: no mingw gcc (skip)" >&2; exit 77; fi

rm -rf "$WORK"; mkdir -p "$WORK"

"$GCC" -w "$SRC" -o "$WORK/ref.exe" 2>"$WORK/g.log" \
	|| { echo "ms-bitfield-sizing: reference (mingw) build failed" >&2; cat "$WORK/g.log" >&2; exit 1; }
"$WORK/ref.exe" >"$WORK/ref.out" 2>&1 \
	|| { echo "ms-bitfield-sizing: reference run failed" >&2; cat "$WORK/ref.out" >&2; exit 1; }

"$MCC" -w -mms-bitfields "$SRC" -o "$WORK/mcc.exe" 2>"$WORK/m.log" \
	|| { echo "ms-bitfield-sizing: mcc -mms-bitfields build failed" >&2; cat "$WORK/m.log" >&2; exit 1; }
"$WORK/mcc.exe" >"$WORK/mcc.out" 2>&1 \
	|| { echo "ms-bitfield-sizing: mcc build ran but exited non-zero" >&2; cat "$WORK/mcc.out" >&2; exit 1; }

if ! diff -u "$WORK/ref.out" "$WORK/mcc.out" >"$WORK/diff.txt"; then
	echo "ms-bitfield-sizing: mcc -mms-bitfields sizing disagrees with mingw:" >&2
	echo "  ref(mingw):" >&2; sed 's/^/    /' "$WORK/ref.out" >&2
	echo "  mcc:" >&2; sed 's/^/    /' "$WORK/mcc.out" >&2
	exit 1
fi
echo "PASS: mcc -mms-bitfields sizing matches mingw ($(tr '\n' ' ' <"$WORK/mcc.out"))"
exit 0
