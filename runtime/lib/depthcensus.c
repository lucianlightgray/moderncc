#include <stdio.h>
#include <stdlib.h>

#define DC_MAX 8192
#define DC_STACK 8192
#define DC_LVL 32

typedef struct dc_rec {
	unsigned long long calls;
	unsigned long long lvl[DC_LVL];
	unsigned live;
	unsigned maxdepth;
} dc_rec;

typedef struct dc_ent {
	unsigned long anchor;
	unsigned id;
} dc_ent;

static dc_rec dc_tab[DC_MAX];
static dc_ent dc_stk[DC_STACK];
static unsigned dc_stk_n;
static unsigned dc_seen;
static unsigned long long dc_over;
static unsigned long long dc_stkover;

static FILE *dc_open(void) {
	const char *p = getenv("MCC_DEPTH_CENSUS");
	if (!p || !p[0])
		return NULL;
	if (p[0] == '-' && !p[1])
		return stderr;
	return fopen(p, "a");
}

void __attribute__((destructor)) __mcc_depth_census_dump(void) {
	FILE *f;
	unsigned i;
	int j;
	unsigned long long tc = 0;
	if (!dc_seen)
		return;
	f = dc_open();
	if (!f)
		return;
	for (i = 0; i < dc_seen; i++) {
		dc_rec *r = &dc_tab[i];
		unsigned long long roots = r->lvl[1];
		unsigned long long wmax = 0;
		int last = 0;
		if (!r->calls)
			continue;
		for (j = 1; j < DC_LVL; j++) {
			unsigned long long w;
			if (!r->lvl[j])
				continue;
			last = j;
			w = roots ? (r->lvl[j] + roots - 1) / roots : r->lvl[j];
			if (w > wmax)
				wmax = w;
		}
		fprintf(f, "[depth] id=%u calls=%llu max=%u roots=%llu wmax=%llu w=",
						i, r->calls, r->maxdepth, roots, wmax);
		for (j = 1; j <= last; j++)
			fprintf(f, "%s%llu", j > 1 ? "," : "", r->lvl[j]);
		fprintf(f, "\n");
		tc += r->calls;
	}
	fprintf(f, "[depth-tot] fns=%u calls=%llu overflow=%llu stack-overflow=%llu\n",
					dc_seen, tc, dc_over, dc_stkover);
	if (f != stderr)
		fclose(f);
}

void __mcc_depth_census(unsigned id) {
	char probe;
	unsigned long here = (unsigned long)(size_t)(void *)&probe;
	dc_rec *r;
	if (id >= DC_MAX) {
		dc_over++;
		return;
	}
	while (dc_stk_n && dc_stk[dc_stk_n - 1].anchor <= here) {
		unsigned pid = dc_stk[dc_stk_n - 1].id;
		if (dc_tab[pid].live)
			dc_tab[pid].live--;
		dc_stk_n--;
	}
	if (dc_stk_n >= DC_STACK) {
		dc_stkover++;
		return;
	}
	dc_stk[dc_stk_n].anchor = here;
	dc_stk[dc_stk_n].id = id;
	dc_stk_n++;
	if (id >= dc_seen)
		dc_seen = id + 1;
	r = &dc_tab[id];
	r->calls++;
	r->live++;
	if (r->live > r->maxdepth)
		r->maxdepth = r->live;
	if (r->live < DC_LVL)
		r->lvl[r->live]++;
}
