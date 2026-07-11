#ifndef MCCFORECAST_H
#define MCCFORECAST_H

/*
 * One-step-ahead time-series forecasting ensemble, self-contained (no libm) so it
 * works in every build including the asttool harness. Each predictor takes a short
 * sample vector y[0..n-1] (chronological) and predicts y[n]. The driver
 * ast_fc_forecast scores every predictor's online one-step accuracy across the
 * window, keeps the three most accurate, and returns the one whose next-step
 * prediction is closest to their consensus (median) — the least-outlier of the
 * best three. Used by the -O4+ search to predict the next tick's duration
 * (mccast.c) and exposed to the -ffold-math optimizer (mccgen.c).
 *
 * Predictors: random walk, simple exponential smoothing, AR(1), linear regression
 * in time, penalized (ridge) spline, GAM with a truncated-power spline basis,
 * Bayesian dynamic linear model (local-level Kalman / BSTS), Bayesian ridge
 * regression, Gaussian-process regression (RBF kernel), gradient-boosted stumps on
 * a lag feature, Holt linear trend, Theil-Sen robust regression, moving median.
 */

#include <stdint.h>

#define AST_FC_MAXN 16

static double ast_fc_fabs(double x) { return x < 0 ? -x : x; }

static int ast_fc_finite(double x) {
	return x == x && x < 1e18 && x > -1e18;
}

/* self-contained exp, adequate over the RBF-kernel range (x <= 0 in practice). */
static double ast_fc_exp(double x) {
	double u, e;
	int i;
	if (x != x)
		return 1.0;
	if (x > 60.0)
		x = 60.0;
	if (x < -60.0)
		return 0.0;
	u = x / 64.0;
	e = 1.0 + u * (1.0 + u * (0.5 + u * ((1.0 / 6.0) + u * (1.0 / 24.0))));
	for (i = 0; i < 6; i++)
		e *= e; /* (exp(x/64))^64 = exp(x) */
	return e;
}

static void ast_fc_sort(double *a, int n) {
	int i, j;
	for (i = 1; i < n; i++) {
		double v = a[i];
		for (j = i - 1; j >= 0 && a[j] > v; j--)
			a[j + 1] = a[j];
		a[j + 1] = v;
	}
}

static double ast_fc_median(const double *y, int n) {
	double t[AST_FC_MAXN];
	int i;
	if (n <= 0)
		return 0.0;
	if (n > AST_FC_MAXN)
		n = AST_FC_MAXN;
	for (i = 0; i < n; i++)
		t[i] = y[i];
	ast_fc_sort(t, n);
	return (n & 1) ? t[n / 2] : 0.5 * (t[n / 2 - 1] + t[n / 2]);
}

static double ast_fc_mean(const double *y, int n) {
	double s = 0;
	int i;
	if (n <= 0)
		return 0.0;
	for (i = 0; i < n; i++)
		s += y[i];
	return s / n;
}

static double ast_fc_var(const double *y, int n) {
	double mu = ast_fc_mean(y, n), s = 0;
	int i;
	if (n < 2)
		return 0.0;
	for (i = 0; i < n; i++) {
		double d = y[i] - mu;
		s += d * d;
	}
	return s / (n - 1);
}

/* Gaussian elimination with partial pivoting; A row-major m x m (m <= 16). Solves
 * A x = b; returns 0 if (near-)singular. Destroys A and b. */
static int ast_fc_solve(double *A, double *b, int m, double *x) {
	int c, r, k;
	for (c = 0; c < m; c++) {
		int best = c;
		double bestv = ast_fc_fabs(A[c * m + c]);
		for (r = c + 1; r < m; r++) {
			double v = ast_fc_fabs(A[r * m + c]);
			if (v > bestv) {
				bestv = v;
				best = r;
			}
		}
		if (bestv < 1e-12)
			return 0;
		if (best != c) {
			for (k = 0; k < m; k++) {
				double tmp = A[c * m + k];
				A[c * m + k] = A[best * m + k];
				A[best * m + k] = tmp;
			}
			{
				double tb = b[c];
				b[c] = b[best];
				b[best] = tb;
			}
		}
		for (r = c + 1; r < m; r++) {
			double f = A[r * m + c] / A[c * m + c];
			for (k = c; k < m; k++)
				A[r * m + k] -= f * A[c * m + k];
			b[r] -= f * b[c];
		}
	}
	for (r = m - 1; r >= 0; r--) {
		double s = b[r];
		for (k = r + 1; k < m; k++)
			s -= A[r * m + k] * x[k];
		x[r] = s / A[r * m + r];
	}
	return 1;
}

