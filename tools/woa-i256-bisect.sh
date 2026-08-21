#!/usr/bin/env bash
# T-win-50041 crash bisection on windows-11-arm. KEY: compile-only (-c) vs
# compile+LINK (-o .exe) at -O1 — the segfault is link-path, not -O1 codegen.
set -x
S="$PWD"; B="$S/cmake-woa"; OUT="$S/bisect-out"; mkdir -p "$OUT"
cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release > "$OUT/00-cfg.txt" 2>&1 || true
cmake --build "$B" --target mcc > "$OUT/01-build.txt" 2>&1 || true
MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
BIS="$S/tools/woa-i256-bisect.c"
PROBE="$S/tools/woa-int256-probe.c"

cc() {   # label src flags...   (compile-only)
  local label="$1" src="$2"; shift 2
  "$MCC" $RT "$@" -c "$src" -o "$OUT/o.o" 2>"$OUT/e.txt"; local rc=$?
  [ $rc -eq 0 ] && echo "$label [-c]   => OK" || echo "$label [-c]   => FAIL rc=$rc"
}
ln() {   # label src flags...   (compile+link)
  local label="$1" src="$2"; shift 2
  "$MCC" $RT "$@" "$src" -o "$OUT/o.exe" 2>"$OUT/e.txt"; local rc=$?
  [ $rc -eq 0 ] && echo "$label [link] => OK" || echo "$label [link] => FAIL rc=$rc  [$(tail -1 "$OUT/e.txt")]"
}

{
  echo "== mcc = $MCC =="; "$MCC" -v 2>&1 | head -1
  echo "=== full probe: compile-only vs LINK, per opt level ==="
  cc "probe -O0" "$PROBE" -O0 ; ln "probe -O0" "$PROBE" -O0
  cc "probe -O1" "$PROBE" -O1 ; ln "probe -O1" "$PROBE" -O1
  cc "probe -O2" "$PROBE" -O2 ; ln "probe -O2" "$PROBE" -O2
  echo "=== isolated constructs, LINK at -O1 (which one crashes the link path?) ==="
  ln "SHL      -O1" "$BIS" -O1 -DPART_SHL
  ln "NEG      -O1" "$BIS" -O1 -DPART_NEG
  ln "MAG      -O1" "$BIS" -O1 -DPART_MAG
  ln "MAG+SHL  -O1" "$BIS" -O1 -DPART_MAG -DPART_SHL
  ln "ALL      -O1" "$BIS" -O1 -DPART_SHL -DPART_NEG -DPART_MAG
  echo "=== same isolated, LINK at -O0 (all should be OK) ==="
  ln "ALL      -O0" "$BIS" -O0 -DPART_SHL -DPART_NEG -DPART_MAG
  echo "=== does an EMPTY main crash at -O1 link? (isolates CRT/link vs probe code) ==="
  ln "empty    -O1" "$BIS" -O1
} > "$OUT/bisect.txt" 2>&1
echo "===== BISECT RESULTS ====="
cat "$OUT/bisect.txt"
