#if __SIZEOF_POINTER__ == 4
#if defined  __OpenBSD__
	#define __SIZE_TYPE__ unsigned long
	#define __PTRDIFF_TYPE__ long
#else
	#define __SIZE_TYPE__ unsigned int
	#define __PTRDIFF_TYPE__ int
#endif
	#define __ILP32__ 1
	#define __INT64_TYPE__ long long
#elif __SIZEOF_LONG__ == 4
	#define __SIZE_TYPE__ unsigned long long
	#define __PTRDIFF_TYPE__ long long
	#define __LLP64__ 1
	#define __INT64_TYPE__ long long
#else
	#define __SIZE_TYPE__ unsigned long
	#define __PTRDIFF_TYPE__ long
	#define __LP64__ 1
# if defined __linux__
	#define __INT64_TYPE__ long
# else
	#define __INT64_TYPE__ long long
# endif
#endif
	#define __SIZEOF_INT__ 4
	#define __INT_MAX__ 0x7fffffff
#if __SIZEOF_LONG__ == 4
	#define __LONG_MAX__ 0x7fffffffL
#else
	#define __LONG_MAX__ 0x7fffffffffffffffL
#endif
	#define __SIZEOF_LONG_LONG__ 8
	#define __LONG_LONG_MAX__ 0x7fffffffffffffffLL
	#define __CHAR_BIT__ 8
	#define __ORDER_LITTLE_ENDIAN__ 1234
	#define __ORDER_BIG_ENDIAN__ 4321
	#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#if defined _WIN32
	#define __WCHAR_TYPE__ unsigned short
	#define __WINT_TYPE__ unsigned short
#elif defined __linux__ && (defined __arm__ || defined __aarch64__)
	#define __WCHAR_TYPE__ unsigned int
	#define __WINT_TYPE__ unsigned int
#elif defined __linux__
	#define __WCHAR_TYPE__ int
	#define __WINT_TYPE__ unsigned int
#else
	#define __WCHAR_TYPE__ int
	#define __WINT_TYPE__ int
#endif

#if !defined _WIN32
	#define __STDC_ISO_10646__ 201706L
#endif

	#define __STDC_IEC_559__ 1
	#define __STDC_IEC_559_COMPLEX__ 1
	#define __GCC_IEC_559 2
	#define __GCC_IEC_559_COMPLEX 2

	#define __STDC_UTF_16__ 1
	#define __STDC_UTF_32__ 1

#if defined _WIN32
	#define __declspec(x) __attribute__((x))
	#define __cdecl

#elif defined __FreeBSD__
	#define __GNUC__ 9
	#define __GNUC_MINOR__ 3
	#define __GNUC_PATCHLEVEL__ 0
	#define __GNUC_STDC_INLINE__ 1
	#define __NO_TLS 1
	#define __RUNETYPE_INTERNAL 1
# if __SIZEOF_POINTER__ == 8
	#define __SIZEOF_SIZE_T__ 8
	#define __SIZEOF_PTRDIFF_T__ 8
#else
	#define __SIZEOF_SIZE_T__ 4
	#define __SIZEOF_PTRDIFF_T__ 4
# endif

#elif defined __FreeBSD_kernel__

#elif defined __NetBSD__
	#define __GNUC__ 4
	#define __GNUC_MINOR__ 1
	#define __GNUC_PATCHLEVEL__ 0
	#define _Pragma(x)
	#define __ELF__ 1
#if defined __aarch64__
	#define _LOCORE
#endif

#elif defined __OpenBSD__
	#define __GNUC__ 4
	#define _ANSI_LIBRARY 1

#elif defined __APPLE__
	#define __GNUC__ 4
	#define __APPLE_CC__ 1
	#define __LITTLE_ENDIAN__ 1
	#define _DONT_USE_CTYPE_INLINE_ 1

	#define __FINITE_MATH_ONLY__ 0
	#define __FLT_MIN__ 1.17549435082228750797e-38F
	#define __DBL_MIN__ 2.2250738585072014e-308
	#if defined __aarch64__
	#define __LDBL_MIN__ 2.2250738585072014e-308L
	#else
	#define __LDBL_MIN__ 3.36210314311209350626e-4932L
	#endif
	#define _Float16 short unsigned int

#elif defined __ANDROID__
	#define  BIONIC_IOCTL_NO_SIGNEDNESS_OVERLOAD
	#define __ELF__ 1

#elif defined __linux__
	#define __ELF__ 1

#else

#endif

#ifndef __NetBSD__
	#define __UINTPTR_TYPE__ unsigned __PTRDIFF_TYPE__
	#define __INTPTR_TYPE__ __PTRDIFF_TYPE__
#endif
	#define __INT8_TYPE__ signed char
	#define __UINT8_TYPE__ unsigned char
	#define __INT16_TYPE__ short
	#define __UINT16_TYPE__ unsigned short
	#define __INT32_TYPE__ int
	#define __UINT32_TYPE__ unsigned int
	#define __CHAR16_TYPE__ unsigned short
	#define __CHAR32_TYPE__ unsigned int
	#define __CHAR8_TYPE__ unsigned char

	#define __SCHAR_MAX__ 0x7f
	#define __SHRT_MAX__ 0x7fff
	#define __INT_MIN__ (-__INT_MAX__ - 1)
	#define __LONG_MIN__ (-__LONG_MAX__ - 1L)
	#define __LONG_LONG_MIN__ (-__LONG_LONG_MAX__ - 1LL)

	#define __INT8_TYPE__ signed char
	#define __UINT8_TYPE__ unsigned char
	#define __INT8_MAX__ 0x7f
	#define __UINT8_MAX__ 0xff
	#define __INT16_TYPE__ short int
	#define __UINT16_TYPE__ short unsigned int
	#define __INT16_MAX__ 0x7fff
	#define __UINT16_MAX__ 0xffff
	#define __INT32_MAX__ 0x7fffffff
	#define __UINT32_MAX__ 0xffffffffU

