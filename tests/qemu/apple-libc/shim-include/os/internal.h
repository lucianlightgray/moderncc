#ifndef _COMPAT_OS_INTERNAL_H
#define _COMPAT_OS_INTERNAL_H
/* These fire only on buffer overflow / vm failure -- not on the fixed-buffer
   _simple_vsnprintf path under test. Map to abort so misuse is loud. */
void abort(void);
#define __LIBPLATFORM_CLIENT_CRASH__(k, msg)   abort()
#define __LIBPLATFORM_INTERNAL_CRASH__(k, msg) abort()
#endif
