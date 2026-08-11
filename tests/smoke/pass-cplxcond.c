extern int printf(const char *, ...);

static int smp_calls;

static _Complex int smp_get(_Complex int v)
{
	smp_calls++;
	return v;
}

static _Complex double smp_getd(_Complex double v)
{
	smp_calls++;
	return v;
}

int main(void)
{
	_Complex int r = 5 + 6i;
	_Complex int nz = smp_get(1 + 2i) ? : r;
	_Complex int z = smp_get(0) ? : r;
	_Complex int imagonly = smp_get(0 + 7i) ? : r;
	_Complex double dr = 9.0 + 8.0i;
	_Complex double dnz = smp_getd(3.0 + 4.0i) ? : dr;
	_Complex double dz = smp_getd(0.0) ? : dr;

	printf("cplxcond %d %d %d %d %d %d %.1f %.1f %.1f %.1f %d\n",
				 (int)__real__ nz, (int)__imag__ nz, (int)__real__ z,
				 (int)__imag__ z, (int)__real__ imagonly, (int)__imag__ imagonly,
				 (double)__real__ dnz, (double)__imag__ dnz, (double)__real__ dz,
				 (double)__imag__ dz, smp_calls);
	return 0;
}
