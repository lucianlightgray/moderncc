#if __SIZEOF_POINTER__ == 4 //@
#if defined  __OpenBSD__ //@
	#define __SIZE_TYPE__ unsigned long
	#define __PTRDIFF_TYPE__ long
#else //@
	#define __SIZE_TYPE__ unsigned int
	#define __PTRDIFF_TYPE__ int
#endif //@
	#define __ILP32__ 1
	#define __INT64_TYPE__ long long
#elif __SIZEOF_LONG__ == 4 //@
	#define __SIZE_TYPE__ unsigned long long
	#define __PTRDIFF_TYPE__ long long
	#define __LLP64__ 1
	#define __INT64_TYPE__ long long
#else //@
	#define __SIZE_TYPE__ unsigned long
	#define __PTRDIFF_TYPE__ long
	#define __LP64__ 1
# if defined __linux__ //@
	#define __INT64_TYPE__ long
# else //@
	#define __INT64_TYPE__ long long
# endif //@
#endif //@
	#define __SIZEOF_INT__ 4
	#define __INT_MAX__ 0x7fffffff
#if __SIZEOF_LONG__ == 4 //@
	#define __LONG_MAX__ 0x7fffffffL
#else //@
	#define __LONG_MAX__ 0x7fffffffffffffffL
#endif //@
	#define __SIZEOF_LONG_LONG__ 8
	#define __LONG_LONG_MAX__ 0x7fffffffffffffffLL
	#define __CHAR_BIT__ 8
	#define __ORDER_LITTLE_ENDIAN__ 1234
	#define __ORDER_BIG_ENDIAN__ 4321
	#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#if defined _WIN32 //@
	#define __WCHAR_TYPE__ unsigned short
	#define __WINT_TYPE__ unsigned short
#elif defined __linux__ && (defined __arm__ || defined __aarch64__) //@
	#define __WCHAR_TYPE__ unsigned int
	#define __WINT_TYPE__ unsigned int
#elif defined __linux__ //@
	#define __WCHAR_TYPE__ int
	#define __WINT_TYPE__ unsigned int
#else //@
	#define __WCHAR_TYPE__ int
	#define __WINT_TYPE__ int
#endif //@

#if !defined _WIN32 //@
	#define __STDC_ISO_10646__ 201706L
#endif //@

	#define __STDC_IEC_559__ 1
	#define __STDC_IEC_559_COMPLEX__ 1

	#define __STDC_UTF_16__ 1
	#define __STDC_UTF_32__ 1

#if defined _WIN32 //@
	#define __declspec(x) __attribute__((x))
	#define __cdecl

#elif defined __FreeBSD__ //@
	#define __GNUC__ 9
	#define __GNUC_MINOR__ 3
	#define __GNUC_PATCHLEVEL__ 0
	#define __GNUC_STDC_INLINE__ 1
	#define __NO_TLS 1
	#define __RUNETYPE_INTERNAL 1
# if __SIZEOF_POINTER__ == 8 //@
	#define __SIZEOF_SIZE_T__ 8
	#define __SIZEOF_PTRDIFF_T__ 8
#else //@
	#define __SIZEOF_SIZE_T__ 4
	#define __SIZEOF_PTRDIFF_T__ 4
# endif //@

#elif defined __FreeBSD_kernel__ //@

#elif defined __NetBSD__ //@
	#define __GNUC__ 4
	#define __GNUC_MINOR__ 1
	#define __GNUC_PATCHLEVEL__ 0
	#define _Pragma(x)
	#define __ELF__ 1
#if defined __aarch64__ //@
	#define _LOCORE
#endif //@

#elif defined __OpenBSD__ //@
	#define __GNUC__ 4
	#define _ANSI_LIBRARY 1

#elif defined __APPLE__ //@
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
	#define _FORTIFY_SOURCE 0
	#define _Float16 short unsigned int

#elif defined __ANDROID__ //@
	#define  BIONIC_IOCTL_NO_SIGNEDNESS_OVERLOAD

#else //@

