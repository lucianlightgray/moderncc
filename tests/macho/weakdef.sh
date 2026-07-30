#!/bin/sh
# Mach-O weak-definition encoding for -fc99-inline-body.
#
# The flag makes each TU emit a weak out-of-line copy of a plain-`inline`
# function, to be collapsed at link. Two independent code paths in mccmacho.c
# have to agree about that symbol: the export trie
# (EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION) and the symtab (N_EXT | N_WEAK_DEF).
# Only the trie half was ever right -- the symtab half emitted the symbol as
# non-external, which nm, dsymutil and every debugger read as private and which
# cannot coalesce. The image still ran, so no other cell noticed.
#
# N_WEAK_REF is checked too: on a DEFINITION it means weak_def_can_be_hidden,
# so setting it unconditionally told the linker it could hide a symbol the user
# made weak on purpose. clang emits plain `weak external` here; so does mcc now.
#
# Usage: weakdef.sh <mcc> <srcdir> <workdir> [-B<prefix>]
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

# Guard against a silent pass on a non-Mach-O output: an ELF image would make
# every assertion below vacuous.
python3 - "$EXE" <<'EOF' || { echo "SKIP: linked output is not a 64-bit Mach-O"; exit 77; }
import struct, sys
sys.exit(0 if struct.unpack_from("<I", open(sys.argv[1], "rb").read(4))[0] == 0xFEEDFACF else 1)
EOF

python3 "$SRC/tests/macho/weakdef.py" "$EXE" _c99inline_add3 weakdef
python3 "$SRC/tests/macho/weakdef.py" "$EXE" _main global

# The out-of-line body must also still be the thing that runs: the 2-TU fixture
# exits non-zero on a wrong value and on a &add3 that did not collapse.
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