#if __SIZEOF_LONG__ == 8 && defined __linux__
	#define __UINT64_TYPE__ long unsigned int
	#define __INT64_MAX__ 0x7fffffffffffffffL
	#define __UINT64_MAX__ 0xffffffffffffffffUL
#else
	#define __UINT64_TYPE__ long long unsigned int
	#define __INT64_MAX__ 0x7fffffffffffffffLL
	#define __UINT64_MAX__ 0xffffffffffffffffULL
#endif

	#define __INTMAX_TYPE__ __INT64_TYPE__
	#define __UINTMAX_TYPE__ __UINT64_TYPE__
	#define __INTMAX_MAX__ __INT64_MAX__
	#define __UINTMAX_MAX__ __UINT64_MAX__

	#define __INT_LEAST8_TYPE__ __INT8_TYPE__
	#define __UINT_LEAST8_TYPE__ __UINT8_TYPE__
	#define __INT_LEAST8_MAX__ __INT8_MAX__
	#define __UINT_LEAST8_MAX__ __UINT8_MAX__
	#define __INT_LEAST16_TYPE__ __INT16_TYPE__
	#define __UINT_LEAST16_TYPE__ __UINT16_TYPE__
	#define __INT_LEAST16_MAX__ __INT16_MAX__
	#define __UINT_LEAST16_MAX__ __UINT16_MAX__
	#define __INT_LEAST32_TYPE__ __INT32_TYPE__
	#define __UINT_LEAST32_TYPE__ __UINT32_TYPE__
	#define __INT_LEAST32_MAX__ __INT32_MAX__
	#define __UINT_LEAST32_MAX__ __UINT32_MAX__
	#define __INT_LEAST64_TYPE__ __INT64_TYPE__
	#define __UINT_LEAST64_TYPE__ __UINT64_TYPE__
	#define __INT_LEAST64_MAX__ __INT64_MAX__
	#define __UINT_LEAST64_MAX__ __UINT64_MAX__

	#define __INT_FAST8_TYPE__ signed char
	#define __UINT_FAST8_TYPE__ unsigned char
	#define __INT_FAST8_MAX__ __INT8_MAX__
	#define __UINT_FAST8_MAX__ __UINT8_MAX__
#if __SIZEOF_LONG__ == 8 && defined __linux__
	#define __INT_FAST16_TYPE__ long int
	#define __UINT_FAST16_TYPE__ long unsigned int
	#define __INT_FAST32_TYPE__ long int
	#define __UINT_FAST32_TYPE__ long unsigned int
	#define __INT_FAST64_TYPE__ long int
	#define __UINT_FAST64_TYPE__ long unsigned int
	#define __INT_FAST16_MAX__ __INT64_MAX__
	#define __UINT_FAST16_MAX__ __UINT64_MAX__
	#define __INT_FAST32_MAX__ __INT64_MAX__
	#define __UINT_FAST32_MAX__ __UINT64_MAX__
	#define __INT_FAST64_MAX__ __INT64_MAX__
	#define __UINT_FAST64_MAX__ __UINT64_MAX__
#else
	#define __INT_FAST16_TYPE__ int
	#define __UINT_FAST16_TYPE__ unsigned int
	#define __INT_FAST32_TYPE__ int
	#define __UINT_FAST32_TYPE__ unsigned int
	#define __INT_FAST64_TYPE__ __INT64_TYPE__
	#define __UINT_FAST64_TYPE__ __UINT64_TYPE__
	#define __INT_FAST16_MAX__ __INT32_MAX__
	#define __UINT_FAST16_MAX__ __UINT32_MAX__
	#define __INT_FAST32_MAX__ __INT32_MAX__
	#define __UINT_FAST32_MAX__ __UINT32_MAX__
	#define __INT_FAST64_MAX__ __INT64_MAX__
	#define __UINT_FAST64_MAX__ __UINT64_MAX__
#endif

#if __SIZEOF_PTRDIFF_T__ == 8
	#define __PTRDIFF_MAX__ 0x7fffffffffffffffL
#else
	#define __PTRDIFF_MAX__ 0x7fffffff
#endif
	#define __PTRDIFF_MIN__ (-__PTRDIFF_MAX__ - 1)
#if __SIZEOF_SIZE_T__ == 8
	#define __SIZE_MAX__ 0xffffffffffffffffUL
#else
	#define __SIZE_MAX__ 0xffffffffU
#endif

#if defined __linux__ && (defined __arm__ || defined __aarch64__)
	#define __WCHAR_MAX__ 0xffffffffU
	#define __WCHAR_MIN__ 0U
#else
	#define __WCHAR_MAX__ 0x7fffffff
	#define __WCHAR_MIN__ (-__WCHAR_MAX__ - 1)