#endif //@

#ifndef __NetBSD__ //@
	#define __UINTPTR_TYPE__ unsigned __PTRDIFF_TYPE__
	#define __INTPTR_TYPE__ __PTRDIFF_TYPE__
#endif //@
	#define __INT32_TYPE__ int
	#define __CHAR16_TYPE__ unsigned short
	#define __CHAR32_TYPE__ unsigned int
	#define __ATOMIC_RELAXED 0
	#define __ATOMIC_CONSUME 1
	#define __ATOMIC_ACQUIRE 2
	#define __ATOMIC_RELEASE 3
	#define __ATOMIC_ACQ_REL 4
	#define __ATOMIC_SEQ_CST 5

#if !defined _WIN32 //@
	#define __REDIRECT(name, proto, alias) name proto __asm__ (#alias)
	#define __REDIRECT_NTH(name, proto, alias) name proto __asm__ (#alias) __THROW
	#define __REDIRECT_NTHNL(name, proto, alias) name proto __asm__ (#alias) __THROWNL
#endif //@

	#define  __PRETTY_FUNCTION__ __FUNCTION__
	#define __has_builtin(x) 0
	#define __has_feature(x) 0
	#define __has_attribute(x) 0
	#define _Nonnull
	#define _Nullable
	#define _Nullable_result
	#define _Null_unspecified

	#ifndef __MCC_PP__

	#define __builtin_offsetof(type, field) ((__SIZE_TYPE__)&((type*)0)->field)
	#define __builtin_extract_return_addr(x) x
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
	({ __typeof__(*(p)) __o=(o), __n=(n); \
	__atomic_compare_exchange((p),&__o,&__n,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST); })
	#define __sync_val_compare_and_swap(p,o,n) \
	({ __typeof__(*(p)) __o=(o), __n=(n); \
	__atomic_compare_exchange((p),&__o,&__n,0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST); __o; })
	#define __sync_lock_test_and_set(p,v) \
	({ __typeof__(*(p)) __v=(v), __o; \
	__atomic_exchange((p),&__v,&__o,__ATOMIC_SEQ_CST); __o; })
	#define __sync_lock_release(p) \
	({ __typeof__(*(p)) __z=0; __atomic_store((p),&__z,__ATOMIC_SEQ_CST); })
	#define __sync_synchronize() \
	({ volatile int __mcc_bar = 0; \
	(void)__atomic_fetch_add(&__mcc_bar, 0, __ATOMIC_SEQ_CST); (void)0; })
	#define __atomic_load_n(p, o) \
	({ __typeof__(*(p)) __r; __atomic_load((p), &__r, (o)); __r; })
	#define __atomic_store_n(p, v, o) \
	({ __typeof__(*(p)) __v = (v); __atomic_store((p), &__v, (o)); })
	#define __atomic_exchange_n(p, v, o) \
	({ __typeof__(*(p)) __v = (v), __r; \
	__atomic_exchange((p), &__v, &__r, (o)); __r; })
	#define __atomic_compare_exchange_n(p, e, d, w, s, f) \
	({ __typeof__(*(p)) __d = (d); \
	__atomic_compare_exchange((p), (e), &__d, (w), (s), (f)); })

#if !defined __linux__ && !defined _WIN32 //@
# if defined __APPLE__ //@
	#define __builtin_flt_rounds() 1
	#define __builtin_bzero(p, ignored_size) bzero(p, sizeof(*(p)))
