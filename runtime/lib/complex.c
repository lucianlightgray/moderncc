/* Annex G robust complex multiply / divide helpers for mcc.
 *
 * C11 Annex G.5.1/G.5.2 specify the inf/nan-correct multiply and divide that
 * gcc/clang use by default (their __mul?c3 / __div?c3).  mcc's inline complex
 * codegen (gen_complex_op in src/mccgen.c) only implements the naive
 * (ac-bd)+(ad+bc)i form, which is the CX_LIMITED_RANGE-ON regime; when that
 * pragma / -fcx-limited-range is not in force the codegen routes * and / here.
 *
 * The mcc/libgcc complex return ABI differs per target, so rather than depend
 * on it these helpers take the result by pointer (res[0]=real, res[1]=imag)
 * and return void — portable across all five backends with no ABI work in the
 * codegen.  They are mcc-internal (not libgcc-ABI __muldc3 &c.).
 *
 * Freestanding: no <math.h> / <float.h>.  isnan/isinf/copysign/fabs and a
 * +infinity are built from arithmetic and one bit pattern so this file builds
 * with the bundled headers alone (like the other runtime/lib/*.c sources).
 */

/* +inf as a double via its IEEE-754 bit pattern; cast to float/long double
   yields the corresponding +inf.  Avoids a literal divide-by-zero (which would
   raise FE_DIVBYZERO) or a folded over-large constant. */
static double mcc_dinf(void)
{
    union { unsigned long long u; double d; } x;
    x.u = 0x7FF0000000000000ULL;
    return x.d;
}

/* A NaN is the only value unequal to itself.  inf-inf and nan-nan are NaN, so
   x-x is NaN exactly for the non-finite x; finite x gives 0. */
#define ISNAN(x)     ((x) != (x))
#define ISFINITE(x)  (((x) - (x)) == ((x) - (x)))
#define ISINF(x)     (!ISNAN(x) && !ISFINITE(x))
/* signbit, incl. -0.0 (1/-0 = -inf < 0) and -inf; NaN has no sign here.
   1.0/(x) keeps x's reciprocal sign for every float type, so no T needed. */
#define SIGNBIT(x)   (((x) < 0) || ((x) == 0 && 1.0 / (x) < 0))
#define FABS(x)      (SIGNBIT(x) ? -(x) : (x))
#define COPYSIGN(m,s) (SIGNBIT(s) ? -FABS(m) : FABS(m))

#define GEN_MUL(NAME, T)                                                      \
void NAME(T *res, T a, T b, T c, T d)                                         \
{                                                                             \
    T ac = a * c, bd = b * d, ad = a * d, bc = b * c;                         \
    T x = ac - bd, y = ad + bc;                                               \
    if (ISNAN(x) && ISNAN(y)) {                                               \
        T inf = (T)mcc_dinf();                                                \
        int recalc = 0;                                                       \
        if (ISINF(a) || ISINF(b)) {              /* lhs is infinite */        \
            a = COPYSIGN(ISINF(a) ? (T)1 : (T)0, a);                          \
            b = COPYSIGN(ISINF(b) ? (T)1 : (T)0, b);                          \
            if (ISNAN(c)) c = COPYSIGN((T)0, c);                              \
            if (ISNAN(d)) d = COPYSIGN((T)0, d);                              \
            recalc = 1;                                                       \
        }                                                                     \
        if (ISINF(c) || ISINF(d)) {              /* rhs is infinite */        \
            c = COPYSIGN(ISINF(c) ? (T)1 : (T)0, c);                          \
            d = COPYSIGN(ISINF(d) ? (T)1 : (T)0, d);                          \
            if (ISNAN(a)) a = COPYSIGN((T)0, a);                              \
            if (ISNAN(b)) b = COPYSIGN((T)0, b);                              \
            recalc = 1;                                                       \
        }                                                                     \
        if (!recalc && (ISINF(ac) || ISINF(bd) ||                            \
                        ISINF(ad) || ISINF(bc))) {                            \
            if (ISNAN(a)) a = COPYSIGN((T)0, a);                              \
            if (ISNAN(b)) b = COPYSIGN((T)0, b);                              \
            if (ISNAN(c)) c = COPYSIGN((T)0, c);                              \
            if (ISNAN(d)) d = COPYSIGN((T)0, d);                              \
            recalc = 1;                                                       \
        }                                                                     \
        if (recalc) {                                                         \
            x = inf * (a * c - b * d);                                        \
            y = inf * (a * d + b * c);                                        \
        }                                                                     \
    }                                                                         \
    res[0] = x;                                                               \
    res[1] = y;                                                               \
}

/* Smith's scaled division (branch on the larger of |c|,|d| to avoid spurious
   overflow) followed by the Annex G.5.2 inf/zero recovery. */
#define GEN_DIV(NAME, T)                                                      \
void NAME(T *res, T a, T b, T c, T d)                                         \
{                                                                             \
    T x, y;                                                                   \
    if (FABS(c) >= FABS(d)) {                                                 \
        T r = d / c;                                                          \
        T den = c + d * r;                                                    \
        x = (a + b * r) / den;                                                \
        y = (b - a * r) / den;                                                \
    } else {                                                                  \
        T r = c / d;                                                          \
        T den = c * r + d;                                                    \
        x = (a * r + b) / den;                                                \
        y = (b * r - a) / den;                                                \
    }                                                                         \
    if (ISNAN(x) && ISNAN(y)) {                                               \
        T inf = (T)mcc_dinf();                                                \
        if (c == 0 && d == 0 && (!ISNAN(a) || !ISNAN(b))) {                   \
            x = COPYSIGN(inf, c) * a;                                         \
            y = COPYSIGN(inf, c) * b;                                         \
        } else if ((ISINF(a) || ISINF(b)) &&                                  \
                   ISFINITE(c) && ISFINITE(d)) {                              \
            a = COPYSIGN(ISINF(a) ? (T)1 : (T)0, a);                          \
            b = COPYSIGN(ISINF(b) ? (T)1 : (T)0, b);                          \
            x = inf * (a * c + b * d);                                        \
            y = inf * (b * c - a * d);                                        \
        } else if ((ISINF(c) || ISINF(d)) &&                                  \
                   ISFINITE(a) && ISFINITE(b)) {                              \
            c = COPYSIGN(ISINF(c) ? (T)1 : (T)0, c);                          \
            d = COPYSIGN(ISINF(d) ? (T)1 : (T)0, d);                          \
            x = (T)0 * (a * c + b * d);                                       \
            y = (T)0 * (b * c - a * d);                                       \
        }                                                                     \
    }                                                                         \
    res[0] = x;                                                               \
    res[1] = y;                                                               \
}

GEN_MUL(__mcc_cmulf, float)
GEN_MUL(__mcc_cmul,  double)
GEN_MUL(__mcc_cmull, long double)

GEN_DIV(__mcc_cdivf, float)
GEN_DIV(__mcc_cdiv,  double)
GEN_DIV(__mcc_cdivl, long double)