#endif
	#define __WINT_MAX__ 0xffffffffU
	#define __WINT_MIN__ 0U

	#define __SIG_ATOMIC_TYPE__ int
	#define __SIG_ATOMIC_MAX__ 0x7fffffff
	#define __SIG_ATOMIC_MIN__ (-__SIG_ATOMIC_MAX__ - 1)

	#define __BIGGEST_ALIGNMENT__ 16
	#define __FLT_EVAL_METHOD__ 0

	#define __FLT_RADIX__ 2
	#define __FLT_MANT_DIG__ 24
	#define __FLT_DIG__ 6
	#define __FLT_MIN_EXP__ (-125)
	#define __FLT_MIN_10_EXP__ (-37)
	#define __FLT_MAX_EXP__ 128
	#define __FLT_MAX_10_EXP__ 38
	#define __FLT_MAX__ 3.40282346638528859811704183484516925e+38F
	#define __FLT_EPSILON__ 1.19209289550781250000000000000000000e-7F
	#define __FLT_DENORM_MIN__ 1.40129846432481707092372958328991613e-45F
	#define __FLT_HAS_DENORM__ 1
	#define __FLT_HAS_INFINITY__ 1
	#define __FLT_HAS_QUIET_NAN__ 1

	#define __DBL_MANT_DIG__ 53
	#define __DBL_DIG__ 15
	#define __DBL_MIN_EXP__ (-1021)
	#define __DBL_MIN_10_EXP__ (-307)
	#define __DBL_MAX_EXP__ 1024
	#define __DBL_MAX_10_EXP__ 308
	#define __DBL_MAX__ ((double)1.79769313486231570814527423731704357e+308L)
	#define __DBL_EPSILON__ ((double)2.22044604925031308084726333618164062e-16L)
	#define __DBL_DENORM_MIN__ ((double)4.94065645841246544176568792868221372e-324L)
	#define __DBL_HAS_DENORM__ 1
	#define __DBL_HAS_INFINITY__ 1
	#define __DBL_HAS_QUIET_NAN__ 1

#if defined __x86_64__ || defined __i386__
	#define __LDBL_MANT_DIG__ 64
	#define __LDBL_DIG__ 18
	#define __LDBL_MIN_EXP__ (-16381)
	#define __LDBL_MIN_10_EXP__ (-4931)
	#define __LDBL_MAX_EXP__ 16384
	#define __LDBL_MAX_10_EXP__ 4932
	#define __LDBL_MAX__ 1.18973149535723176502126385303097021e+4932L
	#define __LDBL_EPSILON__ 1.08420217248550443400745280086994171e-19L
	#define __LDBL_DENORM_MIN__ 3.64519953188247460252840593361941982e-4951L
#else
	#define __LDBL_MANT_DIG__ __DBL_MANT_DIG__
	#define __LDBL_DIG__ __DBL_DIG__
	#define __LDBL_MIN_EXP__ __DBL_MIN_EXP__
	#define __LDBL_MIN_10_EXP__ __DBL_MIN_10_EXP__
	#define __LDBL_MAX_EXP__ __DBL_MAX_EXP__
	#define __LDBL_MAX_10_EXP__ __DBL_MAX_10_EXP__
	#define __LDBL_MAX__ __DBL_MAX__
	#define __LDBL_EPSILON__ __DBL_EPSILON__
	#define __LDBL_DENORM_MIN__ __DBL_DENORM_MIN__
#endif
	#define __LDBL_HAS_DENORM__ 1
	#define __LDBL_HAS_INFINITY__ 1
	#define __LDBL_HAS_QUIET_NAN__ 1

	#define __ATOMIC_RELAXED 0
	#define __ATOMIC_CONSUME 1
	#define __ATOMIC_ACQUIRE 2
	#define __ATOMIC_RELEASE 3
	#define __ATOMIC_ACQ_REL 4
	#define __ATOMIC_SEQ_CST 5

#if !defined _WIN32
	#define __REDIRECT(name, proto, alias) name proto __asm__ (#alias)
	#define __REDIRECT_NTH(name, proto, alias) name proto __asm__ (#alias) __THROW
	#define __REDIRECT_NTHNL(name, proto, alias) name proto __asm__ (#alias) __THROWNL
