#ifndef MCC_SLICE_H
#define MCC_SLICE_H

#ifndef AST_EVAL_SLICE_PROVIDED
#error "mccslice.h needs the real ast_eval_slice: the CPU runner is the reference the device is measured against, and the mccast.c stub returns 1 without writing *out."
#endif

#define MCC_SLICE_MAXLIVE 8
#define MCC_SLICE_MAXBATCH (1 << 16)

/* A unit of schedulable work: the slice, the live-in vector that is its calling
 * convention, and a batch of argument tuples to evaluate it over. `done` is the
 * resume point, so a runner may be ticked with a budget and picked up later
 * without changing any result. */
typedef struct MccSliceWork {
	AstArena *a;
	AstLocal root;
	int32_t off[MCC_SLICE_MAXLIVE];
	int nlive;
	int nodes;
	int wtype;
	const int64_t *in;
	int64_t *out;
	unsigned char *def;
	int ntuple;
	int done;
	int budget;
	struct MccSliceKernel *kernel;
} MccSliceWork;

/* A built device kernel. The key is the S6b cache identity: the same subtree
 * with a permuted live-in vector is a different kernel, because the offset
 * ordering is the ABI the emitter indexes lanes by. */
typedef struct MccSliceKernel {
	MccGpuCode code;
	int32_t off[MCC_SLICE_MAXLIVE];
	int nlive;
	int wtype;
	uint64_t key;
} MccSliceKernel;

static long mcc_slice_dispatch_count;
static long mcc_slice_emit_count;

/* Known-positive switch. When set, every built kernel returns a value one bit
 * wrong. A differential that still reports OK under this is blind, and a blind
 * differential is worse than no differential -- it reads as evidence. */
static int mcc_slice_mutate;

static long mcc_slice_dispatches(void) { return mcc_slice_dispatch_count; }
static long mcc_slice_emits(void) { return mcc_slice_emit_count; }
static void mcc_slice_set_mutate(int on) { mcc_slice_mutate = on; }

/* H6. Neither executor was timed anywhere in this tree before: src/mccgpu.c
 * contains no clock call of any kind, nothing brackets ast_eval_slice_rec, and
 * the ladder budgets by dispatch *count* rather than duration. Without both
 * numbers no promotion decision can be made at all, so this is Phase 0a and it
 * comes before everything.
 *
 * The device figure deliberately spans upload, dispatch, download and readback:
 * that is the cost a caller actually pays, and excluding it is exactly how this
 * decision gets made wrong. */
static double mcc_slice_cpu_ns;
static double mcc_slice_gpu_ns;
static int32_t *mcc_slice_scratch_in;
static int32_t *mcc_slice_scratch_out;
static size_t mcc_slice_scratch_in_sz;
static size_t mcc_slice_scratch_out_sz;

static double mcc_slice_now(void) {
#if defined(CLOCK_MONOTONIC)
	struct timespec ts;
	if (!clock_gettime(CLOCK_MONOTONIC, &ts))
		return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
#endif
	return (double)clock() * (1e9 / (double)CLOCKS_PER_SEC);
}

static double mcc_slice_cpu_time(void) { return mcc_slice_cpu_ns; }
static double mcc_slice_gpu_time(void) { return mcc_slice_gpu_ns; }
static void mcc_slice_time_reset(void) {
	mcc_slice_cpu_ns = 0;
	mcc_slice_gpu_ns = 0;
}


static int mcc_slice_nodes(AstArena *a, AstLocal n) {
	AstLocal c;
	int t = 1;
	if (n == AST_NONE)
		return 0;
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		t += mcc_slice_nodes(a, c);
	return t;
}

static int mcc_slice_work_from_ast(AstArena *a, AstLocal root, MccSliceWork *w) {
	int cnt = 0;
	if (!a || !w || root == AST_NONE || root >= ast_count(a))
		return 0;
	if (!ast_eval_slice_kind_ok(a, root, 0))
		return 0;
	memset(w, 0, sizeof *w);
	if (!ast_eval_slice_livein(a, root, w->off, &cnt, MCC_SLICE_MAXLIVE))
		return 0;
	if (cnt < 1)
		return 0;
	/* The inferred width is safe to use, but only because the three divergences
	 * that made it unsafe are fixed: the CPU evaluator now narrows a live-in to
	 * the type of the Ref that reads it, both emitters now do the same, and the
	 * device result is re-narrowed with ast_eval_narrow rather than fitted to a
	 * declared type finer than 32 bits. Requiring a declared root type instead
	 * was the earlier workaround, and it cost 99.2% of all candidate slices --
	 * 3157 of 3181 lowerable subtrees in the corpus are untyped at the root, and
	 * every subtree of 15 nodes or more, which is the band where the device
	 * starts to win, was among them. */
	w->wtype = ast_type_t(a, root);
	if (!w->wtype)
		w->wtype = ast_eval_slice_wtype(a, root);
	if (!w->wtype || ast_bad_type(w->wtype) || is_float(w->wtype) ||
			!ast_eval_slice_intt(w->wtype))
		return 0;
	w->a = a;
	w->root = root;
	w->nlive = cnt;
	w->nodes = mcc_slice_nodes(a, root);
	return 1;
}

static void mcc_slice_work_bind(MccSliceWork *w, const int64_t *in, int ntuple,
																int64_t *out, unsigned char *def) {
	if (!w)
		return;
	w->in = in;
	w->ntuple = ntuple;
	w->out = out;
	w->def = def;
	w->done = 0;
}

/* The CPU executor. Always available, needs no evidence to run, and is the
 * reference every device bid is measured against. */
static int mcc_slice_run_cpu(MccSliceWork *w, int budget) {
	int64_t vals[MCC_SLICE_MAXLIVE];
	int n = 0, j;
	double t0;
	if (!w || !w->a || !w->in || !w->out || !w->def)
		return MCC_TASK_FAILED;
	t0 = mcc_slice_now();
	while (w->done < w->ntuple) {
		int64_t o = 0;
		int d;
		if (budget && n >= budget) {
			mcc_slice_cpu_ns += mcc_slice_now() - t0;
			return MCC_TASK_YIELDED;
		}
		for (j = 0; j < w->nlive; j++)
			vals[j] = w->in[(long)w->done * w->nlive + j];
		d = ast_eval_slice(w->a, w->root, w->off, vals, w->nlive, &o);
		w->def[w->done] = (unsigned char)(d ? 1 : 0);
		w->out[w->done] = d ? o : 0;
		w->done++;
		n++;
	}
	mcc_slice_cpu_ns += mcc_slice_now() - t0;
	return MCC_TASK_DONE;
}

