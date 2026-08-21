#!/usr/bin/env bash
# T-win-50041 round 15: lldb single-step (non-perturbing, unlike the printf that
# caused the Heisenbug). Break at mcc_w256_shl on the min repro; the 4 shl hits are
# in order: neg-1e30, pos-1e30, pos-1e18, neg-1e18. Dump the shift count (x2) and
# the input limbs (@x0) at each — is the NEG-context shl fed the same as POS?
set -x
S="$PWD"; B="$S/cmake-woa-rdi"; OUT="$S/bisect-out"; mkdir -p "$OUT"
cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo > "$OUT/00.txt" 2>&1 || true
cmake --build "$B" --target mcc exec_runner > "$OUT/01.txt" 2>&1 || true
( cd "$B" && ctest -R "mcc_build" --timeout 300 ) > "$OUT/02.txt" 2>&1 || true
MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
MIN="$S/tools/woa-i256-min.c"
LLDB="$(command -v lldb || echo '/c/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/Llvm/ARM64/bin/lldb')"
{
  echo "== mcc = $MCC ; lldb = $LLDB =="
  "$MCC" $RT -g "$MIN" -o "$OUT/min.exe" 2>/dev/null; echo "min compile rc=$?"
  echo "=== plain run (confirm the bug is present in this binary) ==="
  "$OUT/min.exe" 2>&1 | grep -E 'run-neg-1e30|run-pos-1e30'
  echo "=== lldb: dump x2(shift) + input limbs @x0 at each mcc_w256_shl hit ==="
  "$LLDB" --batch \
    -o "breakpoint set -n mcc_w256_shl" \
    -o "run" \
    -o "frame info" -o "register read x0 x1 x2" -o "memory read -c4 -f x -s8 \$x0" -o "continue" \
    -o "register read x0 x1 x2" -o "memory read -c4 -f x -s8 \$x0" -o "continue" \
    -o "register read x0 x1 x2" -o "memory read -c4 -f x -s8 \$x0" -o "continue" \
    -o "register read x0 x1 x2" -o "memory read -c4 -f x -s8 \$x0" -o "continue" \
    -o "quit" \
    -- "$OUT/min.exe" 2>&1 | grep -vE '^\(lldb\) *$|Executing commands'
} > "$OUT/bisect.txt" 2>&1
echo "===== ROUND 15 (lldb shl inspection) ====="
cat "$OUT/bisect.txt"