#endif

	#define  __PRETTY_FUNCTION__ __FUNCTION__
	#define __has_builtin(x) 0
	#define __has_feature(x) 0
	#define __has_attribute(x) 0
	#define _Nonnull
	#define _Nullable
	#define _Nullable_result
	#define _Null_unspecified

	#ifndef __MCC_PP__

	typedef char __mcc_char_t;
	typedef signed char __mcc_schar_t;
	typedef unsigned char __mcc_uchar_t;
	typedef short __mcc_short_t;
	typedef unsigned short __mcc_ushort_t;
	typedef int __mcc_int_t;
	typedef unsigned int __mcc_uint_t;
	typedef long __mcc_long_t;
	typedef unsigned long __mcc_ulong_t;
	typedef long long __mcc_llong_t;
	typedef unsigned long long __mcc_ullong_t;
	typedef float __mcc_float_t;
	typedef double __mcc_double_t;
	typedef long double __mcc_ldouble_t;
	typedef __SIZE_TYPE__ __mcc_size_t;

	#define __builtin_offsetof(type, field) ((__mcc_size_t)&((type*)0)->field)
	#define __builtin_extract_return_addr(x) x
	#define __builtin_frob_return_addr(x) x
	#define __builtin_memcpy_inline(d, s, n)  __builtin_memcpy((d), (s), (n))
	#define __builtin_memset_inline(d, c, n)  __builtin_memset((d), (c), (n))
	#define __builtin_memmove_inline(d, s, n) __builtin_memmove((d), (s), (n))
	#define __sync_fetch_and_add(p,v) __atomic_fetch_add((p),(v),__ATOMIC_SEQ_CST)
	#define __sync_fetch_and_sub(p,v) __atomic_fetch_sub((p),(v),__ATOMIC_SEQ_CST)
	#define __sync_fetch_and_or(p,v)  __atomic_fetch_or((p),(v),__ATOMIC_SEQ_CST)
	#define __sync_fetch_and_and(p,v) __atomic_fetch_and((p),(v),__ATOMIC_SEQ_CST)
	#define __sync_fetch_and_xor(p,v) __atomic_fetch_xor((p),(v),__ATOMIC_SEQ_CST)
	#define __sync_add_and_fetch(p,v) (__atomic_fetch_add((p),(v),__ATOMIC_SEQ_CST)+(v))
	#define __sync_sub_and_fetch(p,v) (__atomic_fetch_sub((p),(v),__ATOMIC_SEQ_CST)-(v))
	#define __sync_or_and_fetch(p,v)  (__atomic_fetch_or((p),(v),__ATOMIC_SEQ_CST)|(v))
	#define __sync_and_and_fetch(p,v) (__atomic_fetch_and((p),(v),__ATOMIC_SEQ_CST)&(v))
	#define __sync_xor_and_fetch(p,v) (__atomic_fetch_xor((p),(v),__ATOMIC_SEQ_CST)^(v))
	#define __sync_bool_compare_and_swap(p,o,n) \
	__extension__ ({ __typeof__(*(p)) __o=(o), __n=(n); \
	__atomic_compare_exchange((p),&__o,&__n,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST); })
	#define __sync_val_compare_and_swap(p,o,n) \
	__extension__ ({ __typeof__(*(p)) __o=(o), __n=(n); \
	__atomic_compare_exchange((p),&__o,&__n,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST); __o; })
	#define __sync_lock_test_and_set(p,v) \
	__extension__ ({ __typeof__(*(p)) __v=(v), __o; \
	__atomic_exchange((p),&__v,&__o,__ATOMIC_SEQ_CST); __o; })
	#define __sync_lock_release(p) \
	__extension__ ({ __typeof__(*(p)) __z=0; __atomic_store((p),&__z,__ATOMIC_SEQ_CST); })
	#define __sync_synchronize() \
	__extension__ ({ volatile __mcc_int_t __mcc_bar = 0; \
	(void)__atomic_fetch_add(&__mcc_bar, 0, __ATOMIC_SEQ_CST); (void)0; })
	#define __atomic_load_n(p, o) \
	__extension__ ({ __typeof__(*(p)) __r; __atomic_load((p), &__r, (o)); __r; })
	#define __atomic_store_n(p, v, o) \
	__extension__ ({ __typeof__(*(p)) __v = (v); __atomic_store((p), &__v, (o)); })
	#define __atomic_exchange_n(p, v, o) \
	__extension__ ({ __typeof__(*(p)) __v = (v), __r; \
	__atomic_exchange((p), &__v, &__r, (o)); __r; })
	#define __atomic_compare_exchange_n(p, e, d, w, s, f) \
	__extension__ ({ __typeof__(*(p)) __d = (d); \
	__atomic_compare_exchange((p), (e), &__d, (w), (s), (f)); })

#if !defined __linux__ && !defined _WIN32
# if defined __APPLE__
	#define __builtin_flt_rounds() 1
	#define __builtin_bzero(p, ignored_size) bzero(p, sizeof(*(p)))
# endif
#endif
	#ifndef __builtin_nansf
	#define __builtin_nansf(s) (0.0f / 0.0f)
	#endif
	#ifndef __builtin_nans
	#define __builtin_nans(s) (0.0 / 0.0)
	#endif
	#ifndef __builtin_nansl
	#define __builtin_nansl(s) (0.0L / 0.0L)
	#endif
	#define __builtin_isnan(x) ((x) != (x))
	#define __builtin_isnanf(x) __builtin_isnan(x)
	#define __builtin_isnanl(x) __builtin_isnan(x)
	#define __builtin_annotation(x, ...) (x)
	#define __builtin_creal(z) (__real__(double _Complex)(z))
	#define __builtin_crealf(z) (__real__(float _Complex)(z))
	#define __builtin_creall(z) (__real__(long double _Complex)(z))
	#define __builtin_cimag(z) (__imag__(double _Complex)(z))
	#define __builtin_cimagf(z) (__imag__(float _Complex)(z))
	#define __builtin_cimagl(z) (__imag__(long double _Complex)(z))
	#define __builtin_conj(z) __builtin_complex(__builtin_creal(z), -__builtin_cimag(z))
	#define __builtin_conjf(z) __builtin_complex(__builtin_crealf(z), -__builtin_cimagf(z))
	#define __builtin_conjl(z) __builtin_complex(__builtin_creall(z), -__builtin_cimagl(z))
	#define __builtin_isinf(x) ((x) == __builtin_inf() || (x) == -__builtin_inf())
	#define __builtin_isfinite(x) (!__builtin_isnan(x) && !__builtin_isinf(x))
	#define __builtin_isunordered(a, b) (__builtin_isnan(a) || __builtin_isnan(b))
	#define __builtin_isgreater(a, b) (!__builtin_isunordered(a, b) && (a) > (b))
	#define __builtin_isgreaterequal(a, b) (!__builtin_isunordered(a, b) && (a) >= (b))
	#define __builtin_isless(a, b) (!__builtin_isunordered(a, b) && (a) < (b))
	#define __builtin_islessequal(a, b) (!__builtin_isunordered(a, b) && (a) <= (b))
	#define __builtin_islessgreater(a, b) (!__builtin_isunordered(a, b) && ((a) < (b) || (a) > (b)))
	#define __builtin_fabsf(x) ((__mcc_float_t)((x) <= 0 ? 0.0f - (x) : (x)))
	#define __builtin_fabs(x)  ((__mcc_double_t)((x) <= 0 ? 0.0 - (x) : (x)))
	#define __builtin_fabsl(x) ((__mcc_ldouble_t)((x) <= 0 ? 0.0L - (x) : (x)))
	#define __builtin_abs(x)   ((__mcc_int_t)((x) < 0 ? -(x) : (x)))
	#define __builtin_labs(x)  ((__mcc_long_t)((x) < 0 ? -(x) : (x)))
	#define __builtin_llabs(x) ((__mcc_llong_t)((x) < 0 ? -(x) : (x)))
	#define __builtin_copysignf(x, y) (__builtin_signbit(y) ? -__builtin_fabsf(x) : __builtin_fabsf(x))
	#define __builtin_copysign(x, y)  (__builtin_signbit(y) ? -__builtin_fabs(x)  : __builtin_fabs(x))
	#define __builtin_copysignl(x, y) (__builtin_signbit(y) ? -__builtin_fabsl(x) : __builtin_fabsl(x))
	#ifndef __FLT_MIN__
	#define __FLT_MIN__ 1.17549435082228750797e-38F
	#endif
	#ifndef __DBL_MIN__
	#define __DBL_MIN__ 2.2250738585072014e-308
	#endif
	#ifndef __LDBL_MIN__
	#if defined _WIN32 || defined __arm__ || (defined __aarch64__ && (defined __APPLE__ || defined _WIN32))
	#define __LDBL_MIN__ 2.2250738585072014e-308L
	#else
	#define __LDBL_MIN__ 3.36210314311209350626e-4932L
	#endif
	#endif
	#define __builtin_isnormal(x) (__builtin_isfinite(x) && (x) != 0 && _Generic((x), \
		__mcc_float_t:   __builtin_fabsf(x) >= __FLT_MIN__, \
		__mcc_ldouble_t: __builtin_fabsl(x) >= __LDBL_MIN__, \
		default:     __builtin_fabs(x)  >= __DBL_MIN__))
	#define __builtin_fpclassify(nan, inf, norm, sub, zero, x) \
		(__builtin_isnan(x) ? (nan) : __builtin_isinf(x) ? (inf) \
		 : (x) == 0 ? (zero) : __builtin_isnormal(x) ? (norm) : (sub))