/* --- device frame storage ------------------------------------------------ *
 * A frame slice is a run of statements that read and write local frame slots.
 * It exists because Store/Load are what terminate lowerable subtrees: measured
 * over 32,373 corpus nodes, Store 3.0% + StoreVal 3.0% + BasicBlock 3.0% +
 * Invoke 4.0% are the terminators, against a census that is 80% expression
 * nodes. Expressions are not scarce; C statement boundaries chop them into 3-4
 * node pieces, and no relaxation of the expression predicate can change that.
 *
 * The frame is the existing input buffer, used read-write. Slots are dense
 * indices over the distinct local offsets the run touches; the host seeds all
 * of them, the kernel reads and writes them in place, and the host reads the
 * whole frame back. The buffer was never decorated NonWritable, so this needs
 * no new binding and no ABI version bump.
 *
 * v1 scope, deliberately narrow: AST_Store with a local Ref destination (834 of
 * 987 stores in the corpus, 84.5%), sequenced by AST_BasicBlock. AST_StoreVal
 * is excluded on purpose -- it is a vstack-ordering marker for the replay
 * machinery, not a store, and it references its AST_Store by ival. */

/* C3's step budget, at the value the measurement settled on. A loop that hits
 * it makes the run undefined, not truncated. */
#define MCC_SLICE_TRIP_MAX (1 << 16)
#define MCC_SLICE_MAXSLOT 16
#define MCC_SLICE_MAXSTMT 64

typedef struct MccSliceFrame {
	AstArena *a;
	AstLocal root;
	int32_t slot[MCC_SLICE_MAXSLOT];
	int nslot;
	int nstmt;
	AstLocal top[MCC_SLICE_MAXSTMT];
	int ntop;
	int nctrl;
	int nloop;
	/* A run may end in a Return, whose value goes to the existing out slots.
	 * Measured: allowing it takes corpus eligibility from 44 of 947 non-empty
	 * BasicBlocks to 255 (4.6% -> 26.9%), because Return is the single most
	 * common statement kind a block ends with -- it blocked 353 blocks on its
	 * own, more than Invoke's 294 or If's 238. */
	AstLocal ret;
	int rettype;
	int nodes;
	/* B1 minimal. When a run needs a real address -- a runtime index, or a
	 * local whose address is taken and then dereferenced -- the frame stops
	 * being a dense slot table and becomes a byte-addressed region, addressed
	 * as 4-byte words. Selected per run, so a run that needs no address keeps
	 * the proven slot path untouched and cannot regress. */
	int bytemode;
	int32_t base_min;
	int32_t base_max;
	int nword;
	int nword_mask;
} MccSliceFrame;

static int mcc_slice_slot_of(MccSliceFrame *f, int32_t off) {
	int i;
	for (i = 0; i < f->nslot; i++)
		if (f->slot[i] == off)
			return i;
	if (f->nslot >= MCC_SLICE_MAXSLOT)
		return -1;
	f->slot[f->nslot] = off;
	return f->nslot++;
}

/* A local Ref, i.e. a frame slot rather than a global or an address-of.
 *
 * VT_LVAL is load-bearing and its absence is not cosmetic. A local Ref *with*
 * VT_LVAL is the variable, and its value lives in the frame. A local Ref
 * *without* it is the variable's ADDRESS -- a decayed array base, measured 24
 * times as the base of `arr[i]` against 45 LVAL derefs. The shared evaluator
 * (`ast_eval_slice.h`) checks neither and looks both up in the environment, so
 * an address is evaluated as a value.
 *
 * OPEN QUESTION, not resolved here. Refusing non-LVAL local Refs outright was
 * tried and reverted: this tree's own test idiom (`tools/spvgate.c` and
 * `tools/slicerun.c` both write plain VT_LOCAL for a value reference) says the
 * evaluator's convention is deliberate, and spvgate's cases pass against real
 * device execution. So the store-destination evidence does not generalise to
 * every expression position, and the distinction needs establishing rather than
 * inferring before anything acts on it. See docs/TODO.md. */
static int mcc_slice_is_local_ref(AstArena *a, AstLocal n, int32_t *off) {
	int r;
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		return 0;
	r = ast_op(a, n);
	if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
		return 0;
	*off = (int32_t)(int64_t)ast_ival(a, n);
	return 1;
}


static int mcc_slice_frame_scan(MccSliceFrame *f, AstLocal n) {
	AstArena *a = f->a;
	AstLocal c;
	int32_t off;
	int cnt = 0;
	if (n == AST_NONE)
		return 0;
	if (!ast_eval_slice_kind_ok(a, n, 1))
		return 0;
	if (!ast_eval_slice_livein(a, n, f->slot, &f->nslot, MCC_SLICE_MAXSLOT))
		return 0;
	(void)off;
	(void)c;
	(void)cnt;
	return 1;
}

/* A statement-if is `AST_If` with op 0 and two or three children: condition,
 * then, and optionally else. It is tractable here for one specific reason --
 * the run's outputs are frame stores, i.e. memory, so the two arms need no
 * OpPhi to reconcile a value. That is the whole payoff of having a frame:
 * without it, control flow forces value merging and this is the ternary
 * machinery all over again; with it, a statement-if is a SelectionMerge, two
 * blocks of stores, and a label. */
static int mcc_slice_frame_stmt_ok(MccSliceFrame *f, AstLocal s, int depth);

static int mcc_slice_frame_seq_ok(MccSliceFrame *f, AstLocal n, int depth) {
	AstLocal c;
	if (n == AST_NONE)
		return 1;
	if (ast_kind(f->a, n) != AST_BasicBlock)
		return mcc_slice_frame_stmt_ok(f, n, depth);
	for (c = ast_first_child(f->a, n); c != AST_NONE; c = ast_next_sib(f->a, c))
		if (!mcc_slice_frame_stmt_ok(f, c, depth))
			return 0;
	return 1;
}

