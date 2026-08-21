#!/usr/bin/env bash
# T-win-50041 round 4: the crash is in __int256-TYPE codegen (u64 helpers are
# clean). Isolate which __int256 op crashes mcc -c -O1 on windows-11-arm.
set -x
S="$PWD"; B="$S/cmake-woa"; OUT="$S/bisect-out"; mkdir -p "$OUT"
cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release > "$OUT/00-cfg.txt" 2>&1 || true
cmake --build "$B" --target mcc > "$OUT/01-build.txt" 2>&1 || true
MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
PROBE="$S/tools/woa-int256-probe.c"
BIS="$S/tools/woa-i256-bisect.c"
N=20

reps() {  # label src flags...
  local label="$1" src="$2"; shift 2
  local crash=0 ok=0 i rc
  for i in $(seq 1 $N); do
    "$MCC" $RT "$@" -c "$src" -o "$OUT/o.o" 2>/dev/null; rc=$?
    if [ $rc -eq 0 ]; then ok=$((ok+1)); else crash=$((crash+1)); fi
  done
  echo "$label: CRASH $crash/$N"
}

{
  echo "== mcc = $MCC =="; "$MCC" -v 2>&1 | head -1
  echo "--- positive control (full probe, expect >0 in a bad-build run) ---"
  reps "PROBE       -O1" "$PROBE" -O1
  echo "--- isolate __int256 operations at -O1 ---"
  reps "I256_CASTV  -O1" "$BIS" -O1 -DPART_I256_CASTV
  reps "I256_CASTC  -O1" "$BIS" -O1 -DPART_I256_CASTC
  reps "I256_NEG    -O1" "$BIS" -O1 -DPART_I256_NEG
  reps "I256_ALIAS  -O1" "$BIS" -O1 -DPART_I256_ALIAS
  reps "I256_ADD    -O1" "$BIS" -O1 -DPART_I256_ADD
  echo "--- combos (whole-probe interaction?) ---"
  reps "CASTV+ALIAS -O1" "$BIS" -O1 -DPART_I256_CASTV -DPART_I256_ALIAS
  reps "ALL_I256    -O1" "$BIS" -O1 -DPART_I256_CASTV -DPART_I256_CASTC -DPART_I256_NEG -DPART_I256_ALIAS -DPART_I256_ADD
  echo "--- controls at -O0 (expect 0) ---"
  reps "ALL_I256    -O0" "$BIS" -O0 -DPART_I256_CASTV -DPART_I256_CASTC -DPART_I256_NEG -DPART_I256_ALIAS -DPART_I256_ADD
} > "$OUT/bisect.txt" 2>&1
echo "===== BISECT RESULTS ====="
cat "$OUT/bisect.txt"
