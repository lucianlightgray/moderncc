#include "mccstats.h"

#ifndef MCC_TRACE
#define MCC_TRACE(...) ((void)0)
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if MCC_HOST_POSIX
#include <unistd.h>
#endif

unsigned mcc_stats_mask = 0;

#define MCCSTATS_STRAT_N 20
#define MCCSTATS_GATE_N 42
#define MCCSTATS_SPARK_N 40
#define MCCSTATS_MAXROWS 48
#define MCCSTATS_COLW 240

static const char *const mccstats_strat_name[MCCSTATS_STRAT_N] = {
		"bfold", "ident", "narrow", "cprop", "cse", "ltemp", "ivsr",
		"pre", "licm", "dse", "sccp", "jt", "bf", "range",
		"divmagic", "abs", "reassoc", "sethi", "tco", "inline"};

static const char *const mccstats_gate_name[MCCSTATS_GATE_N] = {
		"templ", "narrow", "bflag", "sethi", "nrwfix", "shleaf", "promo",
		"inline", "nocall", "cprpjn", "csejn", "ltemp", "ivsr", "pre",
		"dsecal", "tcoptr", "csecom", "range", "divmag", "abs", "reasoc",
		"sccpfx", "idconv", "idshft", "idarit", "idbit", "idrel", "idurng",
		"rasoc", "rslsr", "rsrsl", "rmuld", "bsqrt", "bsign", "bround",
		"bmnmx", "nrwc0", "nrwc1", "nrwc2", "nrwc3", "jitdsp", "jitgrd"};

typedef struct McccStats {
	int active;
	int tty;
	int prev_lines;
	unsigned last_paint_ms;

	char func[96];
	uint64_t hash;
	uint64_t base;
	uint64_t searchable;
	int nitems;
	int walk;
	int ordered;

	uint64_t cand_gates;
	char cand_names[160];
	int cand_k;
	long cand_score;

	uint64_t fn_best_gates;
	long fn_best_score;
	uint64_t best_gates;
	long best_score;

	long evaluated;
	long total_evaluated;
	int funcs_searched;
	int memo_n;
	unsigned elapsed_ms;
	unsigned budget_ms;
	unsigned expect_ms;

	long spark[MCCSTATS_SPARK_N];
	int spark_n;
	int spark_head;

	unsigned long strat_hits[MCCSTATS_STRAT_N];
	unsigned long strat_calls;

	unsigned long jit_recompiles;
	unsigned long jit_kgc_hits;
	unsigned long jit_kgc_misses;
	unsigned long jit_poison;
	unsigned long jit_promote_sync;
	unsigned long jit_promote_async;
} McccStats;

static McccStats mcs;

static unsigned mccstats_now_ms(void) { MCC_TRACE("enter\n");
	return (unsigned)((unsigned long long)clock() * 1000ull / CLOCKS_PER_SEC);
}

static const char *mccstats_walk_name(int w) { MCC_TRACE("enter\n");
	switch (w) { MCC_TRACE("br\n");
	case 0:
		return "linear";
	case 1:
		return "dfs";
	case 2:
		return "bfs";
	case 3:
		return "product";
	default:
		return "?";
	}
}

static void mccstats_fmt_u(unsigned long v, char *out, int cap) { MCC_TRACE("enter\n");
	if (v < 100000)
		{ MCC_TRACE("br\n"); snprintf(out, cap, "%lu", v); }
	else if (v < 100000000ul)
		{ MCC_TRACE("br\n"); snprintf(out, cap, "%lu.%luk", v / 1000, (v % 1000) / 100); }
	else
		{ MCC_TRACE("br\n"); snprintf(out, cap, "%lu.%luM", v / 1000000, (v % 1000000) / 100000); }
}

static void mccstats_gate_names(uint64_t g, char *out, int cap) { MCC_TRACE("enter\n");
	int b, p = 0;
	out[0] = '\0';
	for (b = 0; b < MCCSTATS_GATE_N; b++) { MCC_TRACE("br\n");
		if (!(g & ((uint64_t)1 << b)))
			{ MCC_TRACE("br\n"); continue; }
		p += snprintf(out + p, cap - p, "%s%s", p ? "," : "", mccstats_gate_name[b]);
		if (p >= cap - 8) { MCC_TRACE("br\n");
			snprintf(out + p, cap - p, "..");
			break;
		}
	}
}

