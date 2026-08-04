#ifdef __MCC__
#define BUILTIN(x) __builtin_##x
#define BUILTINN(x) "__builtin_" #x
#else
#define BUILTIN(x) __mcc_builtin_##x
#define BUILTINN(x) "__mcc_builtin_" #x
#endif

#define MCC_STR2(x) #x
#define MCC_STR(x) MCC_STR2(x)
#define MCC_ULP MCC_STR(__USER_LABEL_PREFIX__)
#if defined __APPLE__ && !defined __MCC__
#define MCC_ALIAS(decl, aliasnm, target) \
	__asm__(".globl " MCC_ULP aliasnm "\n\t.set " MCC_ULP aliasnm "," MCC_ULP target)
#else
#define MCC_ALIAS(decl, aliasnm, target) decl __attribute__((alias(target)))
#endif

static const unsigned char table_1_32[] = {
		0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
		31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9};
static const unsigned char table_2_32[32] = {
		31, 22, 30, 21, 18, 10, 29, 2, 20, 17, 15, 13, 9, 6, 28, 1,
		23, 19, 11, 3, 16, 14, 7, 24, 12, 4, 8, 25, 5, 26, 27, 0};
static const unsigned char table_1_64[] = {
		0, 1, 2, 53, 3, 7, 54, 27, 4, 38, 41, 8, 34, 55, 48, 28,
		62, 5, 39, 46, 44, 42, 22, 9, 24, 35, 59, 56, 49, 18, 29, 11,
		63, 52, 6, 26, 37, 40, 33, 47, 61, 45, 43, 21, 23, 58, 17, 10,
		51, 25, 36, 32, 60, 20, 57, 16, 50, 31, 19, 15, 30, 14, 13, 12};
static const unsigned char table_2_64[] = {
		63, 16, 62, 7, 15, 36, 61, 3, 6, 14, 22, 26, 35, 47, 60, 2,
		9, 5, 28, 11, 13, 21, 42, 19, 25, 31, 34, 40, 46, 52, 59, 1,
		17, 8, 37, 4, 23, 27, 48, 10, 29, 12, 43, 20, 32, 41, 53, 18,
		38, 24, 49, 30, 44, 33, 54, 39, 50, 45, 55, 51, 56, 57, 58, 0};

#define FFSI(x) \
	return table_1_32[((x & -x) * 0x077cb531u) >> 27] + (x != 0);
#define FFSL(x) \
	return table_1_64[((x & -x) * 0x022fdd63cc95386dull) >> 58] + (x != 0);
#define CTZI(x) \
	return table_1_32[((x & -x) * 0x077cb531u) >> 27];
#define CTZL(x) \
	return table_1_64[((x & -x) * 0x022fdd63cc95386dull) >> 58];
#define CLZI(x) \
	x |= x >> 1;  \
	x |= x >> 2;  \
	x |= x >> 4;  \
	x |= x >> 8;  \
	x |= x >> 16; \
	return table_2_32[(x * 0x07c4acddu) >> 27];
#define CLZL(x) \
	x |= x >> 1;  \
	x |= x >> 2;  \
	x |= x >> 4;  \
	x |= x >> 8;  \
	x |= x >> 16; \
	x |= x >> 32; \
	return table_2_64[x * 0x03f79d71b4cb0a89ull >> 58];
#define POPCOUNTI(x, m)                           \
	x = x - ((x >> 1) & 0x55555555);                \
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333); \
	x = (x + (x >> 4)) & 0xf0f0f0f;                 \
	return ((x * 0x01010101) >> 24) & m;
#define POPCOUNTL(x, m)                                                 \
	x = x - ((x >> 1) & 0x5555555555555555ull);                           \
	x = (x & 0x3333333333333333ull) + ((x >> 2) & 0x3333333333333333ull); \
	x = (x + (x >> 4)) & 0xf0f0f0f0f0f0f0full;                            \
	return ((x * 0x0101010101010101ull) >> 56) & m;

int BUILTIN(ffs)(int x) {
	FFSI(x)
}

int BUILTIN(ffsll)(long long x){
		FFSL(x)}
