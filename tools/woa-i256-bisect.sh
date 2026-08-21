#!/usr/bin/env bash
# T-win-50041 round 16: is cdb (the Windows PE debugger) present at the SDK path
# (not just $PATH)? If so, single-step mcc_w256_shl to catch the neg-context
# reg corruption (arm64 cdb, break by COFF symbol, dump x2 + input limbs).
set -x
S="$PWD"; B="$S/cmake-woa-rdi"; OUT="$S/bisect-out"; mkdir -p "$OUT"
cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo > "$OUT/00.txt" 2>&1 || true
cmake --build "$B" --target mcc exec_runner > "$OUT/01.txt" 2>&1 || true
( cd "$B" && ctest -R "mcc_build" --timeout 300 ) > "$OUT/02.txt" 2>&1 || true
MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
MIN="$S/tools/woa-i256-min.c"
{
  echo "=== search for a Windows debugger (cdb/ntsd/windbg) under Windows Kits ==="
  find "/c/Program Files (x86)/Windows Kits" "/c/Program Files/Windows Kits" \
       \( -iname 'cdb.exe' -o -iname 'ntsd.exe' -o -iname 'windbg*.exe' \) 2>/dev/null | head -20
  CDB="$(find '/c/Program Files (x86)/Windows Kits' '/c/Program Files/Windows Kits' -iname 'cdb.exe' 2>/dev/null | grep -iE 'arm64' | head -1)"
  [ -z "$CDB" ] && CDB="$(find '/c/Program Files (x86)/Windows Kits' '/c/Program Files/Windows Kits' -iname 'cdb.exe' 2>/dev/null | head -1)"
  echo "USING cdb = ${CDB:-NONE FOUND}"
  "$MCC" $RT -g "$MIN" -o "$OUT/min.exe" 2>/dev/null; echo "min compile rc=$?"
  if [ -n "$CDB" ]; then
    echo "=== cdb: symbols matching w256 in the binary ==="
    "$CDB" -c "x min!*w256*; q" "$OUT/min.exe" 2>&1 | grep -iE 'w256|error|no symbol' | head -20
    echo "=== cdb: break mcc_w256_shl; at each of 4 shl hits dump x2(shift) + input limbs @x0 ==="
    "$CDB" -c "bp min!mcc_w256_shl; g; r x2; dq @x0 L4; g; r x2; dq @x0 L4; g; r x2; dq @x0 L4; g; r x2; dq @x0 L4; q" "$OUT/min.exe" 2>&1 | grep -iE 'x2=|x2 |^[0-9a-f]{8}`|Breakpoint|error|hit' | head -50
  else
    echo "(no cdb — tooling wall confirmed a second way)"
  fi
} > "$OUT/bisect.txt" 2>&1
echo "===== ROUND 16 (cdb single-step) ====="
cat "$OUT/bisect.txt"
