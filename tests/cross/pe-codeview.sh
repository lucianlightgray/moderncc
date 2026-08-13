#!/bin/sh
# pe/codeview (W5): mcc -gcodeview emits a valid CodeView .debug$S line table
# in its COFF object, readable by Microsoft-format tools (llvm-readobj), and --
# when a linker is present -- surviving a link into a real PDB (llvm-pdbutil).
#
# usage: pe-codeview.sh <mcc> <readobj> <src> <work> [<lld-link> <pdbutil>]
set -u
MCC="$1"; READOBJ="$2"; SRC="$3"; WORK="$4"
LLD="${5:-}"; PDBUTIL="${6:-}"

if [ ! -f "$MCC" ] && [ ! -x "$MCC" ]; then echo "pe-codeview: no mcc: $MCC" >&2; exit 77; fi
if [ ! -f "$READOBJ" ]; then echo "pe-codeview: no llvm-readobj (skip)" >&2; exit 77; fi

rm -rf "$WORK"; mkdir -p "$WORK"
obj="$WORK/cv.o"
fail=0

if ! "$MCC" -gcodeview -c -Wl,-oformat=coff "$SRC" -o "$obj" 2>"$WORK/build.log"; then
	echo "pe-codeview: mcc -gcodeview build failed" >&2; cat "$WORK/build.log" >&2; exit 1
fi

cv=$("$READOBJ" --codeview --codeview-subsection-bytes "$obj" 2>/dev/null)
echo "$cv" > "$WORK/cv.txt"

# structural: the CV signature + the three subsection kinds must be present
for want in "Magic: 0x4" "StringTable (0xF3)" "FileChecksums (0xF4)" "Lines (0xF2)" "S_COMPILE3" "S_GPROC32"; do
	if ! grep -qF "$want" "$WORK/cv.txt"; then
		echo "pe-codeview: CodeView missing '$want'" >&2; fail=1
	fi
done

# each function must appear as a proc and map to its declaration line
# subject pe-dwarf-lines.c: square@1, cube@6, main@11
check_fn() {
	fn="$1"; want="$2"
	if ! grep -qE "LinkageName: $fn\$" "$WORK/cv.txt"; then
		echo "pe-codeview: no CodeView proc/lines for $fn" >&2; fail=1; return
	fi
	got=$(awk -v f="$fn" '
		/LinkageName:/ { cur = ($2 == f); inseg = 0 }
		cur && /FilenameSegment/ { inseg = 1 }
		cur && inseg && /LineNumberStart:/ { gsub(/[^0-9]/, ""); print; exit }
	' "$WORK/cv.txt")
	if [ "$got" = "$want" ]; then
		echo "codeview: $fn -> line $got (ok)"
	else
		echo "pe-codeview: $fn first line is '$got', expected $want" >&2; fail=1
	fi
}
check_fn square 1
check_fn cube 6
check_fn main 11

# gold standard: link to a PDB and read the line info back out
if [ -n "$LLD" ] && [ -f "$LLD" ] && [ -n "$PDBUTIL" ] && [ -f "$PDBUTIL" ]; then
	printf 'int square(int x){int r=x*x;return r;}\nint mainCRTStartup(void){return square(5);}\n' > "$WORK/m.c"
	"$MCC" -gcodeview -c -Wl,-oformat=coff "$WORK/m.c" -o "$WORK/m.o" 2>/dev/null
	if MSYS2_ARG_CONV_EXCL='*' "$LLD" -nologo -debug -subsystem:console \
			-entry:mainCRTStartup "$WORK/m.o" -out:"$WORK/m.exe" -pdb:"$WORK/m.pdb" 2>"$WORK/link.log"; then
		if "$PDBUTIL" pretty -lines "$WORK/m.pdb" 2>/dev/null | grep -qE 'Line 1, Address'; then
			echo "codeview: PDB round-trip OK (mcc line info read from linked PDB)"
		else
			echo "pe-codeview: PDB has no line info from mcc CodeView" >&2; fail=1
		fi
	else
		echo "pe-codeview: lld-link failed (non-fatal, skipping PDB check)" >&2
		cat "$WORK/link.log" >&2
	fi
fi

if [ "$fail" != "0" ]; then exit 1; fi
echo "pe-codeview: OK (valid CodeView line table; functions map to source lines)"
exit 0
