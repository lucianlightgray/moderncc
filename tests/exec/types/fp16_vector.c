/* T-lin-10006: _Float16 (and __bf16) as a vector element type -- the gap that
 * blocked the __m128h / __m256h intrinsic spellings. mcc used to refuse
 * `_Float16 __attribute__((vector_size(N)))` ("applies to integer or floating-
 * point types only"); it lowers such vectors generically (no AVX512-FP16 ISA), so
 * they run and agree with gcc on any CPU. Self-checking: prints OK iff every
 * arithmetic/subscript/size result is the expected value (gcc prints OK too). */
#include <stdio.h>

typedef _Float16 v8h  __attribute__((__vector_size__(16)));   /* __m128h: 8 x _Float16 */
typedef _Float16 v16h __attribute__((__vector_size__(32)));   /* __m256h: 16 x _Float16 */

__attribute__((noinline)) static v8h vadd(v8h a, v8h b) { return a + b; }
__attribute__((noinline)) static v8h vmul(v8h a, v8h b) { return a * b; }

int main(void) {
	int ok = 1, i;
	v8h a = {1, 2, 3, 4, 5, 6, 7, 8};
	v8h b = {10, 10, 10, 10, 10, 10, 10, 10};
	v8h s = vadd(a, b);          /* pass + return _Float16 vectors by value */
	v8h p = vmul(a, b);
	for (i = 0; i < 8; i++) {
		if ((float)s[i] != (float)(i + 1) + 10.0f) ok = 0;
		if ((float)p[i] != (float)(i + 1) * 10.0f) ok = 0;
	}
	v16h w = {0};
	w[0] = (_Float16)3;
	w[15] = (_Float16)4;
	w = w + w;                   /* 256-bit _Float16 vector arithmetic */
	if ((float)w[0] != 6.0f || (float)w[15] != 8.0f) ok = 0;
	if (sizeof(v8h) != 16 || sizeof(v16h) != 32) ok = 0;
	/* NB: _Alignof(v16h) is deliberately not asserted -- gcc without -mavx caps a
	 * 32-byte vector's alignment at 16 while mcc and clang use the natural 32, an
	 * ABI-alignment quirk, not an arithmetic difference. */
	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}
