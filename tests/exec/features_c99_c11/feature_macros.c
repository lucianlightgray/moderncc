extern int printf(const char *, ...);

int main(void) {
	int ok = 1;

#if !defined(__STDC__) || __STDC__ != 1
	ok = 0;
#endif
#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__ != 1
	ok = 0;
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

	{
		_Atomic int a = 40;
		a += 2;
		if (a != 42)
			ok = 0;
	}

	{
		double _Complex z = 3.0 + 4.0i;
		if (__real__ z != 3.0 || __imag__ z != 4.0)
			ok = 0;
	}

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
