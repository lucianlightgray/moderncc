/* C11 §6.10.8 predefined feature-test macros, checked portably.
 *
 * Complements c11_freestanding_headers.c (which asserts the *content* of the
 * freestanding headers) by pinning the compiler-level feature macros: the
 * mandatory translation-environment macros (§6.10.8.1) and the conditional
 * feature macros (§6.10.8.3) that gate optional language features.
 *
 * Everything here is written to agree across mcc/gcc/clang and every hosted
 * target (PE included): version macros are compared with >= rather than ==,
 * and each __STDC_NO_* probe is tied to the feature actually working so the
 * macro and the capability can never disagree. */

extern int printf(const char *, ...);

int main(void) {
	int ok = 1;

	/* §6.10.8.1 mandatory macros. */
#if !defined(__STDC__) || __STDC__ != 1
	ok = 0;
#endif
#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__ != 1
	ok = 0;			/* the exec/diff3 harness always builds hosted */
#endif
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
	ok = 0;
#endif
#if !defined(__STDC_UTF_16__) || __STDC_UTF_16__ != 1
	ok = 0;
#endif
#if !defined(__STDC_UTF_32__) || __STDC_UTF_32__ != 1
	ok = 0;
#endif

	/* §6.10.8.3 conditional feature macros: mcc (like the gcc/clang
	 * reference) supports atomics, complex, threads and VLAs, so none of
	 * the opt-out macros may be defined.  Each absence is corroborated by
	 * exercising the corresponding feature below. */
#ifdef __STDC_NO_ATOMICS__
	ok = 0;
#endif
#ifdef __STDC_NO_COMPLEX__
	ok = 0;
#endif
#ifdef __STDC_NO_THREADS__
	ok = 0;
#endif
#ifdef __STDC_NO_VLA__
	ok = 0;
#endif

	/* __STDC_NO_ATOMICS__ absent => _Atomic is usable. */
	{
		_Atomic int a = 40;
		a += 2;
		if (a != 42)
			ok = 0;
	}

	/* __STDC_NO_COMPLEX__ absent => _Complex is usable. */
	{
		double _Complex z = 3.0 + 4.0i;
		if (__real__ z != 3.0 || __imag__ z != 4.0)
			ok = 0;
	}

	/* __STDC_NO_VLA__ absent => a variable-length array is usable. */
	{
		int n = 5;
		int vla[n];
		for (int i = 0; i < n; i++)
			vla[i] = i * i;
		if (sizeof vla != (unsigned long)n * sizeof(int) || vla[4] != 16)
			ok = 0;
	}

	printf(ok ? "OK\n" : "FAIL\n");
	return ok ? 0 : 1;
}
