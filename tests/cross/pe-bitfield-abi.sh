#!/bin/sh
# pe/bitfield-abi (T-win-50015): a struct with bit-fields written by an mcc TU
# and read by a mingw-gcc TU must agree on layout. Both Windows ABIs (MSVC and
# mingw-GNU) lay plain bit-field structs with the MS algorithm; mcc's win32
# default must match or every cross-TU bit-field struct is silently
# incompatible with every native compiler.
#
# usage: pe-bitfield-abi.sh <mcc> <gcc> <src> <work>
set -u
MCC="$1"; GCC="$2"; SRC="$3"; WORK="$4"

if [ ! -f "$MCC" ] && [ ! -x "$MCC" ]; then echo "pe-bitfield-abi: no mcc: $MCC" >&2; exit 77; fi
if [ ! -f "$GCC" ]; then echo "pe-bitfield-abi: no mingw gcc (skip)" >&2; exit 77; fi

rm -rf "$WORK"; mkdir -p "$WORK"

"$GCC" -w -DBFABI_WRITER -c "$SRC" -o "$WORK/writer-ref.o" 2>"$WORK/g1.log" \
	|| { echo "pe-bitfield-abi: reference writer build failed" >&2; cat "$WORK/g1.log" >&2; exit 1; }
"$GCC" -w "$SRC" "$WORK/writer-ref.o" -o "$WORK/ref.exe" 2>"$WORK/g2.log" \
	|| { echo "pe-bitfield-abi: reference link failed" >&2; cat "$WORK/g2.log" >&2; exit 1; }
"$WORK/ref.exe" >"$WORK/ref.out" 2>&1 \
	|| { echo "pe-bitfield-abi: the all-reference control failed; the fixture itself is broken" >&2; cat "$WORK/ref.out" >&2; exit 1; }

"$MCC" -w -DBFABI_WRITER -c "$SRC" -o "$WORK/writer-mcc.o" 2>"$WORK/m1.log" \
	|| { echo "pe-bitfield-abi: mcc writer build failed" >&2; cat "$WORK/m1.log" >&2; exit 1; }
"$GCC" -w "$SRC" "$WORK/writer-mcc.o" -o "$WORK/cross.exe" 2>"$WORK/g3.log" \
	|| { echo "pe-bitfield-abi: cross link failed" >&2; cat "$WORK/g3.log" >&2; exit 1; }
if ! "$WORK/cross.exe" >"$WORK/cross.out" 2>&1; then
	echo "pe-bitfield-abi: an mcc-written struct read back scrambled by the reference reader" >&2
	cat "$WORK/cross.out" >&2
	exit 1
fi

cat "$WORK/cross.out"
echo "pe-bitfield-abi: OK (mcc writer, mingw reader, layouts agree)"
exit 0
