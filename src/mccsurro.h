#ifndef MCC_SURRO_H
#define MCC_SURRO_H

#include <stdint.h>
#include <string.h>

#define SURRO_MAXN 16
#define SURRO_MAXOBS 256
#define SURRO_MAXPROP 16
#define SURRO_ENUM_MAXN 12

typedef struct SurroObs {
	uint32_t mask;
	long score;
} SurroObs;

typedef struct SurroFit {
	int n;
	int nobs;
	int have_base;
	uint32_t base;
	int64_t fbase;
	int known1;
	int known2;
	unsigned char have1[SURRO_MAXN];
	int64_t d1[SURRO_MAXN];
	unsigned char have2[SURRO_MAXN][SURRO_MAXN];
	int64_t d2[SURRO_MAXN][SURRO_MAXN];
} SurroFit;

static int surro_spin(uint32_t mask, int i) {
	return (mask >> i) & 1u ? -1 : 1;
}

static int surro_lookup(const SurroObs *obs, int nobs, uint32_t mask,
												int64_t *out) {
	int k;
	for (k = 0; k < nobs; k++)
		if (obs[k].mask == mask) {
			*out = (int64_t)obs[k].score;
			return 1;
		}
	return 0;
}

static void surro_fit(const SurroObs *obs, int nobs, int n, uint32_t base,
											SurroFit *f) {
	int i, j;
	memset(f, 0, sizeof *f);
	if (n > SURRO_MAXN)
		n = SURRO_MAXN;
	if (nobs > SURRO_MAXOBS)
		nobs = SURRO_MAXOBS;
	f->n = n;
	f->nobs = nobs;
	f->base = base;
	if (!surro_lookup(obs, nobs, base, &f->fbase))
		return;
	f->have_base = 1;
	for (i = 0; i < n; i++) {
		int64_t y;
		if (!surro_lookup(obs, nobs, base ^ ((uint32_t)1 << i), &y))
			continue;
		f->have1[i] = 1;
		f->d1[i] = y - f->fbase;
		f->known1++;
	}
	for (i = 0; i < n; i++)
		for (j = i + 1; j < n; j++) {
			int64_t y;
			if (!f->have1[i] || !f->have1[j])
				continue;
			if (!surro_lookup(obs, nobs,
												base ^ ((uint32_t)1 << i) ^ ((uint32_t)1 << j), &y))
				continue;
			f->have2[i][j] = 1;
			f->d2[i][j] = y - f->fbase - f->d1[i] - f->d1[j];
			f->known2++;
		}
}

static int64_t surro_predict_deg1(const SurroFit *f, uint32_t mask) {
	uint32_t s = mask ^ f->base;
	int64_t v = f->fbase;
	int i;
	for (i = 0; i < f->n; i++)
		if ((s >> i) & 1u)
			v += f->d1[i];
	return v;
}

static int64_t surro_predict(const SurroFit *f, uint32_t mask) {
	uint32_t s = mask ^ f->base;
	int64_t v = f->fbase;
	int i, j;
	for (i = 0; i < f->n; i++) {
		if (!((s >> i) & 1u))
			continue;
		v += f->d1[i];
		for (j = i + 1; j < f->n; j++)
			if ((s >> j) & 1u)
				v += f->d2[i][j];
	}
	return v;
}

static int surro_seen(const SurroObs *obs, int nobs, uint32_t mask) {
	int k;
	for (k = 0; k < nobs; k++)
		if (obs[k].mask == mask)
			return 1;
	return 0;
}

typedef struct SurroProp {
	uint32_t mask[SURRO_MAXPROP];
	int64_t pred[SURRO_MAXPROP];
	int n;
} SurroProp;

static void surro_prop_offer(SurroProp *p, int want, uint32_t mask,
														 int64_t pred) {
	int i, j;
	for (i = 0; i < p->n; i++)
		if (p->mask[i] == mask)
			return;
	for (i = 0; i < p->n; i++)
		if (pred < p->pred[i] || (pred == p->pred[i] && mask < p->mask[i]))
			break;
	if (i >= want)
		return;
	if (p->n < want)
		p->n++;
	for (j = p->n - 1; j > i; j--) {
		p->mask[j] = p->mask[j - 1];
		p->pred[j] = p->pred[j - 1];
	}
	p->mask[i] = mask;
	p->pred[i] = pred;
}

static void surro_greedy(const SurroFit *f, const SurroObs *obs, int nobs,
												 uint32_t allowed, uint32_t start, SurroProp *out,
												 int want) {
	uint32_t cur = start & allowed;
	int guard;
	for (guard = 0; guard <= f->n; guard++) {
		uint32_t best = cur;
		int64_t bestv = surro_predict(f, cur);
		int i;
		for (i = 0; i < f->n; i++) {
			uint32_t cand;
			int64_t v;
			if (!((allowed >> i) & 1u))
				continue;
			cand = cur ^ ((uint32_t)1 << i);
			v = surro_predict(f, cand);
			if (!surro_seen(obs, nobs, cand))
				surro_prop_offer(out, want, cand, v);
			if (v < bestv || (v == bestv && cand < best)) {
				bestv = v;
				best = cand;
			}
		}
		if (best == cur)
			break;
		cur = best;
	}
}

static int surro_propose(const SurroFit *f, const SurroObs *obs, int nobs,
												 uint32_t allowed, SurroProp *out, int want) {
	out->n = 0;
	if (want > SURRO_MAXPROP)
		want = SURRO_MAXPROP;
	if (want <= 0 || f->n <= 0 || !f->have_base)
		return 0;
	if (f->n <= SURRO_ENUM_MAXN) {
		uint32_t lim = (uint32_t)1 << f->n;
		uint32_t m;
		for (m = 0; m < lim; m++) {
			if (m & ~allowed)
				continue;
			if (surro_seen(obs, nobs, m))
				continue;
			surro_prop_offer(out, want, m, surro_predict(f, m));
		}
	} else {
		surro_greedy(f, obs, nobs, allowed, f->base, out, want);
		surro_greedy(f, obs, nobs, allowed, 0, out, want);
		surro_greedy(f, obs, nobs, allowed, allowed, out, want);
	}
	return out->n;
}

static int surro_axis_vertex(long x0, long y0, long x1, long y1, long x2,
														 long y2, long lo, long hi, long *out) {
	long h = x1 - x0;
	int64_t den, num, step;
	if (h <= 0 || x2 - x1 != h)
		return 0;
	den = (int64_t)y0 - 2 * (int64_t)y1 + (int64_t)y2;
	if (den <= 0)
		return 0;
	num = (int64_t)y0 - (int64_t)y2;
	step = num * h;
	if (step >= 0)
		step = (step + den) / (2 * den);
	else
		step = -((-step + den) / (2 * den));
	*out = x1 + (long)step;
	if (*out < lo)
		*out = lo;
	if (*out > hi)
		*out = hi;
	return 1;
}

#endif
