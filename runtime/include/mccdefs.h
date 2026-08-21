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

#if !defined _WIN32 && !defined __APPLE__
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
	#define __GNUC_MINOR__ 2
	#define __GNUC_PATCHLEVEL__ 1
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
#elif defined _WIN32
	#define __WCHAR_MAX__ 0xffff
	#define __WCHAR_MIN__ 0
#else
	#define __WCHAR_MAX__ 0x7fffffff
	#define __WCHAR_MIN__ (-__WCHAR_MAX__ - 1)
#endif
#if defined _WIN32 || defined __linux__
	#define __WINT_MAX__ 0xffffffffU
	#define __WINT_MIN__ 0U
#else
	#define __WINT_MAX__ 0x7fffffff
	#define __WINT_MIN__ (-__WINT_MAX__ - 1)
#endif

	#define __SIG_ATOMIC_TYPE__ int
	#define __SIG_ATOMIC_MAX__ 0x7fffffff
	#define __SIG_ATOMIC_MIN__ (-__SIG_ATOMIC_MAX__ - 1)

	#define __BIGGEST_ALIGNMENT__ 16
	#if defined __i386__ && !defined __SSE2_MATH__
	#define __FLT_EVAL_METHOD__ 2
	#else
	#define __FLT_EVAL_METHOD__ 0
	#endif

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
	#define __FLT_NORM_MAX__ __FLT_MAX__
	#define __FLT_DECIMAL_DIG__ 9
	#define __FLT_IS_IEC_60559__ 1

	#define __FLT16_MANT_DIG__ 11
	#define __FLT16_DIG__ 3
	#define __FLT16_MIN_EXP__ (-13)
	#define __FLT16_MIN_10_EXP__ (-4)
	#define __FLT16_MAX_EXP__ 16
	#define __FLT16_MAX_10_EXP__ 4
	#define __FLT16_DECIMAL_DIG__ 5
	#define __FLT16_MAX__ 6.55040000000000000000000000000000000e+4F16
	#define __FLT16_NORM_MAX__ 6.55040000000000000000000000000000000e+4F16
	#define __FLT16_MIN__ 6.10351562500000000000000000000000000e-5F16
	#define __FLT16_EPSILON__ 9.76562500000000000000000000000000000e-4F16
	#define __FLT16_DENORM_MIN__ 5.96046447753906250000000000000000000e-8F16
	#define __FLT16_HAS_DENORM__ 1
	#define __FLT16_HAS_INFINITY__ 1
	#define __FLT16_HAS_QUIET_NAN__ 1
	#define __FLT16_IS_IEC_60559__ 1

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
	#define __DBL_NORM_MAX__ __DBL_MAX__
	#define __DBL_DECIMAL_DIG__ 17
	#define __DBL_IS_IEC_60559__ 1

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
	#define __LDBL_NORM_MAX__ __LDBL_MAX__
	#define __LDBL_DECIMAL_DIG__ 21
	#define __LDBL_IS_IEC_60559__ 1
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
	#define __LDBL_NORM_MAX__ __LDBL_MAX__
	#define __LDBL_DECIMAL_DIG__ __DBL_DECIMAL_DIG__
	#define __LDBL_IS_IEC_60559__ __DBL_IS_IEC_60559__
#endif
#if defined __aarch64__ || defined __riscv || (defined __x86_64__ && !defined _WIN32)
	#define __FLT128_MANT_DIG__ 113
	#define __FLT128_DIG__ 33
	#define __FLT128_MIN_EXP__ (-16381)
	#define __FLT128_MIN_10_EXP__ (-4931)
	#define __FLT128_MAX_EXP__ 16384
	#define __FLT128_MAX_10_EXP__ 4932
	#define __FLT128_DECIMAL_DIG__ 36
	#define __FLT128_MAX__ 1.18973149535723176508575932662800702e+4932F128
	#define __FLT128_NORM_MAX__ 1.18973149535723176508575932662800702e+4932F128
	#define __FLT128_MIN__ 3.36210314311209350626267781732175260e-4932F128
	#define __FLT128_EPSILON__ 1.92592994438723585305597794258492732e-34F128
	#define __FLT128_DENORM_MIN__ 6.47517511943802511092443895822764655e-4966F128
	#define __FLT128_HAS_DENORM__ 1
	#define __FLT128_HAS_INFINITY__ 1
	#define __FLT128_HAS_QUIET_NAN__ 1
	#define __FLT128_IS_IEC_60559__ 1
#endif
#if defined __riscv
	#define __NO_LONG_DOUBLE_MATH 1
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
	#define __sync_fetch_and_nand(p,v) __atomic_fetch_nand((p),(v),__ATOMIC_SEQ_CST)
	#define __sync_add_and_fetch(p,v) (__atomic_fetch_add((p),(v),__ATOMIC_SEQ_CST)+(v))
	#define __sync_sub_and_fetch(p,v) (__atomic_fetch_sub((p),(v),__ATOMIC_SEQ_CST)-(v))
	#define __sync_or_and_fetch(p,v)  (__atomic_fetch_or((p),(v),__ATOMIC_SEQ_CST)|(v))
	#define __sync_and_and_fetch(p,v) (__atomic_fetch_and((p),(v),__ATOMIC_SEQ_CST)&(v))
	#define __sync_xor_and_fetch(p,v) (__atomic_fetch_xor((p),(v),__ATOMIC_SEQ_CST)^(v))
	#define __sync_nand_and_fetch(p,v) __atomic_nand_fetch((p),(v),__ATOMIC_SEQ_CST)
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
	/* T-mac-30171: bzero(p,n) zeros n bytes; the old macro dropped the size
	 * and zeroed sizeof(*p). Delegate to memset (a working redirect that
	 * needs no <strings.h>). */
	#define __builtin_bzero(p, n) __builtin_memset((p), 0, (n))
# endif
#endif
#if defined _WIN32
	/* The win CRT (msvcrt/ucrt) has no bzero symbol; delegate to memset like
	 * macOS so __builtin_bzero links (mirrors the T-mac-30171 macOS redirect). */
	#define __builtin_bzero(p, n) __builtin_memset((p), 0, (n))
