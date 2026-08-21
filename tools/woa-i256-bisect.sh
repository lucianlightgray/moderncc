#!/usr/bin/env bash
# T-win-50041 crash bisection on windows-11-arm: which construct segfaults mcc -O1?
set -x
S="$PWD"; B="$S/cmake-woa"; OUT="$S/bisect-out"; mkdir -p "$OUT"
cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release > "$OUT/00-cfg.txt" 2>&1 || true
cmake --build "$B" --target mcc > "$OUT/01-build.txt" 2>&1 || true
MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
BIS="$S/tools/woa-i256-bisect.c"
PROBE="$S/tools/woa-int256-probe.c"

run() {  # label  src  flags...
  local label="$1" src="$2"; shift 2
  "$MCC" $RT "$@" -c "$src" -o "$OUT/o.o" 2>"$OUT/e.txt"; local rc=$?
  if [ $rc -eq 0 ]; then echo "$label => OK"; else echo "$label => FAIL rc=$rc  [$(tail -1 "$OUT/e.txt")]"; fi
}

{
  echo "== mcc = $MCC =="; "$MCC" -v 2>&1 | head -1
  echo "=== baseline: full probe ==="
  run "probe -O0" "$PROBE" -O0
  run "probe -O1" "$PROBE" -O1
  echo "=== bisect single constructs at -O1 (find the crasher) ==="
  run "SHL      -O1" "$BIS" -O1 -DPART_SHL
  run "NEG      -O1" "$BIS" -O1 -DPART_NEG
  run "MAG      -O1" "$BIS" -O1 -DPART_MAG
  run "MAG+shl  -O1" "$BIS" -O1 -DPART_MAG -DPART_MAG_CALLS_SHL
  run "VARSHIFT -O1" "$BIS" -O1 -DPART_VARSHIFT
  echo "=== the same constructs at -O0 (all should be OK) ==="
  run "SHL      -O0" "$BIS" -O0 -DPART_SHL
  run "NEG      -O0" "$BIS" -O0 -DPART_NEG
  run "MAG+shl  -O0" "$BIS" -O0 -DPART_MAG -DPART_MAG_CALLS_SHL
  run "VARSHIFT -O0" "$BIS" -O0 -DPART_VARSHIFT
} > "$OUT/bisect.txt" 2>&1
echo "===== BISECT RESULTS ====="
cat "$OUT/bisect.txt"
