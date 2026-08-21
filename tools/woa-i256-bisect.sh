#!/usr/bin/env bash
# T-win-50041 round 7: minimal Bug-B repro. Build a NON-crashing mcc
# (RelWithDebInfo sidesteps Bug A) + runtime, compile+link+run the 4-line repro at
# -O0 and -O1, print the __int256 values. Confirms Bug B is a real, minimal,
# opt-independent (__int256)double conversion miscompile on arm64-Windows.
set -x
S="$PWD"; B="$S/cmake-woa-rdi"; OUT="$S/bisect-out"; mkdir -p "$OUT"
cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo > "$OUT/00.txt" 2>&1 || true
cmake --build "$B" --target mcc exec_runner > "$OUT/01.txt" 2>&1 || true
MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
MIN="$S/tools/woa-i256-min.c"

runit() {  # optflag
  local o="$1"
  "$MCC" $RT $o "$MIN" -o "$OUT/min$o.exe" 2>"$OUT/mc$o.txt"
  echo "--- min $o (compile rc=$?) ---"
  [ -x "$OUT/min$o.exe" ] && "$OUT/min$o.exe" 2>&1 || echo "(no exe)"
}

{
  echo "== mcc = $MCC =="; "$MCC" -v 2>&1 | head -1
  runit -O0
  runit -O1
} > "$OUT/bisect.txt" 2>&1
echo "===== ROUND 7: minimal Bug-B repro ====="
cat "$OUT/bisect.txt"