#endif
	__mcc_float_t __mcc_nansf(const char *);
	__mcc_double_t __mcc_nans(const char *);
	__mcc_ldouble_t __mcc_nansl(const char *);
	#define __mcc_bitsof_f(x) (__extension__ (union { __mcc_float_t __f; \
	__mcc_uint_t __u; }){ .__f = (x) }.__u)
	#define __mcc_bitsof_d(x) (__extension__ (union { __mcc_double_t __d; \
	__mcc_ullong_t __u; }){ .__d = (x) }.__u)
	#define __mcc_issig_f(x) ((__mcc_bitsof_f(x) & 0x7fc00000U) == 0x7f800000U \
	&& (__mcc_bitsof_f(x) & 0x003fffffU) != 0)
	#define __mcc_issig_d(x) ((__mcc_bitsof_d(x) & 0x7ff8000000000000ULL) \
	== 0x7ff0000000000000ULL && (__mcc_bitsof_d(x) & 0x0007ffffffffffffULL) != 0)
	#if defined __i386__ || defined __x86_64__
	#define __mcc_ldparts(x) (__extension__ (union { __mcc_ldouble_t __l; \
	struct { __mcc_ullong_t __s; __mcc_ushort_t __e; } __p; }){ .__l = (x) }.__p)
	#define __mcc_issig_l(x) ((__mcc_ldparts(x).__e & 0x7fffU) == 0x7fffU \
	&& (__mcc_ldparts(x).__s & 0x4000000000000000ULL) == 0 \
	&& (__mcc_ldparts(x).__s & 0x3fffffffffffffffULL) != 0)
	#else
	#define __mcc_issig_l(x) __mcc_issig_d((__mcc_double_t)(x))
	#endif
	#define __builtin_issignaling(x) _Generic((x), \
	__mcc_float_t: __mcc_issig_f((__mcc_float_t)(x)), \
	__mcc_ldouble_t: __mcc_issig_l((__mcc_ldouble_t)(x)), \
	default: __mcc_issig_d((__mcc_double_t)(x)))
	#define __mcc_nanbits_f(u) (__extension__ (union { __mcc_uint_t __u; \
	__mcc_float_t __f; }){ .__u = (u) }.__f)
	#define __mcc_nanbits_d(u) (__extension__ (union { __mcc_ullong_t __u; \
	__mcc_double_t __d; }){ .__u = (u) }.__d)
	#if defined _WIN32 || defined __arm__ \
	|| (defined __aarch64__ && (defined __APPLE__ || defined _WIN32))
	#define __mcc_nanbits_l(hi, lo) (__extension__ (union { __mcc_ullong_t __u; \
	__mcc_ldouble_t __l; }){ .__u = (lo) }.__l)
	#define __MCC_SNAN_L __mcc_nanbits_l(0, 0x7ff4000000000000ULL)
	#elif defined __i386__ || defined __x86_64__
	#define __mcc_nanbits_l(hi, lo) (__extension__ (union { struct { \
	__mcc_ullong_t __s; __mcc_ushort_t __e; } __p; __mcc_ldouble_t __l; }) \
	{ .__p = { (lo), (__mcc_ushort_t)(hi) } }.__l)
	#define __MCC_SNAN_L __mcc_nanbits_l(0x7fff, 0xa000000000000000ULL)
	#else
	#define __mcc_nanbits_l(hi, lo) (__extension__ (union { __mcc_ullong_t __u[2]; \
	__mcc_ldouble_t __l; }){ .__u = { (lo), (hi) } }.__l)
	#define __MCC_SNAN_L __mcc_nanbits_l(0x7fff400000000000ULL, 0)
	#endif
	#define __MCC_SNAN_F __mcc_nanbits_f(0x7fa00000U)
	#define __MCC_SNAN_D __mcc_nanbits_d(0x7ff4000000000000ULL)
	#ifndef __builtin_nansf
	#define __builtin_nansf(s) (sizeof(s) == 1 ? __MCC_SNAN_F : __mcc_nansf(s))
	#endif
	#ifndef __builtin_nans
	#define __builtin_nans(s) (sizeof(s) == 1 ? __MCC_SNAN_D : __mcc_nans(s))
	#endif
	#ifndef __builtin_nansl
	#define __builtin_nansl(s) (sizeof(s) == 1 ? __MCC_SNAN_L : __mcc_nansl(s))
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
	#if !defined __x86_64__ && !defined __i386__ && !defined __aarch64__ && !defined __riscv
	#define __builtin_fabsf(x) ((__mcc_float_t)(__builtin_signbitf((__mcc_float_t)(x)) \
	? -(__mcc_float_t)(x) : (__mcc_float_t)(x)))
	#define __builtin_fabs(x)  ((__mcc_double_t)(__builtin_signbit((__mcc_double_t)(x)) \
	? -(__mcc_double_t)(x) : (__mcc_double_t)(x)))
	#endif
	#define __builtin_fabsl(x) ((__mcc_ldouble_t)(__builtin_signbitl((__mcc_ldouble_t)(x)) \
	? -(__mcc_ldouble_t)(x) : (__mcc_ldouble_t)(x)))
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
	if (size == 8) {
	if (ap->fp_offset < 128 + 48) {
	ap->fp_offset += 16;
	return ap->reg_save_area + ap->fp_offset - 16;
	}
	} else if (ap->fp_offset + 32 <= 128 + 48) {
	*(long long *)(ap->reg_save_area + ap->fp_offset + 8) =
	*(long long *)(ap->reg_save_area + ap->fp_offset + 16);
	ap->fp_offset += 32;
	return ap->reg_save_area + ap->fp_offset - 32;
	}
	} else if (arg_type == 3 || arg_type == 4) {
	if (ap->gp_offset + 8 <= 48 && ap->fp_offset < 128 + 48) {
	char *__mcc_vg = ap->reg_save_area + ap->gp_offset;
	char *__mcc_vf = ap->reg_save_area + ap->fp_offset;
	ap->gp_offset += 8;
	ap->fp_offset += 16;
	if (arg_type == 3) {
	*(long long *)(__mcc_vf + 8) = *(long long *)__mcc_vf;
	*(long long *)__mcc_vf = *(long long *)__mcc_vg;
	} else {
	*(long long *)(__mcc_vf + 8) = *(long long *)__mcc_vg;
	}
	return __mcc_vf;
	}
	} else if (arg_type == 5) {
	if (ap->fp_offset < 128 + 48) {
	ap->fp_offset += 16;
	return ap->reg_save_area + ap->fp_offset - 16;
	}
	}
	ap->overflow_arg_area += size;
	ap->overflow_arg_area = (char *)((long long)(ap->overflow_arg_area + align - 1) & -align);
	return ap->overflow_arg_area - size;
	}
	#define __builtin_va_start(ap, last) \
	(__builtin_va_start_check(last), \
	 *(ap) = *(struct __va_list_tag *)((__mcc_char_t*)__builtin_frame_address(0) - 24))
	#define __builtin_c23_va_start(ap, ...) \
	(__builtin_va_start_check(__VA_ARGS__), \
	 *(ap) = *(struct __va_list_tag *)((__mcc_char_t*)__builtin_frame_address(0) - 24))
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
#if defined __APPLE__ || defined _WIN32
	__BUILTIN(int, vprintf, (const char*, __builtin_va_list))
	__BUILTIN(int, vfprintf, (void*, const char*, __builtin_va_list))
	static __inline int __builtin_fputs_unlocked(const char *__s, void *__f) {
		return __builtin_fputs(__s, __f);
	}
	static __inline int __builtin_printf_unlocked(const char *__f, ...) {
		__builtin_va_list __ap; int __r;
		__builtin_va_start(__ap, __f);
		__r = __builtin_vprintf(__f, __ap);
		__builtin_va_end(__ap);
		return __r;
	}
	static __inline int __builtin_fprintf_unlocked(void *__s, const char *__f, ...) {
		__builtin_va_list __ap; int __r;
		__builtin_va_start(__ap, __f);
		__r = __builtin_vfprintf(__s, __f, __ap);
		__builtin_va_end(__ap);
		return __r;
	}
#else
	__BUILTIN(int, fprintf_unlocked, (void*, const char*, ...))
	__BUILTIN(int, fputs_unlocked, (const char*, void*))
	__BUILTIN(int, printf_unlocked, (const char*, ...))
#endif
	__BUILTIN(void, exit, (int))
	__BUILTIN(void*, memchr, (const void*, int, __SIZE_TYPE__))
#if defined __APPLE__ || defined _WIN32
	/* T-mac-30170: macOS libc (and the win msvcrt/ucrt) has no mempcpy symbol,
	 * so the external redirect would leave an unresolved reference. Provide it
	 * inline (memcpy then return dst+n, the GNU semantics). */
	static __inline void *__builtin_mempcpy(void *__d, const void *__s, __SIZE_TYPE__ __n) {
		__builtin_memcpy(__d, __s, __n);
		return (char *)__d + __n;
	}
#else
	__BUILTIN(void*, mempcpy, (void*, const void*, __SIZE_TYPE__))
#endif
	__BUILTIN(char*, stpcpy, (char*, const char*))
	__BUILTIN(char*, stpncpy, (char*, const char*, __SIZE_TYPE__))
	__BUILTIN(__SIZE_TYPE__, strnlen, (const char*, __SIZE_TYPE__))
	__BUILTIN(char*, strstr, (const char*, const char*))
	__BUILTIN(__SIZE_TYPE__, strspn, (const char*, const char*))
	__BUILTIN(__SIZE_TYPE__, strcspn, (const char*, const char*))
	__BUILTIN(char*, strpbrk, (const char*, const char*))
	__BOTH(void, bcopy, (const void*, void*, __SIZE_TYPE__))
	__BUILTIN(int, bcmp, (const void*, const void*, __SIZE_TYPE__))
	__BOTH(char*, index, (const char*, int))
	__BOTH(char*, rindex, (const char*, int))
#ifndef __builtin_bzero
	__BUILTIN(void, bzero, (void*, __SIZE_TYPE__))
