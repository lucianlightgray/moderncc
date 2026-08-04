#ifndef _MMINTRIN_H_INCLUDED
#define _MMINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "mmintrin.h is only supported on x86 targets"
#endif

typedef int __m64 __attribute__((__vector_size__(8), __may_alias__));

typedef int __v2si __attribute__((__vector_size__(8)));
typedef unsigned int __v2su __attribute__((__vector_size__(8)));
typedef short __v4hi __attribute__((__vector_size__(8)));
typedef unsigned short __v4hu __attribute__((__vector_size__(8)));
typedef char __v8qi __attribute__((__vector_size__(8)));
typedef signed char __v8qs __attribute__((__vector_size__(8)));
typedef unsigned char __v8qu __attribute__((__vector_size__(8)));
typedef long long __v1di __attribute__((__vector_size__(8)));
typedef unsigned long long __v1du __attribute__((__vector_size__(8)));

#define __MCC_MM_INLINE static __inline__ __attribute__((__always_inline__, __nodebug__))

__MCC_MM_INLINE int __mcc_sat_sb(int __v) {
	return __v > 127 ? 127 : __v < -128 ? -128 : __v;
}

__MCC_MM_INLINE int __mcc_sat_ub(int __v) {
	return __v > 255 ? 255 : __v < 0 ? 0 : __v;
}

__MCC_MM_INLINE int __mcc_sat_sw(int __v) {
	return __v > 32767 ? 32767 : __v < -32768 ? -32768 : __v;
}

__MCC_MM_INLINE int __mcc_sat_uw(int __v) {
	return __v > 65535 ? 65535 : __v < 0 ? 0 : __v;
}

__MCC_MM_INLINE void _mm_empty(void) {}

__MCC_MM_INLINE void _m_empty(void) {}

__MCC_MM_INLINE __m64 _mm_cvtsi32_si64(int __i) {
	return (__m64)(__v2si){__i, 0};
}

__MCC_MM_INLINE __m64 _m_from_int(int __i) {
	return _mm_cvtsi32_si64(__i);
}

__MCC_MM_INLINE int _mm_cvtsi64_si32(__m64 __m) {
	return ((__v2si)__m)[0];
}

__MCC_MM_INLINE int _m_to_int(__m64 __m) {
	return _mm_cvtsi64_si32(__m);
}

__MCC_MM_INLINE __m64 _mm_cvtsi64_m64(long long __i) {
	return (__m64)(__v1di){__i};
}

__MCC_MM_INLINE __m64 _m_from_int64(long long __i) {
	return _mm_cvtsi64_m64(__i);
}

__MCC_MM_INLINE long long _mm_cvtm64_si64(__m64 __m) {
	return ((__v1di)__m)[0];
}

__MCC_MM_INLINE long long _m_to_int64(__m64 __m) {
	return _mm_cvtm64_si64(__m);
}

__MCC_MM_INLINE __m64 _mm_setzero_si64(void) {
	return (__m64)(__v1di){0};
}

__MCC_MM_INLINE __m64 _mm_set_pi32(int __i1, int __i0) {
	return (__m64)(__v2si){__i0, __i1};
}

__MCC_MM_INLINE __m64 _mm_set_pi16(short __w3, short __w2, short __w1, short __w0) {
	return (__m64)(__v4hi){__w0, __w1, __w2, __w3};
}

__MCC_MM_INLINE __m64 _mm_set_pi8(char __b7, char __b6, char __b5, char __b4,
		char __b3, char __b2, char __b1, char __b0) {
	return (__m64)(__v8qi){__b0, __b1, __b2, __b3, __b4, __b5, __b6, __b7};
}

__MCC_MM_INLINE __m64 _mm_setr_pi32(int __i0, int __i1) {
	return (__m64)(__v2si){__i0, __i1};
}

__MCC_MM_INLINE __m64 _mm_setr_pi16(short __w0, short __w1, short __w2, short __w3) {
	return (__m64)(__v4hi){__w0, __w1, __w2, __w3};
}

