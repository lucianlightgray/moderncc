#!/bin/sh
# T-mac-30114 regression: `used` forces emission of an unreferenced static inline.
set -e
MCC="$1"; SRC="$2"; OUT="$3"
command -v objdump >/dev/null 2>&1 || { echo "SKIP: no objdump"; exit 77; }
"$MCC" -c "$SRC" -o "$OUT" || { echo "FAIL: compile"; exit 1; }
syms=$(objdump -t "$OUT" 2>/dev/null || true)
echo "$syms" | grep -q 'keepme' || { echo "FAIL: keepme (used) not emitted"; exit 1; }
if echo "$syms" | grep -q 'dropme'; then echo "FAIL: dropme (unused) wrongly emitted"; exit 1; fi
echo "PASS: keepme emitted, dropme dropped"
