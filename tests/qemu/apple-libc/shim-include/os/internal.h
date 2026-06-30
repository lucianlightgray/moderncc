#ifndef _COMPAT_OS_INTERNAL_H
#define _COMPAT_OS_INTERNAL_H


void abort(void);
#define __LIBPLATFORM_CLIENT_CRASH__(k, msg)   abort()
#define __LIBPLATFORM_INTERNAL_CRASH__(k, msg) abort()
#endif
