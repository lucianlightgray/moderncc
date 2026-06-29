#ifndef _STDDEF_H
#define _STDDEF_H

typedef __SIZE_TYPE__ size_t;
typedef __WCHAR_TYPE__ wchar_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;
/* 7.19: a strictly-conforming <stddef.h> defines only ptrdiff_t, size_t,
   max_align_t, wchar_t, NULL and offsetof.  ssize_t (POSIX) and
   intptr_t/uintptr_t (<stdint.h>) used to be declared here but pollute the
   namespace; like gcc's own <stddef.h> they are no longer provided on the
   ELF/standard path.  The bundled win32 (MinGW-style) headers, however, expect
   <stddef.h> to supply intptr_t/uintptr_t exactly as MinGW's own <stddef.h>
   does, so they are kept on the PE target only. */
#if defined _WIN32
typedef __PTRDIFF_TYPE__ ssize_t;
typedef __PTRDIFF_TYPE__ intptr_t;
typedef __SIZE_TYPE__ uintptr_t;
#endif

#if __STDC_VERSION__ >= 201112L
typedef union { long long __ll; long double __ld; } max_align_t;
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#undef offsetof
#define offsetof(type, field) __builtin_offsetof(type, field)

/* 7.19: alloca is not a <stddef.h> member.  It is declared unconditionally in
   the always-injected <mccdefs.h> predefs, so it stays available everywhere
   without polluting <stddef.h>. */

#endif

#if defined (__need_wint_t)
#ifndef _WINT_T
#define _WINT_T
typedef __WINT_TYPE__ wint_t;
#endif
#undef __need_wint_t
#endif