# endif //@
#endif //@
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
	#define __builtin_isinf(x) ((x) == __builtin_inf() || (x) == -__builtin_inf())
	#define __builtin_isfinite(x) (!__builtin_isnan(x) && !__builtin_isinf(x))
	#define __builtin_isunordered(a, b) (__builtin_isnan(a) || __builtin_isnan(b))
	#define __builtin_isgreater(a, b) (!__builtin_isunordered(a, b) && (a) > (b))
	#define __builtin_isgreaterequal(a, b) (!__builtin_isunordered(a, b) && (a) >= (b))
	#define __builtin_isless(a, b) (!__builtin_isunordered(a, b) && (a) < (b))
	#define __builtin_islessequal(a, b) (!__builtin_isunordered(a, b) && (a) <= (b))
	#define __builtin_islessgreater(a, b) (!__builtin_isunordered(a, b) && ((a) < (b) || (a) > (b)))
	#define __builtin_fabsf(x) ((float)((x) < 0 ? -(x) : (x)))
	#define __builtin_fabs(x)  ((double)((x) < 0 ? -(x) : (x)))
	#define __builtin_fabsl(x) ((long double)((x) < 0 ? -(x) : (x)))
	#define __builtin_copysignf(x, y) (__builtin_signbit(y) ? -__builtin_fabsf(x) : __builtin_fabsf(x))
	#define __builtin_copysign(x, y)  (__builtin_signbit(y) ? -__builtin_fabs(x)  : __builtin_fabs(x))
	#define __builtin_copysignl(x, y) (__builtin_signbit(y) ? -__builtin_fabsl(x) : __builtin_fabsl(x))

	struct __uint128__ { char x[16]; } __attribute((__aligned__(16)));
	#define __int128_t struct __uint128__
	#define __uint128_t struct __uint128__

#if defined __x86_64__ //@
#if !defined _WIN32 //@
	typedef struct __va_list_tag {
	unsigned gp_offset, fp_offset;
	union {
	unsigned overflow_offset;
	char *overflow_arg_area;
	};
	char *reg_save_area;
	} __builtin_va_list[1];

	void *__va_arg(__builtin_va_list ap, int arg_type, int size, int align);
	#define __builtin_va_start(ap, last) \
	(*(ap) = *(struct __va_list_tag *)((char*)__builtin_frame_address(0) - 24))
	#define __builtin_va_arg(ap, t)   \
	(*(t *)(__va_arg(ap, __builtin_va_arg_types(t), sizeof(t), __alignof__(t))))
	#define __builtin_va_copy(dest, src) (*(dest) = *(src))

#else //@
	typedef char *__builtin_va_list;
	#define __builtin_va_arg(ap, t) ((sizeof(t) > 8 || (sizeof(t) & (sizeof(t) - 1))) \
	? **(t **)((ap += 8) - 8) : *(t  *)((ap += 8) - 8))
#endif //@

#elif defined __arm__ //@
	typedef char *__builtin_va_list;
	#define _mcc_alignof(type) ((int)&((struct {char c;type x;} *)0)->x)
	#define _mcc_align(addr,type) (((unsigned)addr + _mcc_alignof(type) - 1) \
	& ~(_mcc_alignof(type) - 1))
	#define __builtin_va_start(ap,last) (ap = ((char *)&(last)) + ((sizeof(last)+3)&~3))
	#define __builtin_va_arg(ap,type) (ap = (void *) ((_mcc_align(ap,type)+sizeof(type)+3) \
	&~3), *(type *)(ap - ((sizeof(type)+3)&~3)))

#elif defined __aarch64__ //@
#if defined _WIN32 //@
	typedef char *__builtin_va_list;
#elif defined __APPLE__ //@
	typedef struct {
	void *__stack;
	} __builtin_va_list;

#else //@
	typedef struct {
	void *__stack, *__gr_top, *__vr_top;
	int   __gr_offs, __vr_offs;
	} __builtin_va_list;

#endif //@
#elif defined __riscv //@
	typedef char *__builtin_va_list;
	#define __va_reg_size (__riscv_xlen >> 3)
	#define _mcc_align(addr,type) (((unsigned long)addr + __alignof__(type) - 1) \
	& -(__alignof__(type)))
	#define __builtin_va_arg(ap,type) (*(sizeof(type) > (2*__va_reg_size) ? *(type **)((ap += __va_reg_size) - __va_reg_size) : (ap = (va_list)(_mcc_align(ap,type) + (sizeof(type)+__va_reg_size - 1)& -__va_reg_size), (type *)(ap - ((sizeof(type)+ __va_reg_size - 1)& -__va_reg_size)))))

