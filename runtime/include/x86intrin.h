#ifndef _X86INTRIN_H_INCLUDED
#define _X86INTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "x86intrin.h is only supported on x86 targets"
#endif

#include <ia32intrin.h>
#include <immintrin.h>

#endif
