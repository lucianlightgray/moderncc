#ifndef _COMPAT_LIBC_HOOKS_IMPL_H
#define _COMPAT_LIBC_HOOKS_IMPL_H

static inline void libc_hooks_will_read_cstring(const char *s) { (void)s; }
static inline void libc_hooks_will_read(const void *p, unsigned long n) { (void)p; (void)n; }




#ifndef __weak_reference
#define __weak_reference(sym, alias) \
    __asm__(".globl _" #alias "\n.set _" #alias ", _" #sym)
#endif
#endif
