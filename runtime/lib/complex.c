static double mcc_dinf(void) {
	union {
		unsigned long long u;
		double d;
	} x;
	x.u = 0x7FF0000000000000ULL;
	return x.d;
}

#define ISNAN(x) ((x) != (x))
#define ISFINITE(x) (((x) - (x)) == ((x) - (x)))
#define ISINF(x) (!ISNAN(x) && !ISFINITE(x))
#define SIGNBIT(x) (__builtin_signbit(x) != 0)
#define FABS(x) (SIGNBIT(x) ? -(x) : (x))
#define COPYSIGN(m, s) (SIGNBIT(s) ? -FABS(m) : FABS(m))

#define GEN_MUL(NAME, T)                              \
	void NAME(T *res, T a, T b, T c, T d) {             \
		T ac = a * c, bd = b * d, ad = a * d, bc = b * c; \
		T x = ac - bd, y = ad + bc;                       \
		if (ISNAN(x) && ISNAN(y)) {                       \
			T inf = (T)mcc_dinf();                          \
			int recalc = 0;                                 \
			if (ISINF(a) || ISINF(b)) {                     \
				a = COPYSIGN(ISINF(a) ? (T)1 : (T)0, a);      \
				b = COPYSIGN(ISINF(b) ? (T)1 : (T)0, b);      \
				if (ISNAN(c))                                 \
					c = COPYSIGN((T)0, c);                      \
				if (ISNAN(d))                                 \
					d = COPYSIGN((T)0, d);                      \
				recalc = 1;                                   \
			}                                               \
			if (ISINF(c) || ISINF(d)) {                     \
				c = COPYSIGN(ISINF(c) ? (T)1 : (T)0, c);      \
				d = COPYSIGN(ISINF(d) ? (T)1 : (T)0, d);      \
				if (ISNAN(a))                                 \
					a = COPYSIGN((T)0, a);                      \
				if (ISNAN(b))                                 \
					b = COPYSIGN((T)0, b);                      \
				recalc = 1;                                   \
			}                                               \
			if (!recalc && (ISINF(ac) || ISINF(bd) ||       \
											ISINF(ad) || ISINF(bc))) {      \
				if (ISNAN(a))                                 \
					a = COPYSIGN((T)0, a);                      \
				if (ISNAN(b))                                 \
					b = COPYSIGN((T)0, b);                      \
				if (ISNAN(c))                                 \
					c = COPYSIGN((T)0, c);                      \
				if (ISNAN(d))                                 \
					d = COPYSIGN((T)0, d);                      \
				recalc = 1;                                   \
			}                                               \
			if (recalc) {                                   \
				x = inf * (a * c - b * d);                    \
				y = inf * (a * d + b * c);                    \
			}                                               \
		}                                                 \
		res[0] = x;                                       \
		res[1] = y;                                       \
	}

#define CDIV_SCALE(T, RMINSCAL) \
	a = a * (T)(RMINSCAL);        \
	b = b * (T)(RMINSCAL);        \
	c = c * (T)(RMINSCAL);        \
	d = d * (T)(RMINSCAL);

#define CDIV_SMITH(T, TMAX, TMIN, TEPS)                        \
	{                                                            \
		const T rbig = (T)(TMAX) / 2;                              \
		const T rmin = (T)(TMIN);                                  \
		const T rmin2 = (T)(TEPS);                                 \
		const T rminscal = (T)1 / (T)(TEPS);                       \
		const T rmax2 = ((T)(TMAX) / 2) * (T)(TEPS);               \
		T ratio, den;                                              \
		if (FABS(c) < FABS(d)) {                                   \
			if (FABS(d) >= rbig) {                                   \
				a = a / 2;                                             \
				b = b / 2;                                             \
				c = c / 2;                                             \
				d = d / 2;                                             \
			}                                                        \
			if (FABS(d) < rmin2) {                                   \
				CDIV_SCALE(T, rminscal)                                \
			} else if (((FABS(a) < rmin) && (FABS(b) < rmax2) &&      \
									(FABS(d) < rmax2)) ||                        \
								 ((FABS(b) < rmin) && (FABS(a) < rmax2) &&       \
									(FABS(d) < rmax2))) {                        \
				CDIV_SCALE(T, rminscal)                                \
			}                                                        \
			ratio = c / d;                                           \
			den = (c * ratio) + d;                                   \
			if (FABS(ratio) > rmin) {                                \
				x = ((a * ratio) + b) / den;                           \
				y = ((b * ratio) - a) / den;                           \
			} else {                                                 \
				x = ((c * (a / d)) + b) / den;                         \
				y = ((c * (b / d)) - a) / den;                         \
			}                                                        \
		} else {                                                   \
			if (FABS(c) >= rbig) {                                   \
				a = a / 2;                                             \
				b = b / 2;                                             \
				c = c / 2;                                             \
				d = d / 2;                                             \
			}                                                        \
			if (FABS(c) < rmin2) {                                   \
				CDIV_SCALE(T, rminscal)                                \
			} else if (((FABS(a) < rmin) && (FABS(b) < rmax2) &&      \
									(FABS(c) < rmax2)) ||                        \
								 ((FABS(b) < rmin) && (FABS(a) < rmax2) &&       \
									(FABS(c) < rmax2))) {                        \
				CDIV_SCALE(T, rminscal)                                \
			}                                                        \
			ratio = d / c;                                           \
			den = (d * ratio) + c;                                   \
			if (FABS(ratio) > rmin) {                                \
				x = ((b * ratio) + a) / den;                           \
				y = (b - (a * ratio)) / den;                           \
			} else {                                                 \
				x = (a + (d * (b / c))) / den;                         \
				y = (b - (d * (a / c))) / den;                         \
			}                                                        \
		}                                                          \
	}

