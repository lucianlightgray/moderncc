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
/* base_score is the cost of the baseline (pre-search) config for this function, or
   -1 when the search path did not score it; it lets the panel report how much cost
   the search actually removed (WINNER cost vs. where it started), not just activity. */
void mcc_stats_search_end(uint64_t best_gates, long best_score, long base_score,
													long evaluated, int memo_n);
/* One function entering the gate-search driver (before any memo/budget early-out):
   the denominator for coverage — searched+memoized vs. eligible. */
void mcc_stats_search_enter(void);
/* One enumerated perm/combo candidate: improved=new running best, rejected=invalid
   score, ordered=permutation mode (else combination/subset mode). */
void mcc_stats_combo_outcome(int improved, int rejected, int ordered);
/* One per-function search-memo lookup: hit=served from memo, else a fresh search. */
void mcc_stats_search_memo(int hit);
void mcc_stats_strat_hits(const int *sf, int n);

void mcc_stats_jit_recompile(void);
void mcc_stats_jit_kgc_hit(void);
void mcc_stats_jit_kgc_miss(void);
void mcc_stats_jit_poison(void);
/* K-patch near-match: one recorded (input->baseline) correction, and one variant
   accepted via near-match (kept + patched instead of poisoned). */
void mcc_stats_jit_kgc_correction(void);
void mcc_stats_jit_nearmatch(void);
void mcc_stats_jit_promote(int async);
void mcc_stats_jit_memo(unsigned long tuples, unsigned long raw_bytes,
											 unsigned long comp_bytes);
void mcc_stats_jit_specfold(int folds);
void mcc_stats_jit_kgc_stub(void);

/* Runtime JIT dispatch outcome for one boot/hot swap attempt. */
enum {
	MCC_JIT_OUT_SWAPPED = 0, /* variant installed into the slot */
	MCC_JIT_OUT_REFUSED,		 /* built but KGC-unverifiable, kept AOT */
	MCC_JIT_OUT_KEPT_AOT,		 /* no variant, kept the AOT init */
	MCC_JIT_OUT_BUDGET_SKIP, /* over time budget before compiling */
	MCC_JIT_OUT_OVER_BUDGET	 /* compiled but over budget, dropped */
};
void mcc_stats_jit_outcome(int outcome);
void mcc_stats_jit_compile(unsigned ms);              /* one recompile, wall ms */
void mcc_stats_jit_capture(unsigned long intent_bytes); /* one fn stashed for JIT */
void mcc_stats_jit_gsearch(long cands, long admits, int budget_hit,
													 uint64_t best_mask);
void mcc_stats_jit_kgc_warm(unsigned long tuples);   /* persisted memo reloaded */
void mcc_stats_jit_hot(void);                         /* fn crossed hot threshold */
void mcc_stats_jit_prof_spec(void);                   /* profile-const speculation */

/* Per-iteration strategy deltas for one committed optimizer fixpoint cycle.
   `delta` is a snapshot of how many times each strategy fired in this pass only
   (indexed like mccstats_strat_name / the AST_STRAT_* enum); `iter` is 1-based.
   Attributes constant producers (bfold/cprop/sccp) to the downstream consumers
   (jt/dse/divmagic/... in iter>1) their new constants unlocked. */
void mcc_stats_fold_cycle(const int *delta, int n, int iter);
void mcc_stats_set_flush_hook(void (*fn)(void));

#endif
