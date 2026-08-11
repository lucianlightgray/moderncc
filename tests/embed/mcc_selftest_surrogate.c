#include <stdio.h>
#include <stdlib.h>

#include "src/mccsurro.h"

static int fails;

static void ck(int ok, const char *what) {
	printf("mcc-selftest-surrogate: %-58s %s\n", what, ok ? "OK" : "FAIL");
	if (!ok)
		fails++;
}

static long deg2_truth(uint32_t m) {
	int s0 = surro_spin(m, 0), s1 = surro_spin(m, 1);
	int s2 = surro_spin(m, 2), s3 = surro_spin(m, 3);
	return 100 + 7 * s0 - 3 * s1 + 5 * s0 * s2 - 2 * s1 * s3;
}

static long pairwise_truth(uint32_t m) {
	long x0 = (m >> 0) & 1u, x1 = (m >> 1) & 1u;
	long x2 = (m >> 2) & 1u, x3 = (m >> 3) & 1u;
	long x4 = (m >> 4) & 1u, x5 = (m >> 5) & 1u;
	return 100 + 6 * x0 + 6 * x1 - 20 * x0 * x1 + 5 * x2 + 5 * x3 -
				 18 * x2 * x3 + 2 * x4 + 4 * x5;
}

static void t_exact_full_design(void) {
	SurroObs obs[16];
	SurroFit f;
	uint32_t m;
	int ok = 1;
	for (m = 0; m < 16; m++) {
		obs[m].mask = m;
		obs[m].score = deg2_truth(m);
	}
	surro_fit(obs, 16, 4, 0u, &f);
	for (m = 0; m < 16; m++)
		if (surro_predict(&f, m) != (int64_t)deg2_truth(m))
			ok = 0;
	ck(ok, "degree-2 fit reproduces a degree-2 landscape exactly");
}

static void t_deg1_cannot_see_interaction(void) {
	SurroObs obs[16];
	SurroFit f;
	uint32_t m;
	int differs = 0;
	for (m = 0; m < 16; m++) {
		obs[m].mask = m;
		obs[m].score = deg2_truth(m);
	}
	surro_fit(obs, 16, 4, 0u, &f);
	for (m = 0; m < 16; m++)
		if (surro_predict_deg1(&f, m) != (int64_t)deg2_truth(m))
			differs = 1;
	ck(differs, "degree-1 fit alone cannot reproduce it (interaction is real)");
}

static const uint32_t g_seen[] = {0u,  1u,  2u,  4u,  8u,  16u,
																	32u, 3u,  12u, 48u};
#define G_SEEN_N ((int)(sizeof g_seen / sizeof g_seen[0]))

static void t_anticipates_unobserved_combination(void) {
	SurroObs obs[G_SEEN_N];
	SurroFit f;
	SurroProp p;
	int i;
	long best_true = 0;
	uint32_t best_mask = 0;
	for (i = 0; i < G_SEEN_N; i++) {
		obs[i].mask = g_seen[i];
		obs[i].score = pairwise_truth(g_seen[i]);
	}
	surro_fit(obs, G_SEEN_N, 6, 0u, &f);
	surro_propose(&f, obs, G_SEEN_N, 0x3fu, &p, 4);

	for (i = 0; i < 64; i++)
		if (!surro_seen(obs, G_SEEN_N, (uint32_t)i))
			if (!best_mask || pairwise_truth((uint32_t)i) < best_true) {
				best_true = pairwise_truth((uint32_t)i);
				best_mask = (uint32_t)i;
			}

	printf("mcc-selftest-surrogate: proposed 0x%02x (true %ld), "
				 "true optimum 0x%02x (%ld)\n",
				 p.n ? p.mask[0] : 0u, p.n ? pairwise_truth(p.mask[0]) : 0L, best_mask,
				 best_true);
	ck(p.n > 0 && p.mask[0] == best_mask,
		 "predicts the best never-evaluated mask by combining two pairs");
}

static void t_deg1_picks_worse(void) {
	SurroObs obs[G_SEEN_N];
	SurroFit f;
	uint32_t m, b1 = 0, b2 = 0;
	int64_t v1 = 0, v2 = 0;
	int i, have1 = 0, have2 = 0;
	for (i = 0; i < G_SEEN_N; i++) {
		obs[i].mask = g_seen[i];
		obs[i].score = pairwise_truth(g_seen[i]);
	}
	surro_fit(obs, G_SEEN_N, 6, 0u, &f);
	for (m = 0; m < 64; m++) {
		int64_t d1, d2;
		if (surro_seen(obs, G_SEEN_N, m))
			continue;
		d1 = surro_predict_deg1(&f, m);
		d2 = surro_predict(&f, m);
		if (!have1 || d1 < v1) { v1 = d1; b1 = m; have1 = 1; }
		if (!have2 || d2 < v2) { v2 = d2; b2 = m; have2 = 1; }
	}
	printf("mcc-selftest-surrogate: deg1 picks 0x%02x (true %ld), "
				 "deg2 picks 0x%02x (true %ld)\n",
				 b1, pairwise_truth(b1), b2, pairwise_truth(b2));
	ck(pairwise_truth(b2) < pairwise_truth(b1),
		 "degree-2 proposal is strictly better than the degree-1 proposal");
}