__MCC_MM_INLINE __m64 _mm_setr_pi8(char __b0, char __b1, char __b2, char __b3,
		char __b4, char __b5, char __b6, char __b7) {
	return (__m64)(__v8qi){__b0, __b1, __b2, __b3, __b4, __b5, __b6, __b7};
}

__MCC_MM_INLINE __m64 _mm_set1_pi32(int __i) {
	return (__m64)(__v2si){__i, __i};
}

__MCC_MM_INLINE __m64 _mm_set1_pi16(short __w) {
	return (__m64)(__v4hi){__w, __w, __w, __w};
}

__MCC_MM_INLINE __m64 _mm_set1_pi8(char __b) {
	return (__m64)(__v8qi){__b, __b, __b, __b, __b, __b, __b, __b};
}

__MCC_MM_INLINE __m64 _mm_add_pi8(__m64 __m1, __m64 __m2) {
	return (__m64)((__v8qu)__m1 + (__v8qu)__m2);
}

__MCC_MM_INLINE __m64 _m_paddb(__m64 __m1, __m64 __m2) {
	return _mm_add_pi8(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_add_pi16(__m64 __m1, __m64 __m2) {
	return (__m64)((__v4hu)__m1 + (__v4hu)__m2);
}

__MCC_MM_INLINE __m64 _m_paddw(__m64 __m1, __m64 __m2) {
	return _mm_add_pi16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_add_pi32(__m64 __m1, __m64 __m2) {
	return (__m64)((__v2su)__m1 + (__v2su)__m2);
}

__MCC_MM_INLINE __m64 _m_paddd(__m64 __m1, __m64 __m2) {
	return _mm_add_pi32(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_add_si64(__m64 __m1, __m64 __m2) {
	return (__m64)((__v1du)__m1 + (__v1du)__m2);
}

__MCC_MM_INLINE __m64 _mm_adds_pi8(__m64 __m1, __m64 __m2) {
	__v8qs __a = (__v8qs)__m1, __b = (__v8qs)__m2, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (signed char)__mcc_sat_sb((int)__a[__i] + (int)__b[__i]);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_paddsb(__m64 __m1, __m64 __m2) {
	return _mm_adds_pi8(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_adds_pi16(__m64 __m1, __m64 __m2) {
	__v4hi __a = (__v4hi)__m1, __b = (__v4hi)__m2, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (short)__mcc_sat_sw((int)__a[__i] + (int)__b[__i]);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_paddsw(__m64 __m1, __m64 __m2) {
	return _mm_adds_pi16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_adds_pu8(__m64 __m1, __m64 __m2) {
	__v8qu __a = (__v8qu)__m1, __b = (__v8qu)__m2, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (unsigned char)__mcc_sat_ub((int)__a[__i] + (int)__b[__i]);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_paddusb(__m64 __m1, __m64 __m2) {
	return _mm_adds_pu8(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_adds_pu16(__m64 __m1, __m64 __m2) {
	__v4hu __a = (__v4hu)__m1, __b = (__v4hu)__m2, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (unsigned short)__mcc_sat_uw((int)__a[__i] + (int)__b[__i]);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_paddusw(__m64 __m1, __m64 __m2) {
	return _mm_adds_pu16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_sub_pi8(__m64 __m1, __m64 __m2) {
	return (__m64)((__v8qu)__m1 - (__v8qu)__m2);
}

__MCC_MM_INLINE __m64 _m_psubb(__m64 __m1, __m64 __m2) {
	return _mm_sub_pi8(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_sub_pi16(__m64 __m1, __m64 __m2) {
	return (__m64)((__v4hu)__m1 - (__v4hu)__m2);
}

__MCC_MM_INLINE __m64 _m_psubw(__m64 __m1, __m64 __m2) {
	return _mm_sub_pi16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_sub_pi32(__m64 __m1, __m64 __m2) {
	return (__m64)((__v2su)__m1 - (__v2su)__m2);
}

__MCC_MM_INLINE __m64 _m_psubd(__m64 __m1, __m64 __m2) {
	return _mm_sub_pi32(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_sub_si64(__m64 __m1, __m64 __m2) {
	return (__m64)((__v1du)__m1 - (__v1du)__m2);
}

__MCC_MM_INLINE __m64 _mm_subs_pi8(__m64 __m1, __m64 __m2) {
	__v8qs __a = (__v8qs)__m1, __b = (__v8qs)__m2, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (signed char)__mcc_sat_sb((int)__a[__i] - (int)__b[__i]);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_psubsb(__m64 __m1, __m64 __m2) {
	return _mm_subs_pi8(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_subs_pi16(__m64 __m1, __m64 __m2) {
	__v4hi __a = (__v4hi)__m1, __b = (__v4hi)__m2, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (short)__mcc_sat_sw((int)__a[__i] - (int)__b[__i]);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_psubsw(__m64 __m1, __m64 __m2) {
	return _mm_subs_pi16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_subs_pu8(__m64 __m1, __m64 __m2) {
	__v8qu __a = (__v8qu)__m1, __b = (__v8qu)__m2, __r;
	int __i;
	for (__i = 0; __i < 8; __i++)
		__r[__i] = (unsigned char)__mcc_sat_ub((int)__a[__i] - (int)__b[__i]);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_psubusb(__m64 __m1, __m64 __m2) {
	return _mm_subs_pu8(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_subs_pu16(__m64 __m1, __m64 __m2) {
	__v4hu __a = (__v4hu)__m1, __b = (__v4hu)__m2, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (unsigned short)__mcc_sat_uw((int)__a[__i] - (int)__b[__i]);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_psubusw(__m64 __m1, __m64 __m2) {
	return _mm_subs_pu16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_madd_pi16(__m64 __m1, __m64 __m2) {
	__v4hi __a = (__v4hi)__m1, __b = (__v4hi)__m2;
	__v2si __r;
	int __i;
	for (__i = 0; __i < 2; __i++) {
		unsigned int __lo = (unsigned int)((int)__a[2 * __i] * (int)__b[2 * __i]);
		unsigned int __hi = (unsigned int)((int)__a[2 * __i + 1] * (int)__b[2 * __i + 1]);
		__r[__i] = (int)(__lo + __hi);
	}
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_pmaddwd(__m64 __m1, __m64 __m2) {
	return _mm_madd_pi16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_mulhi_pi16(__m64 __m1, __m64 __m2) {
	__v4hi __a = (__v4hi)__m1, __b = (__v4hi)__m2, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (short)(((int)__a[__i] * (int)__b[__i]) >> 16);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_pmulhw(__m64 __m1, __m64 __m2) {
	return _mm_mulhi_pi16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_mullo_pi16(__m64 __m1, __m64 __m2) {
	return (__m64)((__v4hu)__m1 * (__v4hu)__m2);
}

__MCC_MM_INLINE __m64 _m_pmullw(__m64 __m1, __m64 __m2) {
	return _mm_mullo_pi16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_sll_pi16(__m64 __m, __m64 __count) {
	unsigned long long __c = (unsigned long long)((__v1du)__count)[0];
	__v4hu __a = (__v4hu)__m, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __c > 15 ? 0 : (unsigned short)(__a[__i] << __c);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_psllw(__m64 __m, __m64 __count) {
	return _mm_sll_pi16(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_slli_pi16(__m64 __m, int __count) {
	return _mm_sll_pi16(__m, (__m64)(__v1du){(unsigned long long)(unsigned int)__count});
}

__MCC_MM_INLINE __m64 _m_psllwi(__m64 __m, int __count) {
	return _mm_slli_pi16(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_sll_pi32(__m64 __m, __m64 __count) {
	unsigned long long __c = (unsigned long long)((__v1du)__count)[0];
	__v2su __a = (__v2su)__m, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __c > 31 ? 0 : (unsigned int)(__a[__i] << __c);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_pslld(__m64 __m, __m64 __count) {
	return _mm_sll_pi32(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_slli_pi32(__m64 __m, int __count) {
	return _mm_sll_pi32(__m, (__m64)(__v1du){(unsigned long long)(unsigned int)__count});
}

__MCC_MM_INLINE __m64 _m_pslldi(__m64 __m, int __count) {
	return _mm_slli_pi32(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_sll_si64(__m64 __m, __m64 __count) {
	unsigned long long __c = (unsigned long long)((__v1du)__count)[0];
	unsigned long long __a = (unsigned long long)((__v1du)__m)[0];
	return (__m64)(__v1du){__c > 63 ? 0 : __a << __c};
}

__MCC_MM_INLINE __m64 _m_psllq(__m64 __m, __m64 __count) {
	return _mm_sll_si64(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_slli_si64(__m64 __m, int __count) {
	return _mm_sll_si64(__m, (__m64)(__v1du){(unsigned long long)(unsigned int)__count});
}

__MCC_MM_INLINE __m64 _m_psllqi(__m64 __m, int __count) {
	return _mm_slli_si64(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_srl_pi16(__m64 __m, __m64 __count) {
	unsigned long long __c = (unsigned long long)((__v1du)__count)[0];
	__v4hu __a = (__v4hu)__m, __r;
	int __i;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = __c > 15 ? 0 : (unsigned short)(__a[__i] >> __c);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_psrlw(__m64 __m, __m64 __count) {
	return _mm_srl_pi16(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_srli_pi16(__m64 __m, int __count) {
	return _mm_srl_pi16(__m, (__m64)(__v1du){(unsigned long long)(unsigned int)__count});
}

__MCC_MM_INLINE __m64 _m_psrlwi(__m64 __m, int __count) {
	return _mm_srli_pi16(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_srl_pi32(__m64 __m, __m64 __count) {
	unsigned long long __c = (unsigned long long)((__v1du)__count)[0];
	__v2su __a = (__v2su)__m, __r;
	int __i;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __c > 31 ? 0 : (unsigned int)(__a[__i] >> __c);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_psrld(__m64 __m, __m64 __count) {
	return _mm_srl_pi32(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_srli_pi32(__m64 __m, int __count) {
	return _mm_srl_pi32(__m, (__m64)(__v1du){(unsigned long long)(unsigned int)__count});
}

__MCC_MM_INLINE __m64 _m_psrldi(__m64 __m, int __count) {
	return _mm_srli_pi32(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_srl_si64(__m64 __m, __m64 __count) {
	unsigned long long __c = (unsigned long long)((__v1du)__count)[0];
	unsigned long long __a = (unsigned long long)((__v1du)__m)[0];
	return (__m64)(__v1du){__c > 63 ? 0 : __a >> __c};
}

__MCC_MM_INLINE __m64 _m_psrlq(__m64 __m, __m64 __count) {
	return _mm_srl_si64(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_srli_si64(__m64 __m, int __count) {
	return _mm_srl_si64(__m, (__m64)(__v1du){(unsigned long long)(unsigned int)__count});
}

__MCC_MM_INLINE __m64 _m_psrlqi(__m64 __m, int __count) {
	return _mm_srli_si64(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_sra_pi16(__m64 __m, __m64 __count) {
	unsigned long long __c = (unsigned long long)((__v1du)__count)[0];
	__v4hi __a = (__v4hi)__m, __r;
	int __i, __s = __c > 15 ? 15 : (int)__c;
	for (__i = 0; __i < 4; __i++)
		__r[__i] = (short)((int)__a[__i] >> __s);
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_psraw(__m64 __m, __m64 __count) {
	return _mm_sra_pi16(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_srai_pi16(__m64 __m, int __count) {
	return _mm_sra_pi16(__m, (__m64)(__v1du){(unsigned long long)(unsigned int)__count});
}

__MCC_MM_INLINE __m64 _m_psrawi(__m64 __m, int __count) {
	return _mm_srai_pi16(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_sra_pi32(__m64 __m, __m64 __count) {
	unsigned long long __c = (unsigned long long)((__v1du)__count)[0];
	__v2si __a = (__v2si)__m, __r;
	int __i, __s = __c > 31 ? 31 : (int)__c;
	for (__i = 0; __i < 2; __i++)
		__r[__i] = __a[__i] >> __s;
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_psrad(__m64 __m, __m64 __count) {
	return _mm_sra_pi32(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_srai_pi32(__m64 __m, int __count) {
	return _mm_sra_pi32(__m, (__m64)(__v1du){(unsigned long long)(unsigned int)__count});
}

__MCC_MM_INLINE __m64 _m_psradi(__m64 __m, int __count) {
	return _mm_srai_pi32(__m, __count);
}

__MCC_MM_INLINE __m64 _mm_and_si64(__m64 __m1, __m64 __m2) {
	return (__m64)((__v1du)__m1 & (__v1du)__m2);
}

__MCC_MM_INLINE __m64 _m_pand(__m64 __m1, __m64 __m2) {
	return _mm_and_si64(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_andnot_si64(__m64 __m1, __m64 __m2) {
	return (__m64)(~(__v1du)__m1 & (__v1du)__m2);
}

__MCC_MM_INLINE __m64 _m_pandn(__m64 __m1, __m64 __m2) {
	return _mm_andnot_si64(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_or_si64(__m64 __m1, __m64 __m2) {
	return (__m64)((__v1du)__m1 | (__v1du)__m2);
}

__MCC_MM_INLINE __m64 _m_por(__m64 __m1, __m64 __m2) {
	return _mm_or_si64(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_xor_si64(__m64 __m1, __m64 __m2) {
	return (__m64)((__v1du)__m1 ^ (__v1du)__m2);
}

__MCC_MM_INLINE __m64 _m_pxor(__m64 __m1, __m64 __m2) {
	return _mm_xor_si64(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_cmpeq_pi8(__m64 __m1, __m64 __m2) {
	return (__m64)((__v8qi)__m1 == (__v8qi)__m2);
}

__MCC_MM_INLINE __m64 _m_pcmpeqb(__m64 __m1, __m64 __m2) {
	return _mm_cmpeq_pi8(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_cmpeq_pi16(__m64 __m1, __m64 __m2) {
	return (__m64)((__v4hi)__m1 == (__v4hi)__m2);
}

__MCC_MM_INLINE __m64 _m_pcmpeqw(__m64 __m1, __m64 __m2) {
	return _mm_cmpeq_pi16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_cmpeq_pi32(__m64 __m1, __m64 __m2) {
	return (__m64)((__v2si)__m1 == (__v2si)__m2);
}

__MCC_MM_INLINE __m64 _m_pcmpeqd(__m64 __m1, __m64 __m2) {
	return _mm_cmpeq_pi32(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_cmpgt_pi8(__m64 __m1, __m64 __m2) {
	return (__m64)((__v8qs)__m1 > (__v8qs)__m2);
}

__MCC_MM_INLINE __m64 _m_pcmpgtb(__m64 __m1, __m64 __m2) {
	return _mm_cmpgt_pi8(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_cmpgt_pi16(__m64 __m1, __m64 __m2) {
	return (__m64)((__v4hi)__m1 > (__v4hi)__m2);
}

__MCC_MM_INLINE __m64 _m_pcmpgtw(__m64 __m1, __m64 __m2) {
	return _mm_cmpgt_pi16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_cmpgt_pi32(__m64 __m1, __m64 __m2) {
	return (__m64)((__v2si)__m1 > (__v2si)__m2);
}

__MCC_MM_INLINE __m64 _m_pcmpgtd(__m64 __m1, __m64 __m2) {
	return _mm_cmpgt_pi32(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_packs_pi16(__m64 __m1, __m64 __m2) {
	__v4hi __a = (__v4hi)__m1, __b = (__v4hi)__m2;
	__v8qs __r;
	int __i;
	for (__i = 0; __i < 4; __i++) {
		__r[__i] = (signed char)__mcc_sat_sb((int)__a[__i]);
		__r[__i + 4] = (signed char)__mcc_sat_sb((int)__b[__i]);
	}
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_packsswb(__m64 __m1, __m64 __m2) {
	return _mm_packs_pi16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_packs_pi32(__m64 __m1, __m64 __m2) {
	__v2si __a = (__v2si)__m1, __b = (__v2si)__m2;
	__v4hi __r;
	int __i;
	for (__i = 0; __i < 2; __i++) {
		__r[__i] = (short)__mcc_sat_sw(__a[__i]);
		__r[__i + 2] = (short)__mcc_sat_sw(__b[__i]);
	}
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_packssdw(__m64 __m1, __m64 __m2) {
	return _mm_packs_pi32(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_packs_pu16(__m64 __m1, __m64 __m2) {
	__v4hi __a = (__v4hi)__m1, __b = (__v4hi)__m2;
	__v8qu __r;
	int __i;
	for (__i = 0; __i < 4; __i++) {
		__r[__i] = (unsigned char)__mcc_sat_ub((int)__a[__i]);
		__r[__i + 4] = (unsigned char)__mcc_sat_ub((int)__b[__i]);
	}
	return (__m64)__r;
}

__MCC_MM_INLINE __m64 _m_packuswb(__m64 __m1, __m64 __m2) {
	return _mm_packs_pu16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_unpackhi_pi8(__m64 __m1, __m64 __m2) {
	return (__m64)__builtin_shufflevector((__v8qi)__m1, (__v8qi)__m2,
			4, 12, 5, 13, 6, 14, 7, 15);
}

__MCC_MM_INLINE __m64 _m_punpckhbw(__m64 __m1, __m64 __m2) {
	return _mm_unpackhi_pi8(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_unpackhi_pi16(__m64 __m1, __m64 __m2) {
	return (__m64)__builtin_shufflevector((__v4hi)__m1, (__v4hi)__m2, 2, 6, 3, 7);
}

__MCC_MM_INLINE __m64 _m_punpckhwd(__m64 __m1, __m64 __m2) {
	return _mm_unpackhi_pi16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_unpackhi_pi32(__m64 __m1, __m64 __m2) {
	return (__m64)__builtin_shufflevector((__v2si)__m1, (__v2si)__m2, 1, 3);
}

__MCC_MM_INLINE __m64 _m_punpckhdq(__m64 __m1, __m64 __m2) {
	return _mm_unpackhi_pi32(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_unpacklo_pi8(__m64 __m1, __m64 __m2) {
	return (__m64)__builtin_shufflevector((__v8qi)__m1, (__v8qi)__m2,
			0, 8, 1, 9, 2, 10, 3, 11);
}

__MCC_MM_INLINE __m64 _m_punpcklbw(__m64 __m1, __m64 __m2) {
	return _mm_unpacklo_pi8(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_unpacklo_pi16(__m64 __m1, __m64 __m2) {
	return (__m64)__builtin_shufflevector((__v4hi)__m1, (__v4hi)__m2, 0, 4, 1, 5);
}

__MCC_MM_INLINE __m64 _m_punpcklwd(__m64 __m1, __m64 __m2) {
	return _mm_unpacklo_pi16(__m1, __m2);
}

__MCC_MM_INLINE __m64 _mm_unpacklo_pi32(__m64 __m1, __m64 __m2) {
	return (__m64)__builtin_shufflevector((__v2si)__m1, (__v2si)__m2, 0, 2);
}

__MCC_MM_INLINE __m64 _m_punpckldq(__m64 __m1, __m64 __m2) {
	return _mm_unpacklo_pi32(__m1, __m2);
}

#endif
