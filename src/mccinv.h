#ifndef MCC_INV_H
#define MCC_INV_H

/* Emit-coverage inventory.  Env-gated on MCC_INV=1; zero cost otherwise.
 *
 * Counts what each layer actually DID for a compilation, at the point where
 * that layer's own decision is made and the numbers are local:
 *
 *   aot.*   gen_function() completed a body and advanced ind
 *   rir.*   rir_hook_body_begin() decided whether to record this body
 *   ast.*   ast_func_end() computed faithfulness and knows both byte lengths
 *   jit.*   the embed stash accepted a body
 *
 * Deliberately NOT derived from any existing census: the point is a fresh
 * count taken at the site, so a disagreement with another counter is a finding
 * rather than a copy of one.  mcc is a unity build (mcc.c -> libmcc.c ->
 * mccgen.c -> mccast.c/mccrir.c) so these statics are one shared set.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MCC_INV_MAX 32

typedef struct {
	const char *k;
	long long v;
} MccInvRow;

static MccInvRow mcc_inv_rows[MCC_INV_MAX];
static int mcc_inv_n;
static int mcc_inv_on = -1;
static int mcc_inv_hooked;

static void mcc_inv_dump(void)
{
	int i;
	if (!mcc_inv_n)
		return;
	fprintf(stderr, "[inv]");
	for (i = 0; i < mcc_inv_n; i++)
		fprintf(stderr, " %s=%lld", mcc_inv_rows[i].k, mcc_inv_rows[i].v);
	fprintf(stderr, "\n");
}

static void mcc_inv_add(const char *k, long long d)
{
	int i;
	if (mcc_inv_on < 0) {
		const char *e = getenv("MCC_INV");
		mcc_inv_on = (e && *e && *e != '0') ? 1 : 0;
	}
	if (!mcc_inv_on)
		return;
	if (!mcc_inv_hooked) {
		mcc_inv_hooked = 1;
		atexit(mcc_inv_dump);
	}
	for (i = 0; i < mcc_inv_n; i++)
		if (mcc_inv_rows[i].k == k || !strcmp(mcc_inv_rows[i].k, k)) {
			mcc_inv_rows[i].v += d;
			return;
		}
	if (mcc_inv_n >= MCC_INV_MAX)
		return;
	mcc_inv_rows[mcc_inv_n].k = k;
	mcc_inv_rows[mcc_inv_n].v = d;
	mcc_inv_n++;
}

#endif /* MCC_INV_H */