#if defined __x86_64__ && !defined _WIN32
	#define __SIZEOF_INT128__ 16
	typedef __int128 __int128_t;
	typedef unsigned __int128 __uint128_t;
	typedef __int128 __mcc_int128_t;
	typedef unsigned __int128 __mcc_uint128_t;
#else
	struct __uint128__ { char x[16]; } __attribute((__aligned__(16)));
	#define __int128_t struct __uint128__
	#define __uint128_t struct __uint128__
#endif

#if defined __x86_64__
#if !defined _WIN32
	typedef struct __va_list_tag {
	unsigned gp_offset, fp_offset;
	union {
	unsigned overflow_offset;
	char *overflow_arg_area;
	};
	char *reg_save_area;
	} __builtin_va_list[1];

	void *__va_arg(__builtin_va_list ap, int arg_type, int size, int align);
	static __inline void *__va_arg_inline(__builtin_va_list ap, int arg_type, int size, int align) {
	size = (size + 7) & ~7;
	align = (align + 7) & ~7;
	if (arg_type == 0) {
	if (ap->gp_offset + size <= 48) {
	ap->gp_offset += size;
	return ap->reg_save_area + ap->gp_offset - size;
	}
	} else if (arg_type == 1) {
	if (ap->fp_offset < 128 + 48) {
	ap->fp_offset += 16;
	if (size == 8)
	return ap->reg_save_area + ap->fp_offset - 16;
	if (ap->fp_offset < 128 + 48) {
	*(long long *)(ap->reg_save_area + ap->fp_offset - 8) =
	*(long long *)(ap->reg_save_area + ap->fp_offset);
	ap->fp_offset += 16;
	return ap->reg_save_area + ap->fp_offset - 32;
	}
	}
	}
	ap->overflow_arg_area += size;
	ap->overflow_arg_area = (char *)((long long)(ap->overflow_arg_area + align - 1) & -align);
	return ap->overflow_arg_area - size;
	}
	#define __builtin_va_start(ap, last) \
	(*(ap) = *(struct __va_list_tag *)((__mcc_char_t*)__builtin_frame_address(0) - 24))
	#define __builtin_va_arg(ap, t)   \
	(*(t *)(__va_arg_inline(ap, __builtin_va_arg_types(t), sizeof(t), __alignof__(t))))
	#define __builtin_va_copy(dest, src) (*(dest) = *(src))

#else
	typedef char *__builtin_va_list;
	#define __builtin_va_arg(ap, t) ((sizeof(t) > 8 || (sizeof(t) & (sizeof(t) - 1))) \
	? **(t **)((ap += 8) - 8) : *(t  *)((ap += 8) - 8))
#endif

#elif defined __arm__
	typedef char *__builtin_va_list;
	#define _mcc_alignof(type) ((__mcc_int_t)&((struct {__mcc_char_t c;type x;} *)0)->x)
	#define _mcc_align(addr,type) (((__mcc_uint_t)addr + _mcc_alignof(type) - 1) \
	& ~(_mcc_alignof(type) - 1))
	#define __builtin_va_start(ap,last) (ap = ((__mcc_char_t *)&(last)) + ((sizeof(last)+3)&~3))
	#define __builtin_va_arg(ap,type) (ap = (void *) ((_mcc_align(ap,type)+sizeof(type)+3) \
	&~3), *(type *)(ap - ((sizeof(type)+3)&~3)))

#elif defined __aarch64__
#if defined _WIN32
	typedef char *__builtin_va_list;
#elif defined __APPLE__
	typedef struct {
	void *__stack;
	} __builtin_va_list;