static int mcc_slice_frame_stmt_ok(MccSliceFrame *f, AstLocal s, int depth) {
	AstArena *a = f->a;
	AstLocal d, v;
	int32_t off;
	int dt;
	if (depth > 8)
		return 0;
	if (ast_kind(a, s) == AST_If &&
			(ast_op(a, s) == 2 || ast_op(a, s) == 3 || ast_op(a, s) == 4)) {
		/* while {cond, body} | for {cond, body, incr} | do {body, cond}.
		 * Every one gets a hard trip cap: an unbounded device loop is the
		 * occupancy-watchdog hazard C3 exists for, and there is no trip count
		 * available statically anywhere in this tree to bound it with instead.
		 * Exceeding the cap marks the run undefined rather than truncating it,
		 * so the host falls back to the CPU instead of trusting a partial
		 * answer -- and the CPU reference applies the identical cap, or the two
		 * executors would disagree exactly at the boundary. */
		int op = ast_op(a, s);
		uint32_t nc = ast_nchild(a, s);
		AstLocal cond = op == 4 ? ast_child(a, s, 1) : ast_child(a, s, 0);
		AstLocal body = op == 4 ? ast_child(a, s, 0) : ast_child(a, s, 1);
		if (op == 2 && nc != 2)
			return 0;
		if (op == 3 && nc != 3)
			return 0;
		if (op == 4 && nc != 2)
			return 0;
		if (!mcc_slice_frame_scan(f, cond))
			return 0;
		if (!mcc_slice_frame_seq_ok(f, body, depth + 1))
			return 0;
		if (op == 3 && !mcc_slice_frame_seq_ok(f, ast_child(a, s, 2), depth + 1))
			return 0;
		f->nloop++;
		f->nctrl++;
		return 1;
	}
	if (ast_kind(a, s) == AST_If && ast_op(a, s) == 7) {
		/* A ternary in statement position: rir_stmt retags AST_If op 5 as op 7
		 * when it is added as a statement of a basic block (src/mccrir.c:1489),
		 * so its value is computed and discarded. If the whole subtree is a pure
		 * lowerable expression -- no Store, no StoreVal, no Invoke -- then the
		 * statement has no observable effect on the frame or the return value and
		 * both executors skip it, which is trivially agreement. Second largest
		 * non-Invoke blocker at 34 blocks. */
		if (ast_nchild(a, s) != 3)
			return 0;
		if (!ast_eval_slice_kind_ok(a, s, 1))
			return 0;
		f->nctrl++;
		return 1;
	}
	if (ast_kind(a, s) == AST_If && ast_op(a, s) == 0) {
		uint32_t nc = ast_nchild(a, s);
		if (nc != 2 && nc != 3)
			return 0;
		if (!mcc_slice_frame_scan(f, ast_child(a, s, 0)))
			return 0;
		if (!mcc_slice_frame_seq_ok(f, ast_child(a, s, 1), depth + 1))
			return 0;
		if (nc == 3 && !mcc_slice_frame_seq_ok(f, ast_child(a, s, 2), depth + 1))
			return 0;
		f->nctrl++;
		return 1;
	}
	if (ast_kind(a, s) == AST_Unary &&
			(ast_op(a, s) == TOK_INC || ast_op(a, s) == TOK_DEC)) {
		/* `x++` / `x--` as a statement: the value is discarded, so pre and post
		 * are the same thing, and on an integer local it is exactly
		 * frame[slot] = frame[slot] +/- 1. Pointers are excluded because they
		 * scale by element size, which needs the address space this does not
		 * have. Measured: this alone takes eligible blocks from 254 to 350. */
		AstLocal t = ast_first_child(a, s);
		int tt;
		if (t == AST_NONE || !mcc_slice_is_local_ref(a, t, &off))
			return 0;
		tt = ast_type_t(a, t);
		if (!tt || ast_bad_type(tt) || is_float(tt) || !ast_eval_slice_intt(tt) ||
				(tt & VT_BTYPE) == VT_PTR)
			return 0;
		if (mcc_slice_slot_of(f, off) < 0)
			return 0;
		f->nstmt++;
		f->nodes += mcc_slice_nodes(a, s);
		return 1;
	}
	if (ast_kind(a, s) != AST_Store || ast_nchild(a, s) != 2)
		return 0;
	d = ast_child(a, s, 0);
	v = ast_child(a, s, 1);
	if (!mcc_slice_is_local_ref(a, d, &off))
		return 0;
	dt = ast_type_t(a, d);
	if (!dt || ast_bad_type(dt) || is_float(dt) || !ast_eval_slice_intt(dt))
		return 0;
	if (mcc_slice_slot_of(f, off) < 0)
		return 0;
	if (!mcc_slice_frame_scan(f, v))
		return 0;
	f->nstmt++;
	f->nodes += mcc_slice_nodes(a, s);
	return 1;
}

/* Word index of a frame byte offset, and the lane region size. Locals are laid
 * out at their real frame offsets so that ADDR(local) is simply that offset and
 * an address computed at runtime resolves back to the same word. */
static int mcc_slice_word_of(const MccSliceFrame *f, int32_t off) {
	return (int)((off - f->base_min) >> 2);
}

static int mcc_slice_frame_layout(MccSliceFrame *f) {
	int i;
	int32_t lo, hi;
	if (!f->bytemode)
		return 1;
	if (f->nslot < 1)
		return 0;
	lo = hi = f->slot[0];
	for (i = 1; i < f->nslot; i++) {
		if (f->slot[i] < lo)
			lo = f->slot[i];
		if (f->slot[i] > hi)
			hi = f->slot[i];
	}
	/* Align the base down to a word and leave room for the widest access at the
	 * top offset. Measured span over real blocks: median 12 B, p90 88 B. */
	f->base_min = lo & ~(int32_t)3;
	f->base_max = hi + 8;
	f->nword = (int)((f->base_max - f->base_min + 3) >> 2);
	if (f->nword < 1 || f->nword > (1 << 14))
		return 0;
	/* Power-of-two extent so a dynamic index can be masked into the lane's own
	 * region with one AND, making an out-of-range access harmless rather than a
	 * PageFault. J3b requires that no PageFault be reachable by construction. */
	f->nword_mask = 1;
	while (f->nword_mask < f->nword)
		f->nword_mask <<= 1;
	f->nword = f->nword_mask;
	f->nword_mask -= 1;
	return 1;
}

