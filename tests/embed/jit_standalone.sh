#!/bin/sh
# J4A ship-gate test: an --embed-jit executable static-linked against the
# engine archive must be self-contained (no libmcc.so at load) and still
# self-recompile + hot-swap its own function at runtime.
set -e
MCC="$1"
BD="$2"
ARCHIVE="$3"
INC="$4"
WORK="$5"

SRC="$WORK/jit_standalone_prog.c"
EXE="$WORK/jit_standalone_prog"

cat > "$SRC" <<'CEOF'
int printf(const char *, ...);
int f(int x) { return x * 3 + 7; }
int main(void) {
	int r = f(11);
	printf("f(11)=%d\n", r);
	return r == 40 ? 0 : 1;
}
CEOF

MCC_AST_JIT_DISPATCH=6 MCC_EMBED_JIT_LIB="$ARCHIVE" \
	"$MCC" -B"$BD" -I"$INC" -O1 --embed-jit --jit-functions f \
	"$SRC" -o "$EXE" -lm -lpthread -ldl

if readelf -d "$EXE" 2>/dev/null | grep -q 'libmcc'; then
	echo "FAIL: embed exe still has a libmcc dynamic dependency"
	readelf -d "$EXE" | grep NEEDED
	exit 1
fi
echo "OK: no libmcc runtime dependency"

OUT=$(env -u LD_LIBRARY_PATH MCC_JIT_VERBOSE=1 "$EXE" 2>&1) || {
	echo "FAIL: standalone run failed:"
	echo "$OUT"
	exit 1
}
echo "$OUT" | grep -q 'f(11)=40' || {
	echo "FAIL: wrong result:"
	echo "$OUT"
	exit 1
}
echo "$OUT" | grep -q 'swapped' || {
	echo "FAIL: JIT did not swap the slot:"
	echo "$OUT"
	exit 1
}
echo "PASS: self-contained embed exe self-recompiled + hot-swapped standalone"
exit 0