#if __SIZEOF_LONG__ == 4
MCC_ALIAS(int BUILTIN(ffsl)(long x), BUILTINN(ffsl), BUILTINN(ffs));
#else
MCC_ALIAS(int BUILTIN(ffsl)(long x), BUILTINN(ffsl), BUILTINN(ffsll));
#endif

int BUILTIN(clz)(unsigned int x) {
	CLZI(x)
}

int BUILTIN(clzll)(unsigned long long x){
		CLZL(x)}
#if __SIZEOF_LONG__ == 4
MCC_ALIAS(int BUILTIN(clzl)(unsigned long x), BUILTINN(clzl), BUILTINN(clz));
#else
MCC_ALIAS(int BUILTIN(clzl)(unsigned long x), BUILTINN(clzl), BUILTINN(clzll));
#endif

int BUILTIN(ctz)(unsigned int x) {
	CTZI(x)
}

int BUILTIN(ctzll)(unsigned long long x){
		CTZL(x)}
#if __SIZEOF_LONG__ == 4
MCC_ALIAS(int BUILTIN(ctzl)(unsigned long x), BUILTINN(ctzl), BUILTINN(ctz));
#else
MCC_ALIAS(int BUILTIN(ctzl)(unsigned long x), BUILTINN(ctzl), BUILTINN(ctzll));
#endif

int BUILTIN(clrsb)(int x) {
	if (x < 0)
		x = ~x;
	x <<= 1;
	CLZI(x)
}

int BUILTIN(clrsbll)(long long x) {
	if (x < 0)
		x = ~x;
	x <<= 1;
	CLZL(x)
}
#if __SIZEOF_LONG__ == 4
MCC_ALIAS(int BUILTIN(clrsbl)(long x), BUILTINN(clrsbl), BUILTINN(clrsb));
#else
MCC_ALIAS(int BUILTIN(clrsbl)(long x), BUILTINN(clrsbl), BUILTINN(clrsbll));
#endif

int BUILTIN(popcount)(unsigned int x) {
	POPCOUNTI(x, 0x3f)
}

int BUILTIN(popcountll)(unsigned long long x){
		POPCOUNTL(x, 0x7f)}
#if __SIZEOF_LONG__ == 4
MCC_ALIAS(int BUILTIN(popcountl)(unsigned long x), BUILTINN(popcountl), BUILTINN(popcount));
#else
MCC_ALIAS(int BUILTIN(popcountl)(unsigned long x), BUILTINN(popcountl), BUILTINN(popcountll));
#endif

int BUILTIN(parity)(unsigned int x) {
	POPCOUNTI(x, 0x01)
}

int BUILTIN(parityll)(unsigned long long x){
		POPCOUNTL(x, 0x01)}
#if __SIZEOF_LONG__ == 4
MCC_ALIAS(int BUILTIN(parityl)(unsigned long x), BUILTINN(parityl), BUILTINN(parity));
#else
MCC_ALIAS(int BUILTIN(parityl)(unsigned long x), BUILTINN(parityl), BUILTINN(parityll));
#endif

unsigned short BUILTIN(bswap16)(unsigned short x) {
	return (unsigned short)((x >> 8) | (x << 8));
}

unsigned int BUILTIN(bswap32)(unsigned int x) {
	return ((x & 0xff000000u) >> 24) | ((x & 0x00ff0000u) >> 8) | ((x & 0x0000ff00u) << 8) | ((x & 0x000000ffu) << 24);
}

int __mcc_signbitf(float x) {
	union {
		float f;
		unsigned int u;
	} u;
	u.f = x;
	return (int)(u.u >> 31);
}

int __mcc_signbit(double x) {
	union {
		double d;
		unsigned long long u;
	} u;
	u.d = x;
	return (int)(u.u >> 63);
}

int __mcc_signbitl(long double x) {
	union {
		long double ld;
		unsigned char b[sizeof(long double)];
	} u;
	u.ld = x;
#if defined __i386__ || defined __x86_64__

	return sizeof(long double) > 8 ? u.b[9] >> 7 : u.b[7] >> 7;
#else

	return u.b[sizeof(long double) - 1] >> 7;
#endif
}

#if defined _WIN32 || defined __arm__ || \
		(defined __aarch64__ && (defined __APPLE__ || defined _WIN32))