static int mcc_slice_frame_from_ast(AstArena *a, AstLocal root,
																		MccSliceFrame *f) {
	AstLocal s;
	if (!a || !f || root == AST_NONE || root >= ast_count(a))
		return 0;
	if (ast_kind(a, root) != AST_BasicBlock)
		return 0;
	memset(f, 0, sizeof *f);
	f->a = a;
	f->root = root;
	f->ret = AST_NONE;
	for (s = ast_first_child(a, root); s != AST_NONE; s = ast_next_sib(a, s)) {
		if (f->ret != AST_NONE)
			return 0; /* a Return is only a terminator, never mid-run */
		if (ast_kind(a, s) == AST_Return) {
			AstLocal rv = ast_first_child(a, s);
			if (rv == AST_NONE)
				return 0; /* a bare return carries no value to compare */
			if (!mcc_slice_frame_scan(f, rv))
				return 0;
			f->rettype = ast_type_t(a, rv);
			if (!f->rettype)
				f->rettype = ast_eval_slice_wtype(a, rv);
			if (!f->rettype || ast_bad_type(f->rettype) || is_float(f->rettype) ||
					!ast_eval_slice_intt(f->rettype))
				return 0;
			f->ret = rv;
			f->nodes += mcc_slice_nodes(a, s);
			continue;
		}
		if (f->nstmt + f->nctrl >= MCC_SLICE_MAXSTMT)
			return 0;
		if (!mcc_slice_frame_stmt_ok(f, s, 0))
			return 0;
		if (f->ntop < MCC_SLICE_MAXSTMT)
			f->top[f->ntop++] = s;
	}
	if (!mcc_slice_frame_layout(f))
		return 0;
	return f->ntop > 0 || f->ret != AST_NONE;
}

/* The CPU reference for a frame run. Reuses ast_eval_slice_rec for the value of
 * each statement, then narrows to the destination's type and writes it back, so
 * a later statement observes an earlier one -- which is the whole point and the
 * property a per-expression kernel cannot express. */
static int mcc_slice_frame_exec_stmt(MccSliceFrame *f, int64_t *frame,
																		 AstLocal s);

static int mcc_slice_frame_exec_seq(MccSliceFrame *f, int64_t *frame,
																		AstLocal n) {
	AstLocal c;
	if (n == AST_NONE)
		return 1;
	if (ast_kind(f->a, n) != AST_BasicBlock)
		return mcc_slice_frame_exec_stmt(f, frame, n);
	for (c = ast_first_child(f->a, n); c != AST_NONE; c = ast_next_sib(f->a, c))
		if (!mcc_slice_frame_exec_stmt(f, frame, c))
			return 0;
	return 1;
}

/* In byte mode the caller's array is still one int64 per SLOT (the harness and
 * the differential are slot-shaped), but addresses are byte offsets. The map
 * from a byte offset to a slot is exact for every offset the run named; a
 * runtime address that lands on no named offset is out of range for this run
 * and poisons the result, which is the same verdict the device reaches by
 * masking and clearing `def`. */
static int mcc_slice_slot_at(const MccSliceFrame *f, int32_t off) {
	int k;
	for (k = 0; k < f->nslot; k++)
		if (f->slot[k] == off)
			return k;
	return -1;
}

static int mcc_slice_frame_exec_stmt(MccSliceFrame *f, int64_t *frame,
																		 AstLocal s) {
	AstLocal d, v;
	int64_t val = 0;
	int32_t off = 0;
	int k, dt;
	if (ast_kind(f->a, s) == AST_If &&
			(ast_op(f->a, s) == 2 || ast_op(f->a, s) == 3 || ast_op(f->a, s) == 4)) {
		int op = ast_op(f->a, s);
		AstLocal cond = op == 4 ? ast_child(f->a, s, 1) : ast_child(f->a, s, 0);
		AstLocal body = op == 4 ? ast_child(f->a, s, 0) : ast_child(f->a, s, 1);
		long trips = 0;
		for (;;) {
			int64_t cv = 0;
			if (op != 4 || trips > 0) {
				if (!ast_eval_slice_rec(f->a, cond, f->slot, frame, f->nslot, &cv))
					return 0;
				if (!cv)
					break;
			}
			if (trips >= MCC_SLICE_TRIP_MAX)
				return 0; /* over budget: the whole run is undefined */
			if (!mcc_slice_frame_exec_seq(f, frame, body))
				return 0;
			if (op == 3 &&
					!mcc_slice_frame_exec_seq(f, frame, ast_child(f->a, s, 2)))
				return 0;
			trips++;
			if (op == 4) {
				if (!ast_eval_slice_rec(f->a, cond, f->slot, frame, f->nslot, &cv))
					return 0;
				if (!cv)
					break;
			}
		}
		return 1;
	}
	if (ast_kind(f->a, s) == AST_If && ast_op(f->a, s) == 7)
		return 1;
	if (ast_kind(f->a, s) == AST_If && ast_op(f->a, s) == 0) {
		int64_t cv = 0;
		if (!ast_eval_slice_rec(f->a, ast_child(f->a, s, 0), f->slot, frame,
														f->nslot, &cv))
			return 0;
		if (cv)
			return mcc_slice_frame_exec_seq(f, frame, ast_child(f->a, s, 1));
		if (ast_nchild(f->a, s) == 3)
			return mcc_slice_frame_exec_seq(f, frame, ast_child(f->a, s, 2));
		return 1;
	}
	if (ast_kind(f->a, s) == AST_Unary &&
			(ast_op(f->a, s) == TOK_INC || ast_op(f->a, s) == TOK_DEC)) {
		AstLocal t = ast_first_child(f->a, s);
		int delta = ast_op(f->a, s) == TOK_INC ? 1 : -1;
		if (!mcc_slice_is_local_ref(f->a, t, &off))
			return 0;
		for (k = 0; k < f->nslot; k++)
			if (f->slot[k] == off)
				break;
		if (k == f->nslot)
			return 0;
		dt = ast_type_t(f->a, t);
		frame[k] = ast_eval_slice_fit(frame[k] + delta, dt);
		return 1;
	}
	d = ast_child(f->a, s, 0);
	v = ast_child(f->a, s, 1);
	if (!ast_eval_slice_rec(f->a, v, f->slot, frame, f->nslot, &val))
		return 0;
	if (!mcc_slice_is_local_ref(f->a, d, &off))
		return 0;
	for (k = 0; k < f->nslot; k++)
		if (f->slot[k] == off)
			break;
	if (k == f->nslot)
		return 0;
	dt = ast_type_t(f->a, d);
	frame[k] = ast_eval_slice_fit(val, dt);
	return 1;
}

