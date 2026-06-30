#!/bin/sh
# Preprocessor differential test suite.
#
# Runs every tests/preprocess/**/*.{c,S} through `mcc -E` and verifies it against
# the gcc==clang consensus, modelled on the diff3 suite:
#
#   * transformation tests (everything outside diagnostics/) — the normalized
#     `mcc -E` token stream must equal the normalized `gcc -E`==`clang -E`
#     output. If gcc and clang themselves disagree there is no consensus to hold
#     mcc to, so the case is SKIPPED (reported, not failed).
#
#   * diagnostics/ tests (negative cases) — `mcc -E` must emit a diagnostic
#     (error or warning). Enforced only when gcc or clang also diagnoses, so the
#     suite tracks a real consensus rather than an mcc-only quirk.
#
# Usage: run_preprocess.sh <mcc> <build-dir(-B)> <include-dir(-I)> <test-dir> [gcc] [clang]
# Exit: 0 if no real divergence, 1 on any FAIL. Skips (no host gcc/clang) -> 77.
set -u
MCC=${1:?mcc}; BDIR=${2:?build}; IDIR=${3:?include}; TDIR=${4:?testdir}
GCC=${5:-gcc}; CLANG=${6:-clang}

command -v "$GCC"   >/dev/null 2>&1 || { echo "SKIP: no gcc";   exit 77; }
command -v "$CLANG" >/dev/null 2>&1 || { echo "SKIP: no clang"; exit 77; }

mcc_E()   { "$MCC" -B"$BDIR" -I"$IDIR" -E "$1" 2>/dev/null; }
# Normalize a preprocessed token stream: drop line markers (# 1 "file") and
# blank lines, collapse runs of whitespace, trim ends. Cosmetic-only differences
# (spacing, line directives) are not divergences.
norm()    { grep -v '^#' | grep -v '^[[:space:]]*$' | sed 's/[[:space:]][[:space:]]*/ /g; s/^ //; s/ $//'; }
diag()    { "$1" -E "$2" 2>&1 >/dev/null | grep -qiE 'error|warning'; }

PASS=0; SKIP=0; FAIL=0; FAILED=''
for f in $(find "$TDIR" \( -name '*.c' -o -name '*.S' \) | sort); do
  rel=${f#"$TDIR"/}
  case "$rel" in
    *.S)
      # asm preprocessing: `#` is a GAS comment and #line marker emission differs
      # cosmetically across cpp implementations (path vs basename), so the strict
      # token differential does not apply. Smoke-test instead — `mcc -E` must
      # terminate cleanly (these guard against the historical pp hangs/crashes on
      # asm input), bounded by a timeout.
      if timeout 10 "$MCC" -B"$BDIR" -I"$IDIR" -E "$f" >/dev/null 2>&1; then
        PASS=$((PASS+1)); [ "${V:-0}" = 1 ] && echo "ok   $rel (asm -E smoke)"
      else
        FAIL=$((FAIL+1)); FAILED="$FAILED $rel(asm-E-crash/hang)"; echo "FAIL $rel: mcc -E crashed or hung on asm input"
      fi
      ;;
    diagnostics/*)
      # negative case: require a diagnostic, gated on a gcc/clang consensus
      if diag "$GCC" "$f" || diag "$CLANG" "$f"; then
        if "$MCC" -B"$BDIR" -I"$IDIR" -E "$f" 2>&1 >/dev/null | grep -qiE 'error|warning'; then
          PASS=$((PASS+1)); [ "${V:-0}" = 1 ] && echo "ok   $rel (diagnosed)"
        else
          FAIL=$((FAIL+1)); FAILED="$FAILED $rel(no-diagnostic)"; echo "FAIL $rel: mcc emits no diagnostic (gcc/clang do)"
        fi
      else
        SKIP=$((SKIP+1)); [ "${V:-0}" = 1 ] && echo "skip $rel (gcc/clang do not diagnose)"
      fi
      ;;
    *)
      g=$("$GCC" -E "$f" 2>/dev/null | norm)
      c=$("$CLANG" -E "$f" 2>/dev/null | norm)
      if [ "$g" != "$c" ]; then
        SKIP=$((SKIP+1)); [ "${V:-0}" = 1 ] && echo "skip $rel (no gcc==clang consensus)"; continue
      fi
      m=$(mcc_E "$f" | norm)
      if [ "$m" = "$g" ]; then
        PASS=$((PASS+1)); [ "${V:-0}" = 1 ] && echo "ok   $rel"
      else
        FAIL=$((FAIL+1)); FAILED="$FAILED $rel"
        echo "FAIL $rel: mcc -E diverges from gcc==clang"
        [ "${V:-0}" = 1 ] && diff <(printf '%s\n' "$g") <(printf '%s\n' "$m") | head -20
      fi
      ;;
  esac
done

echo "preprocess-suite: PASS=$PASS SKIP=$SKIP FAIL=$FAIL"
[ "$FAIL" -eq 0 ] || { echo "failed:$FAILED"; exit 1; }
exit 0
