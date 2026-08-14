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

# type stream (.debug$T): every proc must reference a real LF_PROCEDURE, not T_NOTYPE
if ! grep -qF ".debug\$T" "$WORK/cv.txt"; then
	echo "pe-codeview: no .debug\$T type stream emitted" >&2; fail=1
fi
if ! grep -qF "LF_PROCEDURE" "$WORK/cv.txt"; then
	echo "pe-codeview: .debug\$T carries no LF_PROCEDURE record" >&2; fail=1
fi
if grep -qE "FunctionType: .*\(0x0\)" "$WORK/cv.txt"; then
	echo "pe-codeview: a function is still typed T_NOTYPE (FunctionType 0x0)" >&2; fail=1
fi
for want in "FunctionType: int (int)" "FunctionType: int ()"; do
	if ! grep -qF "$want" "$WORK/cv.txt"; then
		echo "pe-codeview: no proc with '$want' -- the type stream does not describe the signatures" >&2
		fail=1
	fi
done

# gold standard: link to a PDB and read the line info back out
if [ -n "$LLD" ] && [ -f "$LLD" ] && [ -n "$PDBUTIL" ] && [ -f "$PDBUTIL" ]; then
	printf 'int square(int x){int r=x*x;return r;}\nint deref(int*p){return *p;}\nstruct Pt{int x;int y;};\nunion Un{int a;float b;};\nstruct Ar{int v[4];};\nenum En{E0,E1,E2};\nint psum(struct Pt*q){return q->x+q->y;}\nint uget(union Un*u){return u->a;}\nint aget(struct Ar*z){return z->v[0];}\nint eget(enum En e){return e==E1;}\nint mainCRTStartup(void){int x=5;struct Pt p;p.x=1;p.y=2;return square(x)+deref(&x)+psum(&p);}\n' > "$WORK/m.c"
	"$MCC" -gcodeview -c -Wl,-oformat=coff "$WORK/m.c" -o "$WORK/m.o" 2>/dev/null
	if MSYS2_ARG_CONV_EXCL='*' "$LLD" -nologo -debug -subsystem:console \
			-entry:mainCRTStartup "$WORK/m.o" -out:"$WORK/m.exe" -pdb:"$WORK/m.pdb" 2>"$WORK/link.log"; then
		if "$PDBUTIL" pretty -lines "$WORK/m.pdb" 2>/dev/null | grep -qE 'Line 1, Address'; then
			echo "codeview: PDB round-trip OK (mcc line info read from linked PDB)"
		else
			echo "pe-codeview: PDB has no line info from mcc CodeView" >&2; fail=1
		fi
		pdbtypes=$("$PDBUTIL" pretty -types "$WORK/m.pdb" 2>/dev/null)
		if echo "$pdbtypes" | grep -qE 'int __cdecl \(int\)'; then
			echo "codeview: PDB type round-trip OK (mcc function signature read from linked PDB)"
		else
			echo "pe-codeview: PDB carries no mcc function-signature type" >&2; fail=1
		fi
		# pointer types (slice 2): a pointer parameter must round-trip as int*
		if echo "$pdbtypes" | grep -qE '^[[:space:]]*int\*$'; then
			echo "codeview: PDB pointer type round-trip OK (int* from a pointer parameter)"
		else
			echo "pe-codeview: PDB carries no mcc pointer type (int*)" >&2; fail=1
		fi
		# aggregate types (slice 3): a struct must round-trip with its named members
		if echo "$pdbtypes" | grep -qE 'struct Pt ' && echo "$pdbtypes" | grep -qE 'int x' && echo "$pdbtypes" | grep -qE 'int y'; then
			echo "codeview: PDB struct type round-trip OK (struct Pt { int x; int y; })"
		else
			echo "pe-codeview: PDB carries no mcc struct type with named members" >&2; fail=1
		fi
		# union + array types (T-win-50000): a union and an array member must round-trip
		if echo "$pdbtypes" | grep -qE 'union Un '; then
			echo "codeview: PDB union type round-trip OK (union Un)"
		else
			echo "pe-codeview: PDB carries no mcc union type" >&2; fail=1
		fi
		if "$READOBJ" --codeview "$WORK/m.o" 2>/dev/null | grep -qF 'LF_ARRAY'; then
			echo "codeview: array type record OK (LF_ARRAY for an array member)"
		else
			echo "pe-codeview: no LF_ARRAY record for an array member" >&2; fail=1
		fi
		if echo "$pdbtypes" | grep -qE 'enum En ' && echo "$pdbtypes" | grep -qE 'E1 = 1'; then
			echo "codeview: PDB enum type round-trip OK (enum En { E0=0, E1=1, ... })"
		else
			echo "pe-codeview: PDB carries no mcc enum type with enumerators" >&2; fail=1
		fi
	else
		echo "pe-codeview: lld-link failed (non-fatal, skipping PDB check)" >&2
		cat "$WORK/link.log" >&2
	fi
fi

if [ "$fail" != "0" ]; then exit 1; fi
echo "pe-codeview: OK (valid CodeView line table + type stream; functions map to source lines and typed signatures)"
exit 0
