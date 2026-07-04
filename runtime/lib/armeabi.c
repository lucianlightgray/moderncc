#ifdef __MCC__
#define INT_MIN (-2147483647 - 1)
#define INT_MAX 2147483647
#define UINT_MAX 0xffffffff
#define LONG_MIN (-2147483647L - 1)
#define LONG_MAX 2147483647L
#define ULONG_MAX 0xffffffffUL
#define LLONG_MAX 9223372036854775807LL
#define LLONG_MIN (-9223372036854775807LL - 1)
#define ULLONG_MAX 0xffffffffffffffffULL
#else
#include <limits.h>
#endif

typedef struct double_unsigned_struct {
    unsigned low;
    unsigned high;
} double_unsigned_struct;

typedef struct unsigned_int_struct {
    unsigned low;
    int high;
} unsigned_int_struct;

#define REGS_RETURN(name, type)           \
    static void name##_return(type ret) { \
    }

#define FLOAT_EXP_BITS 8
#define FLOAT_FRAC_BITS 23

#define DOUBLE_EXP_BITS 11
#define DOUBLE_FRAC_BITS 52

#define ONE_EXP(type) ((1 << (type##_EXP_BITS - 1)) - 1)

REGS_RETURN(unsigned_int_struct, unsigned_int_struct)
REGS_RETURN(double_unsigned_struct, double_unsigned_struct)

#define DEFINE__AEABI_F2XLZ(name, with_sign)                     \
    void __aeabi_##name(unsigned val) {                          \
        int exp, high_shift, sign;                               \
        double_unsigned_struct ret;                              \
                                                                 \
        sign = val >> 31;                                        \
                                                                 \
        exp = val >> FLOAT_FRAC_BITS;                            \
        exp &= (1 << FLOAT_EXP_BITS) - 1;                        \
        exp -= ONE_EXP(FLOAT);                                   \
                                                                 \
        if (with_sign) {                                         \
            if (exp > 62)                                        \
                return;                                          \
        } else {                                                 \
            if ((sign && exp >= 0) || exp > 63)                  \
                return;                                          \
        }                                                        \
                                                                 \
        val &= (1 << FLOAT_FRAC_BITS) - 1;                       \
        if (exp >= 32) {                                         \
            ret.high = 1 << (exp - 32);                          \
            if (exp - 32 >= FLOAT_FRAC_BITS) {                   \
                ret.high |= val << (exp - 32 - FLOAT_FRAC_BITS); \
                ret.low = 0;                                     \
            } else {                                             \
                high_shift = FLOAT_FRAC_BITS - (exp - 32);       \
                ret.high |= val >> high_shift;                   \
                ret.low = val << (32 - high_shift);              \
            }                                                    \
        } else {                                                 \
            ret.high = 0;                                        \
            ret.low = 1 << exp;                                  \
            if (exp > FLOAT_FRAC_BITS)                           \
                ret.low |= val << (exp - FLOAT_FRAC_BITS);       \
            else                                                 \
                ret.low |= val >> (FLOAT_FRAC_BITS - exp);       \
        }                                                        \
                                                                 \
        if (with_sign && sign) {                                 \
            ret.low = ~ret.low;                                  \
            ret.high = ~ret.high;                                \
            if (ret.low == UINT_MAX) {                           \
                ret.low = 0;                                     \
                ret.high++;                                      \
            } else                                               \
                ret.low++;                                       \
        }                                                        \
                                                                 \
        double_unsigned_struct_return(ret);                      \
    }

DEFINE__AEABI_F2XLZ(f2ulz, 0)

DEFINE__AEABI_F2XLZ(f2lz, 1)

