#!/bin/sh
# Two-part -O4 regression:
#  Part 1: AOT optimizer (JIT off) engages the ~4s search budget with const-guided ranges.
#  Part 2: JIT on + -O4 recompiles hot functions and hot-swaps the AST via the backend.
# Args: $1 = mcc binary, $2 = repo root, $3 = build dir (for -I/-B).
set -e
MCC="$1"
ROOT="$2"
BLD="$3"
[ -x "$MCC" ] || { echo "regression_o4: mcc not executable: $MCC"; exit 1; }

INCS="-I$BLD -I$ROOT -I$ROOT/src -I$ROOT/src/formats -I$ROOT/src/objfmt -I$ROOT/src/arch/i386 -I$ROOT/src/arch/x86_64 -I$ROOT/src/arch/arm64 -I$ROOT/src/arch/arm -I$ROOT/src/arch/riscv64 -I$ROOT/include -B$ROOT -B$BLD"
TMP="${TMPDIR:-/tmp}/regr_o4_$$"
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT
fail=0

echo "== Part 1: AOT -O4 search (JIT off) engages ~4s with const-guided ranges =="
t0=$(date +%s.%N)
out1=$(env XDG_CACHE_HOME="$TMP/c1" MCC_JIT=0 MCC_AST_SEARCH=1 MCC_SEARCH_WORKER=1 MCC_STATS_FORCE=1 \
	"$MCC" -O4 --stats $INCS -c "$ROOT/src/mcc.c" -o "$TMP/p1.o" 2>&1) || { echo "$out1" | tail -3; echo "Part1: compile FAILED"; exit 1; }
t1=$(date +%s.%N)
clean1=$(printf '%s' "$out1" | sed 's/\x1b\[[0-9;]*[A-Za-z]//g; s/\x1b\[[0-9]*K//g')
wall=$(awk "BEGIN{printf \"%.2f\", $t1-$t0}")
evald=$(printf '%s' "$clean1" | grep -oE 'evaluated [0-9]+' | tail -1 | grep -oE '[0-9]+' || echo 0)
range_on=$(printf '%s' "$clean1" | grep -cE '●range' || true)
echo "Part1: wall=${wall}s evaluated=${evald} range_gate_lines=${range_on}"

awk "BEGIN{exit !($wall > 3.0 && $wall < 8.0)}" || { echo "Part1 FAIL: wall ${wall}s outside 3..8s (4s budget did not engage)"; fail=1; }
[ "${evald:-0}" -gt 1000 ] || { echo "Part1 FAIL: only $evald candidates evaluated (search did not run)"; fail=1; }
[ "${range_on:-0}" -ge 1 ] || { echo "Part1 FAIL: RANGE gate (const-guided ranges) not shown active"; fail=1; }
[ "$fail" = 0 ] && echo "Part1 PASS: 4s AOT search engaged, $evald candidates, const-guided ranges active"

echo "== Part 2: JIT on + -O4, backend hands its compiled AST to the JIT (mcc_jit_submit_ast override) =="
printf '%s\n' \
	'__attribute__((noinline)) static unsigned busy(unsigned x){ unsigned s=0,i; for(i=0;i<64;i++) s += (x+i)/7u + (x*i)%13u; return s; }' \
	'#include <stdio.h>' \
	'int main(void){ unsigned long t=0; int i; for(i=0;i<400000;i++) t+=busy((unsigned)i); printf("%lu\n", t); return 0; }' \
	> "$TMP/hot.c"
ref=$(env MCC_JIT=0 "$MCC" -O2 -run "$TMP/hot.c" 2>/dev/null | tr -dc '0-9')
out2=$(env XDG_CACHE_HOME="$TMP/c2" MCC_JIT=1 MCC_JIT_SUBMIT_AOT=1 MCC_JIT_HOT_THRESHOLD=200 MCC_JIT_VERBOSE=1 \
	"$MCC" -O4 -run "$TMP/hot.c" 2>&1) || { echo "$out2" | tail -5; echo "Part2: run FAILED"; exit 1; }
prog=$(printf '%s' "$out2" | grep -vE 'mccjit-|mccstat' | tr -dc '0-9')
submitted=$(printf '%s' "$out2" | grep -cE 'mccjit-aot-submit\[busy\]' || true)
overrode=$(printf '%s' "$out2" | grep -cE 'mccjit-override\[busy\]' || true)
echo "Part2: prog_output=${prog} ref=${ref} busy_submitted=${submitted} busy_override_used=${overrode}"
[ -n "$prog" ] || { echo "Part2 FAIL: program produced no output"; fail=1; }
[ "$prog" = "$ref" ] || { echo "Part2 FAIL: JIT output $prog != reference $ref (override miscompiled)"; fail=1; }
[ "${submitted:-0}" -ge 1 ] || { echo "Part2 FAIL: backend did not submit busy's AST (mcc_jit_submit_ast not called)"; fail=1; }
[ "${overrode:-0}" -ge 1 ] || { echo "Part2 FAIL: JIT did not use the backend-submitted AST (override path did not fire)"; fail=1; }
[ "$fail" = 0 ] && echo "Part2 PASS: backend submitted busy's AST; JIT recompiled from it (override), correct result"

[ "$fail" = 0 ] && echo "regression_o4: ALL PASS" || echo "regression_o4: FAIL"
exit $fail
