/* Self-test for the mcccombo wrapper: combinatorial enumeration, codec-pipeline
 * search, and the compressed key->value memo (hits + LFU eviction). Lives outside the
 * globbed src source set (it carries a main()); build with -I src so mcccombo.h
 * resolves. Build: cc -std=c99 -Wall -Wextra -I src src/algorithms/combo_selftest.c */
#include "mcccombo.h"
#include <stdio.h>
#include <string.h>

static int fails, checks;
#define CHECK(cond, msg)                                                              \
	do {                                                                               \
		checks++;                                                                        \
		if (!(cond)) {                                                                    \
			printf("FAIL: %s\n", msg);                                                      \
			fails++;                                                                        \
		} else                                                                           \
			printf("ok   %s\n", msg);                                                       \
	} while (0)

/* A toy score for combo_run: prefer the smallest-index subset of a given size. The
 * score is the sum of selected indices, so the best k=2 combination is {0,1}=1. */
static long sum_score(const int *sel, int k, void *user) {
	long s = 0;
	int i;
	(void)user;
	for (i = 0; i < k; i++)
		s += sel[i];
	return s;
}

static void test_enumerate(void) {
	ComboSpec spec;
	ComboBest best;
	spec.nitems = 5;
	spec.min_k = 2;
	spec.max_k = 2;
	spec.ordered = 0;
	spec.budget = 0;
	spec.score = sum_score;
	spec.user = NULL;
	CHECK(combo_run(&spec, &best), "combo_run finds a best");
	CHECK(best.k == 2 && best.score == 1, "best k=2 combination is {0,1} (score 1)");
	CHECK(best.evaluated == 10, "C(5,2) = 10 combinations enumerated");
	CHECK(best.exhausted, "space exhausted within budget");

	/* Permutations: 3 items, all orderings of the full set = 3! = 6 candidates. */
	spec.nitems = 3;
	spec.min_k = 3;
	spec.max_k = 3;
	spec.ordered = 1;
	combo_run(&spec, &best);
	CHECK(best.evaluated == 6, "ordered full-set enumeration visits 3! = 6 permutations");

	/* Budget cuts the search short and flags non-exhaustion. */
	spec.nitems = 5;
	spec.min_k = 1;
	spec.max_k = 5;
	spec.ordered = 1;
	spec.budget = 7;
	combo_run(&spec, &best);
	CHECK(best.evaluated == 7 && !best.exhausted, "budget bounds the candidate count");
}

static void fill(unsigned char *b, long n, int mode) {
	long i;
	unsigned long r = 0x2545f491u;
	for (i = 0; i < n; i++) {
		if (mode == 0)
			b[i] = (unsigned char)('A' + (i % 7)); /* repetitive text */
		else if (mode == 1)
			b[i] = 0x5a; /* one long run */
		else {
			r = r * 1103515245u + 12345u;
			b[i] = (unsigned char)(r >> 16); /* pseudo-random */
		}
	}
}

static void test_pipeline(void) {
	static unsigned char data[8000], a[40000], b[40000], out[40000];
	unsigned char *comp = NULL, *back = NULL;
	long clen, blen;
	ComboBest best;
	fill(data, sizeof data, 0);
	CHECK(combo_pipeline_search(data, sizeof data, 2, a, b, sizeof a, &best),
				"pipeline search returns a best chain");
	printf("     best chain: ");
	{
		int i;
		for (i = 0; i < best.k; i++)
			printf("%s%s", i ? "->" : "", combo_codecs[best.sel[i]].name);
		printf("  size %ld/%ld (%.0f%%)\n", best.score, (long)sizeof data,
					 100.0 * best.score / (double)sizeof data);
	}
	CHECK(best.score < (long)sizeof data, "best chain actually compresses text");
	/* Round-trip the winning chain. */
	clen = combo_pipe_apply(best.sel, best.k, data, sizeof data, a, b, sizeof a, &comp);
	CHECK(clen == best.score, "apply reproduces the searched size");
	memcpy(out, comp, (size_t)clen); /* detach before reusing a/b for decode */
	blen = combo_pipe_unapply(best.sel, best.k, out, clen, a, b, sizeof a, &back);
	CHECK(blen == (long)sizeof data && memcmp(back, data, sizeof data) == 0,
				"pipeline round-trips exactly");
}

static void test_memo(void) {
	static ComboMemo m;
	static unsigned char val[2000], got[2000];
	combo_u64 k0, k1;
	long r;
	int code;
	combo_memo_init(&m, 1ull << 20); /* 1 MiB cap */

	fill(val, sizeof val, 0);
	k0 = combo_hash(val, sizeof val);
	code = combo_memo_put(&m, k0, val, sizeof val);
	CHECK(code >= 0, "memo_put stores a value");
	CHECK(m.rec[0].vlen < (long)sizeof val, "stored value is compressed");
	printf("     value %ld -> %ld bytes via %s\n", (long)sizeof val, m.rec[0].vlen,
				 code == COMBO_STORED ? "stored" : combo_codecs[code].name);

	r = combo_memo_get(&m, k0, got, sizeof got);
	CHECK(r == (long)sizeof val && memcmp(got, val, sizeof val) == 0,
				"cache hit decompresses to the exact value");
	CHECK(combo_memo_get(&m, k0 ^ 0xdeadbeefULL, got, sizeof got) == -1,
				"cache miss on an unknown key");

	/* A second distinct key coexists; both refcounts survive. */
	fill(val, sizeof val, 1);
	k1 = combo_hash(val, sizeof val);
	combo_memo_put(&m, k1, val, sizeof val);
	CHECK(m.n == 2, "two distinct keys held");
	CHECK(combo_memo_get(&m, k1, got, sizeof got) == (long)sizeof val &&
					memcmp(got, val, sizeof val) == 0,
				"second key round-trips too");

	/* LFU eviction. Use incompressible (random) 400-byte values so each stores at ~400
	 * bytes verbatim and the byte math is deterministic: cap 900 holds two, so the
	 * third put must evict exactly one — the least-referenced. ka gets 2 hits, kb/kc
	 * get 0, so kb (first-found minimum) is the one dropped. */
	{
		static ComboMemo tm;
		static unsigned char t[400];
		combo_u64 ka, kb, kc;
		combo_memo_init(&tm, 900);
		fill(t, sizeof t, 2);
		t[0] = 1;
		ka = combo_hash(t, sizeof t);
		combo_memo_put(&tm, ka, t, sizeof t);
		combo_memo_get(&tm, ka, got, sizeof got);
		combo_memo_get(&tm, ka, got, sizeof got); /* ka: 2 refs */
		fill(t, sizeof t, 2);
		t[0] = 2;
		kb = combo_hash(t, sizeof t);
		combo_memo_put(&tm, kb, t, sizeof t); /* kb: 0 refs */
		fill(t, sizeof t, 2);
		t[0] = 3;
		kc = combo_hash(t, sizeof t); /* third put pushes past the cap */
		combo_memo_put(&tm, kc, t, sizeof t);
		CHECK(tm.n == 2, "cap holds two of three incompressible values");
		CHECK(tm.bytes <= tm.cap_bytes, "memo stays under the byte cap after eviction");
		CHECK(combo_memo_find(&tm, ka) >= 0, "most-referenced key survived eviction");
		CHECK(combo_memo_find(&tm, kb) < 0, "least-referenced key was evicted");
	}
}

int main(void) {
	test_enumerate();
	test_pipeline();
	test_memo();
	printf("\n%d checks, %d failures\n", checks, fails);
	return fails ? 1 : 0;
}