#define DEFINE__AEABI_D2XLZ(name, with_sign)                          \
    void __aeabi_##name(double_unsigned_struct val) {                 \
        int exp, high_shift, sign;                                    \
        double_unsigned_struct ret;                                   \
                                                                      \
        if ((val.high & ~0x80000000) == 0 && val.low == 0) {          \
            ret.low = ret.high = 0;                                   \
            goto _ret_;                                               \
        }                                                             \
                                                                      \
        sign = val.high >> 31;                                        \
                                                                      \
        exp = (val.high >> (DOUBLE_FRAC_BITS - 32));                  \
        exp &= (1 << DOUBLE_EXP_BITS) - 1;                            \
        exp -= ONE_EXP(DOUBLE);                                       \
                                                                      \
        if (with_sign) {                                              \
            if (exp > 62)                                             \
                return;                                               \
        } else {                                                      \
            if ((sign && exp >= 0) || exp > 63)                       \
                return;                                               \
        }                                                             \
                                                                      \
        val.high &= (1 << (DOUBLE_FRAC_BITS - 32)) - 1;               \
        if (exp >= 32) {                                              \
            ret.high = 1 << (exp - 32);                               \
            if (exp >= DOUBLE_FRAC_BITS) {                            \
                high_shift = exp - DOUBLE_FRAC_BITS;                  \
                ret.high |= val.high << high_shift;                   \
                ret.high |= val.low >> (32 - high_shift);             \
                ret.low = val.low << high_shift;                      \
            } else {                                                  \
                high_shift = DOUBLE_FRAC_BITS - exp;                  \
                ret.high |= val.high >> high_shift;                   \
                ret.low = val.high << (32 - high_shift);              \
                ret.low |= val.low >> high_shift;                     \
            }                                                         \
        } else {                                                      \
            ret.high = 0;                                             \
            ret.low = 1 << exp;                                       \
            if (exp > DOUBLE_FRAC_BITS - 32) {                        \
                high_shift = exp - (DOUBLE_FRAC_BITS - 32);           \
                ret.low |= val.high << high_shift;                    \
                ret.low |= val.low >> (32 - high_shift);              \
            } else                                                    \
                ret.low |= val.high >> (DOUBLE_FRAC_BITS - 32 - exp); \
        }                                                             \
                                                                      \
        if (with_sign && sign) {                                      \
            ret.low = ~ret.low;                                       \
            ret.high = ~ret.high;                                     \
            if (ret.low == UINT_MAX) {                                \
                ret.low = 0;                                          \
                ret.high++;                                           \
            } else                                                    \
                ret.low++;                                            \
        }                                                             \
                                                                      \
    _ret_:                                                            \
        double_unsigned_struct_return(ret);                           \
    }

DEFINE__AEABI_D2XLZ(d2ulz, 0)

DEFINE__AEABI_D2XLZ(d2lz, 1)

#define DEFINE__AEABI_XL2F(name, with_sign)                              \
    unsigned __aeabi_##name(unsigned long long v) {                      \
        int s, flb, sign = 0;                                            \
        unsigned p = 0, ret;                                             \
        double_unsigned_struct val;                                      \
                                                                         \
        if (with_sign && (v & (1ULL << 63))) {                           \
            sign = 1;                                                    \
            v = ~v + 1;                                                  \
        }                                                                \
        val.low = v;                                                     \
        val.high = v >> 32;                                              \
                                                                         \
        for (s = 31, p = 1 << 31; p && !(val.high & p); s--, p >>= 1)    \
            ;                                                            \
        if (p) {                                                         \
            ret = val.high & (p - 1);                                    \
            if (s < FLOAT_FRAC_BITS) {                                   \
                ret <<= FLOAT_FRAC_BITS - s;                             \
                ret |= val.low >> (32 - (FLOAT_FRAC_BITS - s));          \
                flb = (val.low >> (32 - (FLOAT_FRAC_BITS - s - 1))) & 1; \
            } else {                                                     \
                flb = (ret >> (s - FLOAT_FRAC_BITS - 1)) & 1;            \
                ret >>= s - FLOAT_FRAC_BITS;                             \
            }                                                            \
            s += 32;                                                     \
        } else {                                                         \
            for (s = 31, p = 1 << 31; p && !(val.low & p); s--, p >>= 1) \
                ;                                                        \
            if (p) {                                                     \
                ret = val.low & (p - 1);                                 \
                if (s <= FLOAT_FRAC_BITS) {                              \
                    ret <<= FLOAT_FRAC_BITS - s;                         \
                    flb = 0;                                             \
                } else {                                                 \
                    flb = (ret >> (s - FLOAT_FRAC_BITS - 1)) & 1;        \
                    ret >>= s - FLOAT_FRAC_BITS;                         \
                }                                                        \
            } else                                                       \
                return 0;                                                \
        }                                                                \
        if (flb)                                                         \
            ret++;                                                       \
                                                                         \
        ret |= (s + ONE_EXP(FLOAT)) << FLOAT_FRAC_BITS;                  \
                                                                         \
        ret |= sign << 31;                                               \
                                                                         \
        return ret;                                                      \
    }

