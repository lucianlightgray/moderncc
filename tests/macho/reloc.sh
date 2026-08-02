#!/bin/sh
set -e

MCC=$1
BASE=$2
WORK=$3
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$WORK" ] || {
	echo "usage: reloc.sh <macho-mcc> <mccbase> <workdir>" >&2
	exit 2
}

[ -x "$MCC" ] || { echo "SKIP: no MACHO-target mcc at $MCC"; exit 77; }
for t in clang llvm-objdump llvm-nm; do
	command -v $t >/dev/null 2>&1 || { echo "SKIP: $t not found"; exit 77; }
done

rm -rf "$WORK"
mkdir -p "$WORK"

cat >"$WORK/foreign.c" <<'EOF'
extern int other(int);
static const char msg[] = "hi";
int fn(int a) { return other(a) + (int)msg[0]; }
EOF
cat >"$WORK/native.c" <<'EOF'
extern int fn(int);
int other(int a) { return a * 2; }
int main(void) { return fn(3); }
EOF

clang -target x86_64-apple-macos11 -c "$WORK/foreign.c" -o "$WORK/foreign.o" 2>/dev/null || {
	echo "SKIP: this clang cannot target x86_64-apple-macos"
	exit 77
}

llvm-objdump --macho -r "$WORK/foreign.o" 2>/dev/null | grep -qE 'BRANCH|SIGNED' || {
	echo "FAIL: clang produced no BRANCH/SIGNED relocations; this test would be"
	echo "  vacuous -- check the clang version or the source above"
	exit 1
}

"$MCC" -B"$BASE" -c "$WORK/native.c" -o "$WORK/native.o"
"$MCC" -B"$BASE" -nostdlib "$WORK/native.o" "$WORK/foreign.o" -o "$WORK/out" 2>"$WORK/err" || {
	echo "FAIL: link against a relocated Mach-O object failed:"
	sed 's/^/  /' "$WORK/err" | head -3
	exit 1
}

llvm-objdump --macho -d "$WORK/out" >"$WORK/dis" 2>/dev/null
rc=0

if grep -q 'callq.*_other' "$WORK/dis"; then
	echo "PASS: the BRANCH relocation resolves to _other"
else
	echo "FAIL: no 'callq _other' in the output -- the branch never resolved"
	rc=1
fi

if grep -qE 'e8 00 00 00 00' "$WORK/dis"; then
	echo "FAIL: a placeholder 'e8 00 00 00 00' call survives; relocations were"
	echo "  skipped and the binary calls the next instruction"
	rc=1
else
	echo "PASS: no unrelocated placeholder calls remain"
fi

python3 - "$WORK/dis" "$WORK/out" <<'PY' || rc=1
import re, subprocess, sys

dis, out = sys.argv[1], sys.argv[2]
text = open(dis, errors="replace").read()

m = re.search(r"^([0-9a-f]+):[ \t]+((?:[0-9a-f]{2}[ \t])+)[ \t]*movsbl", text, re.M)
if not m:
    print("FAIL: no movsbl (the _msg load) found in the disassembly")
    sys.exit(1)
addr = int(m.group(1), 16)
raw = m.group(2).split()
ln = len(raw)
disp = int.from_bytes(bytes(int(b, 16) for b in raw[-4:]), "little", signed=True)
target = addr + ln + disp

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
if target != want:
    print("FAIL: the SIGNED relocation computes 0x%x but _msg is at 0x%x"
          % (target, want))
    sys.exit(1)
print("PASS: the SIGNED relocation computes 0x%x, matching _msg" % target)
PY

exit $rc