#else
	typedef struct {
	void *__stack, *__gr_top, *__vr_top;
	int   __gr_offs, __vr_offs;
	} __builtin_va_list;

#endif
#elif defined __riscv
	typedef char *__builtin_va_list;
	#define __va_reg_size (__riscv_xlen >> 3)
	#define _mcc_align(addr,type) (((__mcc_ulong_t)addr + __alignof__(type) - 1) \
	& -(__alignof__(type)))
	#define __builtin_va_arg(ap,type) (*(sizeof(type) > (2*__va_reg_size) ? *(type **)((ap += __va_reg_size) - __va_reg_size) : (ap = (va_list)(_mcc_align(ap,type) + (sizeof(type)+__va_reg_size - 1)& -__va_reg_size), (type *)(ap - ((sizeof(type)+ __va_reg_size - 1)& -__va_reg_size)))))

#else
	typedef char *__builtin_va_list;
	#define __builtin_va_start(ap,last) (ap = ((__mcc_char_t *)&(last)) + ((sizeof(last)+3)&~3))
	#define __builtin_va_arg(ap,t) (*(t*)((ap+=(sizeof(t)+3)&~3)-((sizeof(t)+3)&~3)))

#endif
	#define __builtin_va_end(ap) (void)(ap)
	#ifndef __builtin_va_copy
	# define __builtin_va_copy(dest, src) (dest) = (src)
	#endif

	#ifdef __leading_underscore
	# define __RENAME(X) __asm__("_"X)
	#else
	# define __RENAME(X) __asm__(X)
	#endif

	#ifdef __MCC_BCHECK__
	# define __BUILTINBC(ret,name,params) ret __builtin_##name params __RENAME("__bound_"#name);
	# define __BOUND(ret,name,params) ret name params __RENAME("__bound_"#name);
	#else
	# define __BUILTINBC(ret,name,params) ret __builtin_##name params __RENAME(#name);
	# define __BOUND(ret,name,params)
	#endif
	#define __BOTH(ret,name,params) __BUILTINBC(ret,name,params)__BOUND(ret,name,params)
	#define __BUILTIN(ret,name,params) ret __builtin_##name params __RENAME(#name);

	__BOTH(void*, memcpy, (void *, const void*, __SIZE_TYPE__))
	__BOTH(void*, memmove, (void *, const void*, __SIZE_TYPE__))
	__BOTH(void*, memset, (void *, int, __SIZE_TYPE__))
	__BOTH(int, memcmp, (const void *, const void*, __SIZE_TYPE__))
	__BOTH(__SIZE_TYPE__, strlen, (const char *))
	__BOTH(char*, strcpy, (char *, const char *))
	__BOTH(char*, strncpy, (char *, const char*, __SIZE_TYPE__))
	__BOTH(int, strcmp, (const char*, const char*))
	__BOTH(int, strncmp, (const char*, const char*, __SIZE_TYPE__))
	__BOTH(char*, strcat, (char*, const char*))
	__BOTH(char*, strncat, (char*, const char*, __SIZE_TYPE__))
	__BOTH(char*, strchr, (const char*, int))
	__BOTH(char*, strrchr, (const char*, int))
	__BOTH(char*, strdup, (const char*))
#if defined __ARM_EABI__
	__BOUND(void*,__aeabi_memcpy,(void*,const void*,__SIZE_TYPE__))
	__BOUND(void*,__aeabi_memmove,(void*,const void*,__SIZE_TYPE__))
	__BOUND(void*,__aeabi_memmove4,(void*,const void*,__SIZE_TYPE__))
	__BOUND(void*,__aeabi_memmove8,(void*,const void*,__SIZE_TYPE__))
	__BOUND(void*,__aeabi_memset,(void*,int,__SIZE_TYPE__))
#endif

#if defined __linux__ || defined __APPLE__
	#define __MAYBE_REDIR __BUILTIN
#else
	#define __MAYBE_REDIR __BOTH
#endif
	__MAYBE_REDIR(void*, malloc, (__SIZE_TYPE__))
	__MAYBE_REDIR(void*, realloc, (void *, __SIZE_TYPE__))
	__MAYBE_REDIR(void*, calloc, (__SIZE_TYPE__, __SIZE_TYPE__))
	__MAYBE_REDIR(void*, memalign, (__SIZE_TYPE__, __SIZE_TYPE__))
	__MAYBE_REDIR(void, free, (void*))
	__BOTH(void*, alloca, (__SIZE_TYPE__))
	void *alloca(__SIZE_TYPE__);
	__BUILTIN(void, abort, (void))
	__BOUND(void, longjmp, ())
#if !defined _WIN32
	__BOUND(void*, mmap, ())
	__BOUND(int, munmap, ())