static void mccstats_spark(char *out, int cap) { MCC_TRACE("enter\n");
	static const char *bars[8] = {"\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83",
																"\xe2\x96\x84", "\xe2\x96\x85", "\xe2\x96\x86",
																"\xe2\x96\x87", "\xe2\x96\x88"};
	long lo = 0, hi = 0;
	int i, first = 1, p = 0;
	if (mcs.spark_n == 0) { MCC_TRACE("br\n");
		out[0] = '\0';
		return;
	}
	for (i = 0; i < mcs.spark_n; i++) { MCC_TRACE("br\n");
		long v = mcs.spark[(mcs.spark_head - mcs.spark_n + i + MCCSTATS_SPARK_N * 2) %
											 MCCSTATS_SPARK_N];
		if (first || v < lo)
			{ MCC_TRACE("br\n"); lo = v; }
		if (first || v > hi)
			{ MCC_TRACE("br\n"); hi = v; }
		first = 0;
	}
	for (i = 0; i < mcs.spark_n; i++) { MCC_TRACE("br\n");
		long v = mcs.spark[(mcs.spark_head - mcs.spark_n + i + MCCSTATS_SPARK_N * 2) %
											 MCCSTATS_SPARK_N];
		int idx = (hi > lo) ? (int)((v - lo) * 7 / (hi - lo)) : 0;
		if (idx < 0)
			{ MCC_TRACE("br\n"); idx = 0; }
		if (idx > 7)
			{ MCC_TRACE("br\n"); idx = 7; }
		p += snprintf(out + p, cap - p, "%s", bars[idx]);
		if (p >= cap - 4)
			{ MCC_TRACE("br\n"); break; }
	}
}

typedef struct McccRows {
	char buf[MCCSTATS_MAXROWS][MCCSTATS_COLW];
	int n;
} McccRows;

static void mccstats_row(McccRows *r, const char *fmt, ...) { MCC_TRACE("enter\n");
	va_list ap;
	if (r->n >= MCCSTATS_MAXROWS)
		{ MCC_TRACE("br\n"); return; }
	va_start(ap, fmt);
	vsnprintf(r->buf[r->n], MCCSTATS_COLW, fmt, ap);
	va_end(ap);
	r->n++;
}

