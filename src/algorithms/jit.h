#ifndef MCC_JIT_H
#define MCC_JIT_H

#include <stddef.h>
#include <stdint.h>

#ifndef MCC_JIT_INLINE
#define MCC_JIT_INLINE static inline
#endif

typedef struct JitGraduatedRecord {
	uint64_t intention_hash;
	uint64_t gate_mask;
	int32_t score;
	uint64_t salt;
} JitGraduatedRecord;

static const JitGraduatedRecord jit_graduated_table[] = {
		{0, 0, 0, 0},
};

#define JIT_GRADUATED_SENTINELS 1
#define JIT_GRADUATED_COUNT                                          \
	((int)(sizeof jit_graduated_table / sizeof jit_graduated_table[0]) \
	 - JIT_GRADUATED_SENTINELS)

MCC_JIT_INLINE const JitGraduatedRecord *
jit_graduated_find_in(const JitGraduatedRecord *tab, int n, uint64_t intention_hash,
											uint64_t salt) {
	int i;
	if (intention_hash == 0)
		return NULL;
	for (i = 0; i < n; i++)
		if (tab[i].intention_hash == intention_hash && tab[i].salt == salt)
			return &tab[i];
	return NULL;
}

MCC_JIT_INLINE const JitGraduatedRecord *
jit_graduated_find(uint64_t intention_hash, uint64_t salt) {
	return jit_graduated_find_in(jit_graduated_table, JIT_GRADUATED_COUNT,
															 intention_hash, salt);
}

#ifdef JIT_H_SELFTEST
#include <stdio.h>
int main(void) {
	int fails = 0;
	const JitGraduatedRecord seed[] = {
			{0x1111111111111111ull, 0x5ull, 42, 0xABCDABCDABCDABCDull},
			{0x2222222222222222ull, 0x9ull, 7, 0xABCDABCDABCDABCDull},
			{0, 0, 0, 0},
	};
	int seedn = (int)(sizeof seed / sizeof seed[0]) - JIT_GRADUATED_SENTINELS;
	const JitGraduatedRecord *r;

	if (JIT_GRADUATED_COUNT != 0) {
		printf("FAIL: shipped table not empty (count=%d)\n", JIT_GRADUATED_COUNT);
		fails++;
	}
	if (jit_graduated_find(0x1234ull, 0xabcdull) != NULL) {
		printf("FAIL: find against empty shipped table returned non-NULL\n");
		fails++;
	}
	if (jit_graduated_find(0, 0) != NULL) {
		printf("FAIL: zero intention-hash must never match\n");
		fails++;
	}
	r = jit_graduated_find_in(seed, seedn, 0x2222222222222222ull,
														0xABCDABCDABCDABCDull);
	if (!r || r->score != 7 || r->gate_mask != 0x9ull) {
		printf("FAIL: hit lookup did not return the seeded record\n");
		fails++;
	}
	if (jit_graduated_find_in(seed, seedn, 0x2222222222222222ull, 0xDEADull) != NULL) {
		printf("FAIL: salt mismatch must reject an otherwise-matching hash\n");
		fails++;
	}
	if (jit_graduated_find_in(seed, seedn, 0x9999999999999999ull,
														0xABCDABCDABCDABCDull) != NULL) {
		printf("FAIL: absent hash must not match\n");
		fails++;
	}
	if (jit_graduated_find_in(seed, seedn, 0, 0xABCDABCDABCDABCDull) != NULL) {
		printf("FAIL: zero hash against seeded table must not match\n");
		fails++;
	}
	if (fails) {
		printf("jit.h selftest: %d failure(s)\n", fails);
		return 1;
	}
	printf("jit.h selftest OK\n");
	return 0;
}
#endif

#endif /* MCC_JIT_H */