static int mcc_slice_frame_exec_cpu2(MccSliceFrame *f, int64_t *frame,
																		 int64_t *retval, int *retdef) {
	int i;
	if (!f || !frame)
		return 0;
	for (i = 0; i < f->ntop; i++)
		if (!mcc_slice_frame_exec_stmt(f, frame, f->top[i]))
			return 0;
	if (f->ret != AST_NONE) {
		int64_t rv = 0;
		int d = ast_eval_slice_rec(f->a, f->ret, f->slot, frame, f->nslot, &rv);
		if (retdef)
			*retdef = d;
		if (retval)
			*retval = d ? ast_eval_narrow(rv, ast_eval_slice_is64(f->rettype),
																		(f->rettype & VT_UNSIGNED) != 0)
									: 0;
	} else {
		if (retdef)
			*retdef = 0;
		if (retval)
			*retval = 0;
	}
	return 1;
}

static int mcc_slice_frame_exec_cpu(MccSliceFrame *f, int64_t *frame) {
	return mcc_slice_frame_exec_cpu2(f, frame, NULL, NULL);
}

#ifdef MCC_SLICE_GPU

static uint64_t mcc_slice_kernel_key(const MccSliceWork *w) {
	uint64_t h = ast_slice_ident_hash(w->a, w->root);
	int i;
	h ^= (uint64_t)w->nlive * 0x9e3779b97f4a7c15ull;
	h ^= (uint64_t)(unsigned)w->wtype * 0xff51afd7ed558ccdull;
	for (i = 0; i < w->nlive; i++) {
		h ^= (uint64_t)(uint32_t)w->off[i];
		h *= 0x100000001b3ull;
	}
	return h ? h : 1;
}

/* Emit once, dispatch many. mcc_gpu_emit is static behind MCC_GPU_ORACLE and so
 * is unreachable outside mccast.c's translation unit; this open-codes the same
 * sequence against the emitter primitives, which is also what lets the kernel
 * and its key outlive a single dispatch. */
static int mcc_slice_kernel_build(MccSliceWork *w, MccSliceKernel *k) {
	uint32_t base;
	int i;
	if (!w || !k || !w->a || w->nlive < 1)
		return 0;
	memset(k, 0, sizeof *k);
	for (i = 0; i < w->nlive; i++)
		k->off[i] = w->off[i];
	k->nlive = w->nlive;
	k->wtype = w->wtype;
#if MCC_GPU_LANG_MSL
	{
		MslMod m;
		MslV val;
		char *code;
		int nb = 0;
		msl_module_begin(&m, w->nlive);
		base = msl_main_begin(&m, w->nlive);
		if (!msl_expr(&m, w->a, w->root, w->off, w->nlive, base, &val) || m.failed) {
			msl_module_free(&m);
			return 0;
		}
		if (mcc_slice_mutate) {
			uint32_t p = msl_pair(&m, val);
			val = msl_mk(msl_pv(&m, "int2(p%u.x ^ 1, p%u.y)", p, p), 1, 0);
		}
		msl_main_end(&m, m.lane, val);
		code = msl_module_finish(&m, &nb);
		msl_module_free(&m);
		if (!code || nb <= 0) {
			free(code);
			return 0;
		}
		k->code.p = code;
		k->code.n = nb;
	}
#else
	{
		SpvMod m;
		SpvV val;
		uint32_t *code;
		int nw = 0;
		spv_module_begin(&m, w->nlive);
		base = spv_main_begin(&m, w->nlive);
		if (!spv_expr(&m, w->a, w->root, w->off, w->nlive, base, &val) || m.failed) {
			spv_module_free(&m);
			return 0;
		}
		if (mcc_slice_mutate) {
			uint32_t p = spv_pair(&m, val);
			uint32_t lo = spv_uop(&m, SpvOpBitwiseXor, spv_lo(&m, p), spv_uintc(&m, 1));
			val = spv_mk(spv_u2(&m, lo, spv_hi(&m, p)), 1, 0);
		}
		spv_main_end(&m, m.lane, val);
		code = spv_module_finish(&m, &nw);
		spv_module_free(&m);
		if (!code || nw <= 0) {
			free(code);
			return 0;
		}
		k->code.p = code;
		k->code.n = nw;
	}
#endif
	k->key = mcc_slice_kernel_key(w);
	mcc_slice_emit_count++;
	return 1;
}

static void mcc_slice_kernel_free(MccSliceKernel *k) {
	if (!k)
		return;
	free(k->code.p);
	k->code.p = NULL;
	k->code.n = 0;
}

/* The device executor. A dispatch is atomic, so a budget bounds how many tuples
 * one tick submits, not how far into a dispatch it may stop. */
static int mcc_slice_run_gpu(MccSliceWork *w, MccSliceKernel *k, int budget) {
	int32_t *in32, *out32;
	double gpu_t0;
	int n, t, j, rc;
	size_t need_in, need_out;
	if (!w || !k || !k->code.p || !w->in || !w->out || !w->def)
		return MCC_TASK_FAILED;
	if (k->nlive != w->nlive)
		return MCC_TASK_FAILED;
	if (w->done >= w->ntuple)
		return MCC_TASK_DONE;

	n = w->ntuple - w->done;
	if (budget && n > budget)
		n = budget;
	if (n > MCC_SLICE_MAXBATCH)
		n = MCC_SLICE_MAXBATCH;

	/* Scratch is reused across dispatches and never zeroed: every element of the
	 * input staging area is written by the pack loop below, and every output
	 * element is written by the device. calloc-and-free per dispatch cost more
	 * per lane than the dispatch itself at small batch sizes. */
	gpu_t0 = mcc_slice_now();
	need_in = (size_t)n * w->nlive * MCC_GPU_IN_SLOTS * sizeof(int32_t);
	need_out = (size_t)n * MCC_GPU_OUT_SLOTS * sizeof(int32_t);
	if (need_in > mcc_slice_scratch_in_sz) {
		free(mcc_slice_scratch_in);
		mcc_slice_scratch_in = (int32_t *)malloc(need_in);
		mcc_slice_scratch_in_sz = mcc_slice_scratch_in ? need_in : 0;
	}
	if (need_out > mcc_slice_scratch_out_sz) {
		free(mcc_slice_scratch_out);
		mcc_slice_scratch_out = (int32_t *)malloc(need_out);
		mcc_slice_scratch_out_sz = mcc_slice_scratch_out ? need_out : 0;
	}
	in32 = mcc_slice_scratch_in;
	out32 = mcc_slice_scratch_out;
	if (!in32 || !out32)
		return MCC_TASK_FAILED;
	for (t = 0; t < n; t++) {
		for (j = 0; j < w->nlive; j++) {
			int64_t v = w->in[(long)(w->done + t) * w->nlive + j];
			long s = ((long)t * w->nlive + j) * MCC_GPU_IN_SLOTS;
			in32[s] = (int32_t)(uint32_t)(uint64_t)v;
			in32[s + 1] = (int32_t)(uint32_t)((uint64_t)v >> 32);
		}
	}

	{
		double t0 = mcc_slice_now();
		rc = mcc_gpu_dispatch(k->code.p, k->code.n, in32, n, w->nlive, out32);
		(void)t0;
	}
	if (!rc) {
		mcc_slice_gpu_ns += mcc_slice_now() - gpu_t0;
		return MCC_TASK_FAILED;
	}
	mcc_slice_dispatch_count++;

	/* The out slots are a raw lo/hi pair: the device ABI is not self-describing,
	 * so the high word is only meaningful for a 64-bit result and is whatever
	 * the emitter left there otherwise. The caller has to re-apply the same final
	 * narrowing the CPU evaluator applies, which is ast_eval_narrow -- 32 or 64
	 * bits and a signedness, nothing finer. Fitting to the declared type instead
	 * over-narrows: an unsigned-char-typed add of 255 + 1 evaluates to 256 under
	 * C's integer promotions and under ast_eval_binop, and fitting that result
	 * back to unsigned char turns it into 0. */
	for (t = 0; t < n; t++) {
		long s = (long)t * MCC_GPU_OUT_SLOTS;
		int d = out32[s + 2] != 0;
		uint64_t lo = (uint32_t)out32[s];
		uint64_t hi = (uint32_t)out32[s + 1];
		w->def[w->done + t] = (unsigned char)(d ? 1 : 0);
		w->out[w->done + t] =
				d ? ast_eval_narrow((int64_t)(lo | (hi << 32)),
														ast_eval_slice_is64(k->wtype),
														(k->wtype & VT_UNSIGNED) != 0)
					: 0;
	}
	w->done += n;
	mcc_slice_gpu_ns += mcc_slice_now() - gpu_t0;
	return w->done >= w->ntuple ? MCC_TASK_DONE : MCC_TASK_YIELDED;
}

