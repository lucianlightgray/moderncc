#!/usr/bin/env bash
# T-win-50041 round 5: get a CRASH BACKTRACE. Build mcc RelWithDebInfo (optimized,
# so the crash still repros, but symbolicated), run the crashing -O1 compile under
# a debugger to pinpoint the faulting mcc function.
set -x
S="$PWD"; B="$S/cmake-woa-rdi"; OUT="$S/bisect-out"; mkdir -p "$OUT"
cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo > "$OUT/00-cfg.txt" 2>&1 || true
cmake --build "$B" --target mcc > "$OUT/01-build.txt" 2>&1 || true
MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
PROBE="$S/tools/woa-int256-probe.c"

{
  echo "== mcc = $MCC =="; "$MCC" -v 2>&1 | head -1
  echo "=== confirm crash on RelWithDebInfo mcc (5 reps) ==="
  c=0; for i in 1 2 3 4 5; do "$MCC" $RT -O1 -c "$PROBE" -o "$OUT/o.o" 2>/dev/null; [ $? -ne 0 ] && c=$((c+1)); done
  echo "RelWithDebInfo mcc: CRASH $c/5 at -O1"
  echo "=== available debuggers ==="
  for d in lldb cdb gdb windbg; do echo "$d: $(command -v $d || echo ABSENT)"; done
  echo "=== backtrace via lldb (if present) ==="
  LLDB="$(command -v lldb || true)"
  if [ -n "$LLDB" ]; then
    "$LLDB" --batch -o "run" -o "bt all" -o "quit" -- "$MCC" $RT -O1 -c "$PROBE" -o "$OUT/o.o" 2>&1 | tail -60
  fi
  echo "=== backtrace via cdb (if present) ==="
  CDB="$(command -v cdb || true)"
  if [ -n "$CDB" ]; then
    "$CDB" -g -G -c "g; k 40; q" "$MCC" $RT -O1 -c "$PROBE" -o "$OUT/o.o" 2>&1 | tail -60
  fi
} > "$OUT/bisect.txt" 2>&1
echo "===== BISECT RESULTS ====="
cat "$OUT/bisect.txt"