static void mccstats_build(McccRows *r) { MCC_TRACE("enter\n");
	char a[64], b[64], c[64], d[64];
	r->n = 0;
	mccstats_fmt_u((unsigned long)mcs.total_evaluated, a, sizeof a);
	mccstats_row(r, "\033[1m mcc --stats\033[0m  elapsed %u.%us / %u.%us   evaluated %s",
							 mcs.elapsed_ms / 1000, (mcs.elapsed_ms % 1000) / 100,
							 mcs.budget_ms / 1000, (mcs.budget_ms % 1000) / 100, a);
	mccstats_row(r, " %.*s", 66,
							 "----------------------------------------------------------------------");

	if (mcc_stats_on(MCC_STATS_SEARCH)) { MCC_TRACE("br\n");
		mccstats_row(r, " \033[36mSEARCH\033[0m   func=%s  #%d searched  memo=%d",
								 mcs.func[0] ? mcs.func : "-", mcs.funcs_searched, mcs.memo_n);
		mccstats_row(r, "          base=0x%08llx  searchable=0x%08llx  nitems=%d",
								 (unsigned long long)mcs.base,
								 (unsigned long long)mcs.searchable, mcs.nitems);
		mccstats_row(r, "          walk=%s%s  budget=%ums  next~%ums",
								 mccstats_walk_name(mcs.walk),
								 mcs.ordered ? "/ordered" : "", mcs.budget_ms, mcs.expect_ms);
	}

	if (mcc_stats_on(MCC_STATS_COMBO)) { MCC_TRACE("br\n");
		char spark[MCCSTATS_SPARK_N * 4];
		mccstats_spark(spark, sizeof spark);
		mccstats_row(r, " \033[35mCOMBO\033[0m    in-flight k=%d  [%s]", mcs.cand_k,
								 mcs.cand_names[0] ? mcs.cand_names : "-");
		mccstats_row(r, "          gates=0x%08llx  cost=%ld",
								 (unsigned long long)mcs.cand_gates,
								 mcs.cand_score >= 0 ? (mcs.cand_score >> 12) : -1);
		mccstats_gate_names(mcs.fn_best_gates, a, sizeof a);
		mccstats_row(r, "          fn-best=[%s] cost=%ld",
								 a[0] ? a : "-",
								 mcs.fn_best_score >= 0 ? (mcs.fn_best_score >> 12) : -1);
		mccstats_row(r, "          scores %s", spark);
	}

	if (mcc_stats_on(MCC_STATS_STRATEGY)) { MCC_TRACE("br\n");
		int i;
		mccstats_row(r, " \033[32mSTRATEGY\033[0m fires across %lu functions",
								 mcs.strat_calls);
		for (i = 0; i < MCCSTATS_STRAT_N; i += 4) { MCC_TRACE("br\n");
			int j, more = i + 4 < MCCSTATS_STRAT_N ? 4 : MCCSTATS_STRAT_N - i;
			char *bufs[4] = {a, b, c, d};
			char cnt[24];
			for (j = 0; j < 4; j++)
				{ MCC_TRACE("br\n"); bufs[j][0] = '\0'; }
			for (j = 0; j < more; j++) { MCC_TRACE("br\n");
				mccstats_fmt_u(mcs.strat_hits[i + j], cnt, sizeof cnt);
				snprintf(bufs[j], 64, "%-8s %6s", mccstats_strat_name[i + j], cnt);
			}
			mccstats_row(r, "   %-15s %-15s %-15s %-15s", a, b, c, d);
		}
	}

	if (mcc_stats_on(MCC_STATS_STRATEGY)) { MCC_TRACE("br\n");
		int bcol, p = 0;
		char line[MCCSTATS_COLW];
		line[0] = '\0';
		mccstats_row(r, " \033[33mGATES\033[0m    \342\227\217=on  \302\267=searchable  (of searchable set)");
		for (bcol = 0; bcol < MCCSTATS_GATE_N; bcol++) { MCC_TRACE("br\n");
			uint64_t bit = (uint64_t)1 << bcol;
			int on, mark;
			if (!(mcs.searchable & bit))
				{ MCC_TRACE("br\n"); continue; }
			on = (mcs.best_gates & bit) != 0;
			mark = on;
			p += snprintf(line + p, sizeof line - p, "%s%s ",
										mark ? "\342\227\217" : "\302\267",
										mccstats_gate_name[bcol]);
			if (p >= (int)sizeof line - 40) { MCC_TRACE("br\n");
				mccstats_row(r, "          %s", line);
				p = 0;
				line[0] = '\0';
			}
		}
		if (p)
			{ MCC_TRACE("br\n"); mccstats_row(r, "          %s", line); }
	}

	if (mcc_stats_on(MCC_STATS_JIT)) { MCC_TRACE("br\n");
		mccstats_fmt_u(mcs.jit_kgc_hits, a, sizeof a);
		mccstats_fmt_u(mcs.jit_kgc_misses, b, sizeof b);
		mccstats_row(r, " \033[34mJIT\033[0m      recompiles=%lu  promote=%lu/%lu(async)  poison=%lu",
								 mcs.jit_recompiles, mcs.jit_promote_sync,
								 mcs.jit_promote_async, mcs.jit_poison);
		mccstats_row(r, "          kgc hits=%s  miss=%s", a, b);
	}

	mccstats_gate_names(mcs.best_gates, a, sizeof a);
	mccstats_row(r, " \033[1mWINNER\033[0m   best=[%s] cost=%ld",
							 a[0] ? a : "-",
							 mcs.best_score >= 0 ? (mcs.best_score >> 12) : -1);
	mccstats_row(r, " %.*s", 66,
							 "----------------------------------------------------------------------");
}

