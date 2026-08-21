#!/usr/bin/env bash
# T-win-50041 round 18: cdb-arm64 works; __mcc_i256_from_f64 is a named symbol
# (RVA from llvm-objdump -t). bp there by module-relative RVA; save r ptr (x0) +
# arg (d0), run to return (pt), dump the 32-byte result. Call order: hit1=neg-1e30
# (buggy, expect 0), hit2=pos-1e30 (correct). Confirms cdb single-step is usable.
set -x
S="$PWD"; B="$S/cmake-woa-rdi"; OUT="$S/bisect-out"; mkdir -p "$OUT"
cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo > "$OUT/00.txt" 2>&1 || true
cmake --build "$B" --target mcc exec_runner > "$OUT/01.txt" 2>&1 || true
( cd "$B" && ctest -R "mcc_build" --timeout 300 ) > "$OUT/02.txt" 2>&1 || true
MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
MIN="$S/tools/woa-i256-min.c"
CDB="/c/Program Files (x86)/Windows Kits/10/Debuggers/arm64/cdb.exe"
DIS="/c/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/Llvm/ARM64/bin/llvm-objdump"
{
  "$MCC" $RT -g "$MIN" -o "$OUT/min.exe" 2>/dev/null; echo "min compile rc=$?"
  RVA=$("$DIS" -t "$OUT/min.exe" 2>/dev/null | grep -iE '__mcc_i256_from_f64$' | awk '{print $1}' | sed 's/^0x0*//' | head -1)
  echo "__mcc_i256_from_f64 RVA=0x$RVA"
  echo "=== cdb: bp __mcc_i256_from_f64 (module+RVA), dump result of neg vs pos ==="
  "$CDB" -c "lm1m; bp min+0x$RVA; g; .echo HIT1_NEG; r d0; r? \$t0=@x0; pt; dq @\$t0 L4; g; .echo HIT2_POS; r d0; r? \$t1=@x0; pt; dq @\$t1 L4; q" "$OUT/min.exe" 2>&1 \
    | grep -iE 'HIT1_NEG|HIT2_POS|d0 |x0 |^[0-9a-f]{8}\`|breakpoint|couldn|error|module|min ' | tail -40
} > "$OUT/bisect.txt" 2>&1
echo "===== ROUND 18 (cdb result dump) ====="
cat "$OUT/bisect.txt"