static void ast_fc_line(const double *y, int n, double *a, double *b) {
	double st = 0, sy = 0, stt = 0, sty = 0, d;
	int i;
	for (i = 0; i < n; i++) {
		st += i;
		sy += y[i];
		stt += (double)i * i;
		sty += (double)i * y[i];
	}
	d = (double)n * stt - st * st;
	if (ast_fc_fabs(d) < 1e-12) {
		*a = ast_fc_mean(y, n);
		*b = 0;
		return;
	}
	*b = ((double)n * sty - st * sy) / d;
	*a = (sy - *b * st) / n;
}

static double ast_fc_rw(const double *y, int n) {
	return n > 0 ? y[n - 1] : 0.0;
}

static double ast_fc_ses(const double *y, int n) {
	double a = 0.5, s;
	int i;
	if (n <= 0)
		return 0.0;
	s = y[0];
	for (i = 1; i < n; i++)
		s = a * y[i] + (1 - a) * s;
	return s;
}

static double ast_fc_ar1(const double *y, int n) {
	double sx = 0, sy = 0, sxx = 0, sxy = 0, d, phi, c;
	int i, m = n - 1;
	if (n < 3)
		return ast_fc_rw(y, n);
	for (i = 1; i < n; i++) {
		sx += y[i - 1];
		sy += y[i];
		sxx += y[i - 1] * y[i - 1];
		sxy += y[i - 1] * y[i];
	}
	d = (double)m * sxx - sx * sx;
	if (ast_fc_fabs(d) < 1e-12)
		return ast_fc_mean(y, n);
	phi = ((double)m * sxy - sx * sy) / d;
	c = (sy - phi * sx) / m;
	return c + phi * y[n - 1];
}

static double ast_fc_lin(const double *y, int n) {
	double a, b;
	if (n < 2)
		return ast_fc_rw(y, n);
	ast_fc_line(y, n, &a, &b);
	return a + b * n;
}

/* ridge quadratic (penalized spline proxy): minimize ||y - poly2||^2 + lam*c2^2 */
static double ast_fc_pspline(const double *y, int n) {
	double A[9], b[3], x[3], lam = 1.0;
	double s0, s1 = 0, s2 = 0, s3 = 0, s4 = 0, sy = 0, sty = 0, st2y = 0, t;
	int i;
	if (n < 3)
		return ast_fc_lin(y, n);
	s0 = n;
	for (i = 0; i < n; i++) {
		double tt = i, t2 = tt * tt;
		s1 += tt;
		s2 += t2;
		s3 += t2 * tt;
		s4 += t2 * t2;
		sy += y[i];
		sty += tt * y[i];
		st2y += t2 * y[i];
	}
	A[0] = s0; A[1] = s1; A[2] = s2;
	A[3] = s1; A[4] = s2; A[5] = s3;
	A[6] = s2; A[7] = s3; A[8] = s4 + lam;
	b[0] = sy; b[1] = sty; b[2] = st2y;
	if (!ast_fc_solve(A, b, 3, x))
		return ast_fc_lin(y, n);
	t = n;
	return x[0] + x[1] * t + x[2] * t * t;
}

/* GAM: additive truncated-power spline basis [1, t, (t-k1)+, (t-k2)+], ridge on
 * the hinge coefficients. */
static double ast_fc_gam(const double *y, int n) {
	double A[16], b[4], x[4], lam = 1.0, k1, k2, t, ph[4];
	int i, r, c;
	if (n < 4)
		return ast_fc_pspline(y, n);
	k1 = (n - 1) / 3.0;
	k2 = 2.0 * (n - 1) / 3.0;
	for (i = 0; i < 16; i++)
		A[i] = 0;
	for (i = 0; i < 4; i++)
		b[i] = 0;
	for (i = 0; i < n; i++) {
		t = i;
		ph[0] = 1;
		ph[1] = t;
		ph[2] = t > k1 ? t - k1 : 0;
		ph[3] = t > k2 ? t - k2 : 0;
		for (r = 0; r < 4; r++) {
			for (c = 0; c < 4; c++)
				A[r * 4 + c] += ph[r] * ph[c];
			b[r] += ph[r] * y[i];
		}
	}
	A[2 * 4 + 2] += lam;
	A[3 * 4 + 3] += lam;
	if (!ast_fc_solve(A, b, 4, x))
		return ast_fc_pspline(y, n);
	t = n;
	ph[0] = 1;
	ph[1] = t;
	ph[2] = t > k1 ? t - k1 : 0;
	ph[3] = t > k2 ? t - k2 : 0;
	return x[0] * ph[0] + x[1] * ph[1] + x[2] * ph[2] + x[3] * ph[3];
}