static void mccstats_paint(int force) { MCC_TRACE("enter\n");
	McccRows rows;
	int i;
	unsigned now;
	if (!mcs.active)
		{ MCC_TRACE("br\n"); return; }
	now = mccstats_now_ms();
	if (!force && mcs.prev_lines && (now - mcs.last_paint_ms) < 50)
		{ MCC_TRACE("br\n"); return; }
	mcs.last_paint_ms = now;
	if (!mcs.tty && !force)
		{ MCC_TRACE("br\n"); return; }
	mccstats_build(&rows);
	if (mcs.tty) { MCC_TRACE("br\n");
		if (mcs.prev_lines)
			{ MCC_TRACE("br\n"); fprintf(stderr, "\033[%dA", mcs.prev_lines); }
		for (i = 0; i < rows.n; i++)
			{ MCC_TRACE("br\n"); fprintf(stderr, "\r\033[2K%s\n", rows.buf[i]); }
		for (i = rows.n; i < mcs.prev_lines; i++)
			{ MCC_TRACE("br\n"); fprintf(stderr, "\r\033[2K\n"); }
		if (rows.n < mcs.prev_lines)
			{ MCC_TRACE("br\n"); fprintf(stderr, "\033[%dA", mcs.prev_lines - rows.n); }
		mcs.prev_lines = rows.n;
	} else { MCC_TRACE("br\n");
		for (i = 0; i < rows.n; i++)
			{ MCC_TRACE("br\n"); fprintf(stderr, "%s\n", rows.buf[i]); }
	}
	fflush(stderr);
}

void mcc_stats_enable(unsigned mask) { MCC_TRACE("enter\n");
	mcc_stats_mask = mask;
	if (!mask)
		{ MCC_TRACE("br\n"); return; }
	memset(&mcs, 0, sizeof mcs);
	mcs.active = 1;
	mcs.fn_best_score = -1;
	mcs.best_score = -1;
	mcs.cand_score = -1;
#if MCC_HOST_POSIX
	mcs.tty = isatty(fileno(stderr));
#else
	mcs.tty = 0;
#endif
	if (getenv("MCC_STATS_FORCE"))
		{ MCC_TRACE("br\n"); mcs.tty = 1; }
}

void mcc_stats_env_init(void) { MCC_TRACE("enter\n");
	static int done = 0;
	const char *e;
	unsigned m;
	if (done)
		{ MCC_TRACE("br\n"); return; }
	done = 1;
	if (mcc_stats_mask)
		{ MCC_TRACE("br\n"); return; }
	e = getenv("MCC_STATS");
	if (!e || !e[0])
		{ MCC_TRACE("br\n"); return; }
	m = (unsigned)strtoul(e, NULL, 0);
	if (m <= 1)
		{ MCC_TRACE("br\n"); m = MCC_STATS_ALL; }
	mcc_stats_enable(m);
	atexit(mcc_stats_finish);
}

void mcc_stats_finish(void) { MCC_TRACE("enter\n");
	if (!mcs.active)
		{ MCC_TRACE("br\n"); return; }
	mccstats_paint(1);
	if (mcs.tty)
		{ MCC_TRACE("br\n"); fprintf(stderr, "\n"); }
	fflush(stderr);
	mcs.active = 0;
}

void mcc_stats_search_begin(const char *func, uint64_t hash, uint64_t base,
													 uint64_t searchable, int nitems, int walk,
													 int ordered) { MCC_TRACE("enter\n");
	if (!mcs.active)
		{ MCC_TRACE("br\n"); return; }
	snprintf(mcs.func, sizeof mcs.func, "%s", func ? func : "-");
	mcs.hash = hash;
	mcs.base = base;
	mcs.searchable = searchable;
	mcs.nitems = nitems;
	mcs.walk = walk;
	mcs.ordered = ordered;
	mcs.fn_best_gates = base;
	mcs.fn_best_score = -1;
	mcs.evaluated = 0;
}

