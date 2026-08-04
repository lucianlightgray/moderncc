#ifndef _STDARG_H
#define _STDARG_H

typedef __builtin_va_list va_list;
#if __STDC_VERSION__ >= 202311L
#define __mcc_va_start_sel(_1, _2, _3, _sel, ...) _sel
#define __mcc_va_start_1(ap) __builtin_c23_va_start(ap)
#define __mcc_va_start_n(ap, last, ...) __builtin_va_start(ap, last, __VA_ARGS__)
#define va_start(...)                                          \
	__mcc_va_start_sel(__VA_ARGS__, __mcc_va_start_n,            \
										 __builtin_va_start, __mcc_va_start_1, ) \
	(__VA_ARGS__)
#else
#define va_start __builtin_va_start
#endif
#define va_arg __builtin_va_arg
#define va_copy __builtin_va_copy
#define va_end __builtin_va_end

typedef va_list __gnuc_va_list;
#define _VA_LIST_DEFINED

#if __STDC_VERSION__ >= 202311L
#define __STDC_VERSION_STDARG_H__ 202311L
#endif

#endif
