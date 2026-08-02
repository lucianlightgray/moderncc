#!/bin/sh
set -e

MCC=$1
SRC=$2
WORK=$3
BFLAG=$4
[ -n "$MCC" ] && [ -n "$SRC" ] && [ -n "$WORK" ] || {
	echo "usage: weakdef.sh <mcc> <srcdir> <workdir> [-B<prefix>]" >&2
	exit 2
}

[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
command -v python3 >/dev/null 2>&1 || { echo "SKIP: python3 not found"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"

A="$SRC/tests/behavior/c99inline_a.c"
B="$SRC/tests/behavior/c99inline_b.c"
[ -f "$A" ] && [ -f "$B" ] || { echo "SKIP: c99inline fixtures missing"; exit 77; }

EXE="$WORK/c99inline_weakdef"
"$MCC" $BFLAG -fc99-inline-body "$A" "$B" -o "$EXE" || {
	echo "SKIP: this mcc cannot link a Mach-O image here"
	exit 77
}

python3 - "$EXE" <<'EOF' || { echo "SKIP: linked output is not a 64-bit Mach-O"; exit 77; }
import struct, sys
sys.exit(0 if struct.unpack_from("<I", open(sys.argv[1], "rb").read(4))[0] == 0xFEEDFACF else 1)
EOF

python3 "$SRC/tests/macho/weakdef.py" "$EXE" _c99inline_add3 weakdef
python3 "$SRC/tests/macho/weakdef.py" "$EXE" _main global

if "$EXE" >"$WORK/out.txt" 2>&1; then
	[ "$(cat "$WORK/out.txt")" = "9 17 35" ] || {
		echo "FAIL: wrong output: $(cat "$WORK/out.txt")" >&2
		exit 1
	}
	echo "ok: linked image runs, both TUs' &add3 collapsed"
else
	rc=$?
	echo "SKIP: cannot execute the linked image here (exit $rc)"
	exit 77
fi

echo "macho-weakdef: OK"
