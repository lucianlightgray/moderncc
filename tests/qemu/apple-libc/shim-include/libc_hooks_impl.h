#ifndef _COMPAT_LIBC_HOOKS_IMPL_H
#define _COMPAT_LIBC_HOOKS_IMPL_H
/* Apple's introspection hooks are no-ops off the Darwin runtime. */
static inline void libc_hooks_will_read_cstring(const char *s) { (void)s; }
static inline void libc_hooks_will_read(const void *p, unsigned long n) { (void)p; (void)n; }
/* strchrnul.c defines the impl as __strchrnul and exports the public name via
   this FreeBSD alias macro (it doesn't include <sys/cdefs.h>). On Darwin C
   names gain a leading '_': C `__strchrnul` -> asm `___strchrnul`, C
   `strchrnul` -> `_strchrnul`. */
#ifndef __weak_reference
#define __weak_reference(sym, alias) \
    __asm__(".globl _" #alias "\n.set _" #alias ", _" #sym)
#endif
#endif
