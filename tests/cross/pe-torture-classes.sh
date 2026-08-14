#!/bin/sh
# pe/torture-classes (W2): mcc agrees with a Win64 reference (mingw-gcc) on the
# reconstructed gcc c-torture divergence classes the wt/winspec pilot named.
# The exact upstream corpus (vendor/gcc-c-torture-execute) is not vendored on
# this host; these are faithful reconstructions of the documented classes.
#
# usage: pe-torture-classes.sh <mcc> <gcc> <src> <work>
set -u
MCC="$1"; GCC="$2"; SRC="$3"; WORK="$4"

if [ ! -f "$MCC" ] && [ ! -x "$MCC" ]; then echo "pe-torture-classes: no mcc: $MCC" >&2; exit 77; fi
if [ ! -f "$GCC" ]; then echo "pe-torture-classes: no mingw gcc (skip)" >&2; exit 77; fi

rm -rf "$WORK"; mkdir -p "$WORK"

"$GCC" -w "$SRC" -o "$WORK/ref.exe" 2>"$WORK/g.log" || { echo "pe-torture-classes: reference build failed" >&2; cat "$WORK/g.log" >&2; exit 1; }
ref=$("$WORK/ref.exe" 2>/dev/null)

"$MCC" "$SRC" -o "$WORK/mcc.exe" 2>"$WORK/m.log" || { echo "pe-torture-classes: mcc build failed" >&2; cat "$WORK/m.log" >&2; exit 1; }
got=$("$WORK/mcc.exe" 2>/dev/null)

echo "mingw: $ref"
echo "mcc:   $got"
if [ "$got" != "$ref" ]; then
	echo "pe-torture-classes: mcc diverges from the Win64 reference" >&2
	exit 1
fi
echo "pe-torture-classes: OK (pr92904 varargs / pr23324 bitfields / pr123864 mul-overflow agree)"
exit 0
