#!/usr/bin/env bash
# T-win-50041 crash characterization on windows-11-arm. Round 3: is the -O1/-O2
# compile-only segfault FLAKY? Compile the probe -c N times per opt level, count
# SIGSEGVs. Flaky ⇒ uninitialized-memory / MSVC-arm64 host miscompile of mcc.
set -x
S="$PWD"; B="$S/cmake-woa"; OUT="$S/bisect-out"; mkdir -p "$OUT"
cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release > "$OUT/00-cfg.txt" 2>&1 || true
cmake --build "$B" --target mcc > "$OUT/01-build.txt" 2>&1 || true
MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
PROBE="$S/tools/woa-int256-probe.c"
BIS="$S/tools/woa-i256-bisect.c"
N=30

reps() {  # label src flags...
  local label="$1" src="$2"; shift 2
  local crash=0 ok=0 rcs="" i rc
  for i in $(seq 1 $N); do
    "$MCC" $RT "$@" -c "$src" -o "$OUT/o$i.o" 2>/dev/null; rc=$?
    if [ $rc -eq 0 ]; then ok=$((ok+1)); else crash=$((crash+1)); rcs="$rcs $rc"; fi
  done
  echo "$label: CRASH $crash/$N, OK $ok/$N   (fail rcs:$rcs)"
}

{
  echo "== mcc = $MCC =="; "$MCC" -v 2>&1 | head -1
  reps "probe -c -O0" "$PROBE" -O0
  reps "probe -c -O1" "$PROBE" -O1
  reps "probe -c -O2" "$PROBE" -O2
  echo "--- isolated constructs at -O1, N reps each (find a consistent crasher) ---"
  reps "SHL      -c -O1" "$BIS" -O1 -DPART_SHL
  reps "NEG      -c -O1" "$BIS" -O1 -DPART_NEG
  reps "MAG+SHL  -c -O1" "$BIS" -O1 -DPART_MAG -DPART_SHL
  reps "ALL      -c -O1" "$BIS" -O1 -DPART_SHL -DPART_NEG -DPART_MAG
} > "$OUT/bisect.txt" 2>&1
echo "===== BISECT RESULTS ====="
cat "$OUT/bisect.txt"