#endif
	__BUILTIN(int, printf, (const char*, ...))
	__BUILTIN(int, fprintf, (void*, const char*, ...))
	__BUILTIN(int, sprintf, (char*, const char*, ...))
	__BUILTIN(int, snprintf, (char*, __SIZE_TYPE__, const char*, ...))
	__BUILTIN(int, vsprintf, (char*, const char*, __builtin_va_list))
	__BUILTIN(int, vsnprintf, (char*, __SIZE_TYPE__, const char*, __builtin_va_list))
	__BUILTIN(int, puts, (const char*))
	__BUILTIN(int, fputs, (const char*, void*))
	__BUILTIN(int, fputc, (int, void*))
	__BUILTIN(int, putchar, (int))
	__BUILTIN(__SIZE_TYPE__, fwrite, (const void*, __SIZE_TYPE__, __SIZE_TYPE__, void*))
	__BUILTIN(int, fprintf_unlocked, (void*, const char*, ...))
	__BUILTIN(int, fputs_unlocked, (const char*, void*))
	__BUILTIN(int, printf_unlocked, (const char*, ...))
	__BUILTIN(void, exit, (int))
	__BUILTIN(void*, memchr, (const void*, int, __SIZE_TYPE__))
	__BUILTIN(void*, mempcpy, (void*, const void*, __SIZE_TYPE__))
	__BUILTIN(char*, stpcpy, (char*, const char*))
	__BUILTIN(char*, stpncpy, (char*, const char*, __SIZE_TYPE__))
	__BUILTIN(__SIZE_TYPE__, strnlen, (const char*, __SIZE_TYPE__))
	__BUILTIN(char*, strstr, (const char*, const char*))
	__BUILTIN(__SIZE_TYPE__, strspn, (const char*, const char*))
	__BUILTIN(__SIZE_TYPE__, strcspn, (const char*, const char*))
	__BUILTIN(char*, strpbrk, (const char*, const char*))
	__BUILTIN(void, bcopy, (const void*, void*, __SIZE_TYPE__))
	__BUILTIN(int, bcmp, (const void*, const void*, __SIZE_TYPE__))
	__BUILTIN(char*, index, (const char*, int))
	__BUILTIN(char*, rindex, (const char*, int))
#ifndef __builtin_bzero
	__BUILTIN(void, bzero, (void*, __SIZE_TYPE__))
