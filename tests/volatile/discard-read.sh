#!/bin/sh
# T-mac-30122(1) regression. A discarded *volatile* lvalue read must emit a
# memory load; a discarded non-volatile read must not. Pre-fix mcc dropped the
# volatile forms entirely (no load in the body). Covers arm64 (ldr) and x86
# (mov from a (%reg)/(%rip) memory operand).
set -e
MCC="$1"; SRC="$2"
asm=$("$MCC" -S -O0 "$SRC" -o - 2>/dev/null) || { echo "FAIL: compile"; exit 1; }
body() { printf '%s\n' "$asm" | awk "/^_?$1:/{f=1;next} f&&/^_?[A-Za-z][A-Za-z0-9_]*:/{exit} f{print}"; }
has_load() { grep -qE '\<ldr\>|mov[a-z]?[[:space:]].*\((%r|%e|%rip)'; }
for fn in f_stmt f_void f_comma f_ptr; do
    if body "$fn" | has_load; then
        echo "PASS: $fn emits a volatile load"
    else
        echo "FAIL: $fn emitted no load for the discarded volatile read"; exit 1
    fi
done
if body nonvol | has_load; then
    echo "FAIL: nonvol emitted a spurious load of a non-volatile discard"; exit 1
fi
echo "PASS: nonvol correctly emits no load (fix is volatile-specific)"