#define MCC_NAN_LD_DBL 1
#elif defined __i386__ || defined __x86_64__
#define MCC_NAN_LD_X87 1
#else
#define MCC_NAN_LD_Q128 1
#endif

static int mcc_nan_payload(const char *s, unsigned long long *out) {
	unsigned long long v = 0;
	int base = 10;
	*out = 0;
	if (!s)
		return 0;
	while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\v' || *s == '\f' ||
				 *s == '\r')
		s++;
	if (*s == '-' || *s == '+')
		s++;
	if (*s == '0') {
		s++;
		if (*s == 'x' || *s == 'X') {
			base = 16;
			s++;
		} else {
			base = 8;
		}
	}
	for (;;) {
		int d;
		if (*s >= '0' && *s <= '9')
			d = *s - '0';
		else if (*s >= 'a' && *s <= 'f')
			d = *s - 'a' + 10;
		else if (*s >= 'A' && *s <= 'F')
			d = *s - 'A' + 10;
		else
			break;
		if (d >= base)
			break;
		v = v * (unsigned long long)base + (unsigned long long)d;
		s++;
	}
	*out = v;
	return *s == 0;
}

float __mcc_nansf(const char *s) {
	union {
		unsigned u;
		float f;
	} u;
	unsigned long long v;
	unsigned p;
	if (!mcc_nan_payload(s, &v))
		v = 0;
	p = (unsigned)(v & 0x3fffffu);
	if (!p)
		p = 0x200000u;
	u.u = 0x7f800000u | p;
	return u.f;
}

double __mcc_nans(const char *s) {
	union {
		unsigned long long u;
		double d;
	} u;
	unsigned long long v, p;
	if (!mcc_nan_payload(s, &v))
		v = 0;
	p = v & 0x7ffffffffffffULL;
	if (!p)
		p = 0x4000000000000ULL;
	u.u = 0x7ff0000000000000ULL | p;
	return u.d;
}

long double __mcc_nansl(const char *s) {
	union {
		long double ld;
		unsigned char b[sizeof(long double)];
	} u;
	unsigned long long v, lo, hi;
	unsigned i, n;
	if (!mcc_nan_payload(s, &v))
		v = 0;
#if defined MCC_NAN_LD_X87
	lo = v & 0x3fffffffffffffffULL;
	if (!lo)
		lo = 0x2000000000000000ULL;
	lo |= 0x8000000000000000ULL;
	hi = 0x7fffULL;
	n = 2;
#elif defined MCC_NAN_LD_Q128
	lo = v;
	hi = 0x7fff000000000000ULL;
	if (!lo)
		hi |= 0x400000000000ULL;
	n = 8;
#else
	lo = v & 0x7ffffffffffffULL;
	if (!lo)
		lo = 0x4000000000000ULL;
	lo |= 0x7ff0000000000000ULL;
	hi = 0;
	n = 0;
#endif
	for (i = 0; i < sizeof u.b; i++)
		u.b[i] = 0;
	for (i = 0; i < 8; i++)
		u.b[i] = (unsigned char)(lo >> (8 * i));
	for (i = 0; i < n; i++)
		u.b[8 + i] = (unsigned char)(hi >> (8 * i));
	return u.ld;
}

unsigned long long BUILTIN(bswap64)(unsigned long long x) {
	return ((x & 0xff00000000000000ull) >> 56) | ((x & 0x00ff000000000000ull) >> 40) | ((x & 0x0000ff0000000000ull) >> 24) | ((x & 0x000000ff00000000ull) >> 8) | ((x & 0x00000000ff000000ull) << 8) | ((x & 0x0000000000ff0000ull) << 24) | ((x & 0x000000000000ff00ull) << 40) | ((x & 0x00000000000000ffull) << 56);
}

