#!/bin/sh
# T-mac-30182: annotated-unused entities must NOT warn; un-annotated MUST warn.
set -e
MCC="$1"; SRC="$2"; OBJ="$3"
out=$("$MCC" -std=c23 -Wall -c "$SRC" -o "$OBJ" 2>&1 || true)
fail=0
for name in ann_fn ann_var mu_var; do
    if printf '%s\n' "$out" | grep -q "'$name'"; then
        echo "FAIL: annotated '$name' wrongly warned"; fail=1
    else
        echo "PASS: '$name' suppressed"
    fi
done
for name in plain_fn plain_var; do
    if printf '%s\n' "$out" | grep -q "'$name'"; then
        echo "PASS: un-annotated '$name' still warns"
    else
        echo "FAIL: un-annotated '$name' lost its warning"; fail=1
    fi
done
[ "$fail" -eq 0 ] && echo "PASS: unused-attribute suppression is specific"
exit $fail