DEFINE__AEABI_XL2F(ul2f, 0)

DEFINE__AEABI_XL2F(l2f, 1)

#define __AEABI_XL2D(name, with_sign)                                    \
    void __aeabi_##name(unsigned long long v) {                          \
        int s, high_shift, sign = 0;                                     \
        unsigned tmp, p = 0;                                             \
        double_unsigned_struct val, ret;                                 \
                                                                         \
        if (with_sign && (v & (1ULL << 63))) {                           \
            sign = 1;                                                    \
            v = ~v + 1;                                                  \
        }                                                                \
        val.low = v;                                                     \
        val.high = v >> 32;                                              \
                                                                         \
        for (s = 31, p = 1 << 31; p && !(val.high & p); s--, p >>= 1)    \
            ;                                                            \
        if (p) {                                                         \
            tmp = val.high & (p - 1);                                    \
            if (s < DOUBLE_FRAC_BITS - 32) {                             \
                high_shift = DOUBLE_FRAC_BITS - 32 - s;                  \
                ret.high = tmp << high_shift;                            \
                ret.high |= val.low >> (32 - high_shift);                \
                ret.low = val.low << high_shift;                         \
            } else {                                                     \
                high_shift = s - (DOUBLE_FRAC_BITS - 32);                \
                ret.high = tmp >> high_shift;                            \
                ret.low = tmp << (32 - high_shift);                      \
                ret.low |= val.low >> high_shift;                        \
                if ((val.low >> (high_shift - 1)) & 1) {                 \
                    if (ret.low == UINT_MAX) {                           \
                        ret.high++;                                      \
                        ret.low = 0;                                     \
                    } else                                               \
                        ret.low++;                                       \
                }                                                        \
            }                                                            \
            s += 32;                                                     \
        } else {                                                         \
            for (s = 31, p = 1 << 31; p && !(val.low & p); s--, p >>= 1) \
                ;                                                        \
            if (p) {                                                     \
                tmp = val.low & (p - 1);                                 \
                if (s <= DOUBLE_FRAC_BITS - 32) {                        \
                    high_shift = DOUBLE_FRAC_BITS - 32 - s;              \
                    ret.high = tmp << high_shift;                        \
                    ret.low = 0;                                         \
                } else {                                                 \
                    high_shift = s - (DOUBLE_FRAC_BITS - 32);            \
                    ret.high = tmp >> high_shift;                        \
                    ret.low = tmp << (32 - high_shift);                  \
                }                                                        \
            } else {                                                     \
                ret.high = ret.low = 0;                                  \
                goto _ret_;                                              \
            }                                                            \
        }                                                                \
                                                                         \
        ret.high |= (s + ONE_EXP(DOUBLE)) << (DOUBLE_FRAC_BITS - 32);    \
                                                                         \
        ret.high |= sign << 31;                                          \
                                                                         \
    _ret_:                                                               \
        double_unsigned_struct_return(ret);                              \
    }

__AEABI_XL2D(ul2d, 0)

__AEABI_XL2D(l2d, 1)

#if 1

#define define_aeabi_xdivmod_signed_type(basetype, type) \
    typedef struct type {                                \
        basetype quot;                                   \
        unsigned basetype rem;                           \
    } type

#define define_aeabi_xdivmod_unsigned_type(basetype, type) \
    typedef struct type {                                  \
        basetype quot;                                     \
        basetype rem;                                      \
    } type

#define AEABI_UXDIVMOD(name, type, rettype, typemacro)                      \
    static inline rettype aeabi_##name(type num, type den) {                \
        rettype ret;                                                        \
        type quot = 0;                                                      \
                                                                            \
        while (num >= den) {                                                \
            type q = 1;                                                     \
                                                                            \
            while ((q << 1) * den <= num && q * den <= typemacro##_MAX / 2) \
                q <<= 1;                                                    \
                                                                            \
            num -= q * den;                                                 \
            quot += q;                                                      \
        }                                                                   \
        ret.quot = quot;                                                    \
        ret.rem = num;                                                      \
        return ret;                                                         \
    }

