#ifndef _IMMINTRIN_H_INCLUDED
#define _IMMINTRIN_H_INCLUDED

#if !defined(__i386__) && !defined(__x86_64__)
#error "immintrin.h is only supported on x86 targets"
#endif

#include <x86gprintrin.h>
#include <mmintrin.h>
#include <xmmintrin.h>
#include <emmintrin.h>

#endif
