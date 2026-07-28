#!/bin/sh
# arm64 Mach-O relocations (ARM64_RELOC_BRANCH26 / PAGE21 / PAGEOFF12).
#
# The arm64 half is harder than x86_64 in two ways, and both are checked here:
#   * arm64 instructions have nowhere to store an addend, so Mach-O either uses
#     the ARM64_RELOC_ADDEND pseudo-entry or folds the constant into the
#     instruction's own immediate. clang does the LATTER for `msg[5]`, which
#     means a LO12 relocation that OVERWRITES the immediate silently loses the
#     +5 and reads the wrong byte. The final assertion below pins exactly that.
#   * PAGEOFF12 does not map to one ELF relocation: it becomes
#     R_AARCH64_ADD_ABS_LO12_NC for an ADD and R_AARCH64_LDST<n>_ABS_LO12_NC for
#     a load/store, with <n> decoded from the instruction's size field.
#
# As with the x86_64 cell, the checks are on RESOLVED TARGETS -- an ignored
# relocation does not fail the link, it leaves a branch to itself.
#
# Usage: reloc-arm64.sh <arm64-macho-mcc> <mccbase> <workdir>
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

# ldrsb is an 8-bit load, so the unsigned-offset immediate scales by 1.
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

# The source reads msg[5]; clang folds the +5 into the ldrsb immediate rather
# than emitting ARM64_RELOC_ADDEND, so a LO12 relocation that overwrites the
# immediate loses it and silently reads msg[0].
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

exit $rc
