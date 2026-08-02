#!/bin/sh
set -e

MCC=$1
BASE=$2
WORK=$3
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$WORK" ] || {
	echo "usage: archive-reloc.sh <macho-mcc> <mccbase> <workdir>" >&2
	exit 2
}

[ -x "$MCC" ] || { echo "SKIP: no MACHO-target mcc at $MCC"; exit 77; }
for t in clang llvm-ar llvm-objdump llvm-nm; do
	command -v $t >/dev/null 2>&1 || { echo "SKIP: $t not found"; exit 77; }
done

rm -rf "$WORK"
mkdir -p "$WORK"

cat >"$WORK/a.c" <<'EOF'
extern int b_helper(int);
static const char tag[] = "AAAAAAAA";
int a_entry(int v) { return b_helper(v) + (int)tag[3]; }
EOF
cat >"$WORK/b.c" <<'EOF'
extern int c_leaf(int);
static const int tbl[4] = { 10, 20, 30, 40 };
int b_helper(int v) { return c_leaf(v) + tbl[2]; }
EOF
cat >"$WORK/c.c" <<'EOF'
int c_leaf(int v) { return v * 3; }
EOF
cat >"$WORK/unused.c" <<'EOF'
extern int nonexistent_symbol(void);
int never_pulled(void) { return nonexistent_symbol(); }
EOF
cat >"$WORK/main.c" <<'EOF'
extern int a_entry(int);
int main(void) { return a_entry(2); }
EOF

for f in a b c unused; do
	clang -target x86_64-apple-macos11 -c "$WORK/$f.c" -o "$WORK/$f.o" 2>/dev/null || {
		echo "SKIP: this clang cannot target x86_64-apple-macos"
		exit 77
	}
done

llvm-objdump --macho -r "$WORK/a.o" 2>/dev/null | grep -q 'SIGNED' || {
	echo "FAIL: clang emitted no SIGNED relocation in a.o; this cell would be"
	echo "  vacuous -- it exists to cover the non-zero-addend case"
	exit 1
}

llvm-ar rcs "$WORK/lib.a" "$WORK/a.o" "$WORK/b.o" "$WORK/c.o" "$WORK/unused.o"
"$MCC" -B"$BASE" -c "$WORK/main.c" -o "$WORK/main.o"

if ! "$MCC" -B"$BASE" -nostdlib "$WORK/main.o" "$WORK/lib.a" -o "$WORK/out" \
	2>"$WORK/err"; then
	echo "FAIL: linking against the multi-member archive failed:"
	sed 's/^/  /' "$WORK/err" | head -3
	echo "  (if this is an undefined 'nonexistent_symbol', the loader pulled"
	echo "   unused.o unconditionally instead of a la carte)"
	exit 1
fi

rc=0
llvm-objdump --macho -d "$WORK/out" >"$WORK/dis" 2>/dev/null

for s in _a_entry _b_helper _c_leaf; do
	llvm-nm "$WORK/out" 2>/dev/null | grep -qE " (T|t) $s\$" || {
		echo "FAIL: $s was not pulled -- transitive archive resolution stopped early"
		rc=1
	}
done
[ "$rc" -eq 0 ] && echo "PASS: transitive pull a -> b -> c across archive members"

if llvm-nm "$WORK/out" 2>/dev/null | grep -q 'never_pulled'; then
	echo "FAIL: unused.o was pulled despite nothing referencing it"
	rc=1
else
	echo "PASS: unused.o not pulled"
fi

if grep -q 'e8 00 00 00 00' "$WORK/dis"; then
	echo "FAIL: an unrelocated placeholder call survives"
	rc=1
else
	echo "PASS: no unrelocated placeholder calls"
fi

python3 - "$WORK/dis" "$WORK/out" <<'PY' || rc=1
import re, subprocess, sys

dis, out = sys.argv[1], sys.argv[2]
text = open(dis, errors="replace").read()
nm = subprocess.run(["llvm-nm", "--numeric-sort", out],
                    capture_output=True, text=True).stdout

def sym(name):
    for line in nm.splitlines():
        f = line.split()
        if len(f) >= 3 and f[2] == name:
            return int(f[0], 16)
    return None

def riprel(mnemonic):
    m = re.search(r"^([0-9a-f]+):[ \t]+((?:[0-9a-f]{2}[ \t]+)+)" + mnemonic,
                  text, re.M)
    if not m:
        return None
    at = int(m.group(1), 16)
    raw = m.group(2).split()
    disp = int.from_bytes(bytes(int(b, 16) for b in raw[-4:]), "little",
                          signed=True)
    return at + len(raw) + disp

rc = 0
tag, got = sym("_tag"), riprel("movsbl")
if tag is None or got is None:
    print("FAIL: could not locate _tag or its movsbl reference")
    rc = 1
elif got != tag + 3:
    print("FAIL: tag[3] resolves to 0x%x; expected _tag+3 = 0x%x "
          "(addend counted twice?)" % (got, tag + 3))
    rc = 1
else:
    print("PASS: tag[3] resolves to 0x%x = _tag+3" % got)

tbl = sym("_tbl")
if tbl is not None:
    m = [x for x in re.finditer(
        r"^([0-9a-f]+):[ \t]+((?:[0-9a-f]{2}[ \t]+)+)[a-z][a-z0-9]*[ \t]+"
        r"[^\n]*?(?<![0-9a-fx])(-?0x[0-9a-f]+)\(%rip\)", text, re.M)]
    hits = []
    for g in m:
        at = int(g.group(1), 16)
        raw = g.group(2).split()
        disp = int.from_bytes(bytes(int(b, 16) for b in raw[-4:]), "little",
                              signed=True)
        hits.append(at + len(raw) + disp)
    if tbl + 8 in hits:
        print("PASS: tbl[2] resolves to 0x%x = _tbl+8" % (tbl + 8))
    else:
        print("FAIL: no rip-relative load resolves to _tbl+8 = 0x%x; saw %s"
              % (tbl + 8, [hex(h) for h in hits]))
        rc = 1

sys.exit(rc)
PY

exit $rc
