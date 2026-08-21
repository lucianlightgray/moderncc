#!/usr/bin/env bash
# T-win-50041 round 12: is Bug B negative-branch-specific, or is runtime large
# left-shift broken regardless of sign? Build a non-crashing RelWithDebInfo mcc +
# runtime, run the min repro (volatile pos/neg large + small-shl + shr cases).
set -x
S="$PWD"; B="$S/cmake-woa-rdi"; OUT="$S/bisect-out"; mkdir -p "$OUT"
cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo > "$OUT/00.txt" 2>&1 || true
cmake --build "$B" --target mcc exec_runner > "$OUT/01.txt" 2>&1 || true
( cd "$B" && ctest -R "mcc_build" --timeout 300 ) > "$OUT/02.txt" 2>&1 || true
MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
MIN="$S/tools/woa-i256-min.c"
{
  echo "== mcc = $MCC =="
  "$MCC" $RT "$MIN" -o "$OUT/min.exe" 2>"$OUT/mc.txt"
  echo "compile rc=$?"
  [ -x "$OUT/min.exe" ] && "$OUT/min.exe" 2>&1 || cat "$OUT/mc.txt"
} > "$OUT/bisect.txt" 2>&1
echo "===== ROUND 12 (pos-vs-neg large runtime) ====="
cat "$OUT/bisect.txt"