#endif
	__BUILTIN(void*, aligned_alloc, (__SIZE_TYPE__, __SIZE_TYPE__))
	__BUILTIN(double, cabs, (double _Complex))
	__BUILTIN(float, cabsf, (float _Complex))
	__BUILTIN(long double, cabsl, (long double _Complex))
	__BUILTIN(int, strcasecmp, (const char*, const char*))
	__BUILTIN(int, strncasecmp, (const char*, const char*, __SIZE_TYPE__))
	__BUILTIN(char*, strndup, (const char*, __SIZE_TYPE__))
	#undef __BUILTINBC
	#undef __BUILTIN
	#undef __BOUND
	#undef __BOTH
	#undef __MAYBE_REDIR
	#undef __RENAME

	extern void __chk_fail(void) __attribute__((noreturn));

	static __inline void *__builtin___memcpy_chk(void *dst, const void *src, __mcc_size_t len, __mcc_size_t dstlen) {
	if (dstlen != (__mcc_size_t)-1 && len > dstlen)
	__chk_fail();
	return __builtin_memcpy(dst, src, len);
	}
	static __inline void *__builtin___mempcpy_chk(void *dst, const void *src, __mcc_size_t len, __mcc_size_t dstlen) {
	if (dstlen != (__mcc_size_t)-1 && len > dstlen)
	__chk_fail();
	return __builtin_mempcpy(dst, src, len);
	}
	static __inline void *__builtin___memmove_chk(void *dst, const void *src, __mcc_size_t len, __mcc_size_t dstlen) {
	if (dstlen != (__mcc_size_t)-1 && len > dstlen)
	__chk_fail();
	return __builtin_memmove(dst, src, len);
	}
	static __inline void *__builtin___memset_chk(void *dst, int val, __mcc_size_t len, __mcc_size_t dstlen) {
	if (dstlen != (__mcc_size_t)-1 && len > dstlen)
	__chk_fail();
	return __builtin_memset(dst, val, len);
	}
	static __inline char *__builtin___strcpy_chk(char *dst, const char *src, __mcc_size_t dstlen) {
	__mcc_size_t len = __builtin_strlen(src) + 1;
	if (dstlen != (__mcc_size_t)-1 && len > dstlen)
	__chk_fail();
	return __builtin_strcpy(dst, src);
	}
	static __inline char *__builtin___stpcpy_chk(char *dst, const char *src, __mcc_size_t dstlen) {
	__mcc_size_t len = __builtin_strlen(src) + 1;
	if (dstlen != (__mcc_size_t)-1 && len > dstlen)
	__chk_fail();
	return __builtin_stpcpy(dst, src);
	}
	static __inline char *__builtin___strcat_chk(char *dst, const char *src, __mcc_size_t dstlen) {
	__mcc_size_t dl = __builtin_strlen(dst);
	__mcc_size_t sl = __builtin_strlen(src);
	if (dstlen != (__mcc_size_t)-1 && dl + sl + 1 > dstlen)
	__chk_fail();
	return __builtin_strcat(dst, src);
	}
	static __inline char *__builtin___strncpy_chk(char *dst, const char *src, __mcc_size_t len, __mcc_size_t dstlen) {
	if (dstlen != (__mcc_size_t)-1 && len > dstlen)
	__chk_fail();
	return __builtin_strncpy(dst, src, len);
	}
	static __inline char *__builtin___stpncpy_chk(char *dst, const char *src, __mcc_size_t len, __mcc_size_t dstlen) {
	if (dstlen != (__mcc_size_t)-1 && len > dstlen)
	__chk_fail();
	return __builtin_stpncpy(dst, src, len);
	}
	static __inline char *__builtin___strncat_chk(char *dst, const char *src, __mcc_size_t len, __mcc_size_t dstlen) {
	__mcc_size_t dl = __builtin_strlen(dst);
	__mcc_size_t sl = __builtin_strlen(src);
	__mcc_size_t addlen = sl < len ? sl : len;
	if (dstlen != (__mcc_size_t)-1 && dl + addlen + 1 > dstlen)
	__chk_fail();
	return __builtin_strncat(dst, src, len);
	}
	static __inline int __builtin___vsnprintf_chk(char *s, __mcc_size_t n, int flag, __mcc_size_t slen, const char *fmt, __builtin_va_list ap) {
	(void)flag;
	if (slen != (__mcc_size_t)-1 && n > slen)
	__chk_fail();
	return __builtin_vsnprintf(s, n, fmt, ap);
	}
	static __inline int __builtin___snprintf_chk(char *s, __mcc_size_t n, int flag, __mcc_size_t slen, const char *fmt, ...) {
	__builtin_va_list ap;
	int r;
	(void)flag;
	if (slen != (__mcc_size_t)-1 && n > slen)
	__chk_fail();
	__builtin_va_start(ap, fmt);
	r = __builtin_vsnprintf(s, n, fmt, ap);
	__builtin_va_end(ap);
	return r;
	}
	static __inline int __builtin___vsprintf_chk(char *s, int flag, __mcc_size_t slen, const char *fmt, __builtin_va_list ap) {
	(void)flag;
	if (slen == (__mcc_size_t)-1)
	return __builtin_vsprintf(s, fmt, ap);
	{
	int r = __builtin_vsnprintf(s, slen, fmt, ap);
	if ((__mcc_size_t)r >= slen)
	__chk_fail();
	return r;
	}
	}
	static __inline int __builtin___sprintf_chk(char *s, int flag, __mcc_size_t slen, const char *fmt, ...) {
	__builtin_va_list ap;
	int r;
	(void)flag;
	__builtin_va_start(ap, fmt);
	if (slen == (__mcc_size_t)-1) {
	r = __builtin_vsprintf(s, fmt, ap);
	} else {
	r = __builtin_vsnprintf(s, slen, fmt, ap);
	if ((__mcc_size_t)r >= slen)
	__chk_fail();
	}
	__builtin_va_end(ap);
	return r;
	}

	#define __MCC_OV_DECL(T, NM)			\
	int __mcc_addo_##NM(T, T, T*);		\
	int __mcc_subo_##NM(T, T, T*);		\
	int __mcc_mulo_##NM(T, T, T*);
	#define __MCC_OV_DECL_W(T, W, NM)		\
	int __mcc_addo_##NM(W, W, T*);		\
	int __mcc_subo_##NM(W, W, T*);		\
	int __mcc_mulo_##NM(W, W, T*);
	__MCC_OV_DECL_W(signed char, long long, sc)
	__MCC_OV_DECL_W(char, long long, c)
	__MCC_OV_DECL_W(short, long long, s)
	__MCC_OV_DECL_W(int, long long, i)
	__MCC_OV_DECL_W(unsigned char, unsigned long long, uc)
	__MCC_OV_DECL_W(unsigned short, unsigned long long, us)
	__MCC_OV_DECL_W(unsigned int, unsigned long long, u)
	#if __SIZEOF_LONG__ == 8
	__MCC_OV_DECL(long, l)
	__MCC_OV_DECL(unsigned long, ul)
	#else
	__MCC_OV_DECL_W(long, long long, l)
	__MCC_OV_DECL_W(unsigned long, unsigned long long, ul)
	#endif
	__MCC_OV_DECL(long long, ll)
	__MCC_OV_DECL(unsigned long long, ull)
	#ifdef __SIZEOF_INT128__
	__MCC_OV_DECL(__mcc_int128_t, ti)
	__MCC_OV_DECL(__mcc_uint128_t, uti)
	#endif
	#undef __MCC_OV_DECL
	#undef __MCC_OV_DECL_W
	#ifdef __SIZEOF_INT128__
	#define __mcc_ov_disp_ti(op) __mcc_int128_t: __mcc_##op##o_ti,	\
	__mcc_uint128_t: __mcc_##op##o_uti,
	#else
	#define __mcc_ov_disp_ti(op)
	#endif
	#define __mcc_ov_disp(op, res) _Generic((res),			\
	__mcc_ov_disp_ti(op)						\
	__mcc_schar_t: __mcc_##op##o_sc, __mcc_char_t: __mcc_##op##o_c,	\
	__mcc_short_t: __mcc_##op##o_s, __mcc_int_t: __mcc_##op##o_i,	\
	__mcc_long_t: __mcc_##op##o_l, __mcc_llong_t: __mcc_##op##o_ll,	\
	__mcc_uchar_t: __mcc_##op##o_uc, __mcc_ushort_t: __mcc_##op##o_us, \
	__mcc_uint_t: __mcc_##op##o_u, __mcc_ulong_t: __mcc_##op##o_ul,	\
	__mcc_ullong_t: __mcc_##op##o_ull)
	#define __builtin_add_overflow(a, b, res) \
	(__mcc_ov_disp(add, *(res))((a), (b), (res)))
	#define __builtin_sub_overflow(a, b, res) \
	(__mcc_ov_disp(sub, *(res))((a), (b), (res)))
	#define __builtin_mul_overflow(a, b, res) \
	(__mcc_ov_disp(mul, *(res))((a), (b), (res)))
	#define __builtin_add_overflow_p(a, b, c) \
	({ __typeof__(c) __mcc_ovp_r; __builtin_add_overflow((a), (b), &__mcc_ovp_r); })
	#define __builtin_sub_overflow_p(a, b, c) \
	({ __typeof__(c) __mcc_ovp_r; __builtin_sub_overflow((a), (b), &__mcc_ovp_r); })
	#define __builtin_mul_overflow_p(a, b, c) \
	({ __typeof__(c) __mcc_ovp_r; __builtin_mul_overflow((a), (b), &__mcc_ovp_r); })
	#define __builtin_assoc_barrier(x) (x)
	#define __builtin_assume(cond) ((void)0)

	#endif
