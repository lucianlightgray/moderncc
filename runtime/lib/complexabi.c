extern void __mcc_cmulf(float *res, float a, float b, float c, float d);
extern void __mcc_cmul(double *res, double a, double b, double c, double d);
extern void __mcc_cmull(long double *res, long double a, long double b,
												long double c, long double d);
extern void __mcc_cdivf(float *res, float a, float b, float c, float d);
extern void __mcc_cdiv(double *res, double a, double b, double c, double d);
extern void __mcc_cdivl(long double *res, long double a, long double b,
												long double c, long double d);

#define GEN_ABI(NAME, IMPL, T)                    \
	T _Complex NAME(T a, T b, T c, T d) {           \
		T r[2];                                       \
		T _Complex z;                                 \
		IMPL(r, a, b, c, d);                          \
		__real__ z = r[0];                            \
		__imag__ z = r[1];                            \
		return z;                                     \
	}

GEN_ABI(__mulsc3, __mcc_cmulf, float)
GEN_ABI(__muldc3, __mcc_cmul, double)
GEN_ABI(__divsc3, __mcc_cdivf, float)
GEN_ABI(__divdc3, __mcc_cdiv, double)

#if defined(__i386__) || defined(__x86_64__)
GEN_ABI(__mulxc3, __mcc_cmull, long double)
GEN_ABI(__divxc3, __mcc_cdivl, long double)
#else
GEN_ABI(__multc3, __mcc_cmull, long double)
GEN_ABI(__divtc3, __mcc_cdivl, long double)
#endif
