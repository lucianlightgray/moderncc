

#ifndef _COMPAT_SYS_CDEFS_H
#define _COMPAT_SYS_CDEFS_H
#define __FBSDID(x)
#ifndef __restrict
#define __restrict restrict
#endif


#define __weak_reference(sym, alias) \
    __asm__(".globl _" #alias "\n.set _" #alias ", _" #sym)
#endif