#endif
	__BUILTIN(void*, aligned_alloc, (__SIZE_TYPE__, __SIZE_TYPE__))
	__BUILTIN(double, cabs, (double _Complex))
	__BUILTIN(float, cabsf, (float _Complex))
	__BUILTIN(long double, cabsl, (long double _Complex))
	__BUILTIN(int, finite, (double))
	__BUILTIN(int, finitef, (float))
	__BUILTIN(int, finitel, (long double))
	__BUILTIN(int, ilogb, (double))
	__BUILTIN(int, ilogbf, (float))
	__BUILTIN(int, ilogbl, (long double))
	__BUILTIN(double, ldexp, (double, int))
	__BUILTIN(float, ldexpf, (float, int))
	__BUILTIN(long double, ldexpl, (long double, int))
	__BUILTIN(double, scalbn, (double, int))
	__BUILTIN(float, scalbnf, (float, int))
	__BUILTIN(long double, scalbnl, (long double, int))
	__BUILTIN(double, scalbln, (double, long))
	__BUILTIN(float, scalblnf, (float, long))
	__BUILTIN(long double, scalblnl, (long double, long))
	__BUILTIN(long, lrint, (double))
	__BUILTIN(long, lrintf, (float))
	__BUILTIN(long, lrintl, (long double))
	__BUILTIN(long long, llrint, (double))
	__BUILTIN(long long, llrintf, (float))
	__BUILTIN(long long, llrintl, (long double))
	__BUILTIN(long, lround, (double))
	__BUILTIN(long, lroundf, (float))
	__BUILTIN(long, lroundl, (long double))
	__BUILTIN(long long, llround, (double))
	__BUILTIN(long long, llroundf, (float))
	__BUILTIN(long long, llroundl, (long double))
	__BUILTIN(double, frexp, (double, int*))
	__BUILTIN(float, frexpf, (float, int*))
	__BUILTIN(long double, frexpl, (long double, int*))
	__BUILTIN(double, modf, (double, double*))
	__BUILTIN(float, modff, (float, float*))
	__BUILTIN(long double, modfl, (long double, long double*))
	__BUILTIN(void, sincos, (double, double*, double*))
	__BUILTIN(void, sincosf, (float, float*, float*))
	__BUILTIN(void, sincosl, (long double, long double*, long double*))
	__BUILTIN(double, remquo, (double, double, int*))
	__BUILTIN(float, remquof, (float, float, int*))
	__BUILTIN(long double, remquol, (long double, long double, int*))
	__BUILTIN(int, strcasecmp, (const char*, const char*))
	__BUILTIN(int, strncasecmp, (const char*, const char*, __SIZE_TYPE__))
	__BUILTIN(char*, strndup, (const char*, __SIZE_TYPE__))
	#undef __BUILTINBC
	#undef __BUILTIN
	#undef __BOUND
	#undef __BOTH
	#undef __MAYBE_REDIR
	#undef __RENAME

	static __inline double __builtin_powi(double __x, int __m) {
	unsigned int __n = __m < 0 ? -(unsigned int)__m : (unsigned int)__m;
	double __y = (__n & 1) ? __x : 1.0;
	while (__n >>= 1) {
	__x = __x * __x;
	if (__n & 1)
	__y = __y * __x;
	}
	return __m < 0 ? 1.0 / __y : __y;
	}
	static __inline float __builtin_powif(float __x, int __m) {
	unsigned int __n = __m < 0 ? -(unsigned int)__m : (unsigned int)__m;
	float __y = (__n & 1) ? __x : 1.0f;
	while (__n >>= 1) {
	__x = __x * __x;
	if (__n & 1)
	__y = __y * __x;
	}
	return __m < 0 ? 1.0f / __y : __y;
	}
	static __inline long double __builtin_powil(long double __x, int __m) {
	unsigned int __n = __m < 0 ? -(unsigned int)__m : (unsigned int)__m;
	long double __y = (__n & 1) ? __x : 1.0L;
	while (__n >>= 1) {
	__x = __x * __x;
	if (__n & 1)
	__y = __y * __x;
	}
	return __m < 0 ? 1.0L / __y : __y;
	}

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

	static __inline __mcc_ullong_t __mcc_ov_lo32(__mcc_ullong_t __v) {
	return __v & (__mcc_ullong_t)0xffffffffu;
	}
	static __inline int __mcc_ov_calc(int __op, int __na, __mcc_ullong_t __ma,
	int __nb, __mcc_ullong_t __mb, __mcc_ullong_t __tmax, int __tsig,
	__mcc_ullong_t *__out) {
	__mcc_ullong_t __m;
	int __neg, __wide = 0;
	if (__op == 1)
	__nb = __mb ? !__nb : 0;
	if (__op == 2) {
	__mcc_ullong_t __p0 = __mcc_ov_lo32(__ma) * __mcc_ov_lo32(__mb);
	__mcc_ullong_t __p1 = (__ma >> 32) * __mcc_ov_lo32(__mb);
	__mcc_ullong_t __p2 = __mcc_ov_lo32(__ma) * (__mb >> 32);
	__mcc_ullong_t __p3 = (__ma >> 32) * (__mb >> 32);
	__mcc_ullong_t __mid = (__p0 >> 32) + __mcc_ov_lo32(__p1) + __mcc_ov_lo32(__p2);
	__neg = (__ma && __mb) ? (__na ^ __nb) : 0;
	__m = __mcc_ov_lo32(__p0) | (__mid << 32);
	__wide = (__p3 + (__p1 >> 32) + (__p2 >> 32) + (__mid >> 32)) != 0;
	} else if (__na == __nb) {
	__neg = __na;
	__m = __ma + __mb;
	__wide = __m < __ma;
	} else if (__ma >= __mb) {
	__neg = __na;
	__m = __ma - __mb;
	} else {
	__neg = __nb;
	__m = __mb - __ma;
	}
	if (!__m && !__wide)
	__neg = 0;
	*__out = __neg ? (__mcc_ullong_t)0 - __m : __m;
	if (__wide)
	return 1;
	if (!__neg)
	return __m > __tmax;
	if (!__tsig)
	return 1;
	return __m > __tmax + 1;
	}
	#define __mcc_ov_neg(x) ((x) < 0)
	#define __mcc_ov_mag(x) ((x) < 0 ? (__mcc_ullong_t)0 - (__mcc_ullong_t)(x) \
	: (__mcc_ullong_t)(x))
	#define __mcc_ov_tsig(x) ((__typeof__(x))-1 < 0)
	#define __mcc_ov_tmax(x) ((__mcc_ullong_t)~(__mcc_ullong_t)0 >> \
	(8 * sizeof(x) >= 8 * sizeof(__mcc_ullong_t) ? (int)__mcc_ov_tsig(x) \
	: (int)(8 * sizeof(__mcc_ullong_t) - 8 * sizeof(x) + __mcc_ov_tsig(x))))
	#ifdef __SIZEOF_INT128__
	#define __mcc_ov_is_wide(x) _Generic((x), __mcc_int128_t: 1, \
	__mcc_uint128_t: 1, default: 0)
	typedef __mcc_uint128_t __mcc_ov_wide_t;
	static __inline __mcc_ov_wide_t __mcc_ov_lo64(__mcc_ov_wide_t __v) {
	return (__mcc_ov_wide_t)(__mcc_ullong_t)__v;
	}
	static __inline int __mcc_ov_calc_w(int __op, int __na, __mcc_ov_wide_t __ma,
	int __nb, __mcc_ov_wide_t __mb, __mcc_ov_wide_t __tmax, int __tsig,
	__mcc_ov_wide_t *__out) {
	__mcc_ov_wide_t __m;
	int __neg, __wide = 0;
	if (__op == 1)
	__nb = __mb ? !__nb : 0;
	if (__op == 2) {
	__mcc_ov_wide_t __p0 = __mcc_ov_lo64(__ma) * __mcc_ov_lo64(__mb);
	__mcc_ov_wide_t __p1 = (__ma >> 64) * __mcc_ov_lo64(__mb);
	__mcc_ov_wide_t __p2 = __mcc_ov_lo64(__ma) * (__mb >> 64);
	__mcc_ov_wide_t __p3 = (__ma >> 64) * (__mb >> 64);
	__mcc_ov_wide_t __mid = (__p0 >> 64) + __mcc_ov_lo64(__p1) + __mcc_ov_lo64(__p2);
	__neg = (__ma && __mb) ? (__na ^ __nb) : 0;
	__m = __mcc_ov_lo64(__p0) | (__mid << 64);
	__wide = (__p3 + (__p1 >> 64) + (__p2 >> 64) + (__mid >> 64)) != 0;
	} else if (__na == __nb) {
	__neg = __na;
	__m = __ma + __mb;
	__wide = __m < __ma;
	} else if (__ma >= __mb) {
	__neg = __na;
	__m = __ma - __mb;
	} else {
	__neg = __nb;
	__m = __mb - __ma;
	}
	if (!__m && !__wide)
	__neg = 0;
	*__out = __neg ? (__mcc_ov_wide_t)0 - __m : __m;
	if (__wide)
	return 1;
	if (!__neg)
	return __m > __tmax;
	if (!__tsig)
	return 1;
	return __m > __tmax + 1;
	}
	#define __mcc_ov_mag_w(x) ((x) < 0 ? (__mcc_ov_wide_t)0 - (__mcc_ov_wide_t)(x) \
	: (__mcc_ov_wide_t)(x))
	#define __mcc_ov_tmax_w(x) ((__mcc_ov_wide_t)~(__mcc_ov_wide_t)0 >> \
	(8 * sizeof(x) >= 8 * sizeof(__mcc_ov_wide_t) ? (int)__mcc_ov_tsig(x) \
	: (int)(8 * sizeof(__mcc_ov_wide_t) - 8 * sizeof(x) + __mcc_ov_tsig(x))))
	#else
	#define __mcc_ov_is_wide(x) 0
	typedef __mcc_ullong_t __mcc_ov_wide_t;
	#define __mcc_ov_calc_w __mcc_ov_calc
	#define __mcc_ov_mag_w __mcc_ov_mag
	#define __mcc_ov_tmax_w __mcc_ov_tmax
	#endif
	#define __mcc_ov_gen(code, op, a, b, res) __extension__ ({	\
	__typeof__(a) __mcc_ova = (a);				\
	__typeof__(b) __mcc_ovb = (b);				\
	__typeof__(*(res)) *__mcc_ovp = (res);			\
	__mcc_ullong_t __mcc_ovv;				\
	__mcc_ov_wide_t __mcc_ovw;				\
	int __mcc_ovf;						\
	(void)sizeof((__typeof__(*__mcc_ovp))1 % 1);		\
	if (__mcc_ov_is_wide(*__mcc_ovp) || __mcc_ov_is_wide(__mcc_ova)	\
	|| __mcc_ov_is_wide(__mcc_ovb)) {			\
	__mcc_ovf = __mcc_ov_calc_w((code),			\
	__mcc_ov_neg(__mcc_ova), __mcc_ov_mag_w(__mcc_ova),	\
	__mcc_ov_neg(__mcc_ovb), __mcc_ov_mag_w(__mcc_ovb),	\
	__mcc_ov_tmax_w(*__mcc_ovp), __mcc_ov_tsig(*__mcc_ovp),	\
	&__mcc_ovw);						\
	*__mcc_ovp = (__typeof__(*__mcc_ovp))__mcc_ovw;		\
	} else {						\
	__mcc_ovf = __mcc_ov_calc((code),			\
	__mcc_ov_neg(__mcc_ova), __mcc_ov_mag(__mcc_ova),	\
	__mcc_ov_neg(__mcc_ovb), __mcc_ov_mag(__mcc_ovb),	\
	__mcc_ov_tmax(*__mcc_ovp), __mcc_ov_tsig(*__mcc_ovp),	\
	&__mcc_ovv);						\
	*__mcc_ovp = (__typeof__(*__mcc_ovp))__mcc_ovv;		\
	}							\
	__mcc_ovf; })
	#define __builtin_add_overflow(a, b, res) __mcc_ov_gen(0, add, (a), (b), (res))
	#define __builtin_sub_overflow(a, b, res) __mcc_ov_gen(1, sub, (a), (b), (res))
	#define __builtin_mul_overflow(a, b, res) __mcc_ov_gen(2, mul, (a), (b), (res))
	#define __mcc_ovp_rt(code, a, b, c) (				\
	__mcc_ov_is_wide((a)) || __mcc_ov_is_wide((b))			\
	|| __mcc_ov_is_wide((c))					\
	? __mcc_ov_calc_w((code), __mcc_ov_neg(a), __mcc_ov_mag_w(a),	\
	__mcc_ov_neg(b), __mcc_ov_mag_w(b), __mcc_ov_tmax_w((c)),	\
	__mcc_ov_tsig((c)), &(__mcc_ov_wide_t){0})			\
	: __mcc_ov_calc((code), __mcc_ov_neg(a), __mcc_ov_mag(a),	\
	__mcc_ov_neg(b), __mcc_ov_mag(b), __mcc_ov_tmax((c)),		\
	__mcc_ov_tsig((c)), &(__mcc_ullong_t){0}))
	#define __mcc_ovp_ice(code, a, b, c) __builtin_choose_expr(	\
	__builtin_constant_p(a) && __builtin_constant_p(b)		\
	&& sizeof(a) <= 8 && sizeof(b) <= 8 && sizeof(c) <= 8,		\
	__mcc_overflow_p_const((code), (a), (b), (c)),			\
	__mcc_ovp_rt((code), (a), (b), (c)))
	#define __builtin_add_overflow_p(a, b, c) __mcc_ovp_ice(0, (a), (b), (c))
	#define __builtin_sub_overflow_p(a, b, c) __mcc_ovp_ice(1, (a), (b), (c))
	#define __builtin_mul_overflow_p(a, b, c) __mcc_ovp_ice(2, (a), (b), (c))
	#define __builtin_assoc_barrier(x) (x)

	/* NO __atomic_thread_fence / __atomic_signal_fence here. Both are already
	 * defined by runtime/include/stdatomic.h, which is their right home, and
	 * defining them again in the predefs makes every TU that includes
	 * <stdatomic.h> warn "redefined" -- 154 exec cells, because the runner
	 * compares compiler stderr. They read as missing only if you probe them
	 * without the header. */

	/* clang's carry-chain builtins. Each returns the sum and writes the carry
	 * OUT through the last argument; the carry IN is a third operand, so the
	 * result is two dependent adds and the carry is either of them
	 * overflowing. Written against __builtin_add_overflow rather than a new
	 * primitive because the unsigned overflow test is exactly the carry. */
	#define __mcc_carry_gen(op, T, a, b, cin, cout) __extension__ ({	\
	T __mcc_c_a = (T)(a), __mcc_c_b = (T)(b), __mcc_c_i = (T)(cin);	\
	T __mcc_c_t, __mcc_c_r;						\
	unsigned char __mcc_c_1 =					\
	(unsigned char)__builtin_##op##_overflow(__mcc_c_a, __mcc_c_b, &__mcc_c_t); \
	unsigned char __mcc_c_2 =					\
	(unsigned char)__builtin_##op##_overflow(__mcc_c_t, __mcc_c_i, &__mcc_c_r); \
	*(cout) = (T)(__mcc_c_1 | __mcc_c_2);				\
	__mcc_c_r; })
	#define __builtin_addcb(a, b, cin, cout) \
	__mcc_carry_gen(add, unsigned char, a, b, cin, cout)
	#define __builtin_addcs(a, b, cin, cout) \
	__mcc_carry_gen(add, unsigned short, a, b, cin, cout)
	#define __builtin_addc(a, b, cin, cout) \
	__mcc_carry_gen(add, __mcc_uint_t, a, b, cin, cout)
	#define __builtin_addcl(a, b, cin, cout) \
	__mcc_carry_gen(add, __mcc_ulong_t, a, b, cin, cout)
	#define __builtin_addcll(a, b, cin, cout) \
	__mcc_carry_gen(add, __mcc_ullong_t, a, b, cin, cout)
	#define __builtin_subcb(a, b, cin, cout) \
	__mcc_carry_gen(sub, unsigned char, a, b, cin, cout)
	#define __builtin_subcs(a, b, cin, cout) \
	__mcc_carry_gen(sub, unsigned short, a, b, cin, cout)
	#define __builtin_subc(a, b, cin, cout) \
	__mcc_carry_gen(sub, __mcc_uint_t, a, b, cin, cout)
	#define __builtin_subcl(a, b, cin, cout) \
	__mcc_carry_gen(sub, __mcc_ulong_t, a, b, cin, cout)
	#define __builtin_subcll(a, b, cin, cout) \
	__mcc_carry_gen(sub, __mcc_ullong_t, a, b, cin, cout)

	/* Runtime CPU feature detection. gcc resolves the name at compile time
	 * against a libgcc-owned __cpu_model; mcc resolves it at run time in
	 * runtime/lib/builtin.c, which gives the same answer. The documented
	 * contract is "a positive integer if the run-time CPU supports the
	 * feature", not a specific value -- gcc returns the feature's bitmask, so
	 * neither is 1 and neither has to be. x86 only, which is where gcc
	 * provides them. */
	#if defined __x86_64__ || defined __i386__
	extern void __mcc_cpu_init(void);
	extern int __mcc_cpu_supports(const char *);
	#define __builtin_cpu_init() __mcc_cpu_init()
	#define __builtin_cpu_supports(name) __mcc_cpu_supports(name)
	#endif

	/* __builtin_setjmp / __builtin_longjmp.
	 *
	 * The obvious header-level route is closed and the note is kept because it
	 * is not obvious that it is: the predefs cannot declare _setjmp, because
	 * glibc's <setjmp.h> declares `int _setjmp(struct __jmp_buf_tag[1])` and
	 * any predef form -- `void *`, or even prototype-less -- is an
	 * incompatible redefinition the moment a TU includes that header, which
	 * src/mcc.c does. So the save and restore live in runtime/lib/builtin.c
	 * under names that cannot clash, and this is only the plumbing.
	 *
	 * The slot indirection is what makes gcc's `void *buf[5]` big enough: it
	 * holds one pointer to a full callee-saved-set slot, which buys the
	 * stronger C setjmp contract rather than gcc's restricted three-word one. */
	#if !defined _WIN32 && (defined __x86_64__ || defined __i386__ \
			|| defined __aarch64__ || defined __arm__ || defined __riscv)
	extern int __mcc_setjmp(void *);
	extern void __mcc_longjmp(void *, int);
	extern void *__mcc_sj_slot(void **);
	#define __builtin_setjmp(buf) __mcc_setjmp(__mcc_sj_slot((void **)(buf)))
	#define __builtin_longjmp(buf, val) \
	__mcc_longjmp(__mcc_sj_slot((void **)(buf)), (val))
	#endif

	/* The DWARF register numbers an unwinder writes the exception object and
	 * the selector into before __builtin_eh_return. Pure ABI constants, so
	 * they belong in a header rather than the backend, and the value has to
	 * survive constant folding because an unwinder uses it in a case label.
	 * Verified against gcc-15 and clang on x86_64 (0, 1); the others are their
	 * psABI DWARF numbers for the first two argument registers. */
	#if defined __x86_64__
	#define __builtin_eh_return_data_regno(n) ((n) == 0 ? 0 : 1)
	#elif defined __i386__
	#define __builtin_eh_return_data_regno(n) ((n) == 0 ? 0 : 2)
	#elif defined __aarch64__ || defined __arm__
	#define __builtin_eh_return_data_regno(n) ((n) == 0 ? 0 : 1)
	#elif defined __riscv
	#define __builtin_eh_return_data_regno(n) ((n) == 0 ? 10 : 11)
	#endif

	/* The _chk fortify wrappers. mcc does not implement _FORTIFY_SOURCE, so
	 * the checked forms are the unchecked ones with the object-size flag
	 * discarded -- which is what every one of them degrades to when the size
	 * is not known at compile time anyway. */
	#define __builtin___fprintf_chk(f, flag, ...) fprintf((f), __VA_ARGS__)
	#define __builtin___printf_chk(flag, ...) printf(__VA_ARGS__)
	#define __builtin___vfprintf_chk(f, flag, fmt, ap) vfprintf((f), (fmt), (ap))
	#define __builtin___vprintf_chk(flag, fmt, ap) vprintf((fmt), (ap))
	#define __builtin_vprintf(fmt, ap) vprintf((fmt), (ap))
	#define __builtin_vfprintf(f, fmt, ap) vfprintf((f), (fmt), (ap))

	#define __mcc_ovfw(op, T, a, b, r) __builtin_##op##_overflow((T)(a), (T)(b), (r))
	#define __builtin_sadd_overflow(a, b, r) __mcc_ovfw(add, __mcc_int_t, a, b, r)
	#define __builtin_saddl_overflow(a, b, r) __mcc_ovfw(add, __mcc_long_t, a, b, r)
	#define __builtin_saddll_overflow(a, b, r) __mcc_ovfw(add, __mcc_llong_t, a, b, r)
	#define __builtin_uadd_overflow(a, b, r) __mcc_ovfw(add, __mcc_uint_t, a, b, r)
	#define __builtin_uaddl_overflow(a, b, r) __mcc_ovfw(add, __mcc_ulong_t, a, b, r)
	#define __builtin_uaddll_overflow(a, b, r) __mcc_ovfw(add, __mcc_ullong_t, a, b, r)
	#define __builtin_ssub_overflow(a, b, r) __mcc_ovfw(sub, __mcc_int_t, a, b, r)
	#define __builtin_ssubl_overflow(a, b, r) __mcc_ovfw(sub, __mcc_long_t, a, b, r)
	#define __builtin_ssubll_overflow(a, b, r) __mcc_ovfw(sub, __mcc_llong_t, a, b, r)
	#define __builtin_usub_overflow(a, b, r) __mcc_ovfw(sub, __mcc_uint_t, a, b, r)
	#define __builtin_usubl_overflow(a, b, r) __mcc_ovfw(sub, __mcc_ulong_t, a, b, r)
	#define __builtin_usubll_overflow(a, b, r) __mcc_ovfw(sub, __mcc_ullong_t, a, b, r)
	#define __builtin_smul_overflow(a, b, r) __mcc_ovfw(mul, __mcc_int_t, a, b, r)
	#define __builtin_smull_overflow(a, b, r) __mcc_ovfw(mul, __mcc_long_t, a, b, r)
	#define __builtin_smulll_overflow(a, b, r) __mcc_ovfw(mul, __mcc_llong_t, a, b, r)
	#define __builtin_umul_overflow(a, b, r) __mcc_ovfw(mul, __mcc_uint_t, a, b, r)
	#define __builtin_umull_overflow(a, b, r) __mcc_ovfw(mul, __mcc_ulong_t, a, b, r)
	#define __builtin_umulll_overflow(a, b, r) __mcc_ovfw(mul, __mcc_ullong_t, a, b, r)

	#if defined __x86_64__ || defined __i386__
	#define __builtin___clear_cache(b, e) ((void)(b), (void)(e))
	#elif defined __aarch64__
	#define __builtin___clear_cache(b, e) __arm64_clear_cache((b), (e))
	#elif defined __riscv
	#define __builtin___clear_cache(b, e) __riscv64_clear_cache((b), (e))
	#endif

	#define __mcc_awa_bytes(align) ((__mcc_size_t)(align) / 8)
	#define __builtin_alloca_with_align(size, align) \
	((void *)(((__mcc_size_t)__builtin_alloca((__mcc_size_t)(size) \
	+ __mcc_awa_bytes(align) - 1) + __mcc_awa_bytes(align) - 1) \
	& ~(__mcc_size_t)(__mcc_awa_bytes(align) - 1)))
	#define __builtin_alloca_with_align_and_max(size, align, max) \
	__builtin_alloca_with_align((size), (align))

	#ifdef __SIZEOF_INT128__
	typedef __mcc_uint128_t __mcc_gu_t;
	typedef __mcc_int128_t __mcc_gs_t;
	#define __MCC_GBITS 128
	#else
	typedef __mcc_ullong_t __mcc_gu_t;
	typedef __mcc_llong_t __mcc_gs_t;
	#define __MCC_GBITS 64
	#endif

	static __inline int __mcc_gclz(__mcc_gu_t __w) {
	#if __MCC_GBITS > 64
	__mcc_ullong_t __h = (__mcc_ullong_t)(__w >> 64);
	__mcc_ullong_t __l = (__mcc_ullong_t)__w;
	return __h ? __builtin_clzll(__h) : __l ? 64 + __builtin_clzll(__l) : 128;
	#else
	return __w ? __builtin_clzll(__w) : 64;
	#endif
	}
	static __inline int __mcc_gctz(__mcc_gu_t __w) {
	#if __MCC_GBITS > 64
	__mcc_ullong_t __h = (__mcc_ullong_t)(__w >> 64);
	__mcc_ullong_t __l = (__mcc_ullong_t)__w;
	return __l ? __builtin_ctzll(__l) : __h ? 64 + __builtin_ctzll(__h) : 128;
	#else
	return __w ? __builtin_ctzll(__w) : 64;
	#endif
	}
	static __inline int __mcc_gpop(__mcc_gu_t __w) {
	#if __MCC_GBITS > 64
	return __builtin_popcountll((__mcc_ullong_t)(__w >> 64))
	+ __builtin_popcountll((__mcc_ullong_t)__w);
	#else
	return __builtin_popcountll(__w);
	#endif
	}
	static __inline int __mcc_gclzp(__mcc_gu_t __w, int __p) {
	int __r = __mcc_gclz(__w) - (__MCC_GBITS - __p);
	return __r > __p ? __p : __r;
	}
	static __inline int __mcc_gctzp(__mcc_gu_t __w, int __p) {
	int __r = __mcc_gctz(__w);
	return __r > __p ? __p : __r;
	}
	static __inline int __mcc_gclzf(__mcc_gu_t __w, int __p, int __f) {
	int __r = __mcc_gclzp(__w, __p);
	return __r == __p ? __f : __r;
	}
	static __inline int __mcc_gctzf(__mcc_gu_t __w, int __p, int __f) {
	int __r = __mcc_gctzp(__w, __p);
	return __r == __p ? __f : __r;
	}
	static __inline int __mcc_gclrsb(__mcc_gs_t __v, int __p) {
	__mcc_gu_t __w = (__mcc_gu_t)(__v < 0 ? ~__v : __v);
	return __mcc_gclzp(__w, __p) - 1;
	}
	static __inline int __mcc_gffs(__mcc_gs_t __v, int __p) {
	int __r = __mcc_gctzp((__mcc_gu_t)__v, __p);
	return __r == __p ? 0 : __r + 1;
	}
	static __inline unsigned int __mcc_gflo(__mcc_gu_t __w, int __p) {
	int __r = __mcc_gclzp(__w, __p);
	return __r == __p ? 0u : (unsigned int)__r + 1u;
	}
	static __inline unsigned int __mcc_gfto(__mcc_gu_t __w, int __p) {
	int __r = __mcc_gctzp(__w, __p);
	return __r == __p ? 0u : (unsigned int)__r + 1u;
	}
	static __inline __mcc_gu_t __mcc_gbswap(__mcc_gu_t __w, int __n) {
	__mcc_gu_t __r = 0;
	int __i;
	for (__i = 0; __i < __n; __i++) {
	__r = (__r << 8) | (__w & 0xff);
	__w >>= 8;
	}
	return __r;
	}
	static __inline __mcc_gu_t __mcc_gbitrev(__mcc_gu_t __w, int __n) {
	__mcc_gu_t __r = 0;
	int __i;
	for (__i = 0; __i < __n; __i++) {
	__r = (__r << 1) | (__w & 1);
	__w >>= 1;
	}
	return __r;
	}
	static __inline __mcc_gu_t __mcc_gbitfloor(__mcc_gu_t __w, int __p) {
	int __r = __mcc_gclzp(__w, __p);
	return __r == __p ? (__mcc_gu_t)0 : (__mcc_gu_t)1 << (__p - 1 - __r);
	}
	static __inline __mcc_gu_t __mcc_gbitceil(__mcc_gu_t __w, int __p) {
	int __r;
	if (__w <= 1)
	return (__mcc_gu_t)1;
	__r = __mcc_gclzp((__mcc_gu_t)(__w - 1), __p);
	return (__mcc_gu_t)1 << (__p - __r);
	}
	static __inline __mcc_gu_t __mcc_grot(__mcc_gu_t __w, int __p, __mcc_llong_t __n,
	int __left) {
	int __k = (int)((__n % __p + __p) % __p);
	if (!__left)
	__k = (__p - __k) % __p;
	if (!__k)
	return __w;
	return (__mcc_gu_t)((__w << __k) | (__w >> (__p - __k)));
	}

	#define __mcc_gprec(x) (__builtin_bitprecisionof(x))
	#define __mcc_gsel(_1, _2, __mcc_gn, ...) __mcc_gn
	#define __mcc_gwide(x) (__mcc_gprec(x) > __MCC_GBITS)
	#define __mcc_wpop(x) (__extension__ ({ \
	_Pragma("GCC diagnostic push") \
	_Pragma("GCC diagnostic ignored \"-Wshift-count-overflow\"") \
	_Pragma("GCC diagnostic ignored \"-Wshift-count-negative\"") \
	__typeof__(x) __wt = (x); int __wr = 0; \
	while (__wt) { __wr += __mcc_gpop((__mcc_gu_t)__wt); \
	__wt = (__typeof__(x))((__wt) >> __MCC_GBITS); } \
	_Pragma("GCC diagnostic pop") __wr; }))
	#define __mcc_wclz(x) (__extension__ ({ \
	_Pragma("GCC diagnostic push") \
	_Pragma("GCC diagnostic ignored \"-Wshift-count-overflow\"") \
	_Pragma("GCC diagnostic ignored \"-Wshift-count-negative\"") \
	__typeof__(x) __wt = (x); \
	int __wp = __mcc_gprec(x), __wr = __wp, __wc = 0; \
	while (__wt) { __mcc_gu_t __wl = (__mcc_gu_t)__wt; \
	if (__wl) __wr = __wp - ((__wc + 1) * __MCC_GBITS - __mcc_gclz(__wl)); \
	__wt = (__typeof__(x))((__wt) >> __MCC_GBITS); __wc++; } \
	_Pragma("GCC diagnostic pop") __wr; }))
	#define __mcc_wctz(x) (__extension__ ({ \
	_Pragma("GCC diagnostic push") \
	_Pragma("GCC diagnostic ignored \"-Wshift-count-overflow\"") \
	_Pragma("GCC diagnostic ignored \"-Wshift-count-negative\"") \
	__typeof__(x) __wt = (x); \
	int __wr = __mcc_gprec(x), __wc = 0; \
	while (__wt) { __mcc_gu_t __wl = (__mcc_gu_t)__wt; \
	if (__wl) { __wr = __wc * __MCC_GBITS + __mcc_gctz(__wl); break; } \
	__wt = (__typeof__(x))((__wt) >> __MCC_GBITS); __wc++; } \
	_Pragma("GCC diagnostic pop") __wr; }))
	#define __mcc_clzg1(x) (__mcc_gwide(x) ? __mcc_wclz(x) \
	: __mcc_gclzp((__mcc_gu_t)(x), __mcc_gprec(x)))
	#define __mcc_clzg2(x, f) __mcc_gclzf((__mcc_gu_t)(x), __mcc_gprec(x), (int)(f))
	#define __builtin_clzg(...) \
	__mcc_gsel(__VA_ARGS__, __mcc_clzg2, __mcc_clzg1, 0)(__VA_ARGS__)
	#define __mcc_ctzg1(x) (__mcc_gwide(x) ? __mcc_wctz(x) \
	: __mcc_gctzp((__mcc_gu_t)(x), __mcc_gprec(x)))
	#define __mcc_ctzg2(x, f) __mcc_gctzf((__mcc_gu_t)(x), __mcc_gprec(x), (int)(f))
	#define __builtin_ctzg(...) \
	__mcc_gsel(__VA_ARGS__, __mcc_ctzg2, __mcc_ctzg1, 0)(__VA_ARGS__)
	#define __builtin_clrsbg(x) __mcc_gclrsb((__mcc_gs_t)(x), __mcc_gprec(x))
	#define __builtin_ffsg(x) __mcc_gffs((__mcc_gs_t)(x), __mcc_gprec(x))
	#define __builtin_popcountg(x) \
	(__mcc_gwide(x) ? __mcc_wpop(x) : __mcc_gpop((__mcc_gu_t)(x)))
	#define __builtin_parityg(x) (__builtin_popcountg(x) & 1)
	#define __mcc_wbitfloor(x) (__extension__ ({ \
	unsigned int __mcc_bwf = (unsigned int)__builtin_stdc_bit_width(x); \
	__mcc_bwf == 0u ? (__typeof__(x))0 \
	: ((__typeof__(x))1 << (__mcc_bwf - 1u)); }))
	#define __mcc_wbitceil(x) (__extension__ ({ \
	unsigned int __mcc_bwc = (unsigned int)__builtin_stdc_bit_width(x); \
	unsigned int __mcc_shc = (unsigned int)__builtin_stdc_bit_width((x) - 1); \
	__mcc_bwc <= 1u ? (__typeof__(x))1 : ((__typeof__(x))1 << __mcc_shc); }))
	#define __mcc_wbswap(x) (__extension__ ({ \
	int __mcc_sn = (int)sizeof(x), __mcc_si; \
	__typeof__(x) __mcc_sv = (x), __mcc_sr = (__typeof__(x))0; \
	for (__mcc_si = 0; __mcc_si < __mcc_sn; __mcc_si++) \
		__mcc_sr = (__typeof__(x))(((__mcc_sr) << 8) | (((__mcc_sv) >> (__mcc_si * 8)) & 0xff)); \
	__mcc_sr; }))
	#define __builtin_bswapg(x) (__mcc_gwide(x) \
	? __mcc_wbswap(x) \
	: ((__typeof__(x))__mcc_gbswap((__mcc_gu_t)(x), (int)sizeof(x))))

	#define __builtin_bitreverse8(x) \
	((__mcc_uchar_t)__mcc_gbitrev((__mcc_gu_t)(__mcc_uchar_t)(x), 8))
	#define __builtin_bitreverse16(x) \
	((__mcc_ushort_t)__mcc_gbitrev((__mcc_gu_t)(__mcc_ushort_t)(x), 16))
	#define __builtin_bitreverse32(x) \
	((__mcc_uint_t)__mcc_gbitrev((__mcc_gu_t)(__mcc_uint_t)(x), 32))
	#define __builtin_bitreverse64(x) \
	((__mcc_ullong_t)__mcc_gbitrev((__mcc_gu_t)(__mcc_ullong_t)(x), 64))
	#ifdef __SIZEOF_INT128__
	#define __builtin_bitreverse128(x) \
	((__mcc_uint128_t)__mcc_gbitrev((__mcc_gu_t)(__mcc_uint128_t)(x), 128))
	#endif
	#define __mcc_wbitrev(x) (__extension__ ({ \
	int __mcc_vp = (int)__mcc_gprec(x), __mcc_vi; \
	__typeof__(x) __mcc_vv = (x), __mcc_vr = (__typeof__(x))0; \
	for (__mcc_vi = 0; __mcc_vi < __mcc_vp; __mcc_vi++) \
		__mcc_vr = (__typeof__(x))(((__mcc_vr) << 1) | (((__mcc_vv) >> __mcc_vi) & 1)); \
	__mcc_vr; }))
	#define __builtin_bitreverseg(x) (__mcc_gwide(x) \
	? __mcc_wbitrev(x) \
	: ((__typeof__(x))__mcc_gbitrev((__mcc_gu_t)(x), __mcc_gprec(x))))

	#define __mcc_gnot(x) ((__mcc_gu_t)(__typeof__(x))~(x))
	#define __builtin_stdc_leading_zeros(x) \
	((unsigned int)__mcc_clzg1(x))
	#define __builtin_stdc_leading_ones(x) \
	((unsigned int)__mcc_clzg1((__typeof__(x))~(x)))
	#define __builtin_stdc_trailing_zeros(x) \
	((unsigned int)__mcc_ctzg1(x))
	#define __builtin_stdc_trailing_ones(x) \
	((unsigned int)__mcc_ctzg1((__typeof__(x))~(x)))
	#define __builtin_stdc_first_leading_one(x) (__extension__ ({ \
	int __mcc_fr = __mcc_clzg1(x); \
	__mcc_fr == __mcc_gprec(x) ? 0u : (unsigned int)__mcc_fr + 1u; }))
	#define __builtin_stdc_first_leading_zero(x) (__extension__ ({ \
	unsigned int __mcc_fl = __builtin_stdc_leading_ones(x); \
	__mcc_fl == (unsigned int)__mcc_gprec(x) ? 0u : __mcc_fl + 1u; }))
	#define __builtin_stdc_first_trailing_one(x) (__extension__ ({ \
	int __mcc_fr = __mcc_ctzg1(x); \
	__mcc_fr == __mcc_gprec(x) ? 0u : (unsigned int)__mcc_fr + 1u; }))
	#define __builtin_stdc_first_trailing_zero(x) (__extension__ ({ \
	unsigned int __mcc_ft = __builtin_stdc_trailing_ones(x); \
	__mcc_ft == (unsigned int)__mcc_gprec(x) ? 0u : __mcc_ft + 1u; }))
	#define __builtin_stdc_count_ones(x) ((unsigned int)__builtin_popcountg(x))
	#define __builtin_stdc_count_zeros(x) \
	((unsigned int)(__mcc_gprec(x) - __builtin_popcountg(x)))
	#define __builtin_stdc_has_single_bit(x) ((_Bool)(__builtin_popcountg(x) == 1))
	#define __builtin_stdc_bit_width(x) \
	((unsigned int)(__mcc_gprec(x) - __mcc_clzg1(x)))
	#define __builtin_stdc_bit_floor(x) (__mcc_gwide(x) \
	? __mcc_wbitfloor(x) \
	: ((__typeof__(x))__mcc_gbitfloor((__mcc_gu_t)(x), __mcc_gprec(x))))
	#define __builtin_stdc_bit_ceil(x) (__mcc_gwide(x) \
	? __mcc_wbitceil(x) \
	: ((__typeof__(x))__mcc_gbitceil((__mcc_gu_t)(x), __mcc_gprec(x))))
	#define __mcc_wrotl(x, n) (__extension__ ({ \
	int __mcc_rp = (int)__mcc_gprec(x); \
	int __mcc_rn = (int)(((__mcc_llong_t)(n) % __mcc_rp + __mcc_rp) % __mcc_rp); \
	__mcc_rn == 0 ? (x) \
	: (__typeof__(x))(((x) << __mcc_rn) | ((x) >> (__mcc_rp - __mcc_rn))); }))
	#define __mcc_wrotr(x, n) (__extension__ ({ \
	int __mcc_rp = (int)__mcc_gprec(x); \
	int __mcc_rn = (int)(((__mcc_llong_t)(n) % __mcc_rp + __mcc_rp) % __mcc_rp); \
	__mcc_rn == 0 ? (x) \
	: (__typeof__(x))(((x) >> __mcc_rn) | ((x) << (__mcc_rp - __mcc_rn))); }))
	#define __builtin_stdc_rotate_left(x, n) (__mcc_gwide(x) \
	? __mcc_wrotl(x, n) \
	: ((__typeof__(x))__mcc_grot((__mcc_gu_t)(x), __mcc_gprec(x), (__mcc_llong_t)(n), 1)))
	#define __builtin_stdc_rotate_right(x, n) (__mcc_gwide(x) \
	? __mcc_wrotr(x, n) \
	: ((__typeof__(x))__mcc_grot((__mcc_gu_t)(x), __mcc_gprec(x), (__mcc_llong_t)(n), 0)))

	#define __builtin_isinf_sign(x) __extension__ ({ __typeof__(x) __mcc_iv = (x); \
	__builtin_isinf(__mcc_iv) ? (__builtin_signbit(__mcc_iv) ? -1 : 1) : 0; })

	#if defined __x86_64__ || defined __i386__
	#define __builtin_ia32_ptestz128(a, b) __extension__ ({ \
	__typeof__(a) __mcc_pa = (a), __mcc_pb = (b); \
	(int)(((__mcc_pa[0] & __mcc_pb[0]) | (__mcc_pa[1] & __mcc_pb[1])) == 0); })
	#define __builtin_ia32_ptestc128(a, b) __extension__ ({ \
	__typeof__(a) __mcc_pa = (a), __mcc_pb = (b); \
	(int)(((~__mcc_pa[0] & __mcc_pb[0]) | (~__mcc_pa[1] & __mcc_pb[1])) == 0); })
	#define __builtin_ia32_ptestnzc128(a, b) \
	(!__builtin_ia32_ptestz128((a), (b)) && !__builtin_ia32_ptestc128((a), (b)))
	#define __builtin_ia32_ptestz256(a, b) __extension__ ({ \
	__typeof__(a) __mcc_pa = (a), __mcc_pb = (b); \
	(int)(((__mcc_pa[0] & __mcc_pb[0]) | (__mcc_pa[1] & __mcc_pb[1]) \
	| (__mcc_pa[2] & __mcc_pb[2]) | (__mcc_pa[3] & __mcc_pb[3])) == 0); })
	#define __builtin_ia32_ptestc256(a, b) __extension__ ({ \
	__typeof__(a) __mcc_pa = (a), __mcc_pb = (b); \
	(int)(((~__mcc_pa[0] & __mcc_pb[0]) | (~__mcc_pa[1] & __mcc_pb[1]) \
	| (~__mcc_pa[2] & __mcc_pb[2]) | (~__mcc_pa[3] & __mcc_pb[3])) == 0); })
	#define __builtin_ia32_ptestnzc256(a, b) \
	(!__builtin_ia32_ptestz256((a), (b)) && !__builtin_ia32_ptestc256((a), (b)))

	#define __builtin_ia32_vec_ext_v4si(a, i) __extension__ ({ \
	__typeof__(a) __mcc_ve = (a); (int)__mcc_ve[(i) & 3]; })
	#define __builtin_ia32_vec_ext_v8hi(a, i) __extension__ ({ \
	__typeof__(a) __mcc_ve = (a); (int)__mcc_ve[(i) & 7]; })
	#define __builtin_ia32_vec_ext_v16qi(a, i) __extension__ ({ \
	__typeof__(a) __mcc_ve = (a); (int)__mcc_ve[(i) & 15]; })
	#define __builtin_ia32_vec_ext_v2di(a, i) __extension__ ({ \
	__typeof__(a) __mcc_ve = (a); (__mcc_llong_t)__mcc_ve[(i) & 1]; })
	#define __builtin_ia32_vec_ext_v4sf(a, i) __extension__ ({ \
	__typeof__(a) __mcc_ve = (a); (float)__mcc_ve[(i) & 3]; })
	#define __builtin_ia32_vec_ext_v2df(a, i) __extension__ ({ \
	__typeof__(a) __mcc_ve = (a); (double)__mcc_ve[(i) & 1]; })

	#define __builtin_ia32_vec_set_v16qi(a, d, i) __extension__ ({ \
	__typeof__(a) __mcc_vs = (a); __mcc_vs[(i) & 15] = (char)(d); __mcc_vs; })
	#define __builtin_ia32_vec_set_v8hi(a, d, i) __extension__ ({ \
	__typeof__(a) __mcc_vs = (a); __mcc_vs[(i) & 7] = (short)(d); __mcc_vs; })
	#define __builtin_ia32_vec_set_v4si(a, d, i) __extension__ ({ \
	__typeof__(a) __mcc_vs = (a); __mcc_vs[(i) & 3] = (int)(d); __mcc_vs; })
	#define __builtin_ia32_vec_set_v2di(a, d, i) __extension__ ({ \
	__typeof__(a) __mcc_vs = (a); __mcc_vs[(i) & 1] = (__mcc_llong_t)(d); __mcc_vs; })

	#define __builtin_ia32_pshufd(a, n) __extension__ ({ \
	__typeof__(a) __mcc_ps = (a), __mcc_pr; int __mcc_pn = (int)(n); \
	__mcc_pr[0] = __mcc_ps[__mcc_pn & 3]; \
	__mcc_pr[1] = __mcc_ps[(__mcc_pn >> 2) & 3]; \
	__mcc_pr[2] = __mcc_ps[(__mcc_pn >> 4) & 3]; \
	__mcc_pr[3] = __mcc_ps[(__mcc_pn >> 6) & 3]; __mcc_pr; })

	#define __builtin_ia32_pmulhrsw128(a, b) __extension__ ({ \
	__typeof__(a) __mcc_ma = (a), __mcc_mb = (b), __mcc_mr; int __mcc_mi; \
	for (__mcc_mi = 0; __mcc_mi < 8; __mcc_mi++) \
	__mcc_mr[__mcc_mi] = (short)((((((int)__mcc_ma[__mcc_mi] \
	* (int)__mcc_mb[__mcc_mi]) >> 14) + 1) >> 1)); __mcc_mr; })

	#define __builtin_ia32_cvtsd2ss(a, b) __extension__ ({ \
	__typeof__(a) __mcc_ca = (a); __typeof__(b) __mcc_cb = (b); \
	__mcc_ca[0] = (float)__mcc_cb[0]; __mcc_ca; })

	#define __builtin_ia32_rdtsc() __extension__ ({ \
	unsigned int __mcc_tl, __mcc_th; \
	__asm__ __volatile__("rdtsc" : "=a"(__mcc_tl), "=d"(__mcc_th)); \
	((__mcc_ullong_t)__mcc_th << 32) | __mcc_tl; })

	#define __builtin_ia32_monitor(p, e, h) __extension__ ({ \
	__asm__ __volatile__(".byte 0x0f, 0x01, 0xc8" \
	: : "a"(p), "c"((unsigned int)(e)), "d"((unsigned int)(h))); })
	#define __builtin_ia32_mwait(e, h) __extension__ ({ \
	__asm__ __volatile__(".byte 0x0f, 0x01, 0xc9" \
	: : "a"((unsigned int)(e)), "c"((unsigned int)(h))); })

	#define __builtin_ia32_addcarryx_u32(c, x, y, p) __extension__ ({ \
	__mcc_ullong_t __mcc_as = (__mcc_ullong_t)(unsigned int)(x) \
	+ (__mcc_ullong_t)(unsigned int)(y) + (__mcc_ullong_t)((c) != 0); \
	*(p) = (unsigned int)__mcc_as; (unsigned char)(__mcc_as >> 32); })
	#define __builtin_ia32_sbb_u32(c, x, y, p) __extension__ ({ \
	__mcc_ullong_t __mcc_ds = (__mcc_ullong_t)(unsigned int)(x) \
	- (__mcc_ullong_t)(unsigned int)(y) - (__mcc_ullong_t)((c) != 0); \
	*(p) = (unsigned int)__mcc_ds; (unsigned char)((__mcc_ds >> 32) & 1); })
	#ifdef __x86_64__
	#define __builtin_ia32_addcarryx_u64(c, x, y, p) __extension__ ({ \
	__mcc_ullong_t __mcc_ax = (__mcc_ullong_t)(x), __mcc_ay = (__mcc_ullong_t)(y); \
	__mcc_ullong_t __mcc_ac = (__mcc_ullong_t)((c) != 0), __mcc_as = __mcc_ax + __mcc_ay; \
	unsigned char __mcc_ao = (unsigned char)(__mcc_as < __mcc_ax); \
	__mcc_as += __mcc_ac; __mcc_ao |= (unsigned char)(__mcc_as < __mcc_ac); \
	*(p) = __mcc_as; __mcc_ao; })
	#define __builtin_ia32_sbb_u64(c, x, y, p) __extension__ ({ \
	__mcc_ullong_t __mcc_dx = (__mcc_ullong_t)(x), __mcc_dy = (__mcc_ullong_t)(y); \
	__mcc_ullong_t __mcc_dc = (__mcc_ullong_t)((c) != 0), __mcc_dd = __mcc_dx - __mcc_dy; \
	unsigned char __mcc_do = (unsigned char)(__mcc_dx < __mcc_dy); \
	__mcc_do |= (unsigned char)(__mcc_dd < __mcc_dc); \
	*(p) = __mcc_dd - __mcc_dc; __mcc_do; })
	#endif

	#define __builtin_ia32_bextr_u32(x, y) __extension__ ({ \
	unsigned int __mcc_bx = (unsigned int)(x), __mcc_by = (unsigned int)(y); \
	unsigned int __mcc_bs = __mcc_by & 0xffu, __mcc_bl = (__mcc_by >> 8) & 0xffu; \
	__mcc_bs > 31u ? 0u : (__mcc_bl > 31u ? (__mcc_bx >> __mcc_bs) \
	: ((__mcc_bx >> __mcc_bs) & ((1u << __mcc_bl) - 1u))); })
	#define __builtin_ia32_bzhi_si(x, y) __extension__ ({ \
	unsigned int __mcc_zx = (unsigned int)(x), __mcc_zn = (unsigned int)(y) & 0xffu; \
	__mcc_zn > 31u ? __mcc_zx : (__mcc_zx & ((1u << __mcc_zn) - 1u)); })
	#ifdef __x86_64__
	#define __builtin_ia32_bextr_u64(x, y) __extension__ ({ \
	__mcc_ullong_t __mcc_bx = (__mcc_ullong_t)(x), __mcc_by = (__mcc_ullong_t)(y); \
	__mcc_ullong_t __mcc_bs = __mcc_by & 0xffu, __mcc_bl = (__mcc_by >> 8) & 0xffu; \
	__mcc_bs > 63u ? 0ull : (__mcc_bl > 63u ? (__mcc_bx >> __mcc_bs) \
	: ((__mcc_bx >> __mcc_bs) & ((1ull << __mcc_bl) - 1ull))); })
	#define __builtin_ia32_bzhi_di(x, y) __extension__ ({ \
	__mcc_ullong_t __mcc_zx = (__mcc_ullong_t)(x); \
	__mcc_ullong_t __mcc_zn = (__mcc_ullong_t)(y) & 0xffu; \
	__mcc_zn > 63u ? __mcc_zx : (__mcc_zx & ((1ull << __mcc_zn) - 1ull)); })
	#endif

	#define __builtin_ia32_tzcnt_u16(x) __extension__ ({ \
	unsigned short __mcc_tv = (unsigned short)(x); \
	(unsigned short)(__mcc_tv ? __builtin_ctz((unsigned int)__mcc_tv) : 16); })
	#define __builtin_ia32_lzcnt_u16(x) __extension__ ({ \
	unsigned short __mcc_lv = (unsigned short)(x); \
	(unsigned short)(__mcc_lv ? __builtin_clz((unsigned int)__mcc_lv) - 16 : 16); })
	#define __builtin_ia32_tzcnt_u32(x) __extension__ ({ \
	unsigned int __mcc_tv = (unsigned int)(x); \
	(unsigned int)(__mcc_tv ? __builtin_ctz(__mcc_tv) : 32u); })
	#define __builtin_ia32_lzcnt_u32(x) __extension__ ({ \
	unsigned int __mcc_lv = (unsigned int)(x); \
	(unsigned int)(__mcc_lv ? __builtin_clz(__mcc_lv) : 32u); })
	#ifdef __x86_64__
	#define __builtin_ia32_tzcnt_u64(x) __extension__ ({ \
	__mcc_ullong_t __mcc_tv = (__mcc_ullong_t)(x); \
	(__mcc_ullong_t)(__mcc_tv ? __builtin_ctzll(__mcc_tv) : 64); })
	#define __builtin_ia32_lzcnt_u64(x) __extension__ ({ \
	__mcc_ullong_t __mcc_lv = (__mcc_ullong_t)(x); \
	(__mcc_ullong_t)(__mcc_lv ? __builtin_clzll(__mcc_lv) : 64); })
	#endif
	#endif

	#endif
