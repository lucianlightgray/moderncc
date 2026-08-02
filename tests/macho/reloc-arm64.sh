#!/bin/sh
set -e

MCC=$1
BASE=$2
WORK=$3
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$WORK" ] || {
	echo "usage: reloc-arm64.sh <arm64-macho-mcc> <mccbase> <workdir>" >&2
	exit 2
}

[ -x "$MCC" ] || { echo "SKIP: no arm64 MACHO-target mcc at $MCC"; exit 77; }
for t in clang llvm-objdump llvm-nm; do
	command -v $t >/dev/null 2>&1 || { echo "SKIP: $t not found"; exit 77; }
done

rm -rf "$WORK"
mkdir -p "$WORK"

cat >"$WORK/foreign.c" <<'EOF'
extern int other(int);
static const char msg[] = "abcdefgh";
int fn(int a) { return other(a) + (int)msg[5]; }
EOF
cat >"$WORK/native.c" <<'EOF'
extern int fn(int);
int other(int a) { return a * 2; }
int main(void) { return fn(3); }
EOF

clang -target arm64-apple-macos11 -c "$WORK/foreign.c" -o "$WORK/foreign.o" 2>/dev/null || {
	echo "SKIP: this clang cannot target arm64-apple-macos"
	exit 77
}

llvm-objdump --macho -r "$WORK/foreign.o" 2>/dev/null | grep -qE 'PAGE21|PAGOF12|BR26' || {
	echo "FAIL: clang produced no PAGE21/PAGOF12/BR26 relocations; this test"
	echo "  would be vacuous"
	exit 1
}

"$MCC" -B"$BASE" -c "$WORK/native.c" -o "$WORK/native.o"
"$MCC" -B"$BASE" -nostdlib "$WORK/native.o" "$WORK/foreign.o" -o "$WORK/out" 2>"$WORK/err" || {
	echo "FAIL: link against a relocated arm64 Mach-O object failed:"
	sed 's/^/  /' "$WORK/err" | head -3
	exit 1
}

llvm-objdump --macho -d "$WORK/out" >"$WORK/dis" 2>/dev/null
rc=0

if grep -qE 'bl[[:space:]]+_other' "$WORK/dis"; then
	echo "PASS: BRANCH26 resolves to _other"
else
	echo "FAIL: no 'bl _other' -- BRANCH26 never resolved"
	rc=1
fi

python3 - "$WORK/dis" "$WORK/out" <<'PY' || rc=1
import re, subprocess, sys

dis, out = sys.argv[1], sys.argv[2]
text = open(dis, errors="replace").read()

def insn(mnemonic):
    m = re.search(r"^([0-9a-f]+):[ \t]+((?:[0-9a-f]{2}[ \t]+){4})" + mnemonic, text, re.M)
    if not m:
        return None, None
    raw = [int(b, 16) for b in m.group(2).split()]
    return int(m.group(1), 16), int.from_bytes(bytes(raw), "little")

adrp_at, adrp = insn("adrp")
ldr_at, ldr = insn("ldrsb")
if adrp is None or ldr is None:
    print("FAIL: could not find the adrp/ldrsb pair in the disassembly")
    sys.exit(1)

immlo = (adrp >> 29) & 3
immhi = (adrp >> 5) & 0x7FFFF
imm = (immhi << 2) | immlo
if imm & (1 << 20):
    imm -= 1 << 21
page = (adrp_at & ~0xFFF) + (imm << 12)

off = (ldr >> 10) & 0xFFF
target = page + off

nm = subprocess.run(["llvm-nm", "--numeric-sort", out],
                    capture_output=True, text=True).stdout
want = None
for line in nm.splitlines():
    f = line.split()
    if len(f) >= 3 and f[2] == "_msg":
        want = int(f[0], 16)
if want is None:
    print("FAIL: _msg is not in the linked symbol table")
    sys.exit(1)

if off == 0:
    print("FAIL: the ldrsb offset is 0, but the source reads msg[5] -- the "
          "PAGEOFF12 relocation overwrote clang's folded immediate")
    sys.exit(1)
if target != want + 5:
    print("FAIL: adrp+ldrsb computes 0x%x; expected _msg+5 = 0x%x"
          % (target, want + 5))
    sys.exit(1)
print("PASS: PAGE21+PAGEOFF12 compute 0x%x, matching _msg+5 (offset %d kept)"
      % (target, off))
PY

cat >"$WORK/tail.c" <<'EOF'
extern int callee(int);
int tailer(int a) { return callee(a + 1); }
EOF
cat >"$WORK/tailmain.c" <<'EOF'
extern int tailer(int);
int callee(int a) { return a * 7; }
int main(void) { return tailer(5) == 42 ? 0 : 3; }
EOF

clang -target arm64-apple-macos11 -O2 -c "$WORK/tail.c" -o "$WORK/tail.o" 2>/dev/null || {
	echo "SKIP: this clang cannot target arm64-apple-macos at -O2"
	exit 77
}

if llvm-objdump --macho -d "$WORK/tail.o" 2>/dev/null |
	grep -qE '[[:space:]]b[[:space:]]+_callee'; then
	"$MCC" -B"$BASE" -c "$WORK/tailmain.c" -o "$WORK/tailmain.o"
	if "$MCC" -B"$BASE" -nostdlib "$WORK/tailmain.o" "$WORK/tail.o" \
		-o "$WORK/tailout" 2>"$WORK/tailerr"; then
		llvm-objdump --macho -d "$WORK/tailout" >"$WORK/taildis" 2>/dev/null
		python3 - "$WORK/taildis" <<'PY' || rc=1
import re, sys

body = []
seen = False
for line in open(sys.argv[1], errors="replace"):
    if line.startswith("_tailer:"):
        seen = True
        continue
    if seen:
        if re.match(r"^_\w+:", line):
            break
        body.append(line)
if not seen:
    print("FAIL: _tailer is not in the linked disassembly")
    sys.exit(1)
text = "".join(body)
mnemonics = []
for line in body:
    cols = line.rstrip("\n").split("\t")
    if len(cols) >= 3 and re.match(r"^[0-9a-f]+:$", cols[0]):
        mnemonics.append(cols[2].split()[0] if cols[2].split() else "")
if "bl" in mnemonics:
    print("FAIL: the sibling call in _tailer linked as 'bl' -- BRANCH26 was "
          "read as CALL26 and the writer rewrote the opcode, so _callee returns "
          "into a frameless _tailer and falls through")
    print(text.rstrip())
    sys.exit(1)
if "b" not in mnemonics:
    print("FAIL: no branch in _tailer at all")
    print(text.rstrip())
    sys.exit(1)
print("PASS: the sibling call stayed a 'b' (opcode preserved through BRANCH26)")
PY
		if [ "$(uname -s)" = "Darwin" ] && [ "$(uname -m)" = "arm64" ]; then
			if "$WORK/tailout"; then
				echo "PASS: the tail-called image runs and returns through the caller"
			else
				echo "FAIL: the tail-call image exited $? (want 0) -- control did not"
				echo "  return through _tailer's caller"
				rc=1
			fi
		fi
	else
		echo "FAIL: linking the sibling-call object failed:"
		sed 's/^/  /' "$WORK/tailerr" | head -3
		rc=1
	fi
else
	echo "SKIP-PART: this clang emitted no sibling call at -O2; the tail-call"
	echo "  assertions below would be vacuous"
fi

exit $rc
