#include "mcccombo.h"
#include <stdio.h>
#include <string.h>

#ifndef MCC_TRACE
#define MCC_TRACE(...) ((void)0)
#endif

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

static long sum_score(const int *sel, int k, void *user) { MCC_TRACE("enter\n");
	long s = 0;
	int i;
	(void)user;
	for (i = 0; i < k; i++)
		{ MCC_TRACE("br\n"); s += sel[i]; }
	return s;
}

static void test_enumerate(void) { MCC_TRACE("enter\n");
	ComboSpec spec;
	ComboBest best;
	spec.nitems = 5;
	spec.min_k = 2;
	spec.max_k = 2;
	spec.ordered = 0;
	spec.walk = COMBO_WALK_LINEAR;
	spec.budget = 0;
	spec.score = sum_score;
	spec.visit = NULL;
	spec.user = NULL;
	CHECK(combo_run(&spec, &best), "combo_run finds a best");
	CHECK(best.k == 2 && best.score == 1, "best k=2 combination is {0,1} (score 1)");
	CHECK(best.evaluated == 10, "C(5,2) = 10 combinations enumerated");
	CHECK(best.exhausted, "space exhausted within budget");

	spec.nitems = 3;
	spec.min_k = 3;
	spec.max_k = 3;
	spec.ordered = 1;
	combo_run(&spec, &best);
	CHECK(best.evaluated == 6, "ordered full-set enumeration visits 3! = 6 permutations");

	spec.nitems = 5;
	spec.min_k = 1;
	spec.max_k = 5;
	spec.ordered = 1;
	spec.budget = 7;
	combo_run(&spec, &best);
	CHECK(best.evaluated == 7 && !best.exhausted, "budget bounds the candidate count");
}

static void fill(unsigned char *b, long n, int mode) { MCC_TRACE("enter\n");
	long i;
	unsigned long r = 0x2545f491u;
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		if (mode == 0)
			{ MCC_TRACE("br\n"); b[i] = (unsigned char)('A' + (i % 7)); }
		else if (mode == 1)
			{ MCC_TRACE("br\n"); b[i] = 0x5a; }
		else { MCC_TRACE("br\n");
			r = r * 1103515245u + 12345u;
			b[i] = (unsigned char)(r >> 16);
		}
	}
}

static void test_pipeline(void) { MCC_TRACE("enter\n");
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
			{ MCC_TRACE("br\n"); printf("%s%s", i ? "->" : "", combo_codecs[best.sel[i]].name); }
		printf("  size %ld/%ld (%.0f%%)\n", best.score, (long)sizeof data,
					 100.0 * best.score / (double)sizeof data);
	}
	CHECK(best.score < (long)sizeof data, "best chain actually compresses text");
	clen = combo_pipe_apply(best.sel, best.k, data, sizeof data, a, b, sizeof a, &comp);
	CHECK(clen == best.score, "apply reproduces the searched size");
	memcpy(out, comp, (size_t)clen);
	blen = combo_pipe_unapply(best.sel, best.k, out, clen, a, b, sizeof a, &back);
	CHECK(blen == (long)sizeof data && memcmp(back, data, sizeof data) == 0,
				"pipeline round-trips exactly");
}

static void test_memo(void) { MCC_TRACE("enter\n");
	static ComboMemo m;
	static unsigned char val[2000], got[2000];
	combo_u64 k0, k1;
	long r;
	int code;
	combo_memo_init(&m, 1ull << 20);

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

	fill(val, sizeof val, 1);
	k1 = combo_hash(val, sizeof val);
	combo_memo_put(&m, k1, val, sizeof val);
	CHECK(m.n == 2, "two distinct keys held");
	CHECK(combo_memo_get(&m, k1, got, sizeof got) == (long)sizeof val &&
					memcmp(got, val, sizeof val) == 0,
				"second key round-trips too");

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
		combo_memo_get(&tm, ka, got, sizeof got);
		fill(t, sizeof t, 2);
		t[0] = 2;
		kb = combo_hash(t, sizeof t);
		combo_memo_put(&tm, kb, t, sizeof t);
		fill(t, sizeof t, 2);
		t[0] = 3;
		kc = combo_hash(t, sizeof t);
		combo_memo_put(&tm, kc, t, sizeof t);
		CHECK(tm.n == 2, "cap holds two of three incompressible values");
		CHECK(tm.bytes <= tm.cap_bytes, "memo stays under the byte cap after eviction");
		CHECK(combo_memo_find(&tm, ka) >= 0, "most-referenced key survived eviction");
		CHECK(combo_memo_find(&tm, kb) < 0, "least-referenced key was evicted");
	}
}

int main(void) { MCC_TRACE("enter\n");
	test_enumerate();
	test_pipeline();
	test_memo();
	printf("\n%d checks, %d failures\n", checks, fails);
	return fails ? 1 : 0;
}