#else //@
	typedef char *__builtin_va_list;
	#define __builtin_va_start(ap,last) (ap = ((char *)&(last)) + ((sizeof(last)+3)&~3))
	#define __builtin_va_arg(ap,t) (*(t*)((ap+=(sizeof(t)+3)&~3)-((sizeof(t)+3)&~3)))

#endif //@
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
#if defined __ARM_EABI__ //@
	__BOUND(void*,__aeabi_memcpy,(void*,const void*,__SIZE_TYPE__))
	__BOUND(void*,__aeabi_memmove,(void*,const void*,__SIZE_TYPE__))
	__BOUND(void*,__aeabi_memmove4,(void*,const void*,__SIZE_TYPE__))
	__BOUND(void*,__aeabi_memmove8,(void*,const void*,__SIZE_TYPE__))
	__BOUND(void*,__aeabi_memset,(void*,int,__SIZE_TYPE__))
#endif //@

#if defined __linux__ || defined __APPLE__ //@
	#define __MAYBE_REDIR __BUILTIN
#else //@
	#define __MAYBE_REDIR __BOTH
#endif //@
	__MAYBE_REDIR(void*, malloc, (__SIZE_TYPE__))
	__MAYBE_REDIR(void*, realloc, (void *, __SIZE_TYPE__))
	__MAYBE_REDIR(void*, calloc, (__SIZE_TYPE__, __SIZE_TYPE__))
	__MAYBE_REDIR(void*, memalign, (__SIZE_TYPE__, __SIZE_TYPE__))
	__MAYBE_REDIR(void, free, (void*))
	__BOTH(void*, alloca, (__SIZE_TYPE__))
	void *alloca(__SIZE_TYPE__);
	__BUILTIN(void, abort, (void))
	__BOUND(void, longjmp, ())
#if !defined _WIN32 //@
	__BOUND(void*, mmap, ())
	__BOUND(int, munmap, ())
#endif //@
	#undef __BUILTINBC
	#undef __BUILTIN
	#undef __BOUND
	#undef __BOTH
	#undef __MAYBE_REDIR
	#undef __RENAME

	#define __MCC_OV_DECL(T, NM)			\
	int __mcc_addo_##NM(T, T, T*);		\
	int __mcc_subo_##NM(T, T, T*);		\
	int __mcc_mulo_##NM(T, T, T*);
	__MCC_OV_DECL(signed char, sc)
	__MCC_OV_DECL(char, c)
	__MCC_OV_DECL(short, s)
	__MCC_OV_DECL(int, i)
	__MCC_OV_DECL(unsigned char, uc)
	__MCC_OV_DECL(unsigned short, us)
	__MCC_OV_DECL(unsigned int, u)
	__MCC_OV_DECL(long, l)
	__MCC_OV_DECL(unsigned long, ul)
	__MCC_OV_DECL(long long, ll)
	__MCC_OV_DECL(unsigned long long, ull)
	#undef __MCC_OV_DECL
	#define __mcc_ov_disp(op, res) _Generic((res),			\
	signed char: __mcc_##op##o_sc, char: __mcc_##op##o_c,		\
	short: __mcc_##op##o_s, int: __mcc_##op##o_i,			\
	long: __mcc_##op##o_l, long long: __mcc_##op##o_ll,		\
	unsigned char: __mcc_##op##o_uc, unsigned short: __mcc_##op##o_us, \
	unsigned int: __mcc_##op##o_u, unsigned long: __mcc_##op##o_ul,	\
	unsigned long long: __mcc_##op##o_ull)
	#define __builtin_add_overflow(a, b, res) \
	(__mcc_ov_disp(add, *(res))((a), (b), (res)))
	#define __builtin_sub_overflow(a, b, res) \
	(__mcc_ov_disp(sub, *(res))((a), (b), (res)))
	#define __builtin_mul_overflow(a, b, res) \
	(__mcc_ov_disp(mul, *(res))((a), (b), (res)))

	#endif
