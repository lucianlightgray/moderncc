#!/bin/sh
set -eu

MCC="$1"
GCC="$2"
WORK="$3"

if [ ! -x "$MCC" ] && [ ! -f "$MCC" ]; then
	echo "coff-obj-diff: mcc not found: $MCC" >&2
	exit 77
fi
if [ ! -f "$GCC" ]; then
	echo "coff-obj-diff: mingw gcc not found: $GCC (skipping)" >&2
	exit 77
fi

rm -rf "$WORK"
mkdir -p "$WORK"

cat > "$WORK/a.c" <<'EOF'
#include <stdio.h>
extern int add3(int, int, int);
extern const char *name(void);
int table[3] = {10, 20, 12};
int main(void) {
	int r = add3(table[0], table[1], table[2]);
	printf("%s r=%d\n", name(), r);
	return r == 42 ? 0 : 1;
}
EOF

cat > "$WORK/b.c" <<'EOF'
static int bias = 0;
int add3(int a, int b, int c) { return a + b + c + bias; }
const char *name(void) { return "moderncc"; }
EOF

magic2() { od -An -tx1 -N2 "$1" | tr -d ' \n'; }

golden="moderncc r=42"

"$GCC" -c "$WORK/a.c" -o "$WORK/a_g.o"
"$GCC" -c "$WORK/b.c" -o "$WORK/b_g.o"
"$MCC" -c "$WORK/a.c" -o "$WORK/a_m.o"
"$MCC" -c "$WORK/b.c" -o "$WORK/b_m.o"

for o in a_m b_m; do
	m=$(magic2 "$WORK/$o.o")
	if [ "$m" != "6486" ]; then
		echo "coff-obj-diff: default -c did not emit COFF x86-64 (magic $m); flip regressed" >&2
		exit 1
	fi
done

"$MCC" -c -Wl,-oformat=coff "$WORK/b.c" -o "$WORK/b_expl.o"
xm=$(magic2 "$WORK/b_expl.o")
if [ "$xm" != "6486" ]; then
	echo "coff-obj-diff: explicit -Wl,-oformat=coff no longer emits COFF (magic $xm)" >&2
	exit 1
fi

fail=0
for pair in "a_g b_g gg" "a_g b_m gm" "a_m b_g mg" "a_m b_m mm"; do
	set -- $pair
	if ! "$GCC" "$WORK/$1.o" "$WORK/$2.o" -o "$WORK/out_$3.exe" 2>"$WORK/link_$3.log"; then
		echo "coff-obj-diff: link failed for $1+$2" >&2
		cat "$WORK/link_$3.log" >&2
		fail=1
		continue
	fi
	out=$("$WORK/out_$3.exe" 2>&1) || true
	if [ "$out" != "$golden" ]; then
		echo "coff-obj-diff: $1+$2 produced '$out', expected '$golden'" >&2
		fail=1
	fi
done

"$MCC" -c -Wl,-oformat=pe-x86-64 "$WORK/b.c" -o "$WORK/b_elf.o"
em=$(magic2 "$WORK/b_elf.o")
if [ "$em" != "7f45" ]; then
	echo "coff-obj-diff: -Wl,-oformat=pe-x86-64 no longer emits ELF (magic $em); regression" >&2
	fail=1
fi
if "$GCC" "$WORK/a_g.o" "$WORK/b_elf.o" -o "$WORK/out_neg.exe" 2>/dev/null; then
	echo "coff-obj-diff: mingw linked an mcc ELF object; negative control is blind" >&2
	fail=1
fi

if [ "$fail" != "0" ]; then
	exit 1
fi
echo "coff-obj-diff: OK (default -c COFF, four-way mcc/mingw COFF link parity, explicit-ELF negative control rejected)"
exit 0
