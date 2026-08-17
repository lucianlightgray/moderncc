#!/bin/sh
# T-mac-30031: C23 <stddef.h> unreachable() must be __builtin_unreachable(),
# not ((void)0) — the no-op defeats the UB-if-reached contract and misses the
# optimization (a switch whose default is unreachable() then falsely warns
# "function might return no value"). Guards the one-line header fix without
# growing the walked coverage corpora (this is a shell cell, not tests/exec).
set -u

MCC="$1"
WORK="$2"

mkdir -p "$WORK" 2>/dev/null || exit 2
cd "$WORK" || exit 2
[ -x "$MCC" ] || exit 77

cat > u.c <<'EOF'
#include <stddef.h>
#include <stdio.h>
static int classify(int x) {
	switch (x) {
	case 0: return 10;
	case 1: return 20;
	default: unreachable();
	}
}
int main(void) { printf("%d\n", classify(0) + classify(1)); return 0; }
EOF

# 1. the macro must expand to the builtin, not the no-op
exp=$("$MCC" -E -P u.c 2>&1)
case "$exp" in
	*"__builtin_unreachable"*) : ;;
	*) echo "FAIL: unreachable() did not expand to __builtin_unreachable():"; echo "$exp" | grep -i default; exit 1 ;;
esac

# 2. it must compile WITHOUT the false 'might return no value' warning and run
out=$("$MCC" u.c -o u.exe 2>&1)
rc=$?
if [ "$rc" -ne 0 ]; then
	echo "FAIL: compile failed (rc=$rc): $out"
	exit 1
fi
case "$out" in
	*"might return no value"*)
		echo "FAIL: unreachable() default still warns 'might return no value' (no-op semantics)"
		exit 1 ;;
esac
run=$(./u.exe 2>&1)
if [ "$run" != "30" ]; then
	echo "FAIL: expected 30, got '$run'"
	exit 1
fi

echo "PASS: unreachable() is __builtin_unreachable(); no false warning; runs"
exit 0
