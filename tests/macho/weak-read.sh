#!/bin/sh
set -e

MCC=$1
SRC=$2
WORK=$3
BFLAG=$4
[ -n "$MCC" ] && [ -n "$SRC" ] && [ -n "$WORK" ] || {
	echo "usage: weak-read.sh <mcc> <srcdir> <workdir> [-B<prefix>]" >&2
	exit 2
}

[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }
command -v clang >/dev/null 2>&1 || { echo "SKIP: clang not found"; exit 77; }
"$MCC" -v 2>&1 | head -1 | grep -qF '(AArch64 Darwin)' || {
	echo "SKIP: mcc does not target AArch64 Darwin"
	exit 77
}
[ "$(uname -s)" = "Darwin" ] || { echo "SKIP: needs a Darwin host to run the image"; exit 77; }

rm -rf "$WORK"
mkdir -p "$WORK"

cat >"$WORK/wa.c" <<'EOF'
__attribute__((weak)) int shared(void) { return 1; }
int a_val(void) { return shared() + 10; }
EOF
cat >"$WORK/wb.c" <<'EOF'
__attribute__((weak)) int shared(void) { return 2; }
int b_val(void) { return shared() + 20; }
EOF
cat >"$WORK/wstrong.c" <<'EOF'
int shared(void) { return 99; }
EOF
cat >"$WORK/wmain.c" <<'EOF'
#include <stdio.h>
int a_val(void);
int b_val(void);
int main(void) { printf("%d %d\n", a_val(), b_val()); return 0; }
EOF
cat >"$WORK/amain.c" <<'EOF'
#include <stdio.h>
int a_val(void);
int main(void) { printf("%d\n", a_val()); return 0; }
EOF

for f in wa wb wstrong wmain amain; do
	clang -c "$WORK/$f.c" -o "$WORK/$f.o" 2>"$WORK/cc.err" || {
		echo "SKIP: clang cannot compile the Mach-O objects here"
		sed 's/^/  /' "$WORK/cc.err" | head -3
		exit 77
	}
done

E1=11
E2=21
E3=109
if [ -n "$WEAK_READ_MUTATE" ]; then
	E1=$((E1 + 1))
fi

# Two weak defs of shared(): must link (not "defined twice"), first def wins.
if ! "$MCC" $BFLAG "$WORK/wa.o" "$WORK/wb.o" "$WORK/wmain.o" -o "$WORK/two_weak" 2>"$WORK/link1.err"; then
	echo "FAIL: mcc could not link two clang objects that each weakly define shared()"
	echo "  (the Mach-O reader must map N_WEAK_DEF/N_WEAK_REF to STB_WEAK):"
	sed 's/^/  /' "$WORK/link1.err" | head -4
	exit 1
fi
got1=$("$WORK/two_weak")
if [ "$got1" != "$E1 $E2" ]; then
	if [ -n "$WEAK_READ_MUTATE" ]; then
		echo "PASS: known-positive -- a shifted expectation is observed"
		exit 0
	fi
	echo "FAIL: two weak shared() defs resolved wrong: got '$got1', want '$E1 $E2'"
	exit 1
fi
if [ -n "$WEAK_READ_MUTATE" ]; then
	echo "FAIL (known-positive): the shifted expectation still matched -- the"
	echo "  assertion does not observe the linked value"
	exit 1
fi

# A strong def must override the weak def.
if ! "$MCC" $BFLAG "$WORK/wa.o" "$WORK/wstrong.o" "$WORK/amain.o" -o "$WORK/strong_over" 2>"$WORK/link2.err"; then
	echo "FAIL: mcc could not link a weak + strong definition of shared()"
	sed 's/^/  /' "$WORK/link2.err" | head -4
	exit 1
fi
got2=$("$WORK/strong_over")
if [ "$got2" != "$E3" ]; then
	echo "FAIL: strong shared() did not override the weak one: got '$got2', want '$E3'"
	exit 1
fi

echo "macho-weak-read: OK (two weak defs link + first wins; strong overrides weak)"
exit 0
