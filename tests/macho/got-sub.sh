#!/bin/sh
# Mach-O GOT and SUBTRACTOR relocations, against real clang-produced objects.
#
# TODO listed both as macOS-reserved. Neither is: clang targets
# x86_64-apple-macos / arm64-apple-macos from Linux and emits genuine GOT_LOAD
# (GOT_LOAD_PAGE21 + GOT_LOAD_PAGEOFF12 on arm64) and SUBTRACTOR entries, which
# is all the loader consumes.
#
# Both were HARD ERRORS before, so a link failure is the OLD behaviour and
# "the link succeeded" proves nothing on its own. Every assertion below is
# arithmetic against llvm-nm:
#   1. the GOT load's own encoding must compute a slot address inside __got,
#      and that slot must CONTAIN the address nm reports for the target symbol
#      -- an unrelocated or misdirected slot fails this even though the binary
#      links and disassembles plausibly
#   2. the SUBTRACTOR fields must hold exactly nm(_f) - nm(_d), at both 4 and 8
#      bytes; a difference is invariant under load address, so a wrong answer
#      here is a wrong answer everywhere
#   3. a SUBTRACTOR whose subtrahend is NOT in the relocated section must be
#      REFUSED. That case cannot be expressed as a single ELF relocation, and
#      emitting the nearest one produces a binary that links and holds the
#      wrong number.
#
# Skips unless clang can target the triple and llvm-objdump/llvm-nm exist.
#
# Usage: got-sub.sh <macho-mcc> <mccbase> <workdir> <arch>
set -e

MCC=$1
BASE=$2
WORK=$3
ARCH=$4
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$WORK" ] && [ -n "$ARCH" ] || {
	echo "usage: got-sub.sh <macho-mcc> <mccbase> <workdir> <arch>" >&2
	exit 2
}

[ -x "$MCC" ] || { echo "SKIP: no MACHO-target mcc at $MCC"; exit 77; }
for t in clang llvm-objdump llvm-nm python3; do
	command -v $t >/dev/null 2>&1 || { echo "SKIP: $t not found"; exit 77; }
done

TRIPLE=$ARCH-apple-macos11
rm -rf "$WORK"
mkdir -p "$WORK"

cat >"$WORK/foreign.c" <<'EOF'
extern int ext_var;
extern int ext_fn(int);
int get(void) { return ext_var + ext_fn(1); }
EOF
cat >"$WORK/native.c" <<'EOF'
extern int get(void);
extern int delta32;
int ext_var = 41;
int ext_fn(int x) { return x + 1; }
int main(void) { return get() + delta32; }
EOF
cat >"$WORK/sub.s" <<'EOF'
	.section __TEXT,__text
	.globl _f
_f:
	ret
	.section __DATA,__data
	.globl _d
_d:
	.long 0
	.globl _delta32
_delta32:
	.long _f - _d
	.globl _delta64
_delta64:
	.quad _f - _d
EOF
# The subtrahend (_d) lives in __DATA while the field lives in __TEXT, so the
# difference is not a section-internal constant the assembler can fold.
cat >"$WORK/cross.s" <<'EOF'
	.section __DATA,__data
	.globl _dd
_dd:
	.long 0
	.section __TEXT,__text
	.globl _ff
_ff:
	ret
	.globl _cross
_cross:
	.long _ff - _dd
EOF

clang -target "$TRIPLE" -c -O1 "$WORK/foreign.c" -o "$WORK/foreign.o" 2>/dev/null || {
	echo "SKIP: this clang cannot target $TRIPLE"
	exit 77
}
clang -target "$TRIPLE" -c "$WORK/sub.s" -o "$WORK/sub.o"
clang -target "$TRIPLE" -c "$WORK/cross.s" -o "$WORK/cross.o"

llvm-objdump --macho -r "$WORK/foreign.o" | grep -q 'GOT' || {
	echo "FAIL: clang emitted no GOT relocation; this test would be vacuous"
	exit 1
}
nsub=$(llvm-objdump --macho -r "$WORK/sub.o" | grep -c 'SUB' || true)
[ "$nsub" -ge 2 ] || {
	echo "FAIL: clang emitted $nsub SUBTRACTOR entries, expected at least 2;"
	echo "  this test would be vacuous"
	exit 1
}
llvm-objdump --macho -r "$WORK/cross.o" | grep -q 'SUB' || {
	echo "FAIL: the cross-section case produced no SUBTRACTOR, so the refusal"
	echo "  assertion below would be vacuous"
	exit 1
}

"$MCC" -B"$BASE" -c "$WORK/native.c" -o "$WORK/native.o"
"$MCC" -B"$BASE" -nostdlib "$WORK/native.o" "$WORK/foreign.o" "$WORK/sub.o" \
	-o "$WORK/out" 2>"$WORK/err" || {
	echo "FAIL: link against GOT/SUBTRACTOR relocations failed:"
	sed 's/^/  /' "$WORK/err" | head -5
	exit 1
}

rc=0
python3 - "$WORK/out" "$ARCH" <<'PY' || rc=1
import re, subprocess, sys

out, arch = sys.argv[1], sys.argv[2]


def run(*a):
    return subprocess.run(a, capture_output=True, text=True).stdout