/* Emit a frame kernel: the statements in order, each evaluating its value with
 * the frame as the environment and storing the narrowed result back into the
 * destination slot. A later statement therefore reads what an earlier one
 * wrote, on the device, exactly as the CPU reference does. */
/* Emit one frame statement. A statement-if becomes SelectionMerge plus two
 * blocks of stores and a merge label -- no OpPhi, because the arms communicate
 * through the frame rather than through a value. The definedness flag still
 * needs a phi, so an undefined condition or an undefined store operand in one
 * arm is not laundered by the other arm being clean. */
static int mcc_slice_spv_stmt(SpvMod *m, MccSliceFrame *f, uint32_t base,
															AstLocal s);

static int mcc_slice_spv_seq(SpvMod *m, MccSliceFrame *f, uint32_t base,
														 AstLocal n) {
	AstLocal c;
	if (n == AST_NONE)
		return 1;
	if (ast_kind(f->a, n) != AST_BasicBlock)
		return mcc_slice_spv_stmt(m, f, base, n);
	for (c = ast_first_child(f->a, n); c != AST_NONE; c = ast_next_sib(f->a, c))
		if (!mcc_slice_spv_stmt(m, f, base, c))
			return 0;
	return 1;
}

static int mcc_slice_spv_stmt(SpvMod *m, MccSliceFrame *f, uint32_t base,
															AstLocal s) {
	AstLocal d, v;
	int32_t off = 0;
	SpvV val;
	int j, dt;

	if (ast_kind(f->a, s) == AST_If &&
			(ast_op(f->a, s) == 2 || ast_op(f->a, s) == 3 || ast_op(f->a, s) == 4)) {
		/* A structured SPIR-V loop. The frame is what makes this emittable at all:
		 * loop-carried values live in memory, so the header needs no OpPhi for
		 * them -- only the trip counter and the definedness flag are phis.
		 *
		 * The trip cap is not optional. An unbounded device loop is precisely the
		 * occupancy watchdog hazard cluster C is about, and this tree has no
		 * static trip count to bound it with. Over budget the run is marked
		 * undefined, so the host falls back rather than trusting a truncated
		 * answer, and the CPU reference applies the identical cap. */
		int op = ast_op(f->a, s);
		AstLocal cond = op == 4 ? ast_child(f->a, s, 1) : ast_child(f->a, s, 0);
		AstLocal body = op == 4 ? ast_child(f->a, s, 0) : ast_child(f->a, s, 1);
		uint32_t l_hdr = spv_id(m), l_test = spv_id(m), l_body = spv_id(m);
		uint32_t l_cont = spv_id(m), l_merge = spv_id(m);
		uint32_t i_phi = spv_id(m), d_phi = spv_id(m);
		uint32_t from_pre, i_next, d_body, ilt, go;
		uint32_t def_in = m->def;
		SpvV cv;

		from_pre = m->cur_label;
		spvw_op(&m->body, SpvOpBranch, 2);
		spvw_put(&m->body, l_hdr);

		spv_label_at(m, l_hdr);
		/* The two phis are emitted with placeholder operands and patched below,
		 * because their continue-edge values are not built yet. */
		spvw_op(&m->body, SpvOpPhi, 7);
		spvw_put(&m->body, m->id_int);
		spvw_put(&m->body, i_phi);
		spvw_put(&m->body, spv_const(m, 0));
		spvw_put(&m->body, from_pre);
		{
			int patch_i = (int)m->body.n;
			spvw_put(&m->body, 0);
			spvw_put(&m->body, l_cont);
			spvw_op(&m->body, SpvOpPhi, 7);
			spvw_put(&m->body, m->id_bool);
			spvw_put(&m->body, d_phi);
			spvw_put(&m->body, def_in);
			spvw_put(&m->body, from_pre);
			{
				int patch_d = (int)m->body.n;
				spvw_put(&m->body, 0);
				spvw_put(&m->body, l_cont);

				spvw_op(&m->body, SpvOpLoopMerge, 4);
				spvw_put(&m->body, l_merge);
				spvw_put(&m->body, l_cont);
				spvw_put(&m->body, 0);
				spvw_op(&m->body, SpvOpBranch, 2);
				spvw_put(&m->body, op == 4 ? l_body : l_test);

				if (op != 4) {
					spv_label_at(m, l_test);
					m->def = d_phi;
					if (!spv_expr(m, f->a, cond, f->slot, f->nslot, base, &cv))
						return 0;
					ilt = spv_emit3(m, SpvOpSLessThan, m->id_bool, i_phi,
													spv_const(m, MCC_SLICE_TRIP_MAX));
					go = spv_emit3(m, SpvOpLogicalAnd, m->id_bool,
												 spv_bool_of_v(m, cv), ilt);
					spvw_op(&m->body, SpvOpBranchConditional, 4);
					spvw_put(&m->body, go);
					spvw_put(&m->body, l_body);
					spvw_put(&m->body, l_merge);
				}

				spv_label_at(m, l_body);
				if (op == 4)
					m->def = d_phi;
				if (!mcc_slice_spv_seq(m, f, base, body))
					return 0;
				if (op == 3 && !mcc_slice_spv_seq(m, f, base, ast_child(f->a, s, 2)))
					return 0;
				spvw_op(&m->body, SpvOpBranch, 2);
				spvw_put(&m->body, l_cont);

				spv_label_at(m, l_cont);
				i_next = spv_emit3(m, SpvOpIAdd, m->id_int, i_phi, spv_const(m, 1));
				if (op == 4) {
					if (!spv_expr(m, f->a, cond, f->slot, f->nslot, base, &cv))
						return 0;
					ilt = spv_emit3(m, SpvOpSLessThan, m->id_bool, i_next,
													spv_const(m, MCC_SLICE_TRIP_MAX));
					go = spv_emit3(m, SpvOpLogicalAnd, m->id_bool,
												 spv_bool_of_v(m, cv), ilt);
					spvw_op(&m->body, SpvOpBranchConditional, 4);
					spvw_put(&m->body, go);
					spvw_put(&m->body, l_hdr);
					spvw_put(&m->body, l_merge);
				} else {
					spvw_op(&m->body, SpvOpBranch, 2);
					spvw_put(&m->body, l_hdr);
				}
				d_body = m->def;
				m->body.w[patch_i] = i_next;
				m->body.w[patch_d] = d_body;
			}
		}

		spv_label_at(m, l_merge);
		/* i_phi and ilt dominate the merge, so the over-budget test is available
		 * here without another phi. */
		ilt = spv_emit3(m, SpvOpSLessThan, m->id_bool, i_phi,
										spv_const(m, MCC_SLICE_TRIP_MAX));
		m->def = spv_emit3(m, SpvOpLogicalAnd, m->id_bool, d_phi, ilt);
		return 1;
	}
	if (ast_kind(f->a, s) == AST_If && ast_op(f->a, s) == 7)
		return 1;
	if (ast_kind(f->a, s) == AST_If && ast_op(f->a, s) == 0) {
		SpvV cv;
		uint32_t cb, l_then, l_else, l_merge, def_in, def_then, def_else;
		uint32_t from_then, from_else, dphi;
		if (!spv_expr(m, f->a, ast_child(f->a, s, 0), f->slot, f->nslot, base, &cv))
			return 0;
		cb = spv_bool_of_v(m, cv);
		l_then = spv_id(m);
		l_else = spv_id(m);
		l_merge = spv_id(m);
		spvw_op(&m->body, SpvOpSelectionMerge, 3);
		spvw_put(&m->body, l_merge);
		spvw_put(&m->body, 0);
		spvw_op(&m->body, SpvOpBranchConditional, 4);
		spvw_put(&m->body, cb);
		spvw_put(&m->body, l_then);
		spvw_put(&m->body, l_else);

		def_in = m->def;
		spv_label_at(m, l_then);
		m->def = def_in;
		if (!mcc_slice_spv_seq(m, f, base, ast_child(f->a, s, 1)))
			return 0;
		def_then = m->def;
		from_then = m->cur_label;
		spvw_op(&m->body, SpvOpBranch, 2);
		spvw_put(&m->body, l_merge);

		spv_label_at(m, l_else);
		m->def = def_in;
		if (ast_nchild(f->a, s) == 3 &&
				!mcc_slice_spv_seq(m, f, base, ast_child(f->a, s, 2)))
			return 0;
		def_else = m->def;
		from_else = m->cur_label;
		spvw_op(&m->body, SpvOpBranch, 2);
		spvw_put(&m->body, l_merge);

		spv_label_at(m, l_merge);
		dphi = spv_id(m);
		spvw_op(&m->body, SpvOpPhi, 7);
		spvw_put(&m->body, m->id_bool);
		spvw_put(&m->body, dphi);
		spvw_put(&m->body, def_then);
		spvw_put(&m->body, from_then);
		spvw_put(&m->body, def_else);
		spvw_put(&m->body, from_else);
		m->def = dphi;
		return 1;
	}

	if (ast_kind(f->a, s) == AST_Unary &&
			(ast_op(f->a, s) == TOK_INC || ast_op(f->a, s) == TOK_DEC)) {
		AstLocal t = ast_first_child(f->a, s);
		int delta = ast_op(f->a, s) == TOK_INC ? 1 : -1;
		SpvV cur;
		if (!mcc_slice_is_local_ref(f->a, t, &off))
			return 0;
		dt = ast_type_t(f->a, t);
		for (j = 0; j < f->nslot; j++)
			if (f->slot[j] == off)
				break;
		if (j == f->nslot)
			return 0;
		cur = spv_load_live_v(m, base, j, ast_eval_slice_is64(dt),
													(dt & VT_UNSIGNED) != 0);
		if (cur.w64) {
			SpvV one = spv_mk(spv_u2(m, spv_uintc(m, (uint32_t)(delta > 0 ? 1 : -1)),
															 spv_uintc(m, delta > 0 ? 0u : 0xffffffffu)),
												1, 0);
			cur = spv_add64(m, cur, one, (dt & VT_UNSIGNED) != 0);
		} else {
			cur = spv_mk(spv_emit3(m, SpvOpIAdd, m->id_int, spv_val_lo(m, cur),
														 spv_const(m, delta)),
									 0, (dt & VT_UNSIGNED) != 0);
		}
		spv_store_live_v(m, base, j, spv_fit_v(m, cur, dt));
		return 1;
	}
	d = ast_child(f->a, s, 0);
	v = ast_child(f->a, s, 1);
	dt = ast_type_t(f->a, d);
	if (!mcc_slice_is_local_ref(f->a, d, &off))
		return 0;
	if (!spv_expr(m, f->a, v, f->slot, f->nslot, base, &val))
		return 0;
	for (j = 0; j < f->nslot; j++)
		if (f->slot[j] == off)
			break;
	if (j == f->nslot)
		return 0;
	if (mcc_slice_mutate) {
		uint32_t p = spv_pair(m, val);
		uint32_t lo = spv_uop(m, SpvOpBitwiseXor, spv_lo(m, p), spv_uintc(m, 1));
		val = spv_mk(spv_u2(m, lo, spv_hi(m, p)), 1, 0);
	}
	spv_store_live_v(m, base, j, spv_fit_v(m, val, dt));
	return 1;
}

