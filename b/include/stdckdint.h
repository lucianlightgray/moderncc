#ifndef _STDCKDINT_H
#define _STDCKDINT_H

#include <limits.h>

#define __STDC_VERSION_STDCKDINT_H__ 202311L

#define __mcc_ckd_is_unsigned(x) _Generic((x), \
	_Bool: 1, \
	unsigned char: 1, \
	unsigned short: 1, \
	unsigned int: 1, \
	unsigned long: 1, \
	unsigned long long: 1, \
	default: 0)

#define __mcc_ckd_min(x) _Generic((x), \
	_Bool: (__int128)0, \
	char: (__int128)CHAR_MIN, \
	signed char: (__int128)SCHAR_MIN, \
	short: (__int128)SHRT_MIN, \
	int: (__int128)INT_MIN, \
	long: (__int128)LONG_MIN, \
	long long: (__int128)LLONG_MIN, \
	unsigned char: (__int128)0, \
	unsigned short: (__int128)0, \
	unsigned int: (__int128)0, \
	unsigned long: (__int128)0, \
	unsigned long long: (__int128)0, \
	default: (__int128)0)

#define __mcc_ckd_max(x) _Generic((x), \
	_Bool: (__int128)1, \
	char: (__int128)CHAR_MAX, \
	signed char: (__int128)SCHAR_MAX, \
	short: (__int128)SHRT_MAX, \
	int: (__int128)INT_MAX, \
	long: (__int128)LONG_MAX, \
	long long: (__int128)LLONG_MAX, \
	unsigned char: (__int128)UCHAR_MAX, \
	unsigned short: (__int128)USHRT_MAX, \
	unsigned int: (__int128)UINT_MAX, \
	unsigned long: (__int128)ULONG_MAX, \
	unsigned long long: (__int128)ULLONG_MAX, \
	default: (__int128)0)

#define ckd_add(r, a, b) (__extension__({ \
	__typeof__(+(a)) __ckd_a = (a); \
	__typeof__(+(b)) __ckd_b = (b); \
	__typeof__(*(r)) *__ckd_r = (r); \
	__int128 __ckd_res = (__int128)__ckd_a + (__int128)__ckd_b; \
	_Bool __ckd_ovf = __ckd_res < __mcc_ckd_min(*__ckd_r) || \
			   __ckd_res > __mcc_ckd_max(*__ckd_r); \
	*__ckd_r = (__typeof__(*__ckd_r))__ckd_res; \
	__ckd_ovf; \
}))

#define ckd_sub(r, a, b) (__extension__({ \
	__typeof__(+(a)) __ckd_a = (a); \
	__typeof__(+(b)) __ckd_b = (b); \
	__typeof__(*(r)) *__ckd_r = (r); \
	__int128 __ckd_res = (__int128)__ckd_a - (__int128)__ckd_b; \
	_Bool __ckd_ovf = __ckd_res < __mcc_ckd_min(*__ckd_r) || \
			   __ckd_res > __mcc_ckd_max(*__ckd_r); \
	*__ckd_r = (__typeof__(*__ckd_r))__ckd_res; \
	__ckd_ovf; \
}))

#define ckd_mul(r, a, b) (__extension__({ \
	__typeof__(+(a)) __ckd_a = (a); \
	__typeof__(+(b)) __ckd_b = (b); \
	__typeof__(*(r)) *__ckd_r = (r); \
	_Bool __ckd_ovf; \
	if (__mcc_ckd_is_unsigned(__ckd_a) && __mcc_ckd_is_unsigned(__ckd_b)) { \
		unsigned __int128 __ckd_ures = (unsigned __int128)__ckd_a * \
						(unsigned __int128)__ckd_b; \
		__ckd_ovf = __ckd_ures > (unsigned __int128)__mcc_ckd_max(*__ckd_r); \
		*__ckd_r = (__typeof__(*__ckd_r))__ckd_ures; \
	} else { \
		__int128 __ckd_sres = (__int128)__ckd_a * (__int128)__ckd_b; \
		__ckd_ovf = __ckd_sres < __mcc_ckd_min(*__ckd_r) || \
			     __ckd_sres > __mcc_ckd_max(*__ckd_r); \
		*__ckd_r = (__typeof__(*__ckd_r))__ckd_sres; \
	} \
	__ckd_ovf; \
}))

#endif