#define CDIV_RECOVER(T)                                   \
	if (ISNAN(x) && ISNAN(y)) {                             \
		T inf = (T)mcc_dinf();                                \
		if (c == 0 && d == 0 && (!ISNAN(a) || !ISNAN(b))) {   \
			x = COPYSIGN(inf, c) * a;                           \
			y = COPYSIGN(inf, c) * b;                           \
		} else if ((ISINF(a) || ISINF(b)) &&                  \
							 ISFINITE(c) && ISFINITE(d)) {              \
			a = COPYSIGN(ISINF(a) ? (T)1 : (T)0, a);            \
			b = COPYSIGN(ISINF(b) ? (T)1 : (T)0, b);            \
			x = inf * (a * c + b * d);                          \
			y = inf * (b * c - a * d);                          \
		} else if ((ISINF(c) || ISINF(d)) &&                  \
							 ISFINITE(a) && ISFINITE(b)) {              \
			c = COPYSIGN(ISINF(c) ? (T)1 : (T)0, c);            \
			d = COPYSIGN(ISINF(d) ? (T)1 : (T)0, d);            \
			x = (T)0 * (a * c + b * d);                         \
			y = (T)0 * (b * c - a * d);                         \
		}                                                     \
	}

#define GEN_DIV(NAME, T, TMAX, TMIN, TEPS)                \
	void NAME(T *res, T a, T b, T c, T d) {                 \
		T x, y;                                               \
		CDIV_SMITH(T, TMAX, TMIN, TEPS)                       \
		CDIV_RECOVER(T)                                       \
		res[0] = x;                                           \
		res[1] = y;                                           \
	}

#define GEN_DIV_WIDE(NAME, T, W)                          \
	void NAME(T *res, T a, T b, T c, T d) {                 \
		W aa = (W)a, bb = (W)b, cc = (W)c, dd = (W)d;         \
		W denom = (cc * cc) + (dd * dd);                      \
		T x = (T)(((aa * cc) + (bb * dd)) / denom);           \
		T y = (T)(((bb * cc) - (aa * dd)) / denom);           \
		CDIV_RECOVER(T)                                       \
		res[0] = x;                                           \
		res[1] = y;                                           \
	}

GEN_MUL(__mcc_cmulf, float)
GEN_MUL(__mcc_cmul, double)
GEN_MUL(__mcc_cmull, long double)

GEN_DIV_WIDE(__mcc_cdivf, float, double)
GEN_DIV(__mcc_cdiv, double, __DBL_MAX__, __DBL_MIN__, __DBL_EPSILON__)
GEN_DIV(__mcc_cdivl, long double, __LDBL_MAX__, __LDBL_MIN__, __LDBL_EPSILON__)

#define GEN_DIV_INT(NAME, T)                     \
	void NAME(T *res, T a, T b, T c, T d) {         \
		T r, den;                                     \
		if ((c < 0 ? (T)-c : c) >= (d < 0 ? (T)-d : d)) { \
			r = d / c;                                   \
			den = c + d * r;                             \
			res[0] = (a + b * r) / den;                  \
			res[1] = (b - a * r) / den;                  \
		} else {                                       \
			r = c / d;                                   \
			den = c * r + d;                             \
			res[0] = (a * r + b) / den;                  \
			res[1] = (b * r - a) / den;                  \
		}                                             \
	}

GEN_DIV_INT(__mcc_cdivi64, long long)
GEN_DIV_INT(__mcc_cdivu64, unsigned long long)
