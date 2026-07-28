#!/bin/sh
# Mach-O ARCHIVE member loading -- the `--embed-jit` path into libmcc_jitengine.a.
#
# TODO recorded this as "reserved for macOS" on the grounds that GNU ar cannot
# index Mach-O members, so any archive built on Linux is invalid by construction.
# That is true of GNU ar and false of llvm-ar, which writes a correct BSD armap;
# with llvm-ar the whole path is testable on Linux against the synthetic fixture.
#
# Two things had to be true and neither was:
#   * BSD extended names. llvm-ar writes `#1/<len>` headers where the member NAME
#     is stored in the first <len> bytes of the member DATA. Without handling
#     that, the computed data offset points at the filename rather than the
#     Mach-O header -- which is why a previous attempt's hooks "never pulled the
#     member" despite being in the right places.
#   * A BSD symbol index. The a la carte loader only recognized the SysV names
#     (`/`, `/SYM64/`); a `__.SYMDEF` archive matched nothing, walked every
#     member, and silently loaded NOTHING. Note the BSD index is little-endian
#     where the SysV one is big-endian.
#
# Checks, in increasing strength:
#   1. the archive link SUCCEEDS (it used to fail "unresolved reference")
#   2. it is BYTE-IDENTICAL to linking the same member as a bare object
#   3. the pulled member's actual code is present in the output
#   4. an archive whose symbols are unreferenced pulls NOTHING (a la carte is
#      selective, not "load everything")
#
# Skips unless llvm-ar and a MACHO-target mcc are both present.
#
# Usage: archive.sh <macho-mcc> <mccbase> <fixture-script> <workdir>
set -e

MCC=$1
BASE=$2
FIXTURE=$3
WORK=$4
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$FIXTURE" ] && [ -n "$WORK" ] || {
	echo "usage: archive.sh <macho-mcc> <mccbase> <fixture-script> <workdir>" >&2
	exit 2
}

command -v llvm-ar >/dev/null 2>&1 || {
	echo "SKIP: llvm-ar not found (GNU ar cannot index Mach-O members, so it"
	echo "  cannot substitute here)"
	exit 77
}
[ -x "$MCC" ] || { echo "SKIP: no MACHO-target mcc at $MCC"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"

python3 "$FIXTURE" "$WORK/fixture.o" --arch x86_64

cat >"$WORK/usefx.c" <<'EOF'
extern int mcc_fixture_defined(void);
extern int mcc_fixture_undefined(void);
int main(void) { return mcc_fixture_defined() + mcc_fixture_undefined(); }
EOF
cat >"$WORK/prov.c" <<'EOF'
int mcc_fixture_undefined(void) { return 7; }
EOF
cat >"$WORK/unused.c" <<'EOF'
int mcc_unused_thing(void) { return 1; }
EOF

"$MCC" -B"$BASE" -c "$WORK/usefx.c" -o "$WORK/usefx.o"
"$MCC" -B"$BASE" -c "$WORK/prov.c" -o "$WORK/prov.o"
"$MCC" -B"$BASE" -c "$WORK/unused.c" -o "$WORK/unused.o"

llvm-ar rcs "$WORK/fix.a" "$WORK/fixture.o"
llvm-ar rcs "$WORK/unused.a" "$WORK/unused.o"

# The armap must actually name the symbol, or this test proves nothing.
llvm-nm --print-armap "$WORK/fix.a" 2>/dev/null | grep -q '_mcc_fixture_defined in' || {
	echo "FAIL: llvm-ar produced no armap entry for _mcc_fixture_defined; the"
	echo "  archive is not indexed and the rest of this test is vacuous"
	exit 1
}

rc=0

if ! "$MCC" -B"$BASE" -nostdlib "$WORK/usefx.o" "$WORK/fix.a" "$WORK/prov.o" \
	-o "$WORK/out.ar" 2>"$WORK/err.ar"; then
	echo "FAIL: linking against the Mach-O archive failed:"
	sed 's/^/  /' "$WORK/err.ar" | head -3
	exit 1
fi

"$MCC" -B"$BASE" -nostdlib "$WORK/usefx.o" "$WORK/fixture.o" "$WORK/prov.o" \
	-o "$WORK/out.bare" 2>/dev/null

if cmp -s "$WORK/out.bare" "$WORK/out.ar"; then
	echo "PASS: archive link is byte-identical to the bare-object link"
else
	echo "FAIL: archive link differs from the bare-object link"
	cmp "$WORK/out.bare" "$WORK/out.ar" | head -2 | sed 's/^/  /'
	rc=1
fi

if command -v llvm-objdump >/dev/null 2>&1; then
	if llvm-objdump --macho -d "$WORK/out.ar" 2>/dev/null |
		grep -A2 '^_mcc_fixture_defined' | grep -q '0x2a'; then
		echo "PASS: the pulled member's code (mov eax,42) is in the output"
	else
		echo "FAIL: _mcc_fixture_defined's body is not in the output -- the member"
		echo "  was not really pulled, or its section data was dropped"
		rc=1
	fi
fi

# A la carte must be SELECTIVE: an archive nothing references contributes nothing.
if "$MCC" -B"$BASE" -nostdlib "$WORK/usefx.o" "$WORK/fix.a" "$WORK/unused.a" \
	"$WORK/prov.o" -o "$WORK/out.un" 2>/dev/null; then
	if cmp -s "$WORK/out.ar" "$WORK/out.un"; then
		echo "PASS: an unreferenced archive pulled nothing"
	else
		echo "FAIL: adding an archive with no referenced symbol changed the output;"
		echo "  the a la carte loader is pulling members unconditionally"
		rc=1
	fi
else
	echo "FAIL: link with an extra unreferenced archive failed outright"
	rc=1
fi

exit $rc