#ifndef __MCC__
#if defined __APPLE__
#define MCC_EXPORT(sfx)                                                             \
	__asm__(".globl " MCC_ULP "__builtin_" #sfx "\n\t.set " MCC_ULP "__builtin_" #sfx \
					"," MCC_ULP "__mcc_builtin_" #sfx)
MCC_EXPORT(ffs);
MCC_EXPORT(ffsl);
MCC_EXPORT(ffsll);
MCC_EXPORT(clz);
MCC_EXPORT(clzl);
MCC_EXPORT(clzll);
MCC_EXPORT(ctz);
MCC_EXPORT(ctzl);
MCC_EXPORT(ctzll);
MCC_EXPORT(clrsb);
MCC_EXPORT(clrsbl);
MCC_EXPORT(clrsbll);
MCC_EXPORT(popcount);
MCC_EXPORT(popcountl);
MCC_EXPORT(popcountll);
MCC_EXPORT(parity);
MCC_EXPORT(parityl);
MCC_EXPORT(parityll);
MCC_EXPORT(bswap16);
MCC_EXPORT(bswap32);
MCC_EXPORT(bswap64);
#undef MCC_EXPORT
#else
#if defined(__GNUC__) && (__GNUC__ >= 6)
__asm__(".globl  __builtin_ffs");
__asm__(".set __builtin_ffs,__mcc_builtin_ffs");
__asm__(".globl  __builtin_ffsl");
__asm__(".set __builtin_ffsl,__mcc_builtin_ffsl");
__asm__(".globl  __builtin_ffsll");
__asm__(".set __builtin_ffsll,__mcc_builtin_ffsll");
#else
int __builtin_ffs(int x) __attribute__((alias("__mcc_builtin_ffs")));
int __builtin_ffsl(long x) __attribute__((alias("__mcc_builtin_ffsl")));
int __builtin_ffsll(long long x) __attribute__((alias("__mcc_builtin_ffsll")));
#endif
int __builtin_clz(unsigned int x) __attribute__((alias("__mcc_builtin_clz")));
int __builtin_clzl(unsigned long x) __attribute__((alias("__mcc_builtin_clzl")));
int __builtin_clzll(unsigned long long x) __attribute__((alias("__mcc_builtin_clzll")));
int __builtin_ctz(unsigned int x) __attribute__((alias("__mcc_builtin_ctz")));
int __builtin_ctzl(unsigned long x) __attribute__((alias("__mcc_builtin_ctzl")));
int __builtin_ctzll(unsigned long long x) __attribute__((alias("__mcc_builtin_ctzll")));
int __builtin_clrsb(int x) __attribute__((alias("__mcc_builtin_clrsb")));
int __builtin_clrsbl(long x) __attribute__((alias("__mcc_builtin_clrsbl")));
int __builtin_clrsbll(long long x) __attribute__((alias("__mcc_builtin_clrsbll")));
int __builtin_popcount(unsigned int x) __attribute__((alias("__mcc_builtin_popcount")));
int __builtin_popcountl(unsigned long x) __attribute__((alias("__mcc_builtin_popcountl")));
int __builtin_popcountll(unsigned long long x) __attribute__((alias("__mcc_builtin_popcountll")));
int __builtin_parity(unsigned int x) __attribute__((alias("__mcc_builtin_parity")));
int __builtin_parityl(unsigned long x) __attribute__((alias("__mcc_builtin_parityl")));
int __builtin_parityll(unsigned long long x) __attribute__((alias("__mcc_builtin_parityll")));
unsigned short __builtin_bswap16(unsigned short x) __attribute__((alias("__mcc_builtin_bswap16")));
unsigned int __builtin_bswap32(unsigned int x) __attribute__((alias("__mcc_builtin_bswap32")));
__asm__(".globl __builtin_bswap64\n\t.set __builtin_bswap64,__mcc_builtin_bswap64");
#endif
#endif

#define MCC_OV_SMALL_U(T, W, NM)                \
	int __mcc_addo_##NM(W a, W b, T *r) {         \
		W s = a + b;                                \
		*r = (T)s;                                  \
		return s < a || (T)s != s;                  \
	}                                             \
	int __mcc_subo_##NM(W a, W b, T *r) {         \
		W s = a - b;                                \
		*r = (T)s;                                  \
		return a < b || (T)s != s;                  \
	}                                             \
	int __mcc_mulo_##NM(W a, W b, T *r) {         \
		W s = a * b;                                \
		*r = (T)s;                                  \
		return (a != 0 && s / a != b) || (T)s != s; \
	}

