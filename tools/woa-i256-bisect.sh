#!/usr/bin/env bash
# T-win-50041 round 10: disassemble __mcc_i256_from_f64 (arm64-Windows) to find
# the mis-generated instruction in the negative branch (the -x -> nested-call
# marshalling). Use a NON-crashing RelWithDebInfo mcc.
set -x
S="$PWD"; B="$S/cmake-woa-rdi"; OUT="$S/bisect-out"; mkdir -p "$OUT"
cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo > "$OUT/00.txt" 2>&1 || true
cmake --build "$B" --target mcc exec_runner > "$OUT/01.txt" 2>&1 || true
( cd "$B" && ctest -R "mcc_build" --timeout 300 ) > "$OUT/02.txt" 2>&1 || true
MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
DIS="$(command -v llvm-objdump || true)"

{
  echo "== mcc = $MCC =="
  echo "=== mcc -S of __mcc_i256_from_f64 (source-structured asm) ==="
  "$MCC" $RT -S "$S/runtime/lib/int256.c" -o "$OUT/int256.s" 2>/dev/null
  awk '/^[[:space:]]*__mcc_i256_from_f64:/{p=1}
       p{print}
       p&&/\.size[[:space:]]+__mcc_i256_from_f64/{exit}
       p&&/^[[:space:]]*(w256_from_double_mag|__mcc_i256_from_f32):/{if(seen)exit;seen=1}' "$OUT/int256.s" | head -120
  echo "=== also w256_from_double_mag (the callee) — first 60 lines ==="
  awk '/^[[:space:]]*w256_from_double_mag:/{p=1} p{print} p&&/\.size[[:space:]]+w256_from_double_mag/{exit}' "$OUT/int256.s" | head -60
} > "$OUT/bisect.txt" 2>&1
echo "===== ROUND 10: __mcc_i256_from_f64 disasm ====="
cat "$OUT/bisect.txt"
