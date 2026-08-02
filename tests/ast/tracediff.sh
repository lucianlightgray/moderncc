#!/bin/sh
set -e

MCC=$1
TOOL=$2
WORK=$3
[ -n "$MCC" ] && [ -n "$TOOL" ] && [ -n "$WORK" ] || {
	echo "usage: tracediff.sh <mcc> <tracediff-tool> <workdir>" >&2
	exit 2
}

mkdir -p "$WORK"
SRC=$WORK/opassign.c
cat >"$SRC" <<'EOF'
struct B { double x, y, vx, vy; };
void f(struct B *p, double d) { p->vx -= d; }
EOF

BASE=$(cd "$(dirname "$MCC")" && pwd)

if [ -z "$("$MCC" -B"$BASE" -v128 -O2 -c "$SRC" -o /dev/null 2>&1 | grep '^\[TRACE\]' || true)" ]; then
	echo "SKIP: this mcc was built without MCC_CONFIG_TRACE"
	exit 77
fi

out=$("$TOOL" "$MCC" "$SRC" MCC_AST_OPASSIGN=0 MCC_AST_OPASSIGN=1 2>&1)
rc=0

echo "$out" | grep -q 'IDENTICAL' && {
	echo "FAIL: OPASSIGN=0 and =1 traced identical paths -- tracing stopped"
	echo "  discriminating, or the gate no longer changes this body."
	rc=1
}

sizes=$(echo "$out" | sed -n 's/^== trace sizes: A=\([0-9]*\)  B=\([0-9]*\).*/\1 \2/p')
a=$(echo "$sizes" | cut -d' ' -f1)
b=$(echo "$sizes" | cut -d' ' -f2)
if [ -z "$a" ] || [ -z "$b" ]; then
	echo "FAIL: could not read trace sizes from the tool's output"
	rc=1
elif [ "$a" -lt 1 ] || [ "$a" -gt 60000 ] || [ "$b" -lt 1 ] || [ "$b" -gt 60000 ]; then
	echo "FAIL: trace sizes A=$a B=$b are outside the scoped range -- MCC_TRACE_FILE"
	echo "  scoping (mcclog.h) is no longer narrowing the trace"
	rc=1
fi

if ! echo "$out" | grep -q 'ast_hook_vdup'; then
	echo "FAIL: first divergence is not in ast_hook_vdup; the OPASSIGN path moved"
	rc=1
fi
if ! echo "$out" | grep -qE 'ast_hook_(vdup|vpush|vstore): enter r=0x[0-9a-f]+ t=0x[0-9a-f]+'; then
	echo "FAIL: recorder hooks no longer print values (r=/t=) -- a diff can show"
	echo "  which line ran but not which value decided; see mccast.c MCC_TRACE_IF"
	rc=1
fi

if ! echo "$out" | grep -qE 'DESYNC vn=[0-9]+'; then
	echo "FAIL: AST_SET_DESYNC no longer dumps recorder state"
	rc=1
fi

if [ "$rc" -eq 0 ]; then
	echo "PASS: scoped traces (A=$a B=$b), divergence in ast_hook_vdup, values and"
	echo "  desync state present"
else
	echo "--- tool output ---"
	echo "$out"
fi
exit $rc
