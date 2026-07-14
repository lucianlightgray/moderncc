#ifndef MCC_STATS_H
#define MCC_STATS_H

#include <stdint.h>

enum {
	MCC_STATS_JIT = 2,
	MCC_STATS_STRATEGY = 4,
	MCC_STATS_COMBO = 8,
	MCC_STATS_SEARCH = 16,
	MCC_STATS_ALL = 0x7ffffffeu
};

extern unsigned mcc_stats_mask;

static inline int mcc_stats_on(unsigned cat) {
	return (mcc_stats_mask & cat) != 0;
}

void mcc_stats_enable(unsigned mask);
void mcc_stats_env_init(void);
void mcc_stats_finish(void);

void mcc_stats_search_begin(const char *func, uint64_t hash, uint64_t base,
													 uint64_t searchable, int nitems, int walk,
													 int ordered);
void mcc_stats_combo_cand(uint64_t gates, const int *sel, int k,
													const uint64_t *item_bits, long score, long evaluated,
													unsigned elapsed_ms, unsigned budget_ms,
													unsigned expect_ms);
void mcc_stats_search_end(uint64_t best_gates, long best_score, long evaluated,
													int memo_n);
void mcc_stats_strat_hits(const int *sf, int n);

void mcc_stats_jit_recompile(void);
void mcc_stats_jit_kgc_hit(void);
void mcc_stats_jit_kgc_miss(void);
void mcc_stats_jit_poison(void);
void mcc_stats_jit_promote(int async);

#endif
