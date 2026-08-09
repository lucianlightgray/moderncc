#!/bin/sh
set -u

MCC=${1:-}
BDIR=${2:-}
WORK=${3:-}
STACK_KIB=1024
DEPTH=40000

[ -n "$MCC" ] && [ -n "$BDIR" ] && [ -n "$WORK" ] || {
	echo "usage: parse-depth.sh <mcc> <bdir> <workdir>" >&2
	exit 2
}
[ -x "$MCC" ] || { echo "SKIP: no mcc at $MCC"; exit 77; }

echo "parse-depth: every recursive path must reach the depth diagnostic, not the guard page."
echo "parse-depth: lowering this cell's stack rlimit to ${STACK_KIB} KiB; a pass at the"
echo "parse-depth: runner's inherited limit would prove nothing, so refusing to run without it."

ulimit -s "$STACK_KIB" 2>/dev/null || {
	echo "parse-depth: FAIL, could not lower the stack rlimit to ${STACK_KIB} KiB" >&2
	exit 1
}
got=$(ulimit -s)
[ "$got" = "$STACK_KIB" ] || {
	echo "parse-depth: FAIL, asked for ${STACK_KIB} KiB, ulimit -s reports '${got}'" >&2
	exit 1
}

rm -rf "$WORK"
mkdir -p "$WORK" || exit 1

rep() {
	awk -v n="$2" -v s="$1" 'BEGIN {
		r = ""; c = s; k = n
		while (k > 0) { if (k % 2) r = r c; c = c c; k = int(k / 2) }
		printf "%s", r
	}'
}

fails=0
checked=0

expect_diag() {
	name=$1
	out=$("$MCC" "-B$BDIR" -c "$WORK/$name.c" -o "$WORK/$name.o" 2>&1)
	rc=$?
	checked=$((checked + 1))
	if [ "$rc" -ge 128 ]; then
		echo "parse-depth: SIGNAL rc=$rc (signal $((rc - 128))) on $name" >&2
		fails=$((fails + 1))
		return
	fi
	if [ "$rc" -eq 0 ]; then
		echo "parse-depth: $name compiled cleanly at depth $DEPTH, the guard never fired" >&2
		fails=$((fails + 1))
		return
	fi
	case "$out" in
	*"program nests too deeply"*) ;;
	*)
		echo "parse-depth: $name exited $rc without the depth diagnostic:" >&2
		echo "$out" | head -3 >&2
		fails=$((fails + 1))
		;;
	esac
}

{ printf 'int main(void){return '; rep '(' "$DEPTH"; printf '0'; rep ')' "$DEPTH"; printf ';}\n'; } >"$WORK/paren.c"
expect_diag paren

{ printf 'int main(void){return '; rep '(int)' "$DEPTH"; printf '0;}\n'; } >"$WORK/cast.c"
expect_diag cast

{ printf 'int main(void){return (int)'; rep 'sizeof ' "$DEPTH"; printf '(0);}\n'; } >"$WORK/sizeofchain.c"
expect_diag sizeofchain

{ printf 'int main(void){int x=0;return '; rep '*&' "$DEPTH"; printf 'x;}\n'; } >"$WORK/derefchain.c"
expect_diag derefchain

{ printf 'int main(void){return '; rep '1?0:' "$DEPTH"; printf '0;}\n'; } >"$WORK/ternary.c"
expect_diag ternary

{ printf 'int main(void){'; rep '{' "$DEPTH"; printf 'return 0;'; rep '}' "$DEPTH"; printf '}\n'; } >"$WORK/blocks.c"
expect_diag blocks

{ printf 'int main(void){'; rep 'if(1)' "$DEPTH"; printf 'return 0;return 1;}\n'; } >"$WORK/ifchain.c"
expect_diag ifchain

{ printf 'int main(void){'; rep 'for(;;)' "$DEPTH"; printf 'break;return 0;}\n'; } >"$WORK/forchain.c"
expect_diag forchain

{ printf 'int a[1] = '; rep '{' "$DEPTH"; printf '0'; rep '}' "$DEPTH"; printf ';\nint main(void){return a[0];}\n'; } >"$WORK/initbraces.c"
expect_diag initbraces

{ printf 'int '; rep '(' "$DEPTH"; printf 'x'; rep ')' "$DEPTH"; printf ';\nint main(void){return 0;}\n'; } >"$WORK/declparen.c"
expect_diag declparen

{ printf 'int '; rep '(*' "$DEPTH"; printf 'f'; rep ')(void)' "$DEPTH"; printf ';\nint main(void){return 0;}\n'; } >"$WORK/declfnptr.c"
expect_diag declfnptr

{
	i=0
	while [ "$i" -lt 2000 ]; do printf 'struct s%d { ' "$i"; i=$((i + 1)); done
	printf 'int x;'
	i=0
	while [ "$i" -lt 2000 ]; do printf ' };'; i=$((i + 1)); done
	printf '\nint main(void){return 0;}\n'
} >"$WORK/structnest.c"
expect_diag structnest

{ printf '#define M(x) x\nint main(void){return '; rep 'M(' "$DEPTH"; printf '0'; rep ')' "$DEPTH"; printf ';}\n'; } >"$WORK/macronest.c"
expect_diag macronest

{ printf 'int '; rep '_Alignas(' "$DEPTH"; printf 'int'; rep ') int' "$((DEPTH - 1))"; printf ') x;\nint main(void){return 0;}\n'; } >"$WORK/alignasnest.c"
expect_diag alignasnest

{ printf '__asm__(".set mccz, '; rep '(' "$DEPTH"; printf '1'; rep ')' "$DEPTH"; printf '");\nint main(void){return 0;}\n'; } >"$WORK/asmparen.c"
expect_diag asmparen

{
	i=0
	while [ "$i" -lt 63 ]; do printf '#if 1\n'; i=$((i + 1)); done
	printf 'struct n0 { '
	i=1
	while [ "$i" -lt 63 ]; do printf 'struct n%d { ' "$i"; i=$((i + 1)); done
	printf 'int leaf;'
	i=0
	while [ "$i" -lt 63 ]; do printf ' } f%d;' "$i"; i=$((i + 1)); done
	printf '\nint main(void) {\n'
	rep '{' 127
	printf '\nint '; rep '*' 12; printf 'q[1]; (void)q;\n'
	printf 'int '; rep '(' 63; printf 'dp'; rep ')' 63; printf ';\n'
	printf 'dp = '; rep '(' 63; printf '0'; rep ')' 63; printf ';\n'
	printf 'if (dp) return 1;\n'
	rep '}' 127
	printf '\nreturn 0;\n}\n'
	i=0
	while [ "$i" -lt 63 ]; do printf '#endif\n'; i=$((i + 1)); done
} >"$WORK/c11floor.c"

out=$("$MCC" "-B$BDIR" -c "$WORK/c11floor.c" -o "$WORK/c11floor.o" 2>&1)
rc=$?
checked=$((checked + 1))
if [ "$rc" -ne 0 ]; then
	echo "parse-depth: the C11 5.2.4.1 nesting floor no longer compiles (rc=$rc):" >&2
	echo "$out" | head -3 >&2
	fails=$((fails + 1))
fi

if [ "$checked" -lt 16 ]; then
	echo "parse-depth: only $checked cases ran, this check is vacuous" >&2
	exit 1
fi

if [ "$fails" -ne 0 ]; then
	echo "parse-depth: $fails of $checked cases failed at a ${STACK_KIB} KiB stack" >&2
	exit 1
fi

echo "parse-depth: $checked cases OK at a ${STACK_KIB} KiB stack"
exit 0
