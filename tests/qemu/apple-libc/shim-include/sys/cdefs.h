/* Minimal compat shim so Apple's vendored libc sources compile freestanding
   off-macOS. Provides only the macros the vendored *.c reference. */
#ifndef _COMPAT_SYS_CDEFS_H
#define _COMPAT_SYS_CDEFS_H
#define __FBSDID(x)
#ifndef __restrict
#define __restrict restrict
#endif
/* FreeBSD/Apple public-name alias macro. On Darwin C names get a leading '_',
   so C `__strchrnul` -> asm `___strchrnul` and C `strchrnul` -> `_strchrnul`. */
#define __weak_reference(sym, alias) \
    __asm__(".globl _" #alias "\n.set _" #alias ", _" #sym)
#endif