#define __AEABI_XDIVMOD(name, type, uiname, rettype, urettype, typemacro)     \
    void __aeabi_##name(type numerator, type denominator) {                   \
        unsigned type num, den;                                               \
        urettype uxdiv_ret;                                                   \
        rettype ret;                                                          \
                                                                              \
        if (numerator >= 0)                                                   \
            num = numerator;                                                  \
        else                                                                  \
            num = 0 - numerator;                                              \
        if (denominator >= 0)                                                 \
            den = denominator;                                                \
        else                                                                  \
            den = 0 - denominator;                                            \
        uxdiv_ret = aeabi_##uiname(num, den);                                 \
                                                                              \
        if ((numerator & typemacro##_MIN) != (denominator & typemacro##_MIN)) \
            ret.quot = 0 - uxdiv_ret.quot;                                    \
        else                                                                  \
            ret.quot = uxdiv_ret.quot;                                        \
        if (numerator < 0)                                                    \
            ret.rem = 0 - uxdiv_ret.rem;                                      \
        else                                                                  \
            ret.rem = uxdiv_ret.rem;                                          \
                                                                              \
        rettype##_return(ret);                                                \
    }

define_aeabi_xdivmod_signed_type(long long, lldiv_t);
define_aeabi_xdivmod_unsigned_type(unsigned long long, ulldiv_t);
define_aeabi_xdivmod_signed_type(int, idiv_t);
define_aeabi_xdivmod_unsigned_type(unsigned, uidiv_t);

REGS_RETURN(lldiv_t, lldiv_t)
REGS_RETURN(ulldiv_t, ulldiv_t)
REGS_RETURN(idiv_t, idiv_t)
REGS_RETURN(uidiv_t, uidiv_t)

AEABI_UXDIVMOD(uldivmod, unsigned long long, ulldiv_t, ULLONG)

__AEABI_XDIVMOD(ldivmod, long long, uldivmod, lldiv_t, ulldiv_t, LLONG)

void __aeabi_uldivmod(unsigned long long num, unsigned long long den) {
    ulldiv_t_return(aeabi_uldivmod(num, den));
}

void __aeabi_llsl(double_unsigned_struct val, int shift) {
    double_unsigned_struct ret;

    if (shift >= 32) {
        val.high = val.low;
        val.low = 0;
        shift -= 32;
    }
    if (shift > 0) {
        ret.low = val.low << shift;
        ret.high = (val.high << shift) | (val.low >> (32 - shift));
        double_unsigned_struct_return(ret);
        return;
    }
    double_unsigned_struct_return(val);
}

#define aeabi_lsr(val, shift, fill, type)                          \
    type##_struct ret;                                             \
                                                                   \
    if (shift >= 32) {                                             \
        val.low = val.high;                                        \
        val.high = fill;                                           \
        shift -= 32;                                               \
    }                                                              \
    if (shift > 0) {                                               \
        ret.high = val.high >> shift;                              \
        ret.low = (val.high << (32 - shift)) | (val.low >> shift); \
        type##_struct_return(ret);                                 \
        return;                                                    \
    }                                                              \
    type##_struct_return(val);

void __aeabi_llsr(double_unsigned_struct val, int shift) {
    aeabi_lsr(val, shift, 0, double_unsigned);
}

void __aeabi_lasr(unsigned_int_struct val, int shift) {
    aeabi_lsr(val, shift, val.high >> 31, unsigned_int);
}

#if 0
AEABI_UXDIVMOD(uidivmod, unsigned, uidiv_t, UINT)

int __aeabi_idiv(int numerator, int denominator)
{
    unsigned num, den;
    uidiv_t ret;

    if (numerator >= 0)
        num = numerator;
    else
        num = 0 - numerator;
    if (denominator >= 0)
        den = denominator;
    else
        den = 0 - denominator;
    ret = aeabi_uidivmod(num, den);
    if ((numerator & INT_MIN) != (denominator & INT_MIN))
        ret.quot *= -1;
    return ret.quot;
}

unsigned __aeabi_uidiv(unsigned num, unsigned den)
{
    return aeabi_uidivmod(num, den).quot;
}

__AEABI_XDIVMOD(idivmod, int, uidivmod, idiv_t, uidiv_t, INT)

void __aeabi_uidivmod(unsigned num, unsigned den)
{
    uidiv_t_return(aeabi_uidivmod(num, den));
}
#else
#define UIDIVMOD_ASM 1
#endif

typedef __SIZE_TYPE__ size_t;
extern void *memcpy(void *dest, const void *src, size_t n);
extern void *memmove(void *dest, const void *src, size_t n);
extern void *memset(void *s, int c, size_t n);

void *
__aeabi_memcpy(void *dest, const void *src, size_t n) {
    return memcpy(dest, src, n);
}

void *
__aeabi_memmove(void *dest, const void *src, size_t n) {
    return memmove(dest, src, n);
}

void *
__aeabi_memmove4(void *dest, const void *src, size_t n) {
    return memmove(dest, src, n);
}

void *
__aeabi_memmove8(void *dest, const void *src, size_t n) {
    return memmove(dest, src, n);
}

void *
__aeabi_memset(void *s, size_t n, int c) {
    return memset(s, c, n);
}

#if UIDIVMOD_ASM
__asm__(
    "\n  .global __aeabi_idiv, __aeabi_idivmod"
    "\n  .global __aeabi_uidiv, __aeabi_uidivmod"
#if __ARM_FEATURE_IDIV
    "\n__aeabi_idiv:"
    "\n__aeabi_idivmod:"
    "\n  mov     r2, r0"
    "\n  sdiv    r0, r0, r1"
    "\n  mls     r1, r1, r0, r2"
    "\n  bx      lr"

    "\n__aeabi_uidiv:"
    "\n__aeabi_uidivmod:"
    "\n  mov     r2, r0"
    "\n  udiv    r0, r0, r1"
    "\n  mls     r1, r1, r0, r2"
    "\n  bx      lr"
#else
    "\n__aeabi_idiv:"
    "\n__aeabi_idivmod:"
    "\n  cmp     r0, #0"
    "\n  bge     .Lnumerator_pos"
    "\n  rsb     r0, r0, #0"
    "\n  cmp     r1, #0"
    "\n  bge     .Lboth_neg"
    "\n  rsb     r1, r1, #0"
    "\n  push    {lr}"
    "\n  bl      __aeabi_uidivmod"
    "\n  rsb     r1, r1, #0"
    "\n  pop     {pc}"
    "\n.Lboth_neg:"
    "\n  push    {lr}"
    "\n  bl      __aeabi_uidivmod"
    "\n  rsb     r0, r0, #0"
    "\n  rsb     r1, r1, #0"
    "\n  pop     {pc}"
    "\n.Ldenom_neg:"
    "\n  rsb     r1, r1, #0"
    "\n  push    {lr}"
    "\n  bl      __aeabi_uidivmod"
    "\n  rsb     r0, r0, #0"
    "\n  pop     {pc}"
    "\n.Lnumerator_pos:"
    "\n  cmp     r1, #0"
    "\n  blt     .Ldenom_neg"

    "\n__aeabi_uidiv:"
    "\n__aeabi_uidivmod:"
    "\n  mov     r2, #1"
    "\n  mov     r3, #0"
    "\n  cmp     r0, r1"
    "\n  bls     .Lsub_loop"
    "\n  adds    r1, #0"
    "\n  bmi     .Lsub_loop"
    "\n  beq     .Luidiv0"
    "\n.Ldenom_shift_loop:"
    "\n  lsl     r2, #1"
    "\n  lsls    r1, #1"
    "\n  bmi     .Lsub_loop"
    "\n  cmp     r0, r1"
    "\n  bhi     .Ldenom_shift_loop"
    "\n.Lsub_loop:"
    "\n  cmp     r0, r1"
    "\n  subcs   r0, r1"
    "\n  orrcs   r3, r2"
    "\n  lsr     r1, #1"
    "\n  lsrs    r2, #1"
    "\n  bne     .Lsub_loop"
    "\n  mov     r1, r0"
    "\n  mov     r0, r3"
    "\n  bx      lr"
    "\n.Luidiv0:"
    "\n  mov     r0, #0"
    "\n  bx      lr"
#endif
);
#endif
#endif