static int mcc_slice_frame_kernel_build(MccSliceFrame *f, MccSliceKernel *k) {
	uint32_t base;
	int i, j;
	/* A run with no stores but a Return still computes a value and is worth
	 * dispatching -- refusing it counted 174 accepted runs that were never
	 * built, never dispatched and never compared, which inflated the coverage
	 * figure 2.4x. */
	if (!f || !k || f->nslot < 1)
		return 0;
	if (f->nstmt < 1 && f->ret == AST_NONE)
		return 0;
	memset(k, 0, sizeof *k);
	for (i = 0; i < f->nslot; i++)
		k->off[i] = f->slot[i];
	k->nlive = f->nslot;
	k->wtype = VT_INT;
#if MCC_GPU_LANG_MSL
	(void)base; (void)j;
	return 0; /* Metal arm: frame stores not emitted yet. */
#else
	{
		SpvMod m;
		uint32_t *code;
		int nw = 0;
		spv_module_begin(&m, f->nslot);
		base = spv_main_begin(&m, f->nslot);
		for (i = 0; i < f->ntop; i++)
			if (!mcc_slice_spv_stmt(&m, f, base, f->top[i]) || m.failed) {
				spv_module_free(&m);
				return 0;
			}
		(void)j;
		/* The frame carries the stores; the out slots carry the Return value, if
		 * the run has one. spv_main_end already writes the defined flag, and
		 * spv_expr's own guards set it to 0 for a UB operand, so an undefined
		 * return reaches the host as undefined rather than as a plausible zero. */
		if (f->ret != AST_NONE) {
			SpvV rv;
			if (!spv_expr(&m, f->a, f->ret, f->slot, f->nslot, base, &rv) ||
					m.failed) {
				spv_module_free(&m);
				return 0;
			}
			spv_main_end(&m, m.lane, rv);
		} else {
			spv_main_end(&m, m.lane, spv_mk(spv_const(&m, 0), 0, 0));
		}
		code = spv_module_finish(&m, &nw);
		spv_module_free(&m);
		if (!code || nw <= 0) {
			free(code);
			return 0;
		}
		k->code.p = code;
		k->code.n = nw;
	}
#endif
	k->key = ast_slice_ident_hash(f->a, f->root) ^ ((uint64_t)f->nslot << 32) ^
					 (uint64_t)f->nstmt;
	if (!k->key)
		k->key = 1;
	mcc_slice_emit_count++;
	return 1;
}

