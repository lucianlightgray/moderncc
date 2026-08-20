#!/usr/bin/env bash
# Diagnostic for T-win-50023 (arm64-Windows __float128) + T-win-50024
# (arm64-Windows integer_promotion). Runs on a windows-11-arm GitHub runner
# under an arm64 MSVC dev shell. Builds mcc, runs the two exec cells, and
# captures mcc's raw output, a clang oracle, and disassembly so the failure
# is observable off-box. Best-effort throughout: never hard-fail, gather signal.
set -x
S="$PWD"
B="$S/cmake-woa"
OUT="$S/diag-out"
mkdir -p "$OUT"

# --- build mcc + exec_runner (MSVC arm64) ---
cmake -S "$S" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release > "$OUT/00-cmake-configure.txt" 2>&1 || true
cmake --build "$B" --target mcc exec_runner > "$OUT/01-cmake-build.txt" 2>&1 || true

MCC="$B/mcc.exe"; [ -x "$MCC" ] || MCC="$B/mcc"
{ echo "== mcc = $MCC =="; "$MCC" -v; "$MCC" -dumpmachine; } > "$OUT/02-mcc-identity.txt" 2>&1 || true

# --- exact ctest cells (the real golden comparison) ---
( cd "$B" && ctest -R "^exec/float128$|^exec/integer_promotion$|^exec/int256$" --output-on-failure --timeout 120 ) \
    > "$OUT/03-ctest.txt" 2>&1 || true

# --- full exec suite sweep (find any other arm64-Windows reds); set WOA_FULL_SWEEP=1 ---
if [ -n "$WOA_FULL_SWEEP" ]; then
( cd "$B" && ctest -R "^exec/" --output-on-failure --timeout 120 -j4 ) > "$OUT/05-exec-suite.txt" 2>&1 || true
fi
{
  grep -E '% tests passed|tests failed out of' "$OUT/05-exec-suite.txt" || true
  echo "--- FAILED cells ---"
  grep -E '\*\*\*Failed|\*\*\*Timeout' "$OUT/05-exec-suite.txt" || echo "(none)"
} > "$OUT/06-exec-fails.txt" 2>&1 || true

# --- direct compile + run + disasm + clang oracle ---
RT="-B $S/runtime/win32 -B $S/runtime -I $S/runtime/include -I $S/tests/support"
CLANG="$(command -v clang || true)"
DIS="$(command -v llvm-objdump || true)"

diag_one() {
  local subj="$1" name="$2"
  local src="$S/tests/exec/$subj.c"
  echo "===== $name ($src) =====" > "$OUT/${name}-00-summary.txt"
  # mcc compile+run
  "$MCC" $RT "$src" -o "$OUT/${name}-mcc.exe" > "$OUT/${name}-10-mcc-compile.txt" 2>&1
  echo "mcc compile rc=$?" >> "$OUT/${name}-10-mcc-compile.txt"
  if [ -x "$OUT/${name}-mcc.exe" ]; then
    "$OUT/${name}-mcc.exe" > "$OUT/${name}-11-mcc-run.txt" 2>&1
    echo "mcc run rc=$?" >> "$OUT/${name}-11-mcc-run.txt"
  fi
  # mcc object + disasm
  "$MCC" $RT -c "$src" -o "$OUT/${name}-mcc.o" > "$OUT/${name}-12-mcc-obj.txt" 2>&1 || true
  if [ -n "$DIS" ] && [ -f "$OUT/${name}-mcc.o" ]; then
    "$DIS" -d "$OUT/${name}-mcc.o" > "$OUT/${name}-13-mcc-disasm.txt" 2>&1 || true
  fi
  # clang oracle (self-contained; clang finds its own headers)
  if [ -n "$CLANG" ]; then
    "$CLANG" -O0 "$src" -o "$OUT/${name}-clang.exe" > "$OUT/${name}-20-clang-compile.txt" 2>&1
    echo "clang compile rc=$?" >> "$OUT/${name}-20-clang-compile.txt"
    if [ -x "$OUT/${name}-clang.exe" ]; then
      "$OUT/${name}-clang.exe" > "$OUT/${name}-21-clang-run.txt" 2>&1
      echo "clang run rc=$?" >> "$OUT/${name}-21-clang-run.txt"
    fi
  fi
}

diag_one "types/float128" "float128"
diag_one "expressions/integer_promotion" "integer_promotion"

# --- instrumented float128 probe: print each check's value to find the failure ---
"$MCC" $RT "$S/tools/woa-float128-probe.c" -o "$OUT/f128probe.exe" > "$OUT/30-f128probe-compile.txt" 2>&1
echo "probe compile rc=$?" >> "$OUT/30-f128probe-compile.txt"
if [ -x "$OUT/f128probe.exe" ]; then
  "$OUT/f128probe.exe" > "$OUT/31-f128probe-run.txt" 2>&1
  echo "probe run rc=$?" >> "$OUT/31-f128probe-run.txt"
fi

# --- int256 probe: negative-double -> __int256 (T-win-50025) ---
"$MCC" $RT "$S/tools/woa-int256-probe.c" -o "$OUT/i256probe.exe" > "$OUT/32-i256probe-compile.txt" 2>&1
echo "probe compile rc=$?" >> "$OUT/32-i256probe-compile.txt"
if [ -x "$OUT/i256probe.exe" ]; then
  "$OUT/i256probe.exe" > "$OUT/33-i256probe-run.txt" 2>&1
  echo "probe run rc=$?" >> "$OUT/33-i256probe-run.txt"
fi
"$MCC" $RT -c "$S/tools/woa-int256-probe.c" -o "$OUT/i256probe.o" 2>/dev/null || true
if [ -n "$DIS" ] && [ -f "$OUT/i256probe.o" ]; then
  "$DIS" -d "$OUT/i256probe.o" > "$OUT/34-i256probe-disasm.txt" 2>&1 || true
fi
# disasm the REAL runtime int256.c (__mcc_i256_from_f64 + w256_from_double_mag)
"$MCC" $RT -c "$S/runtime/lib/int256.c" -o "$OUT/int256-rt.o" 2>/dev/null || true
if [ -n "$DIS" ] && [ -f "$OUT/int256-rt.o" ]; then
  "$DIS" -d "$OUT/int256-rt.o" > "$OUT/40-int256-rt-disasm.txt" 2>&1 || true
fi

# the committed goldens for reference
grep -nE '"(float128|integer_promotion)"' "$S/tests/exec/goldens.h" > "$OUT/04-goldens.txt" 2>&1 || true

ls -la "$OUT" > "$OUT/_listing.txt" 2>&1 || true
echo "woa-diag done"