def syms():
    d = {}
    for line in run("llvm-nm", "--numeric-sort", out).splitlines():
        f = line.split()
        if len(f) >= 3:
            d[f[2]] = int(f[0], 16)
    return d


def contents(sect):
    """vaddr -> byte, from llvm-objdump's hex dump of one section.

    Only the first of the two dumps llvm-objdump prints is byte-ordered; the
    second ("Contents of (SEG,SECT) section") prints 32-bit words numerically,
    so parsing both silently reverses every group of four bytes.
    """
    txt = run("llvm-objdump", "--macho", "-s", "-j", sect, out)
    body = txt.split("Contents of section ", 1)
    if len(body) < 2:
        return {}
    body = body[1].split("Contents of (", 1)[0]
    m = {}
    for line in body.splitlines():
        g = re.match(r"^\s*([0-9a-f]{8,16})\s+((?:[0-9a-f]{8}\s+)+)", line)
        if not g:
            continue
        addr = int(g.group(1), 16)
        for word in g.group(2).split():
            for i in range(0, len(word), 2):
                m[addr] = int(word[i:i + 2], 16)
                addr += 1
    return m


def body_of(dis, name):
    """The disassembly lines of one function, so a probe cannot match the
    right instruction shape in the wrong function."""
    i = dis.find("\n%s:\n" % name)
    if i < 0:
        return ""
    rest = dis[i + len(name) + 3:]
    j = re.search(r"^_\w+:$", rest, re.M)
    return rest[:j.start()] if j else rest


def read(mem, addr, n):
    b = bytes(mem[addr + i] for i in range(n))
    return int.from_bytes(b, "little")


sym = syms()
rc = 0

dis = run("llvm-objdump", "--macho", "-d", out)
get = body_of(dis, "_get")
if not get:
    print("FAIL: _get is not in the disassembly")
    sys.exit(1)
if arch == "x86_64":
    m = re.search(r"^([0-9a-f]+):\t((?:[0-9a-f]{2} ?)+)\tmovq\t"
                  r"-?0x[0-9a-f]+\(%rip\)", get, re.M)
    if not m:
        print("FAIL: no rip-relative movq (the GOT load) inside _get")
        sys.exit(1)
    at = int(m.group(1), 16)
    raw = m.group(2).split()
    disp = int.from_bytes(bytes(int(b, 16) for b in raw[-4:]), "little",
                          signed=True)
    slot = at + len(raw) + disp
else:
    m = re.search(r"^([0-9a-f]+):\t[0-9a-f ]+\tadrp\tx(\d+), (-?\d+)", get, re.M)
    if not m:
        print("FAIL: no adrp (the GOT page) inside _get")
        sys.exit(1)
    at = int(m.group(1), 16)
    reg = m.group(2)
    page = (at & ~0xfff) + int(m.group(3)) * 0x1000
    n = re.search(r"^[0-9a-f]+:\t[0-9a-f ]+\tldr\tx\d+, \[x%s(?:, #(0x[0-9a-f]+))?\]"
                  % reg, get[get.index(m.group(0)):], re.M)
    if not n:
        print("FAIL: no ldr through the adrp register (the GOT offset)")
        sys.exit(1)
    slot = page + (int(n.group(1), 16) if n.group(1) else 0)

got = contents("__got")
if slot not in got:
    print("FAIL: the GOT load computes 0x%x, which is not inside __got" % slot)
    sys.exit(1)
have = read(got, slot, 8)
want = sym["_ext_var"]
if have != want:
    print("FAIL: GOT slot 0x%x holds 0x%x but _ext_var is at 0x%x"
          % (slot, have, want))
    rc = 1
else:
    print("PASS: the GOT load reaches slot 0x%x, which holds _ext_var (0x%x)"
          % (slot, want))

data = contents("__data")
want = (sym["_f"] - sym["_d"]) & 0xffffffffffffffff
h32 = read(data, sym["_delta32"], 4)
if h32 != want & 0xffffffff:
    print("FAIL: _delta32 holds 0x%x but _f - _d is 0x%x"
          % (h32, want & 0xffffffff))
    rc = 1
else:
    print("PASS: the 4-byte SUBTRACTOR holds _f - _d (0x%x)" % h32)
h64 = read(data, sym["_delta64"], 8)
if h64 != want:
    print("FAIL: _delta64 holds 0x%x but _f - _d is 0x%x" % (h64, want))
    rc = 1
else:
    print("PASS: the 8-byte SUBTRACTOR holds _f - _d (0x%x)" % h64)

sys.exit(rc)
PY

if "$MCC" -B"$BASE" -nostdlib "$WORK/native.o" "$WORK/foreign.o" "$WORK/sub.o" \
	"$WORK/cross.o" -o "$WORK/out2" 2>"$WORK/err2"; then
	echo "FAIL: a SUBTRACTOR whose subtrahend is in another section linked"
	echo "  cleanly; it cannot be expressed as one ELF relocation, so the"
	echo "  result holds the wrong number"
	rc=1
elif grep -q 'SUBTRACTOR whose subtrahend' "$WORK/err2"; then
	echo "PASS: a cross-section SUBTRACTOR is refused by name"
else
	echo "FAIL: the cross-section SUBTRACTOR failed for the wrong reason:"
	sed 's/^/  /' "$WORK/err2" | head -3
	rc=1
fi

exit $rc