/* One dispatch, ntuple independent frames. `frames` is ntuple * nslot int64,
 * seeded by the caller and overwritten in place with the device's result. */
static int mcc_slice_run_frame_gpu(MccSliceFrame *f, MccSliceKernel *k,
																	 int64_t *frames, int ntuple,
																	 int64_t *retval, unsigned char *retdef) {
	int32_t *buf, *ob = NULL;
	int t, j, rc;
	if (!f || !k || !k->code.p || !frames || ntuple < 1)
		return MCC_TASK_FAILED;
	if (retval || retdef) {
		ob = (int32_t *)malloc((size_t)ntuple * MCC_GPU_OUT_SLOTS * sizeof *ob);
		if (!ob)
			return MCC_TASK_FAILED;
	}
	buf = (int32_t *)malloc((size_t)ntuple * f->nslot * MCC_GPU_IN_SLOTS *
													sizeof *buf);
	if (!buf)
		return MCC_TASK_FAILED;
	for (t = 0; t < ntuple; t++)
		for (j = 0; j < f->nslot; j++) {
			int64_t v = frames[(long)t * f->nslot + j];
			long sp = ((long)t * f->nslot + j) * MCC_GPU_IN_SLOTS;
			buf[sp] = (int32_t)(uint32_t)(uint64_t)v;
			buf[sp + 1] = (int32_t)(uint32_t)((uint64_t)v >> 32);
		}
	rc = mcc_gpu_dispatch_rw2(k->code.p, k->code.n, buf, ntuple, f->nslot, ob);
	if (rc)
		mcc_slice_dispatch_count++;
	if (!rc) {
		free(buf);
		free(ob);
		return MCC_TASK_FAILED;
	}
	if (ob) {
		for (t = 0; t < ntuple; t++) {
			long sp = (long)t * MCC_GPU_OUT_SLOTS;
			int d = ob[sp + 2] != 0;
			uint64_t lo = (uint32_t)ob[sp], hi = (uint32_t)ob[sp + 1];
			if (retdef)
				retdef[t] = (unsigned char)(d ? 1 : 0);
			if (retval)
				retval[t] = d ? ast_eval_narrow((int64_t)(lo | (hi << 32)),
																				ast_eval_slice_is64(f->rettype),
																				(f->rettype & VT_UNSIGNED) != 0)
											: 0;
		}
	}
	for (t = 0; t < ntuple; t++)
		for (j = 0; j < f->nslot; j++) {
			long sp = ((long)t * f->nslot + j) * MCC_GPU_IN_SLOTS;
			uint64_t lo = (uint32_t)buf[sp];
			uint64_t hi = (uint32_t)buf[sp + 1];
			frames[(long)t * f->nslot + j] = (int64_t)(lo | (hi << 32));
		}
	free(buf);
	free(ob);
	return MCC_TASK_DONE;
}

static int mcc_slice_tick_gpu(MccTask *t) {
	MccSliceWork *w = (MccSliceWork *)t->ctx;
	return mcc_slice_run_gpu(w, w->kernel, w->budget);
}

static void mcc_slice_task_gpu(MccTask *t, MccSliceWork *w, MccSliceKernel *k,
															 int budget) {
	w->kernel = k;
	w->budget = budget;
	mcc_task_init(t, mcc_slice_tick_gpu, w);
}

#endif /* MCC_SLICE_GPU */

static int mcc_slice_tick_cpu(MccTask *t) {
	MccSliceWork *w = (MccSliceWork *)t->ctx;
	return mcc_slice_run_cpu(w, w->budget);
}

static void mcc_slice_task_cpu(MccTask *t, MccSliceWork *w, int budget) {
	w->budget = budget;
	mcc_task_init(t, mcc_slice_tick_cpu, w);
}

#endif
