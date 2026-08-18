#!/bin/sh
# T-mac-30103 regression: the two shadowed `v` variables must nest at increasing
# DIE depth (inner block inside outer block), not be siblings / in an empty block.
set -e
MCC="$1"; SRC="$2"; OBJ="$3"
command -v dwarfdump >/dev/null 2>&1 || { echo "SKIP: no dwarfdump"; exit 77; }
"$MCC" -g -c "$SRC" -o "$OBJ" || { echo "FAIL: compile"; exit 1; }
# dwarfdump indents DW_TAG by DIE depth (column of "DW_TAG" grows with nesting)
depths=$(dwarfdump "$OBJ" 2>/dev/null | awk '
  /DW_TAG_variable/ { d=index($0,"DW_TAG"); invar=1; next }
  invar && /DW_AT_name.*\("v"\)/ { print d; invar=0; next }
  { invar=0 }')
n=$(printf '%s\n' "$depths" | grep -c .)
[ "$n" -eq 2 ] || { echo "FAIL: expected 2 'v' variables, got $n"; exit 1; }
d1=$(printf '%s\n' "$depths" | sed -n 1p); d2=$(printf '%s\n' "$depths" | sed -n 2p)
[ "$d2" -gt "$d1" ] || { echo "FAIL: inner v (col $d2) not nested deeper than outer v (col $d1)"; exit 1; }
echo "PASS: outer v col=$d1, inner v col=$d2 (nested)"