#define MCC_OV_SMALL_S(T, W, TMIN, NM)                                    \
	int __mcc_addo_##NM(W a, W b, T *r) {                                   \
		unsigned long long u = (unsigned long long)a + (unsigned long long)b; \
		W s = (W)u;                                                           \
		*r = (T)s;                                                            \
		return (~(a ^ b) & (a ^ s)) < 0 || (T)s != s;                        \
	}                                                                       \
	int __mcc_subo_##NM(W a, W b, T *r) {                                   \
		unsigned long long u = (unsigned long long)a - (unsigned long long)b; \
		W s = (W)u;                                                           \
		*r = (T)s;                                                            \
		return ((a ^ b) & (a ^ s)) < 0 || (T)s != s;                         \
	}                                                                       \
	int __mcc_mulo_##NM(W a, W b, T *r) {                                   \
		unsigned long long u = (unsigned long long)a * (unsigned long long)b; \
		W s = (W)u;                                                           \
		int wov;                                                             \
		*r = (T)s;                                                            \
		if (a == 0 || b == 0)                                                \
			wov = 0;                                                           \
		else if (a == -1)                                                    \
			wov = (b == (TMIN));                                              \
		else if (b == -1)                                                    \
			wov = (a == (TMIN));                                              \
		else                                                                 \
			wov = (s / a != b);                                              \
		return wov || (T)s != s;                                             \
	}

#define MCC_OV_BIG_S(T, TMIN, NM)                                         \
	int __mcc_addo_##NM(T a, T b, T *r) {                                   \
		unsigned long long u = (unsigned long long)a + (unsigned long long)b; \
		*r = (T)u;                                                            \
		return (~(a ^ b) & (a ^ (T)u)) < 0;                                   \
	}                                                                       \
	int __mcc_subo_##NM(T a, T b, T *r) {                                   \
		unsigned long long u = (unsigned long long)a - (unsigned long long)b; \
		*r = (T)u;                                                            \
		return ((a ^ b) & (a ^ (T)u)) < 0;                                    \
	}                                                                       \
	int __mcc_mulo_##NM(T a, T b, T *r) {                                   \
		unsigned long long u = (unsigned long long)a * (unsigned long long)b; \
		*r = (T)u;                                                            \
		if (a == 0 || b == 0)                                                 \
			return 0;                                                           \
		if (a == -1)                                                          \
			return b == (TMIN);                                                 \
		if (b == -1)                                                          \
			return a == (TMIN);                                                 \
		return (T)u / a != b;                                                 \
	}

#define MCC_OV_BIG_U(T, NM)             \
	int __mcc_addo_##NM(T a, T b, T *r) { \
		*r = a + b;                         \
		return *r < a;                      \
	}                                     \
	int __mcc_subo_##NM(T a, T b, T *r) { \
		*r = a - b;                         \
		return a < b;                       \
	}                                     \
	int __mcc_mulo_##NM(T a, T b, T *r) { \
		*r = a * b;                         \
		return a != 0 && *r / a != b;       \
	}

MCC_OV_SMALL_S(signed char, long long, (-9223372036854775807LL - 1), sc)
MCC_OV_SMALL_S(char, long long, (-9223372036854775807LL - 1), c)
MCC_OV_SMALL_S(short, long long, (-9223372036854775807LL - 1), s)
MCC_OV_SMALL_S(int, long long, (-9223372036854775807LL - 1), i)
MCC_OV_SMALL_U(unsigned char, unsigned long long, uc)
MCC_OV_SMALL_U(unsigned short, unsigned long long, us)
MCC_OV_SMALL_U(unsigned int, unsigned long long, u)
MCC_OV_BIG_S(long long, (-9223372036854775807LL - 1), ll)
MCC_OV_BIG_U(unsigned long long, ull)
#if __SIZEOF_LONG__ == 8
MCC_OV_BIG_S(long, (-9223372036854775807LL - 1), l)
MCC_OV_BIG_U(unsigned long, ul)
#else
MCC_OV_SMALL_S(long, long long, (-9223372036854775807LL - 1), l)
MCC_OV_SMALL_U(unsigned long, unsigned long long, ul)
#endif
