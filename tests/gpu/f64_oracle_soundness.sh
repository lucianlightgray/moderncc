#!/bin/sh
set -e
MCC="$1"
BD="$2"
WORK="$3"

mkdir -p "$WORK"
SUBJ="$WORK/f64_oracle_subject.c"
CPU="$WORK/f64_oracle_cpu.out"
GPU="$WORK/f64_oracle_gpu.out"
GERR="$WORK/f64_oracle_gpu.err"

cat > "$SUBJ" <<'EOF'
#include <stdio.h>
static double distr(double a, double b, double c) { return (a * b - a * c) * 1e16; }
static double assoc(double a, double b, double c) { return ((a + b) + c) - (a + (b + c)); }
static double cancel(double a, double b) { double s = a + b; return (s - a) - b; }
int main(void) {
	volatile double a = 1.0000000001, b = 3.0000000007, c = 2.9999999993;
	printf("%.17g %.17g %.17g\n", distr(a, b, c), assoc(a, 1e17, -1e17),
				 cancel(1e16, 1.0));
	return 0;
}
EOF

"$MCC" -B"$BD" -w -O0 -run "$SUBJ" > "$CPU" 2>/dev/null || {
	echo "FAIL: the -O0 CPU reference run did not execute"; exit 1; }

MCC_AST_EVAL_LADDER=1 MCC_AST_EVAL_LADDER_GPU=1 \
	"$MCC" -B"$BD" -w -O13 -fopt-search --jit-always-gpu -run "$SUBJ" \
	> "$GPU" 2> "$GERR" || {
	echo "FAIL: the -O13 GPU-oracle run did not execute"; sed -n '1,20p' "$GERR"; exit 1; }

if ! grep -q 'available=1' "$GERR"; then
	echo "SKIP: no GPU device available; the oracle ran on the CPU and proves nothing"
	exit 77
fi

RUNGS=$(sed -n 's/.*rungs=\([0-9]*\).*/\1/p' "$GERR" | head -1)
[ -n "$RUNGS" ] || RUNGS=0
if [ "$RUNGS" -eq 0 ]; then
	echo "FAIL: device armed but ran no double rungs; the fp64 oracle was never exercised"
	grep 'ladder-gpu' "$GERR" || true
	exit 1
fi

if [ ! -s "$CPU" ]; then
	echo "FAIL: the -O0 reference produced no output"
	exit 1
fi

if ! cmp -s "$CPU" "$GPU"; then
	echo "FAIL: the GPU-armed -O13 double oracle changed the result vs the -O0 CPU run"
	echo "  cpu(-O0):  $(cat "$CPU")"
	echo "  gpu(-O13): $(cat "$GPU")"
	exit 1
fi

echo "PASS: GPU double oracle is bit-exact vs the -O0 CPU run; rungs=$RUNGS"
echo "  result: $(cat "$CPU")"
grep 'ladder-gpu' "$GERR" | sed 's/^/  /'
exit 0
