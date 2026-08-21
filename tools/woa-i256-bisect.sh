#!/usr/bin/env bash
# T-win-50041 round 6: ROOT CAUSE is MSVC-arm64 Release-opt miscompiling mcc
# (RelWithDebInfo mcc did NOT crash). FIX HYPOTHESIS: a RelWithDebInfo-built mcc
# also codegens int256.c correctly -> exec/int256 passes. Test it end-to-end,
# and A/B against a Release-built mcc in the same run.
set -x
S="$PWD"; OUT="$S/bisect-out"; mkdir -p "$OUT"
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
PROBE="$S/tools/woa-int256-probe.c"

build_and_test() {  # buildtype dir
  local bt="$1" B="$S/$2"
  cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE="$bt" > "$OUT/cfg-$bt.txt" 2>&1 || true
  cmake --build "$B" --target mcc exec_runner > "$OUT/build-$bt.txt" 2>&1 || true
  local MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
  echo "########## mcc build type = $bt ($MCC) ##########"
  echo "--- ctest exec/int256 ---"
  ( cd "$B" && ctest -R "^exec/int256$" --output-on-failure --timeout 120 ) 2>&1 | grep -E 'tests passed|Passed|Failed|dneg|negtrunc' | head -20
  echo "--- probe const-neg/var-neg/dneg (want nonzero-correct) ---"
  "$MCC" $RT "$PROBE" -o "$OUT/p-$bt.exe" 2>/dev/null && "$OUT/p-$bt.exe" 2>&1 | grep -E 'const-neg|var-neg|repro fromf' | head
}

{
  build_and_test "Release"        "cmake-woa-rel"
  build_and_test "RelWithDebInfo" "cmake-woa-rdi"
} > "$OUT/bisect.txt" 2>&1
echo "===== ROUND 6: Release vs RelWithDebInfo mcc ====="
cat "$OUT/bisect.txt"