/* Bayesian dynamic linear model: local-level (random-walk + noise) Kalman filter;
 * the filtered level is the one-step-ahead forecast (BSTS local level). */
static double ast_fc_bsts(const double *y, int n) {
	double var, q, r, level, P;
	int i;
	if (n < 2)
		return ast_fc_rw(y, n);
	var = ast_fc_var(y, n);
	if (var < 1e-9)
		var = 1e-9;
	q = 0.1 * var;
	r = var;
	level = y[0];
	P = var;
	for (i = 1; i < n; i++) {
		double Ppred = P + q;
		double K = Ppred / (Ppred + r);
		level = level + K * (y[i] - level);
		P = (1 - K) * Ppred;
	}
	return level;
}

/* Bayesian ridge regression: (X'X + lam I) w = X'y, X = [1, t]; posterior mean. */
static double ast_fc_bridge(const double *y, int n) {
	double A[4], b[2], x[2], lam = 1.0, s0, s1 = 0, s2 = 0, sy = 0, sty = 0;
	int i;
	if (n < 2)
		return ast_fc_rw(y, n);
	s0 = n;
	for (i = 0; i < n; i++) {
		double t = i;
		s1 += t;
		s2 += t * t;
		sy += y[i];
		sty += t * y[i];
	}
	A[0] = s0 + lam; A[1] = s1;
	A[2] = s1; A[3] = s2 + lam;
	b[0] = sy; b[1] = sty;
	if (!ast_fc_solve(A, b, 2, x))
		return ast_fc_mean(y, n);
	return x[0] + x[1] * n;
}

/* Gaussian-process regression with an RBF kernel over time; predictive mean at
 * the next index (uses the last m <= 10 samples). */
static double ast_fc_gp(const double *y, int n) {
	double K[100], b[10], alpha[10], mu, var, l, sig, tstar, pred;
	int i, j, m = n, off;
	if (n < 2)
		return ast_fc_rw(y, n);
	if (m > 10)
		m = 10;
	off = n - m;
	mu = ast_fc_mean(y, n);
	var = ast_fc_var(y, n);
	if (var < 1e-9)
		var = 1e-9;
	l = (m - 1) / 2.0;
	if (l < 1)
		l = 1;
	sig = 1e-3 * var + 1e-9;
	for (i = 0; i < m; i++) {
		for (j = 0; j < m; j++) {
			double dt = (double)(i - j);
			K[i * m + j] = ast_fc_exp(-(dt * dt) / (2 * l * l)) + (i == j ? sig : 0);
		}
		b[i] = y[off + i] - mu;
	}
	if (!ast_fc_solve(K, b, m, alpha))
		return ast_fc_mean(y, n);
	tstar = m;
	pred = mu;
	for (i = 0; i < m; i++) {
		double dt = tstar - (double)i;
		pred += ast_fc_exp(-(dt * dt) / (2 * l * l)) * alpha[i];
	}
	return pred;
}

/* Gradient-boosted regression stumps on a lag-1 feature. */
static double ast_fc_gbm(const double *y, int n) {
	double feat[AST_FC_MAXN], targ[AST_FC_MAXN], f[AST_FC_MAXN], res[AST_FC_MAXN];
	double base, predx, px, eta = 0.3;
	int i, r, m = n - 1, off, rounds = 8;
	if (n < 3)
		return ast_fc_rw(y, n);
	if (m > AST_FC_MAXN)
		m = AST_FC_MAXN;
	off = (n - 1) - m;
	for (i = 0; i < m; i++) {
		feat[i] = y[off + i];
		targ[i] = y[off + i + 1];
	}
	base = ast_fc_mean(targ, m);
	for (i = 0; i < m; i++)
		f[i] = base;
	predx = base;
	px = y[n - 1];
	for (r = 0; r < rounds; r++) {
		double bestsse = -1, bestthr = 0, bestlo = 0, besthi = 0;
		int c;
		for (i = 0; i < m; i++)
			res[i] = targ[i] - f[i];
		for (c = 0; c < m; c++) {
			double thr = feat[c], sl = 0, nl = 0, sh = 0, nh = 0, ml, mh, sse = 0;
			for (i = 0; i < m; i++) {
				if (feat[i] <= thr) {
					sl += res[i];
					nl++;
				} else {
					sh += res[i];
					nh++;
				}
			}
			ml = nl > 0 ? sl / nl : 0;
			mh = nh > 0 ? sh / nh : 0;
			for (i = 0; i < m; i++) {
				double p = feat[i] <= thr ? ml : mh, e = res[i] - p;
				sse += e * e;
			}
			if (bestsse < 0 || sse < bestsse) {
				bestsse = sse;
				bestthr = thr;
				bestlo = ml;
				besthi = mh;
			}
		}
		for (i = 0; i < m; i++)
			f[i] += eta * (feat[i] <= bestthr ? bestlo : besthi);
		predx += eta * (px <= bestthr ? bestlo : besthi);
	}
	return predx;
}

