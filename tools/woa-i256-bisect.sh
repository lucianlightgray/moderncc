#!/usr/bin/env bash
# T-win-50041 round 17: cdb-arm64 IS present + launches the process; only symbol
# resolution fails (cdb=PDB, mcc=DWARF). Work around it: get mcc_w256_shl's VA
# from llvm-objdump -t and bp by address. Dump x2(shift)+input-limbs @x0 at each of
# the 4 shl hits (order: neg-1e30, pos-1e30, pos-1e18, neg-1e18).
set -x
S="$PWD"; B="$S/cmake-woa-rdi"; OUT="$S/bisect-out"; mkdir -p "$OUT"
cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo > "$OUT/00.txt" 2>&1 || true
cmake --build "$B" --target mcc exec_runner > "$OUT/01.txt" 2>&1 || true
( cd "$B" && ctest -R "mcc_build" --timeout 300 ) > "$OUT/02.txt" 2>&1 || true
MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
MIN="$S/tools/woa-i256-min.c"
CDB="/c/Program Files (x86)/Windows Kits/10/Debuggers/arm64/cdb.exe"
DIS="$(command -v llvm-objdump || echo '/c/Program Files/Microsoft Visual Studio/2022/Enterprise/VC/Tools/Llvm/ARM64/bin/llvm-objdump')"
{
  "$MCC" $RT -g "$MIN" -o "$OUT/min.exe" 2>/dev/null; echo "min compile rc=$?"
  echo "=== symtab (llvm-objdump -t) for the target fns ==="
  "$DIS" -t "$OUT/min.exe" 2>/dev/null | grep -iE 'w256_shl|w256_from_double_mag|i256_from_f64' | head
  SHL=$("$DIS" -t "$OUT/min.exe" 2>/dev/null | grep -iE 'mcc_w256_shl$' | awk '{print $1}' | head -1)
  MAG=$("$DIS" -t "$OUT/min.exe" 2>/dev/null | grep -iE 'w256_from_double_mag$' | awk '{print $1}' | head -1)
  echo "mcc_w256_shl VA=0x$SHL   w256_from_double_mag VA=0x$MAG"
  echo "=== cdb: lm (module base) + bp by VA at mcc_w256_shl, dump x2 + limbs (4 hits) ==="
  "$CDB" -c "lm; bp 0x$SHL; g; r x2; dq @x0 L4; g; r x2; dq @x0 L4; g; r x2; dq @x0 L4; g; r x2; dq @x0 L4; q" "$OUT/min.exe" 2>&1 | grep -ivE 'symbol file|ntdll|KERNEL|apphelp|Reading initial|^\*\*\*|Symbol search|Deferred|SUCCESS|Path validation|Response|windows-11' | tail -60
} > "$OUT/bisect.txt" 2>&1
echo "===== ROUND 17 (cdb bp-by-VA single-step) ====="
cat "$OUT/bisect.txt"
