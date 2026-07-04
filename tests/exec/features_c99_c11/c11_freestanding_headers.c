#include <iso646.h>
#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stdatomic.h>

extern int printf(const char *, ...);

int main(void) {
    int ok = 1;

#if ATOMIC_INT_LOCK_FREE != 2 || ATOMIC_BOOL_LOCK_FREE != 2 || ATOMIC_POINTER_LOCK_FREE != 2
    ok = 0;
#endif
    if (ATOMIC_CHAR16_T_LOCK_FREE < 1 || ATOMIC_CHAR32_T_LOCK_FREE < 1)
        ok = 0;
    if (kill_dependency(40 + 2) != 42)
        ok = 0;

#if !defined(__ATOMIC_RELAXED) || !defined(__ATOMIC_SEQ_CST)
    ok = 0;
#else
    if (__ATOMIC_RELAXED != 0 || __ATOMIC_SEQ_CST != 5)
        ok = 0;
#endif

#if !defined(__CHAR16_TYPE__) || !defined(__CHAR32_TYPE__)
    ok = 0;
#else
    if (sizeof(__CHAR16_TYPE__) != 2 || sizeof(__CHAR32_TYPE__) != 4)
        ok = 0;
    {
        __CHAR32_TYPE__ cp = 0x10FFFF;
        if (cp != 0x10FFFF)
            ok = 0;
    }
#endif

    if (CHAR_BIT != 8)
        ok = 0;
    if (SCHAR_MAX != 127 || UCHAR_MAX != 255)
        ok = 0;
    if (SHRT_MAX != 32767 || USHRT_MAX != 65535)
        ok = 0;
    if (INT_MAX != 2147483647 || INT_MIN != (-2147483647 - 1))
        ok = 0;
    if (LLONG_MAX != 9223372036854775807LL)
        ok = 0;
    if ((unsigned long long)ULLONG_MAX != 18446744073709551615ULL)
        ok = 0;

    if (sizeof(int8_t) != 1 || sizeof(int16_t) != 2 || sizeof(int32_t) != 4 || sizeof(int64_t) != 8)
        ok = 0;
    if (sizeof(intptr_t) != sizeof(void *))
        ok = 0;
    if (INT32_MAX != 2147483647 || (uint32_t)UINT32_MAX != 4294967295U)
        ok = 0;
    if (INT64_MAX != 9223372036854775807LL)
        ok = 0;
    if (UINT64_C(1) != 1ULL || INTMAX_C(2) != 2)
        ok = 0;

#if !defined(_WIN32)
#if !defined(__STDC_ISO_10646__) || __STDC_ISO_10646__ < 201103L
    ok = 0;
#endif
#endif

#if !defined(__STDC_IEC_559__) || !defined(__STDC_IEC_559_COMPLEX__)
    ok = 0;
#endif

    if (!(1 and 1))
        ok = 0;
    if (!(0 or 1))
        ok = 0;
    if ((compl 0) != ~0)
        ok = 0;
    if ((5 bitand 3) != (5 & 3))
        ok = 0;
    if ((5 bitor 2) != (5 | 2))
        ok = 0;
    if ((5 xor 1) != (5 ^ 1))
        ok = 0;
    if (not 0 != !0)
        ok = 0;
    if ((1 not_eq 2) != (1 != 2))
        ok = 0;

#ifndef FLT_EVAL_METHOD
    ok = 0;
#endif
    if (FLT_HAS_SUBNORM != 1 || DBL_HAS_SUBNORM != 1 || LDBL_HAS_SUBNORM != 1)
        ok = 0;
    if (FLT_DECIMAL_DIG != 9)
        ok = 0;
    if (DBL_DECIMAL_DIG != 17)
        ok = 0;
    if (LDBL_DECIMAL_DIG < 17)
        ok = 0;
    if (!(FLT_TRUE_MIN > 0.0f && FLT_TRUE_MIN < FLT_MIN))
        ok = 0;
    if (!(DBL_TRUE_MIN > 0.0 && DBL_TRUE_MIN < DBL_MIN))
        ok = 0;
    if (!(LDBL_TRUE_MIN > 0.0L && LDBL_TRUE_MIN < LDBL_MIN))
        ok = 0;

    printf(ok ? "OK\n" : "FAIL\n");
    return ok ? 0 : 1;
}