/* Holt linear trend (double exponential smoothing). */
static double ast_fc_holt(const double *y, int n) {
	double a = 0.5, bt = 0.3, level, trend;
	int i;
	if (n < 2)
		return ast_fc_rw(y, n);
	level = y[0];
	trend = y[1] - y[0];
	for (i = 1; i < n; i++) {
		double pl = level;
		level = a * y[i] + (1 - a) * (level + trend);
		trend = bt * (level - pl) + (1 - bt) * trend;
	}
	return level + trend;
}

/* Theil-Sen robust regression: median pairwise slope, median intercept. */
static double ast_fc_theilsen(const double *y, int n) {
	double slopes[AST_FC_MAXN * AST_FC_MAXN], inter[AST_FC_MAXN], b, a;
	int i, j, ns = 0, ni;
	if (n < 2)
		return ast_fc_rw(y, n);
	for (i = 0; i < n; i++)
		for (j = i + 1; j < n; j++)
			if (ns < AST_FC_MAXN * AST_FC_MAXN)
				slopes[ns++] = (y[j] - y[i]) / (double)(j - i);
	b = ast_fc_median(slopes, ns);
	ni = n < AST_FC_MAXN ? n : AST_FC_MAXN;
	for (i = 0; i < ni; i++)
		inter[i] = y[i] - b * i;
	a = ast_fc_median(inter, ni);
	return a + b * n;
}

static double ast_fc_movmed(const double *y, int n) {
	return ast_fc_median(y, n);
}

typedef double (*AstFcFn)(const double *y, int n);

static const struct {
	const char *name;
	AstFcFn fn;
} ast_fc_models[] = {
		{"rw", ast_fc_rw},           {"ses", ast_fc_ses},
		{"ar1", ast_fc_ar1},         {"lin", ast_fc_lin},
		{"pspline", ast_fc_pspline}, {"gam", ast_fc_gam},
		{"bsts", ast_fc_bsts},       {"bridge", ast_fc_bridge},
		{"gp", ast_fc_gp},           {"gbm", ast_fc_gbm},
		{"holt", ast_fc_holt},       {"theilsen", ast_fc_theilsen},
		{"movmed", ast_fc_movmed},
};

#define AST_FC_COUNT ((int)(sizeof ast_fc_models / sizeof ast_fc_models[0]))

static double ast_fc_call(int k, const double *y, int n) {
	double v = ast_fc_models[k].fn(y, n);
	if (!ast_fc_finite(v))
		v = n > 0 ? y[n - 1] : 0.0;
	return v;
}

/* Ensemble one-step-ahead forecast of y[n]: score every predictor's online
 * one-step accuracy over the window, take the three most accurate, and return the
 * one whose next-step prediction is nearest the median of the three (least
 * distance to consensus). */
static double ast_fc_forecast(const double *y, int n) {
	double err[32], next[32], three[3], cons, bestd, d;
	int k, s, t, top[3], best;
	if (n <= 0)
		return 0.0;
	if (n == 1)
		return y[0];
	for (k = 0; k < AST_FC_COUNT; k++) {
		next[k] = ast_fc_call(k, y, n);
		err[k] = 0;
		for (t = 1; t < n; t++)
			err[k] += ast_fc_fabs(ast_fc_call(k, y, t) - y[t]);
	}
	for (s = 0; s < 3; s++) {
		int bi = -1;
		for (k = 0; k < AST_FC_COUNT; k++) {
			int used = 0, u;
			for (u = 0; u < s; u++)
				if (top[u] == k)
					used = 1;
			if (!used && (bi < 0 || err[k] < err[bi]))
				bi = k;
		}
		top[s] = bi;
	}
	for (s = 0; s < 3; s++)
		three[s] = next[top[s]];
	cons = ast_fc_median(three, 3);
	best = top[0];
	bestd = ast_fc_fabs(next[top[0]] - cons);
	for (s = 1; s < 3; s++) {
		d = ast_fc_fabs(next[top[s]] - cons);
		if (d < bestd) {
			bestd = d;
			best = top[s];
		}
	}
	return next[best];
}

#endif
