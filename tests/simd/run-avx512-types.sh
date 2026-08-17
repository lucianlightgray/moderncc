#!/bin/sh
set -e

# T-lin-10006: the __m512 / __m256h / __m128h intrinsic type SPELLINGS are provided
# by <immintrin.h> (runtime/include/avx512fintrin.h + avx512fp16intrin.h) and parse +
# compute. mcc lowers them generically (no AVX-512 ISA), so the program runs and
# agrees with gcc. Deliberately avoids __m512 ARITHMETIC -- gcc emits native AVX-512
# for that and SIGILLs on a CPU without it (no fleet box has AVX-512); __m512 is
# exercised by subscript + sizeof only, the 128/256-bit __m*h forms do the arithmetic
# (which gcc lowers generically too). Lives outside tests/exec so it stays out of the
# rir-coverage corpus.

MCC=$1
BASE=$2
INC=$3   # runtime/include
[ -n "$MCC" ] && [ -n "$BASE" ] && [ -n "$INC" ] || {
	echo "usage: run-avx512-types.sh <mcc> <build-dir> <runtime-include>" >&2
	exit 2
}

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

cat > "$WORK/arch.c" <<'EOF'
#if !defined(__x86_64__) && !defined(__i386__)
#error not an x86 target
#endif
int main(void) { return 0; }
EOF
if ! "$MCC" -B"$BASE" -w -c "$WORK/arch.c" -o "$WORK/arch.o" 2>/dev/null; then
	echo "avx512-types: SKIP -- native target is not x86 (<immintrin.h> is x86-only)"
	exit 77
fi

cat > "$WORK/t.c" <<'EOF'
#include <immintrin.h>
#include <stdio.h>
__attribute__((noinline)) static __m128h addh(__m128h a, __m128h b) { return a + b; }
__attribute__((noinline)) static __m256h mul2(__m256h a) { return a + a; }
int main(void) {
	int ok = 1, i;
	/* __m512 family: the spelling parses; subscript + sizeof, no AVX-512 arithmetic */
	__m512  z  = {0}; z[0] = 2.5f; z[15] = 9.0f;
	__m512d zd = {0}; zd[7] = 3.0;
	__m512i zi = {0}; zi[7] = 42;
	if ((int)z[0] != 2 || (int)z[15] != 9 || (int)zd[7] != 3 || (int)zi[7] != 42) ok = 0;
	if (sizeof(__m512) != 64 || sizeof(__m512d) != 64 || sizeof(__m512i) != 64) ok = 0;
	/* __m256h / __m128h: full arithmetic (generic lowering, gcc agrees) */
	__m128h a = {1,2,3,4,5,6,7,8}, b = {10,10,10,10,10,10,10,10};
	__m128h c = addh(a, b);
	for (i = 0; i < 8; i++) if ((float)c[i] != (float)(i + 1) + 10.0f) ok = 0;
	__m256h w = {0}; w[0] = (_Float16)3; w[15] = (_Float16)4; w = mul2(w);
	if ((float)w[0] != 6.0f || (float)w[15] != 8.0f) ok = 0;
	if (sizeof(__m256h) != 32 || sizeof(__m128h) != 16) ok = 0;
	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}
EOF

if ! "$MCC" -B"$BASE" -w "-I$INC" "$WORK/t.c" -o "$WORK/t" 2>"$WORK/err"; then
	echo "FAIL avx512-types: mcc did not compile the __m512/__m256h/__m128h spellings" >&2
	cat "$WORK/err" >&2
	exit 1
fi
out=$("$WORK/t" || true)
if [ "$out" != "OK" ]; then
	echo "FAIL avx512-types: mcc ran the spellings but got '$out' (expected OK)" >&2
	exit 1
fi

# gcc parity, when available: same program, must also print OK (proves the values are
# not a private mcc convention). Skip if gcc is absent.
if command -v gcc >/dev/null 2>&1; then
	if gcc -w "$WORK/t.c" -o "$WORK/tg" 2>/dev/null; then
		gout=$("$WORK/tg" 2>/dev/null || true)
		if [ "$gout" != "OK" ]; then
			echo "FAIL avx512-types: gcc got '$gout' where mcc got OK -- a real divergence" >&2
			exit 1
		fi
	fi
fi

echo "avx512-types: OK -- __m512{,d,i} + __m256h + __m128h parse via <immintrin.h>, compute, and match gcc (128/256-bit arithmetic; __m512 subscript+sizeof)"