static void t_beats_linear_enumeration_at_equal_budget(void) {
	SurroObs obs[G_SEEN_N];
	SurroFit f;
	SurroProp p;
	int i, budget = 4;
	long lin_best = 0;
	int lin_have = 0;
	uint32_t m;
	long prop_best = 0;
	int prop_have = 0;

	for (i = 0; i < G_SEEN_N; i++) {
		obs[i].mask = g_seen[i];
		obs[i].score = pairwise_truth(g_seen[i]);
	}
	surro_fit(obs, G_SEEN_N, 6, 0u, &f);
	surro_propose(&f, obs, G_SEEN_N, 0x3fu, &p, budget);

	for (i = 0; i < p.n; i++)
		if (!prop_have || pairwise_truth(p.mask[i]) < prop_best) {
			prop_best = pairwise_truth(p.mask[i]);
			prop_have = 1;
		}

	for (m = 0, i = 0; m < 64 && i < budget; m++) {
		if (surro_seen(obs, G_SEEN_N, m))
			continue;
		if (!lin_have || pairwise_truth(m) < lin_best) {
			lin_best = pairwise_truth(m);
			lin_have = 1;
		}
		i++;
	}
	printf("mcc-selftest-surrogate: at %d evaluations -- predicted best %ld, "
				 "linear-enumeration best %ld\n",
				 budget, prop_best, lin_best);
	ck(prop_have && lin_have && prop_best < lin_best,
		 "at equal evaluation budget it beats linear enumeration");
}

static void t_order_independent(void) {
	SurroObs a[G_SEEN_N], b[G_SEEN_N];
	SurroFit fa, fb;
	SurroProp pa, pb;
	int i, ok = 1;
	for (i = 0; i < G_SEEN_N; i++) {
		a[i].mask = g_seen[i];
		a[i].score = pairwise_truth(g_seen[i]);
		b[G_SEEN_N - 1 - i].mask = g_seen[i];
		b[G_SEEN_N - 1 - i].score = pairwise_truth(g_seen[i]);
	}
	surro_fit(a, G_SEEN_N, 6, 0u, &fa);
	surro_fit(b, G_SEEN_N, 6, 0u, &fb);
	surro_propose(&fa, a, G_SEEN_N, 0x3fu, &pa, 4);
	surro_propose(&fb, b, G_SEEN_N, 0x3fu, &pb, 4);
	if (pa.n != pb.n)
		ok = 0;
	for (i = 0; i < pa.n && ok; i++)
		if (pa.mask[i] != pb.mask[i] || pa.pred[i] != pb.pred[i])
			ok = 0;
	ck(ok, "proposals are independent of observation order (deterministic)");
}

static void t_no_float_in_ranking(void) {
	SurroObs obs[4];
	SurroFit f;
	obs[0].mask = 0u; obs[0].score = 1000000000L;
	obs[1].mask = 1u; obs[1].score = 1000000001L;
	obs[2].mask = 2u; obs[2].score = 1000000002L;
	obs[3].mask = 3u; obs[3].score = 1000000003L;
	surro_fit(obs, 4, 2, 0u, &f);
	ck(surro_predict(&f, 0u) == (int64_t)1000000000L &&
				 surro_predict(&f, 3u) == (int64_t)1000000003L,
		 "reconstruction is exact integer arithmetic at large scores");
}

static void t_axis_vertex(void) {
	long v = 0;
	int got = surro_axis_vertex(10, 3 * 27 * 27 + 11, 50, 3 * 13 * 13 + 11, 90,
															3 * 53 * 53 + 11, 0, 160, &v);
	ck(got && v == 37, "parabolic axis projection recovers the vertex exactly");

	got = surro_axis_vertex(0, 0, 10, 100, 20, 200, 0, 160, &v);
	ck(!got, "collinear axis samples are refused, not extrapolated");

	got = surro_axis_vertex(0, 200, 10, 100, 20, 0, 0, 160, &v);
	ck(!got, "a concave axis is refused, not extrapolated");

	got = surro_axis_vertex(100, 11, 120, 1211, 140, 4811, 0, 160, &v);
	ck(got && v == 100, "an out-of-range vertex is clamped to the axis domain");
}

int main(void) {
	printf("mcc-selftest-surrogate: begin (Walsh degree-2 gate surrogate)\n");
	t_exact_full_design();
	t_deg1_cannot_see_interaction();
	t_anticipates_unobserved_combination();
	t_deg1_picks_worse();
	t_beats_linear_enumeration_at_equal_budget();
	t_order_independent();
	t_no_float_in_ranking();
	t_axis_vertex();
	printf("mcc-selftest-surrogate: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
				 fails, fails == 1 ? "" : "s");
	return fails ? 1 : 0;
}