void mcc_stats_combo_cand(uint64_t gates, const int *sel, int k,
													const uint64_t *item_bits, long score, long evaluated,
													unsigned elapsed_ms, unsigned budget_ms,
													unsigned expect_ms) { MCC_TRACE("enter\n");
	int i, p = 0;
	if (!mcs.active)
		{ MCC_TRACE("br\n"); return; }
	mcs.cand_gates = gates;
	mcs.cand_k = k;
	mcs.cand_score = score;
	mcs.evaluated = evaluated;
	mcs.total_evaluated++;
	mcs.elapsed_ms = elapsed_ms;
	mcs.budget_ms = budget_ms;
	mcs.expect_ms = expect_ms;
	mcs.cand_names[0] = '\0';
	for (i = 0; i < k && sel && item_bits; i++) { MCC_TRACE("br\n");
		int b, idx = -1;
		uint64_t bit = item_bits[sel[i]];
		for (b = 0; b < MCCSTATS_GATE_N; b++)
			{ MCC_TRACE("br\n"); if (bit == ((uint64_t)1 << b)) { MCC_TRACE("br\n");
				idx = b;
				break;
			} }
		p += snprintf(mcs.cand_names + p, sizeof mcs.cand_names - p, "%s%s",
									i ? "," : "", idx >= 0 ? mccstats_gate_name[idx] : "?");
		if (p >= (int)sizeof mcs.cand_names - 8)
			{ MCC_TRACE("br\n"); break; }
	}
	if (score >= 0) { MCC_TRACE("br\n");
		mcs.spark[mcs.spark_head] = score >> 12;
		mcs.spark_head = (mcs.spark_head + 1) % MCCSTATS_SPARK_N;
		if (mcs.spark_n < MCCSTATS_SPARK_N)
			{ MCC_TRACE("br\n"); mcs.spark_n++; }
		if (mcs.fn_best_score < 0 || score < mcs.fn_best_score) { MCC_TRACE("br\n");
			mcs.fn_best_score = score;
			mcs.fn_best_gates = gates;
		}
	}
	mccstats_paint(0);
}

void mcc_stats_search_end(uint64_t best_gates, long best_score, long evaluated,
													int memo_n) { MCC_TRACE("enter\n");
	if (!mcs.active)
		{ MCC_TRACE("br\n"); return; }
	mcs.funcs_searched++;
	mcs.memo_n = memo_n;
	if (best_score >= 0 && (mcs.best_score < 0 || best_score < mcs.best_score)) { MCC_TRACE("br\n");
		mcs.best_score = best_score;
		mcs.best_gates = best_gates;
	}
	(void)evaluated;
	mccstats_paint(0);
}

void mcc_stats_strat_hits(const int *sf, int n) { MCC_TRACE("enter\n");
	int i;
	if (!mcs.active || !sf)
		{ MCC_TRACE("br\n"); return; }
	if (n > MCCSTATS_STRAT_N)
		{ MCC_TRACE("br\n"); n = MCCSTATS_STRAT_N; }
	mcs.strat_calls++;
	for (i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); mcs.strat_hits[i] += (unsigned long)(sf[i] > 0 ? sf[i] : 0); }
}

void mcc_stats_jit_recompile(void) { MCC_TRACE("enter\n");
	if (!mcs.active)
		{ MCC_TRACE("br\n"); return; }
	mcs.jit_recompiles++;
}

void mcc_stats_jit_kgc_hit(void) { MCC_TRACE("enter\n");
	if (!mcs.active)
		{ MCC_TRACE("br\n"); return; }
	mcs.jit_kgc_hits++;
}

void mcc_stats_jit_kgc_miss(void) { MCC_TRACE("enter\n");
	if (!mcs.active)
		{ MCC_TRACE("br\n"); return; }
	mcs.jit_kgc_misses++;
}

void mcc_stats_jit_poison(void) { MCC_TRACE("enter\n");
	if (!mcs.active)
		{ MCC_TRACE("br\n"); return; }
	mcs.jit_poison++;
}

void mcc_stats_jit_promote(int async) { MCC_TRACE("enter\n");
	if (!mcs.active)
		{ MCC_TRACE("br\n"); return; }
	if (async)
		{ MCC_TRACE("br\n"); mcs.jit_promote_async++; }
	else
		{ MCC_TRACE("br\n"); mcs.jit_promote_sync++; }
}
