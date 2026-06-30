
static double mcc_dinf(void)
{
    union { unsigned long long u; double d; } x;
    x.u = 0x7FF0000000000000ULL;
    return x.d;
}

#define ISNAN(x)     ((x) != (x))
#define ISFINITE(x)  (((x) - (x)) == ((x) - (x)))
#define ISINF(x)     (!ISNAN(x) && !ISFINITE(x))
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
        if (ISINF(a) || ISINF(b)) {                       \
            a = COPYSIGN(ISINF(a) ? (T)1 : (T)0, a);                          \
            b = COPYSIGN(ISINF(b) ? (T)1 : (T)0, b);                          \
            if (ISNAN(c)) c = COPYSIGN((T)0, c);                              \
            if (ISNAN(d)) d = COPYSIGN((T)0, d);                              \
            recalc = 1;                                                       \
        }                                                                     \
        if (ISINF(c) || ISINF(d)) {                       \
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
