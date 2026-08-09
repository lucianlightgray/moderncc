#include <stdio.h>
#include <stdlib.h>

#define LC_MAX 32768
#define LC_HIST 12
#define LC_NGE 6

typedef struct lc_rec {
	unsigned long long entries;
	unsigned long long exits;
	unsigned long long iters;
	unsigned long long zero;
	unsigned long long maxtrip;
	unsigned long long hist[LC_HIST];
	unsigned long long ge[LC_NGE];
	unsigned long long gew[LC_NGE];
} lc_rec;

static const unsigned long long lc_thr[LC_NGE] = {8, 23, 24, 48, 108, 322};
static const unsigned long lc_bucket_hi[LC_HIST] = {
		1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 0};

static lc_rec lc_tab[LC_MAX];
static unsigned lc_seen;
static unsigned long long lc_over;

static FILE *lc_open(const char **name) {
	const char *p = getenv("MCC_LOOP_CENSUS");
	if (!p || !p[0])
		return NULL;
	*name = p;
	if (p[0] == '-' && !p[1])
		return stderr;
	return fopen(p, "a");
}

void __attribute__((destructor)) __mcc_loop_census_dump(void) {
	const char *name = "-";
	FILE *f;
	unsigned long long te = 0, tx = 0, ti = 0, tl = 0, ts = 0;
	unsigned i;
	int j;
	if (!lc_seen)
		return;
	f = lc_open(&name);
	if (!f)
		return;
	for (i = 0; i < lc_seen; i++) {
		lc_rec *r = &lc_tab[i];
		unsigned long long lost, stray;
		if (!r->entries && !r->exits)
			continue;
		lost = r->entries > r->exits ? r->entries - r->exits : 0;
		stray = r->exits > r->entries ? r->exits - r->entries : 0;
		fprintf(f, "[trip] id=%u entries=%llu exits=%llu lost=%llu stray=%llu "
							 "iters=%llu zero=%llu max=%llu h=",
						i, r->entries, r->exits, lost, stray, r->iters, r->zero,
						r->maxtrip);
		for (j = 0; j < LC_HIST; j++)
			fprintf(f, "%s%llu", j ? "," : "", r->hist[j]);
		fprintf(f, " ge=");
		for (j = 0; j < LC_NGE; j++)
			fprintf(f, "%s%llu:%llu", j ? "," : "", lc_thr[j], r->ge[j]);
		fprintf(f, " gew=");
		for (j = 0; j < LC_NGE; j++)
			fprintf(f, "%s%llu:%llu", j ? "," : "", lc_thr[j], r->gew[j]);
		fprintf(f, "\n");
		te += r->entries;
		tx += r->exits;
		ti += r->iters;
		tl += lost;
		ts += stray;
	}
	fprintf(f, "[trip-tot] loops=%u entries=%llu exits=%llu lost=%llu "
						 "stray=%llu iters=%llu overflow=%llu\n",
					lc_seen, te, tx, tl, ts, ti, lc_over);
	if (f != stderr)
		fclose(f);
}

static void lc_hook(unsigned id) {
	if (id >= lc_seen)
		lc_seen = id + 1;
}

void __mcc_loop_census_enter(unsigned id) {
	if (id >= LC_MAX) {
		lc_over++;
		return;
	}
	lc_hook(id);
	lc_tab[id].entries++;
}

void __mcc_loop_census(unsigned id, unsigned long long trips) {
	lc_rec *r;
	int j;
	if (id >= LC_MAX) {
		lc_over++;
		return;
	}
	lc_hook(id);
	r = &lc_tab[id];
	r->exits++;
	r->iters += trips;
	if (!trips)
		r->zero++;
	if (trips > r->maxtrip)
		r->maxtrip = trips;
	for (j = 0; j < LC_HIST - 1; j++)
		if (trips <= lc_bucket_hi[j])
			break;
	r->hist[j]++;
	for (j = 0; j < LC_NGE; j++)
		if (trips >= lc_thr[j]) {
			r->ge[j]++;
			r->gew[j] += trips;
		}
}
