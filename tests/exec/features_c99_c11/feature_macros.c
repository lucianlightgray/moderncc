/* Feature-subset / conformance macros (C11 §6.10.8).
 * mcc supports _Complex, atomics, VLAs and threads, so none of the
 * __STDC_NO_* macros may be defined, and the positive conformance macros
 * must be present. Mirrors clang C11/n1460.c (subset macros) and
 * n1482.c (ATOMIC_*_LOCK_FREE). Prints OK on success. */
#include <stdio.h>
#include <stdatomic.h>

#if __STDC_VERSION__ < 201112L
#error "expected __STDC_VERSION__ >= 201112L"
#endif
#ifdef __STDC_NO_COMPLEX__
#error "mcc supports _Complex; __STDC_NO_COMPLEX__ must not be defined"
#endif
#ifdef __STDC_NO_ATOMICS__
#error "mcc supports atomics; __STDC_NO_ATOMICS__ must not be defined"
#endif
#ifdef __STDC_NO_VLA__
#error "mcc supports VLAs; __STDC_NO_VLA__ must not be defined"
#endif
#ifndef __STDC_IEC_559__
#error "expected __STDC_IEC_559__"
#endif
#ifndef __STDC_IEC_559_COMPLEX__
#error "expected __STDC_IEC_559_COMPLEX__"
#endif
#ifndef __STDC_HOSTED__
#error "expected __STDC_HOSTED__"
#endif

static int in_range(int v) { return v >= 0 && v <= 2; }

int main(void) {
	int ok = 1;
	/* ATOMIC_*_LOCK_FREE are integer constants in {0,1,2}. */
	ok &= in_range(ATOMIC_BOOL_LOCK_FREE);
	ok &= in_range(ATOMIC_CHAR_LOCK_FREE);
	ok &= in_range(ATOMIC_SHORT_LOCK_FREE);
	ok &= in_range(ATOMIC_INT_LOCK_FREE);
	ok &= in_range(ATOMIC_LONG_LOCK_FREE);
	ok &= in_range(ATOMIC_LLONG_LOCK_FREE);
	ok &= in_range(ATOMIC_POINTER_LOCK_FREE);
	ok &= (__STDC_ISO_10646__ > 0);
	ok &= (__STDC_UTF_16__ == 1 && __STDC_UTF_32__ == 1);
	ok &= (__STDC_HOSTED__ == 1);
	printf(ok ? "OK\n" : "FAIL\n");
	return !ok;
}
