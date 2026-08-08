#ifndef AST_EVAL_SLICE_PROVIDED
#define AST_EVAL_SLICE_PROVIDED 1

#include <limits.h>
#include <stdint.h>

#ifndef TOK_LAND
#define TOK_LAND 0x90
#endif
#ifndef TOK_LOR
#define TOK_LOR 0x91
#endif
#ifndef TOK_ULT
#define TOK_ULT 0x92
#endif
#ifndef TOK_UGE
#define TOK_UGE 0x93
#endif
#ifndef TOK_EQ
#define TOK_EQ 0x94
#endif
#ifndef TOK_NE
#define TOK_NE 0x95
#endif
#ifndef TOK_ULE
#define TOK_ULE 0x96
#endif
#ifndef TOK_UGT
#define TOK_UGT 0x97
#endif
#ifndef TOK_LT
#define TOK_LT 0x9c
#endif
#ifndef TOK_GE
#define TOK_GE 0x9d
#endif
#ifndef TOK_LE
#define TOK_LE 0x9e
#endif
#ifndef TOK_GT
#define TOK_GT 0x9f
#endif
#ifndef TOK_UDIV
#define TOK_UDIV 0x83
#endif
#ifndef TOK_UMOD
#define TOK_UMOD 0x84
#endif
#ifndef TOK_PDIV
#define TOK_PDIV 0x85
#endif
#ifndef TOK_SHL
#define TOK_SHL '<'
#endif
#ifndef TOK_SAR
#define TOK_SAR '>'
#endif
#ifndef TOK_SHR
#define TOK_SHR 0x8b
#endif

static int64_t ast_eval_narrow(int64_t x, int is64, int is_unsigned) {
	if (is64)
		return x;
	if (is_unsigned)
		return (int64_t)(uint32_t)x;
	return (int64_t)(int32_t)x;
}

static int ast_eval_binop(int op, int64_t a, int64_t b, int is64,
													int is_unsigned, int64_t *out) {
	a = ast_eval_narrow(a, is64, is_unsigned);
	b = ast_eval_narrow(b, is64, is_unsigned);
	uint64_t ua = (uint64_t)a, ub = (uint64_t)b;
	int64_t r;
	switch (op) {
	case '+':
		if (is_unsigned) {
			r = (int64_t)(ua + ub);
			break;
		}
		if (is64) {
			r = (int64_t)(ua + ub);
			if (((a ^ r) & (b ^ r)) < 0)
				return 0;
		} else {
			int64_t s = a + b;
			if (s < INT32_MIN || s > INT32_MAX)
				return 0;
			r = s;
		}
		break;
	case '-':
		if (is_unsigned) {
			r = (int64_t)(ua - ub);
			break;
		}
		if (is64) {
			r = (int64_t)(ua - ub);
			if (((a ^ b) & (a ^ r)) < 0)
				return 0;
		} else {
			int64_t s = a - b;
			if (s < INT32_MIN || s > INT32_MAX)
				return 0;
			r = s;
		}
		break;
	case '*':
		if (is_unsigned) {
			r = (int64_t)(ua * ub);
			break;
		}
		if (is64) {
			if (a != 0 && b != 0) {
				r = (int64_t)(ua * ub);
				if ((a == INT64_MIN && b == -1) || (b == INT64_MIN && a == -1) ||
						r / a != b)
					return 0;
			} else {
				r = 0;
			}
		} else {
			int64_t s = a * b;
			if (s < INT32_MIN || s > INT32_MAX)
				return 0;
			r = s;
		}
		break;
	case '/':
	case '%':
	case TOK_PDIV:
	case TOK_UDIV:
	case TOK_UMOD: {
		if (b == 0)
			return 0;
		int is_mod = (op == '%' || op == TOK_UMOD);
		int uns = is_unsigned || op == TOK_UDIV || op == TOK_UMOD;
		if (uns) {
			if (is64) {
				r = (int64_t)(is_mod ? ua % ub : ua / ub);
			} else {
				uint32_t x = (uint32_t)ua, y = (uint32_t)ub;
				r = (int64_t)(uint32_t)(is_mod ? x % y : x / y);
			}
		} else {
			int64_t mn = is64 ? INT64_MIN : INT32_MIN;
			if (a == mn && b == -1)
				return 0;
			int64_t q = a / b;
			r = is_mod ? a - b * q : q;
		}
		break;
	}
	case TOK_SHL:
	case TOK_SHR:
	case TOK_SAR: {
		int width = is64 ? 64 : 32;
		if (b < 0 || b >= width)
			return 0;
		unsigned sh = (unsigned)b;
		if (op == TOK_SHL) {
			if (is64)
				r = (int64_t)(ua << sh);
			else
				r = (int64_t)(int32_t)((uint32_t)ua << sh);
		} else if (op == TOK_SHR) {
			if (is64)
				r = (int64_t)(ua >> sh);
			else
				r = (int64_t)(uint32_t)((uint32_t)ua >> sh);
		} else {
			if (is64)
				r = a >> sh;
			else
				r = (int64_t)((int32_t)a >> sh);
		}
		break;
	}
	case '&':
		r = (int64_t)(ua & ub);
		break;
	case '|':
		r = (int64_t)(ua | ub);
		break;
	case '^':
		r = (int64_t)(ua ^ ub);
		break;
	case TOK_EQ:
		r = (a == b);
		break;
	case TOK_NE:
		r = (a != b);
		break;
	case TOK_LT:
		r = is_unsigned ? (ua < ub) : (a < b);
		break;
	case TOK_GE:
		r = is_unsigned ? (ua >= ub) : (a >= b);
		break;
	case TOK_LE:
		r = is_unsigned ? (ua <= ub) : (a <= b);
		break;
	case TOK_GT:
		r = is_unsigned ? (ua > ub) : (a > b);
		break;
	case TOK_ULT:
		r = (ua < ub);
		break;
	case TOK_UGE:
		r = (ua >= ub);
		break;
	case TOK_ULE:
		r = (ua <= ub);
		break;
	case TOK_UGT:
		r = (ua > ub);
		break;
	case TOK_LAND:
		r = (a != 0 && b != 0);
		break;
	case TOK_LOR:
		r = (a != 0 || b != 0);
		break;
	default:
		return 0;
	}
	*out = ast_eval_narrow(r, is64, is_unsigned);
	return 1;
}

#ifndef AST_EVAL_SLICE_KERNEL_ONLY

static int ast_eval_slice_env(const int32_t *off, const int64_t *val, int n,
															int32_t o, int64_t *out) {
	for (int i = 0; i < n; i++)
		if (off[i] == o) {
			*out = val[i];
			return 1;
		}
	return 0;
}

static int ast_eval_slice_intt(int t) {
	int bt = t & VT_BTYPE;
	return bt == VT_BOOL || bt == VT_BYTE || bt == VT_SHORT || bt == VT_INT ||
				 bt == VT_LLONG || bt == VT_PTR;
}

static int ast_eval_slice_is64(int t) {
	int bt = t & VT_BTYPE;
	return bt == VT_LLONG || (MCC_PTR_SIZE == 8 && bt == VT_PTR);
}

static int64_t ast_eval_slice_fit(int64_t x, int t) {
	int bt = t & VT_BTYPE;
	int uns = (t & VT_UNSIGNED) != 0;
	switch (bt) {
	case VT_BOOL:
		return x != 0;
	case VT_BYTE:
		return uns ? (int64_t)(uint8_t)x : (int64_t)(int8_t)x;
	case VT_SHORT:
		return uns ? (int64_t)(uint16_t)x : (int64_t)(int16_t)x;
	case VT_INT:
		return uns ? (int64_t)(uint32_t)x : (int64_t)(int32_t)x;
	default:
		if (MCC_PTR_SIZE == 8 && bt == VT_PTR)
			return x;
		if (bt == VT_PTR)
			return uns ? (int64_t)(uint32_t)x : (int64_t)(int32_t)x;
		return x;
	}
}

/* B1: a runtime index into a local array.
 *
 * `arr[i]` reaches the arena as Load(Binary('+')(Ref[VT_LOCAL|VT_ARRAY], i)),
 * and the two facts that decide how it can be handled are both measured rather
 * than assumed. First, the '+' replays through gen_op on an array base, so `i`
 * is an ELEMENT index, not a byte offset. Second, the Binary and the Load above
 * it carry type 0 in every real arena -- so the access width is not written
 * down anywhere in the tree, and neither is the element count needed to bound
 * the index. The base Ref's extent column gives the object's total size; the
 * element type word gives the rest. Both arrive through this hook, which is
 * NULL in the compiler itself (where nothing runs frames) and is installed by
 * the runner from the arena dump's extent and element-type columns.
 *
 * A consumer that cannot resolve an object refuses the shape. It never guesses
 * a width, because a guessed width is a wrong answer that both executors would
 * agree on -- the exact failure a differential cannot see. */
static int (*ast_eval_slice_obj_fn)(AstArena *a, AstLocal n, int32_t *extent,
																		int *etype);

static int ast_eval_slice_wtype(AstArena *a, AstLocal n);

static int ast_eval_slice_tsize(int t) {
	switch (t & VT_BTYPE) {
	case VT_BOOL:
	case VT_BYTE:
		return 1;
	case VT_SHORT:
		return 2;
	case VT_INT:
		return 4;
	case VT_LLONG:
		return 8;
	case VT_PTR:
		return MCC_PTR_SIZE;
	default:
		return 0;
	}
}

/* ---- byte-addressed access to a storage region ------------------------- *
 *
 * The reference half of the per-width load/store. A region is (words, nbyte):
 * a run of 32-bit words and how many bytes of it are addressable, and an
 * address is a byte offset into it. Nothing here knows or cares whether the
 * region is one lane's frame or a buffer shared by every lane and the host --
 * that distinction is entirely in who supplies the base, which is why the
 * device side takes the same four things as parameters.
 *
 * Everything is expressed in words and shifts rather than in bytes of host
 * memory, so it is byte-order independent and matches the device's arithmetic
 * instruction for instruction rather than merely in result.
 *
 * The access rule is one comparison, and it is what makes an out-of-region
 * access impossible rather than merely detected: an offset that is out of
 * range or misaligned is replaced by 0, which is in range for every region
 * large enough to hold the access at all, and the definedness flag is cleared.
 * A select is used rather than the power-of-two mask the plan proposed because
 * the mask needs the region padded up to a power of two -- and a region that
 * is not padded (say 48 bytes) would let a masked offset reach byte 63, i.e.
 * into whatever follows. Corrupting a neighbour is strictly worse than
 * corrupting yourself, and the select has no such precondition. */
static int ast_eval_slice_addr_ok(int32_t nbyte, int32_t off, int width) {
	if (width <= 0 || off < 0 || nbyte < width)
		return 0;
	if (off > nbyte - width)
		return 0;
	return (off & (width - 1)) == 0;
}

static int32_t ast_eval_slice_addr_fix(int32_t nbyte, int32_t off, int width) {
	return ast_eval_slice_addr_ok(nbyte, off, width) ? off : 0;
}

static int64_t ast_eval_slice_bytes_load(const uint32_t *w, int32_t nbyte,
																				 int32_t off, int t, int *ok) {
	int width = ast_eval_slice_tsize(t);
	int uns = (t & VT_UNSIGNED) != 0;
	uint32_t lo;
	int sh;
	if (ok)
		*ok = ast_eval_slice_addr_ok(nbyte, off, width);
	off = ast_eval_slice_addr_fix(nbyte, off, width);
	if (width <= 0)
		return 0;
	if (width == 8)
		return (int64_t)((uint64_t)w[off >> 2] |
										 ((uint64_t)w[(off >> 2) + 1] << 32));
	sh = (off & 3) * 8;
	lo = w[off >> 2] >> sh;
	if (width == 1)
		return uns ? (int64_t)(uint8_t)lo : (int64_t)(int8_t)lo;
	if (width == 2)
		return uns ? (int64_t)(uint16_t)lo : (int64_t)(int16_t)lo;
	return uns ? (int64_t)(uint32_t)lo : (int64_t)(int32_t)lo;
}

static void ast_eval_slice_bytes_store(uint32_t *w, int32_t nbyte, int32_t off,
																			 int t, int64_t v, int *ok) {
	int width = ast_eval_slice_tsize(t);
	uint32_t keep, put;
	int sh;
	if (ok)
		*ok = ast_eval_slice_addr_ok(nbyte, off, width);
	off = ast_eval_slice_addr_fix(nbyte, off, width);
	if (width <= 0)
		return;
	if (width == 8) {
		w[off >> 2] = (uint32_t)(uint64_t)v;
		w[(off >> 2) + 1] = (uint32_t)((uint64_t)v >> 32);
		return;
	}
	if (width == 4) {
		w[off >> 2] = (uint32_t)(uint64_t)v;
		return;
	}
	/* A sub-word store is a read-modify-write of the containing word, because
	 * SPIR-V has no 8- or 16-bit storage without StorageBuffer8BitAccess. Within
	 * one lane's own region that is exactly a narrow store; in a region shared
	 * between lanes it is not atomic, and two lanes writing different bytes of
	 * one word would race. That is a property of the region, not of this code,
	 * and it has to be settled by whoever hands out shared addresses. */
	sh = (off & 3) * 8;
	keep = width == 1 ? 0xFFu : 0xFFFFu;
	put = ((uint32_t)(uint64_t)v & keep) << sh;
	w[off >> 2] = (w[off >> 2] & ~(keep << sh)) | put;
}

typedef struct AstEvalSliceIdx {
	int32_t base;
	int32_t esize;
	int etype;
	int nelem;
	int nspan;
	AstLocal idx;
} AstEvalSliceIdx;

/* The whole span is rounded up to a power of two so that masking a wild index
 * into it needs one AND and cannot leave the object. That is what makes J3b's
 * "no PageFault is reachable" argument close by construction rather than by
 * detection: the worst an out-of-range access can do is touch another element
 * of the same array, and the run is discarded anyway because its definedness
 * flag was cleared by the same comparison that produced the mask. */
static int ast_eval_slice_dynidx(AstArena *a, AstLocal n, AstEvalSliceIdx *o) {
	AstLocal x, y, base, idx;
	int32_t extent = 0;
	int etype = 0, r, it;
	if (n == AST_NONE || !ast_eval_slice_obj_fn)
		return 0;
	if (ast_kind(a, n) != AST_Binary || ast_op(a, n) != '+' ||
			ast_nchild(a, n) != 2)
		return 0;
	x = ast_child(a, n, 0);
	y = ast_child(a, n, 1);
	if (ast_kind(a, x) == AST_Ref && (ast_type_t(a, x) & VT_ARRAY)) {
		base = x;
		idx = y;
	} else if (ast_kind(a, y) == AST_Ref && (ast_type_t(a, y) & VT_ARRAY)) {
		base = y;
		idx = x;
	} else {
		return 0;
	}
	r = ast_op(a, base);
	if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
		return 0;
	if (!ast_eval_slice_obj_fn(a, base, &extent, &etype))
		return 0;
	o->esize = ast_eval_slice_tsize(etype);
	if (!o->esize || (etype & VT_ARRAY) || is_float(etype) ||
			!ast_eval_slice_intt(etype))
		return 0;
	if (extent < o->esize || extent % o->esize)
		return 0;
	/* A 64-bit index would have to agree bit for bit between a host int64 and a
	 * device lo/hi pair before the mask is applied; every index in the corpus is
	 * a plain int, so the wide case is refused rather than approximated. */
	it = ast_eval_slice_wtype(a, idx);
	if (!it || is_float(it) || !ast_eval_slice_intt(it) ||
			ast_eval_slice_is64(it))
		return 0;
	o->base = (int32_t)(int64_t)ast_ival(a, base);
	o->etype = etype;
	o->nelem = (int)(extent / o->esize);
	o->nspan = 1;
	while (o->nspan < o->nelem)
		o->nspan <<= 1;
	o->idx = idx;
	return o->nelem >= 1;
}

/* The verdict both executors must reach identically: in range or not, and the
 * masked element either way. Unsigned, so a negative index is out of range by
 * the same single comparison. */
static int ast_eval_slice_idx_ok(const AstEvalSliceIdx *o, int64_t v,
																 int *elem) {
	uint32_t u = (uint32_t)(int32_t)v;
	*elem = (int)(u & (uint32_t)(o->nspan - 1));
	return u < (uint32_t)o->nelem;
}

static int ast_eval_slice_wtype(AstArena *a, AstLocal n) {
	int t;
	if (n == AST_NONE)
		return 0;
	t = ast_type_t(a, n);
	if (!ast_bad_type(t) && !is_float(t) && ast_eval_slice_intt(t))
		return t;
	switch (ast_kind(a, n)) {
	case AST_Binary:
		if (ast_nchild(a, n) >= 1) {
			int wt = ast_eval_slice_wtype(a, ast_child(a, n, 0));
			if (wt)
				return wt;
			if (ast_nchild(a, n) >= 2)
				return ast_eval_slice_wtype(a, ast_child(a, n, 1));
		}
		return 0;
	case AST_Load: {
		/* An indexed element is the one node whose own type word is 0 in every
		 * real arena, so its width has to come from the object it indexes. */
		AstEvalSliceIdx ix;
		if (ast_eval_slice_dynidx(a, ast_first_child(a, n), &ix))
			return ix.etype;
		return 0;
	}
	case AST_Unary:
		return ast_eval_slice_wtype(a, ast_first_child(a, n));
	case AST_If:
		if (ast_nchild(a, n) == 3) {
			int wt = ast_eval_slice_wtype(a, ast_child(a, n, 1));
			return wt ? wt : ast_eval_slice_wtype(a, ast_child(a, n, 2));
		}
		return 0;
	default:
		return 0;
	}
}

/* A frame address that is a constant offset from a local: the local itself, or
 * a chain of `.field` / `&` over one. Measured over 344 real bodies, this
 * resolves 244 of 275 AST_OP_MEMBER nodes (88.7%) and 222 of 312 AST_OP_ADDR
 * (71.2%). AST_OP_MEMBER_ARROW resolves 0 of 59 and never can -- its replay
 * does indir() first (src/mccast.c:5177), i.e. it loads a pointer, and no
 * amount of constant folding crosses that.
 *
 * The point of doing it this way is that the resolved offset is just another
 * frame-slot key, so the existing (off[], val[]) environment carries it with no
 * ABI change and the device sees the same constant OpAccessChain it already
 * emits for a plain local. This only ever turns a refusal into an acceptance --
 * every shape it admits is one the predicates below reject today. */
#define AST_EVAL_OP_ADDR 0x40000
#define AST_EVAL_OP_MEMBER 0x40001

static int ast_eval_slice_frame_off(AstArena *a, AstLocal n, int32_t *off,
																		int depth) {
	int r;
	if (n == AST_NONE || depth > 6)
		return 0;
	if (ast_kind(a, n) == AST_Ref) {
		r = ast_op(a, n);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
			return 0;
		*off = (int32_t)(int64_t)ast_ival(a, n);
		return 1;
	}
	if (ast_kind(a, n) == AST_Unary &&
			(ast_op(a, n) == AST_EVAL_OP_MEMBER || ast_op(a, n) == AST_EVAL_OP_ADDR)) {
		int32_t base = 0;
		if (!ast_eval_slice_frame_off(a, ast_first_child(a, n), &base, depth + 1))
			return 0;
		*off = base + (ast_op(a, n) == AST_EVAL_OP_MEMBER
											 ? (int32_t)(int64_t)ast_ival(a, n)
											 : 0);
		return 1;
	}
	/* `base + K` with a literal K, folding to a constant offset exactly as
	 * `.field` does -- but only where the '+' is plain arithmetic on a byte
	 * address. When the base is a pointer or an array, gen_op scales K by the
	 * ELEMENT size at replay, so folding K as a byte offset is simply the wrong
	 * address; that case belongs to ast_eval_slice_dynidx, which knows the
	 * element size and handles a literal index as one more runtime index. */
	if (ast_kind(a, n) == AST_Binary && ast_op(a, n) == '+' &&
			ast_nchild(a, n) == 2) {
		AstLocal x = ast_child(a, n, 0), y = ast_child(a, n, 1);
		int32_t base = 0;
		AstLocal b = AST_NONE, k = AST_NONE;
		if (ast_eval_slice_frame_off(a, x, &base, depth + 1)) {
			b = x;
			k = y;
		} else if (ast_eval_slice_frame_off(a, y, &base, depth + 1)) {
			b = y;
			k = x;
		} else {
			return 0;
		}
		if ((ast_type_t(a, b) & VT_ARRAY) ||
				(ast_type_t(a, b) & VT_BTYPE) == VT_PTR)
			return 0;
		if (ast_kind(a, k) != AST_Literal)
			return 0;
		if ((ast_op(a, k) & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
			return 0;
		*off = base + (int32_t)(int64_t)ast_ival(a, k);
		return 1;
	}
	return 0;
}

/* Sticky, and cleared by the caller that cares. An out-of-range index does not
 * stop evaluation -- the device cannot stop either -- so the fact that one
 * happened has to travel beside the value rather than instead of it. */
static int ast_eval_slice_undef;

static int ast_eval_slice_rec(AstArena *a, AstLocal n, const int32_t *off,
															const int64_t *val, int nenv, int64_t *out) {
	if (n == AST_NONE)
		return 0;
	switch (ast_kind(a, n)) {
	case AST_Literal: {
		int t = ast_type_t(a, n);
		if (ast_bad_type(t) || is_float(t) || !ast_eval_slice_intt(t))
			return 0;
		if ((ast_op(a, n) & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
			return 0;
		*out = ast_eval_slice_fit((int64_t)ast_ival(a, n), t);
		return 1;
	}
	case AST_Ref: {
		int r = ast_op(a, n);
		int t = ast_type_t(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
			int64_t v;
			if (!ast_eval_slice_env(off, val, nenv,
															(int32_t)(int64_t)ast_ival(a, n), &v))
				return 0;
			if (!ast_eval_slice_intt(t) || is_float(t))
				return 0;
			/* Reading a live-in through a typed Ref yields a value *of that type*,
			 * which is what the Literal arm below already does and what the device
			 * does (spv_load_live_v is handed the ref's own type). Returning the raw
			 * environment word instead diverged for any narrow or unsigned live-in:
			 * a u32 ref holding -12345 read as -12345 here and as 4294954951 on the
			 * device, and both then widened correctly from different starting
			 * points. The ladder never saw it because ast_ladder_gpu_run pre-fits
			 * every input to its live-in's type, so fit here is idempotent for that
			 * caller -- but the precondition was unstated and nothing enforced it. */
			*out = ast_eval_slice_fit(v, t);
			return 1;
		}
		if ((r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
			if (ast_bad_type(t) || is_float(t) || !ast_eval_slice_intt(t))
				return 0;
			*out = ast_eval_slice_fit((int64_t)ast_ival(a, n), t);
			return 1;
		}
		return 0;
	}
	case AST_Load: {
		AstLocal c = ast_first_child(a, n);
		int t = ast_type_t(a, n);
		int32_t fo;
		int64_t v;
		AstEvalSliceIdx ix;
		if (c == AST_NONE)
			return 0;
		/* A local, or a constant offset from one via `.field`/`&`. Both are just
		 * frame-slot keys in the same numbering, so the environment carries them
		 * unchanged. */
		if (ast_eval_slice_intt(t) && !is_float(t) &&
				ast_eval_slice_frame_off(a, c, &fo, 0)) {
			if (!ast_eval_slice_env(off, val, nenv, fo, &v))
				return 0;
			*out = ast_eval_slice_fit(v, t);
			return 1;
		}
		/* An indexed element. Out of range is not a refusal here: the device
		 * cannot refuse mid-kernel, it masks and clears its definedness flag, so
		 * the reference has to read the same masked element and report the same
		 * verdict or the two disagree exactly at the boundary. The verdict is
		 * carried by ast_eval_slice_undef rather than by the return value. */
		if (ast_eval_slice_dynidx(a, c, &ix)) {
			int64_t iv;
			int elem;
			if (!ast_eval_slice_rec(a, ix.idx, off, val, nenv, &iv))
				return 0;
			if (!ast_eval_slice_idx_ok(&ix, iv, &elem))
				ast_eval_slice_undef = 1;
			if (!ast_eval_slice_env(off, val, nenv,
															ix.base + (int32_t)elem * ix.esize, &v))
				return 0;
			*out = ast_eval_slice_fit(v, ix.etype);
			return 1;
		}
		return 0;
	}
	case AST_Convert: {
		int t = ast_type_t(a, n);
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || is_float(t) || is_float(ast_type_t(a, c)))
			return 0;
		if (ast_bad_type(t) || !ast_eval_slice_intt(t))
			return 0;
		int64_t v;
		if (!ast_eval_slice_rec(a, c, off, val, nenv, &v))
			return 0;
		*out = ast_eval_slice_fit(v, t);
		return 1;
	}
	case AST_Unary: {
		int uop = ast_op(a, n);
		int t = ast_eval_slice_wtype(a, n);
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || !t)
			return 0;
		if (uop != '-' && uop != TOK_NEG && uop != '~' && uop != '!')
			return 0;
		int64_t v;
		if (!ast_eval_slice_rec(a, c, off, val, nenv, &v))
			return 0;
		int is64 = ast_eval_slice_is64(t);
		int uns = (t & VT_UNSIGNED) != 0;
		if (uop == '!') {
			*out = (v == 0);
			return 1;
		}
		if (uop == '~') {
			*out = ast_eval_slice_fit(~v, t);
			return 1;
		}
		return ast_eval_binop('-', 0, v, is64, uns, out);
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		if (bop == TOK_LAND || bop == TOK_LOR) {
			int want = (bop == TOK_LAND);
			uint32_t nc = ast_nchild(a, n);
			for (uint32_t k = 0; k < nc; k++) {
				int64_t v;
				if (!ast_eval_slice_rec(a, ast_child(a, n, k), off, val, nenv, &v))
					return 0;
				if ((v != 0) != want) {
					*out = want ? 0 : 1;
					return 1;
				}
			}
			*out = want ? 1 : 0;
			return 1;
		}
		if (ast_nchild(a, n) != 2)
			return 0;
		AstLocal x = ast_child(a, n, 0), y = ast_child(a, n, 1);
		int xt = ast_eval_slice_wtype(a, x);
		if (!xt || is_float(ast_type_t(a, x)) || is_float(ast_type_t(a, y)))
			return 0;
		int64_t lv, rv;
		if (!ast_eval_slice_rec(a, x, off, val, nenv, &lv))
			return 0;
		if (!ast_eval_slice_rec(a, y, off, val, nenv, &rv))
			return 0;
		int is64 = ast_eval_slice_is64(xt);
		int uns = (xt & VT_UNSIGNED) != 0;
		return ast_eval_binop(bop, lv, rv, is64, uns, out);
	}
	case AST_If: {
		if (ast_nchild(a, n) != 3)
			return 0;
		int64_t cv;
		if (!ast_eval_slice_rec(a, ast_child(a, n, 0), off, val, nenv, &cv))
			return 0;
		AstLocal taken = cv != 0 ? ast_child(a, n, 1) : ast_child(a, n, 2);
		return ast_eval_slice_rec(a, taken, off, val, nenv, out);
	}
	default:
		return 0;
	}
}

static int ast_eval_slice(AstArena *a, AstLocal node, const int32_t *off,
													const int64_t *val, int n, int64_t *out) {
	return ast_eval_slice_rec(a, node, off, val, n, out);
}

#define AST_EVAL_SLICE_MAXRET 64
#define AST_EVAL_SLICE_SAMPLE_CAP 8
#define AST_EVAL_SLICE_DOMAIN_CAP 4096

static int ast_eval_slice_returns(AstArena *a, AstLocal *out, int max) {
	int n = 0;
	AstLocal cnt = ast_count(a);
	for (AstLocal r = 0; r < cnt; r++) {
		if (ast_kind(a, r) != AST_Return || ast_nchild(a, r) != 1)
			continue;
		AstLocal v = ast_first_child(a, r);
		if (v != AST_NONE && n < max)
			out[n++] = v;
	}
	return n;
}

static int ast_eval_slice_env_ok(AstArena *base, AstArena *spec, const int32_t *off,
																 const int64_t *val, int nenv) {
	AstLocal brets[AST_EVAL_SLICE_MAXRET], srets[AST_EVAL_SLICE_MAXRET];
	int64_t bvals[AST_EVAL_SLICE_MAXRET];
	int nb = ast_eval_slice_returns(base, brets, AST_EVAL_SLICE_MAXRET);
	int ns = ast_eval_slice_returns(spec, srets, AST_EVAL_SLICE_MAXRET);
	int nbv = 0;
	for (int i = 0; i < nb; i++) {
		int64_t v;
		if (ast_eval_slice(base, brets[i], off, val, nenv, &v))
			bvals[nbv++] = v;
	}
	if (nbv == 0)
		return 1;
	for (int i = 0; i < ns; i++) {
		int64_t sv;
		if (!ast_eval_slice(spec, srets[i], off, val, nenv, &sv))
			continue;
		int found = 0;
		for (int j = 0; j < nbv; j++)
			if (bvals[j] == sv) {
				found = 1;
				break;
			}
		if (!found)
			return 0;
	}
	return 1;
}

static int ast_eval_slice_sound(AstArena *base, AstArena *spec, int mode,
																const int *offs, const int64_t *pvals,
																const int64_t *plos, const int64_t *phis,
																int npoff, int maxp) {
	if (npoff <= 0 || npoff > maxp || npoff > AST_EVAL_SLICE_MAXRET)
		return 1;
	int32_t soff[AST_EVAL_SLICE_MAXRET];
	int64_t sval[AST_EVAL_SLICE_MAXRET];
	for (int i = 0; i < npoff; i++)
		soff[i] = offs[i];
	if (mode == 4) {
		for (int i = 0; i < npoff; i++)
			sval[i] = pvals[i];
		return ast_eval_slice_env_ok(base, spec, soff, sval, npoff);
	}
	if (mode != 5)
		return 1;
	int64_t card = 1;
	for (int i = 0; i < npoff; i++) {
		if (plos[i] > phis[i])
			return 1;
		uint64_t w = (uint64_t)(phis[i] - plos[i]) + 1;
		if (w > (uint64_t)AST_EVAL_SLICE_DOMAIN_CAP)
			return 1;
		card *= (int64_t)w;
		if (card > AST_EVAL_SLICE_DOMAIN_CAP)
			return 1;
	}
	int k[AST_EVAL_SLICE_MAXRET];
	int64_t sp[AST_EVAL_SLICE_MAXRET][4];
	int64_t total = 1;
	for (int i = 0; i < npoff; i++) {
		int64_t lo = plos[i], hi = phis[i];
		int c = 0;
		sp[i][c++] = lo;
		if (hi != lo)
			sp[i][c++] = hi;
		if (hi - lo >= 2)
			sp[i][c++] = lo + (hi - lo) / 2;
		if (hi - lo >= 3)
			sp[i][c++] = lo + (hi - lo) / 3;
		k[i] = c;
		total *= c;
	}
	int lim = total < AST_EVAL_SLICE_SAMPLE_CAP ? (int)total : AST_EVAL_SLICE_SAMPLE_CAP;
	for (int s = 0; s < lim; s++) {
		int rem = s;
		for (int i = 0; i < npoff; i++) {
			int idx = rem % k[i];
			rem /= k[i];
			sval[i] = sp[i][idx];
		}
		if (!ast_eval_slice_env_ok(base, spec, soff, sval, npoff))
			return 0;
	}
	return 1;
}

static int ast_eval_slice_kind_ok(AstArena *a, AstLocal n, int allow_load) {
	if (n == AST_NONE)
		return 0;
	switch (ast_kind(a, n)) {
	case AST_Literal: {
		int t = ast_type_t(a, n);
		return !ast_bad_type(t) && !is_float(t) && ast_eval_slice_intt(t) &&
					 (ast_op(a, n) & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
	}
	case AST_Ref: {
		int r = ast_op(a, n), t = ast_type_t(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM))
			return ast_eval_slice_intt(t) && !is_float(t);
		if ((r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST)
			return !ast_bad_type(t) && !is_float(t) && ast_eval_slice_intt(t);
		return 0;
	}
	case AST_Load: {
		AstLocal c = ast_first_child(a, n);
		int t = ast_type_t(a, n);
		int32_t fo;
		AstEvalSliceIdx ix;
		if (!allow_load)
			return 0;
		if (c == AST_NONE)
			return 0;
		if (!ast_bad_type(t) && !is_float(t) && ast_eval_slice_intt(t) &&
				ast_eval_slice_frame_off(a, c, &fo, 0))
			return 1;
		/* Gated on allow_load, which is exactly the frame runner: the ladder and
		 * the expression slicer both pass 0, so widening this cannot change what
		 * either of them accepts. */
		return ast_eval_slice_dynidx(a, c, &ix) &&
					 ast_eval_slice_kind_ok(a, ix.idx, allow_load);
	}
	case AST_Convert: {
		int t = ast_type_t(a, n);
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || is_float(t) || is_float(ast_type_t(a, c)))
			return 0;
		if (ast_bad_type(t) || !ast_eval_slice_intt(t))
			return 0;
		return ast_eval_slice_kind_ok(a, c, allow_load);
	}
	case AST_Unary: {
		int uop = ast_op(a, n), t = ast_eval_slice_wtype(a, n);
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || !t)
			return 0;
		if (uop != '-' && uop != TOK_NEG && uop != '~' && uop != '!')
			return 0;
		return ast_eval_slice_kind_ok(a, c, allow_load);
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		AstLocal x, y;
		if (bop == TOK_LAND || bop == TOK_LOR) {
			uint32_t nc = ast_nchild(a, n), k;
			for (k = 0; k < nc; k++)
				if (!ast_eval_slice_kind_ok(a, ast_child(a, n, k), allow_load))
					return 0;
			return 1;
		}
		if (ast_nchild(a, n) != 2)
			return 0;
		x = ast_child(a, n, 0);
		y = ast_child(a, n, 1);
		if (!ast_eval_slice_wtype(a, x) || is_float(ast_type_t(a, x)) ||
				is_float(ast_type_t(a, y)))
			return 0;
		return ast_eval_slice_kind_ok(a, x, allow_load) &&
					 ast_eval_slice_kind_ok(a, y, allow_load);
	}
	case AST_If: {
		if (ast_nchild(a, n) != 3 || (ast_op(a, n) != 5 && ast_op(a, n) != 7))
			return 0;
		return ast_eval_slice_kind_ok(a, ast_child(a, n, 0), allow_load) &&
					 ast_eval_slice_kind_ok(a, ast_child(a, n, 1), allow_load) &&
					 ast_eval_slice_kind_ok(a, ast_child(a, n, 2), allow_load);
	}
	default:
		return 0;
	}
}

/* An indexed object takes a whole run of consecutive slots, one per element,
 * padded up to the masked span. Consecutive is the load-bearing word: the
 * device resolves an element as `slot_of(base) + index` at run time, which is
 * only the right slot if the run was laid out in order and nothing was
 * interleaved into it. So a run that would overlap an already-mapped offset is
 * refused rather than reordered.
 *
 * One slot per element, rather than a byte-addressed region, is what keeps the
 * existing two-word store correct: slots stay disjoint 8-byte cells, so
 * spv_store_live_v's lo/hi pair cannot land on a neighbour and no per-width
 * store or sub-word shift/mask is needed anywhere. */
static int ast_eval_slice_livein_obj(const AstEvalSliceIdx *o, int32_t *offs,
																		 int *cnt, int max) {
	int i, k;
	for (i = 0; i < *cnt; i++)
		if (offs[i] == o->base) {
			if (i + o->nspan > *cnt)
				return 0;
			for (k = 0; k < o->nspan; k++)
				if (offs[i + k] != o->base + (int32_t)k * o->esize)
					return 0;
			return 1;
		}
	if (*cnt + o->nspan > max)
		return 0;
	for (k = 0; k < o->nspan; k++) {
		int32_t e = o->base + (int32_t)k * o->esize;
		for (i = 0; i < *cnt; i++)
			if (offs[i] == e)
				return 0;
		offs[(*cnt)++] = e;
	}
	return 1;
}

static int ast_eval_slice_livein(AstArena *a, AstLocal n, int32_t *offs, int *cnt,
																 int max) {
	AstLocal c;
	int i, r;
	int32_t o;
	if (n == AST_NONE)
		return 1;
	if (ast_kind(a, n) == AST_Ref) {
		r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
			o = (int32_t)(int64_t)ast_ival(a, n);
			for (i = 0; i < *cnt; i++)
				if (offs[i] == o)
					return 1;
			if (*cnt >= max)
				return 0;
			offs[(*cnt)++] = o;
		}
		return 1;
	}
	if (ast_kind(a, n) == AST_Load) {
		AstEvalSliceIdx ix;
		c = ast_first_child(a, n);
		if (ast_eval_slice_dynidx(a, c, &ix))
			return ast_eval_slice_livein_obj(&ix, offs, cnt, max) &&
						 ast_eval_slice_livein(a, ix.idx, offs, cnt, max);
		if (c != AST_NONE && ast_kind(a, c) == AST_Unary &&
				ast_eval_slice_frame_off(a, c, &o, 0)) {
			for (i = 0; i < *cnt; i++)
				if (offs[i] == o)
					return 1;
			if (*cnt >= max)
				return 0;
			offs[(*cnt)++] = o;
			return 1;
		}
		if (c != AST_NONE && ast_kind(a, c) == AST_Ref) {
			r = ast_op(a, c);
			if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
				o = (int32_t)(int64_t)ast_ival(a, c);
				for (i = 0; i < *cnt; i++)
					if (offs[i] == o)
						return 1;
				if (*cnt >= max)
					return 0;
				offs[(*cnt)++] = o;
			}
		}
		return 1;
	}
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (!ast_eval_slice_livein(a, c, offs, cnt, max))
			return 0;
	return 1;
}

static int ast_eval_slice_equiv_seed(AstArena *a, AstLocal aroot, AstArena *b,
																		 AstLocal broot) {
	static const int64_t seed[8] = {0, 1, -1, 2, 7, -3, 100, 12345};
	int32_t offs[AST_EVAL_SLICE_MAXRET];
	int64_t val[AST_EVAL_SLICE_MAXRET];
	int no = 0, s, i, evaluated = 0;
	if (!ast_eval_slice_kind_ok(a, aroot, 0) ||
			!ast_eval_slice_kind_ok(b, broot, 0))
		return 0;
	if (!ast_eval_slice_livein(a, aroot, offs, &no, AST_EVAL_SLICE_MAXRET))
		return 0;
	if (!ast_eval_slice_livein(b, broot, offs, &no, AST_EVAL_SLICE_MAXRET))
		return 0;
	for (s = 0; s < 8; s++) {
		int64_t av, bv;
		for (i = 0; i < no; i++)
			val[i] = seed[(i + s) & 7];
		if (!ast_eval_slice(a, aroot, offs, val, no, &av))
			continue;
		if (!ast_eval_slice(b, broot, offs, val, no, &bv))
			return 0;
		if (av != bv)
			return 0;
		evaluated++;
	}
	return evaluated > 0;
}

#define AST_EVAL_LADDER_MAXIN 32
#define AST_EVAL_LADDER_NRUNG 6
#define AST_EVAL_LADDER_CORNERS 6
#define AST_EVAL_LADDER_OBSMAX 12
#define AST_EVAL_LADDER_DEFAULT_BUDGET 1048576UL

enum {
	AST_LADDER_RUNG_NONE = -4,
	AST_LADDER_RUNG_OBSERVED = -2,
	AST_LADDER_RUNG_CORNERS = -1,
	AST_LADDER_RUNG_CONST = 0
};

enum {
	AST_LADDER_R_NONE = 0,
	AST_LADDER_R_STRUCT,
	AST_LADDER_R_OP,
	AST_LADDER_R_TYPE,
	AST_LADDER_R_ARITY,
	AST_LADDER_R_BUDGET,
	AST_LADDER_R_VACUOUS,
	AST_LADDER_R_OBS
};

typedef struct AstEvalLadderIn {
	int32_t off;
	int type;
	int bits;
	int uns;
} AstEvalLadderIn;

typedef struct AstEvalLadderRes {
	int verdict;
	int reason;
	int rung;
	int diff_width;
	int type_complete;
	int nin;
	unsigned long inferred;
	unsigned long points;
	unsigned long informative;
	unsigned long vacuous;
	int64_t diff_in[AST_EVAL_LADDER_MAXIN];
	int64_t diff_a;
	int64_t diff_b;
	int diff_b_undef;
} AstEvalLadderRes;

typedef int (*AstEvalLadderObsFn)(const int32_t *offs, int n, int64_t *tuples,
																	int maxtuples, void *user);

static AstEvalLadderObsFn ast_eval_ladder_obs_fn;
static void *ast_eval_ladder_obs_user;

static int ast_eval_ladder_enabled = -1;
static int ast_eval_ladder_strict_v;
static unsigned long ast_eval_ladder_budget_v;

static void ast_eval_ladder_config(void) {
	if (ast_eval_ladder_enabled >= 0)
		return;
	ast_eval_ladder_enabled = mcc_env_on("MCC_AST_EVAL_LADDER");
	ast_eval_ladder_strict_v = mcc_env_on("MCC_AST_EVAL_LADDER_STRICT_TYPE");
	ast_eval_ladder_budget_v =
			(unsigned long)mcc_env_num("MCC_AST_EVAL_LADDER_BUDGET",
																 (long)AST_EVAL_LADDER_DEFAULT_BUDGET);
}

static int ast_eval_ladder_strict(void) {
	ast_eval_ladder_config();
	return ast_eval_ladder_strict_v;
}

static void ast_eval_ladder_set(int on) {
	ast_eval_ladder_enabled = on ? 1 : 0;
	if (!ast_eval_ladder_budget_v)
		ast_eval_ladder_budget_v =
				(unsigned long)mcc_env_num("MCC_AST_EVAL_LADDER_BUDGET",
																	 (long)AST_EVAL_LADDER_DEFAULT_BUDGET);
}

static int ast_eval_ladder_on(void) {
	ast_eval_ladder_config();
	return ast_eval_ladder_enabled;
}

static unsigned long ast_eval_ladder_budget(void) {
	ast_eval_ladder_config();
	return ast_eval_ladder_budget_v;
}

static int ast_eval_ladder_binop_ok(int op) {
	switch (op) {
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
	case TOK_PDIV:
	case TOK_UDIV:
	case TOK_UMOD:
	case TOK_SHL:
	case TOK_SHR:
	case TOK_SAR:
	case '&':
	case '|':
	case '^':
	case TOK_EQ:
	case TOK_NE:
	case TOK_LT:
	case TOK_GE:
	case TOK_LE:
	case TOK_GT:
	case TOK_ULT:
	case TOK_UGE:
	case TOK_ULE:
	case TOK_UGT:
	case TOK_LAND:
	case TOK_LOR:
		return 1;
	default:
		return 0;
	}
}

static int ast_eval_ladder_ops_ok(AstArena *a, AstLocal n) {
	AstLocal c;
	if (n == AST_NONE)
		return 1;
	if (ast_kind(a, n) == AST_Binary && !ast_eval_ladder_binop_ok(ast_op(a, n)))
		return 0;
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (!ast_eval_ladder_ops_ok(a, c))
			return 0;
	return 1;
}

static int ast_eval_ladder_tbits(int t) {
	switch (t & VT_BTYPE) {
	case VT_BOOL:
		return 1;
	case VT_BYTE:
		return 8;
	case VT_SHORT:
		return 16;
	case VT_INT:
		return 32;
	case VT_LLONG:
		return 64;
	case VT_PTR:
		return MCC_PTR_SIZE * 8;
	default:
		return 0;
	}
}

static int ast_eval_ladder_typed(int t) {
	return !ast_bad_type(t) && !is_float(t) && ast_eval_slice_intt(t) &&
				 ast_eval_ladder_tbits(t) > 0;
}

static int ast_eval_ladder_shape(int t) {
	return (ast_eval_slice_is64(t) << 1) | ((t & VT_UNSIGNED) != 0);
}

static int ast_eval_ladder_scan(AstArena *a, AstLocal n, AstEvalLadderIn *in,
																int *cnt, int max, int *reason,
																unsigned long *inferred, int strict) {
	AstLocal c;
	int t, i;
	if (n == AST_NONE)
		return 1;
	t = ast_type_t(a, n);
	switch (ast_kind(a, n)) {
	case AST_Literal:
	case AST_Convert:
		if (!ast_eval_ladder_typed(t)) {
			*reason = AST_LADDER_R_TYPE;
			return 0;
		}
		break;
	case AST_Ref: {
		int r = ast_op(a, n);
		if (!ast_eval_ladder_typed(t)) {
			*reason = AST_LADDER_R_TYPE;
			return 0;
		}
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
			int32_t o = (int32_t)(int64_t)ast_ival(a, n);
			int bits = ast_eval_ladder_tbits(t);
			int uns = (t & VT_UNSIGNED) != 0;
			for (i = 0; i < *cnt; i++)
				if (in[i].off == o) {
					if (in[i].bits != bits || in[i].uns != uns) {
						*reason = AST_LADDER_R_TYPE;
						return 0;
					}
					return 1;
				}
			if (*cnt >= max) {
				*reason = AST_LADDER_R_ARITY;
				return 0;
			}
			in[*cnt].off = o;
			in[*cnt].type = t;
			in[*cnt].bits = bits;
			in[*cnt].uns = uns;
			(*cnt)++;
		}
		return 1;
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		AstLocal x, y;
		int xt, yt;
		if (bop == TOK_LAND || bop == TOK_LOR)
			break;
		if (ast_nchild(a, n) != 2) {
			*reason = AST_LADDER_R_STRUCT;
			return 0;
		}
		x = ast_child(a, n, 0);
		y = ast_child(a, n, 1);
		xt = ast_eval_slice_wtype(a, x);
		yt = ast_eval_slice_wtype(a, y);
		if (!ast_eval_ladder_typed(xt)) {
			*reason = AST_LADDER_R_TYPE;
			return 0;
		}
		if (!ast_eval_ladder_typed(ast_type_t(a, x))) {
			(*inferred)++;
			if (strict) {
				*reason = AST_LADDER_R_TYPE;
				return 0;
			}
			if (bop != TOK_SHL && bop != TOK_SHR && bop != TOK_SAR && yt &&
					ast_eval_ladder_shape(yt) != ast_eval_ladder_shape(xt)) {
				*reason = AST_LADDER_R_TYPE;
				return 0;
			}
		}
		break;
	}
	case AST_Unary: {
		int uop = ast_op(a, n);
		int wt;
		if (uop == '!')
			break;
		wt = ast_eval_slice_wtype(a, n);
		if (!ast_eval_ladder_typed(wt)) {
			*reason = AST_LADDER_R_TYPE;
			return 0;
		}
		if (!ast_eval_ladder_typed(t)) {
			(*inferred)++;
			if (strict) {
				*reason = AST_LADDER_R_TYPE;
				return 0;
			}
		}
		break;
	}
	default:
		break;
	}
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (!ast_eval_ladder_scan(a, c, in, cnt, max, reason, inferred, strict))
			return 0;
	return 1;
}

static int64_t ast_eval_ladder_sx(uint64_t u, int e) {
	uint64_t m;
	if (e >= 64)
		return (int64_t)u;
	m = (uint64_t)1 << (e - 1);
	u &= ((uint64_t)1 << e) - 1;
	return (int64_t)((u ^ m) - m);
}

static int ast_eval_ladder_point(AstArena *a, AstLocal ar, AstArena *b,
																 AstLocal br, const int32_t *off,
																 const int64_t *val, int n,
																 AstEvalLadderRes *res) {
	int64_t av, bv;
	res->points++;
	if (!ast_eval_slice(a, ar, off, val, n, &av)) {
		res->vacuous++;
		return 1;
	}
	if (!ast_eval_slice(b, br, off, val, n, &bv)) {
		int i;
		for (i = 0; i < n && i < AST_EVAL_LADDER_MAXIN; i++)
			res->diff_in[i] = val[i];
		res->diff_a = av;
		res->diff_b = 0;
		res->diff_b_undef = 1;
		return 0;
	}
	res->informative++;
	if (av != bv) {
		int i;
		for (i = 0; i < n && i < AST_EVAL_LADDER_MAXIN; i++)
			res->diff_in[i] = val[i];
		res->diff_a = av;
		res->diff_b = bv;
		res->diff_b_undef = 0;
		return 0;
	}
	return 1;
}

static int (*ast_ladder_gpu_hook)(AstArena *a, AstLocal ar, AstArena *b,
																	AstLocal br, const AstEvalLadderIn *in,
																	int n, const int *e, const int *sh,
																	int total, uint64_t space,
																	AstEvalLadderRes *res);

static int ast_eval_ladder_rung(AstArena *a, AstLocal ar, AstArena *b,
																AstLocal br, const AstEvalLadderIn *in, int n,
																int w, unsigned long budget, int *exact,
																AstEvalLadderRes *res) {
	int32_t off[AST_EVAL_LADDER_MAXIN];
	int64_t val[AST_EVAL_LADDER_MAXIN];
	int e[AST_EVAL_LADDER_MAXIN], sh[AST_EVAL_LADDER_MAXIN];
	int i, total = 0, full = 1;
	uint64_t code, space;
	for (i = 0; i < n; i++) {
		e[i] = w < in[i].bits ? w : in[i].bits;
		if (e[i] != in[i].bits)
			full = 0;
		sh[i] = total;
		total += e[i];
		off[i] = in[i].off;
	}
	if (total > 40)
		return -1;
	space = (uint64_t)1 << total;
	if (space > (uint64_t)budget)
		return -1;
	*exact = full;
	if (ast_ladder_gpu_hook) {
		int hr = ast_ladder_gpu_hook(a, ar, b, br, in, n, e, sh, total, space, res);
		if (hr >= 0)
			return hr;
	}
	for (code = 0; code < space; code++) {
		for (i = 0; i < n; i++)
			val[i] = ast_eval_slice_fit(
					ast_eval_ladder_sx(code >> sh[i], e[i]), in[i].type);
		if (!ast_eval_ladder_point(a, ar, b, br, off, val, n, res))
			return 0;
	}
	return 1;
}

static int ast_eval_ladder_cornerset(const AstEvalLadderIn *in, int64_t *out) {
	int64_t cand[AST_EVAL_LADDER_CORNERS];
	int bits = in->bits, k = 0, i, j;
	int64_t mn, mx;
	if (in->uns) {
		mn = 0;
		mx = bits >= 64 ? (int64_t)~(uint64_t)0
										: (int64_t)(((uint64_t)1 << bits) - 1);
	} else {
		mn = bits >= 64 ? INT64_MIN : -((int64_t)1 << (bits - 1));
		mx = bits >= 64 ? INT64_MAX : (((int64_t)1 << (bits - 1)) - 1);
	}
	cand[0] = 0;
	cand[1] = 1;
	cand[2] = -1;
	cand[3] = mn;
	cand[4] = mx;
	cand[5] = mx - 1;
	for (i = 0; i < AST_EVAL_LADDER_CORNERS; i++) {
		int64_t v = ast_eval_slice_fit(cand[i], in->type);
		for (j = 0; j < k; j++)
			if (out[j] == v)
				break;
		if (j == k)
			out[k++] = v;
	}
	return k;
}

static int ast_eval_ladder_corner_sweep(AstArena *a, AstLocal ar, AstArena *b,
																				AstLocal br, const AstEvalLadderIn *in,
																				int n, unsigned long budget,
																				AstEvalLadderRes *res) {
	int32_t off[AST_EVAL_LADDER_MAXIN];
	int64_t val[AST_EVAL_LADDER_MAXIN];
	int64_t set[AST_EVAL_LADDER_MAXIN][AST_EVAL_LADDER_CORNERS];
	int k[AST_EVAL_LADDER_MAXIN];
	unsigned long total = 1, s;
	int i;
	for (i = 0; i < n; i++) {
		k[i] = ast_eval_ladder_cornerset(&in[i], set[i]);
		off[i] = in[i].off;
		if (total > budget / (unsigned long)k[i])
			return -1;
		total *= (unsigned long)k[i];
	}
	for (s = 0; s < total; s++) {
		unsigned long rem = s;
		for (i = 0; i < n; i++) {
			val[i] = set[i][rem % (unsigned long)k[i]];
			rem /= (unsigned long)k[i];
		}
		if (!ast_eval_ladder_point(a, ar, b, br, off, val, n, res))
			return 0;
	}
	return 1;
}

static int ast_eval_ladder_observed(AstArena *a, AstLocal ar, AstArena *b,
																		AstLocal br, const AstEvalLadderIn *in,
																		int n, AstEvalLadderRes *res) {
	int32_t off[AST_EVAL_LADDER_MAXIN];
	int64_t tuples[AST_EVAL_LADDER_OBSMAX * AST_EVAL_LADDER_MAXIN];
	int i, nt, t;
	if (!ast_eval_ladder_obs_fn)
		return -1;
	for (i = 0; i < n; i++)
		off[i] = in[i].off;
	nt = ast_eval_ladder_obs_fn(off, n, tuples, AST_EVAL_LADDER_OBSMAX,
															ast_eval_ladder_obs_user);
	if (nt <= 0)
		return -1;
	for (t = 0; t < nt; t++) {
		int64_t val[AST_EVAL_LADDER_MAXIN];
		for (i = 0; i < n; i++)
			val[i] = ast_eval_slice_fit(tuples[t * n + i], in[i].type);
		if (!ast_eval_ladder_point(a, ar, b, br, off, val, n, res))
			return 0;
	}
	return 1;
}

static void ast_eval_slice_ladder(AstArena *a, AstLocal aroot, AstArena *b,
																	AstLocal broot, AstEvalLadderRes *res) {
	static const int wid[AST_EVAL_LADDER_NRUNG] = {1, 2, 4, 8, 16, 32};
	AstEvalLadderIn in[AST_EVAL_LADDER_MAXIN];
	unsigned long budget = ast_eval_ladder_budget();
	int n = 0, i, best = AST_LADDER_RUNG_NONE, exact = 0, r;
	memset(res, 0, sizeof *res);
	res->verdict = -1;
	res->rung = AST_LADDER_RUNG_NONE;
	res->diff_width = -1;
	if (!ast_eval_slice_kind_ok(a, aroot, 0) ||
			!ast_eval_slice_kind_ok(b, broot, 0)) {
		res->reason = AST_LADDER_R_STRUCT;
		return;
	}
	if (!ast_eval_ladder_ops_ok(a, aroot) || !ast_eval_ladder_ops_ok(b, broot)) {
		res->reason = AST_LADDER_R_OP;
		return;
	}
	res->reason = AST_LADDER_R_NONE;
	if (!ast_eval_ladder_scan(a, aroot, in, &n, AST_EVAL_LADDER_MAXIN,
														&res->reason, &res->inferred,
														ast_eval_ladder_strict()) ||
			!ast_eval_ladder_scan(b, broot, in, &n, AST_EVAL_LADDER_MAXIN,
														&res->reason, &res->inferred,
														ast_eval_ladder_strict()))
		return;
	res->nin = n;
	if (n == 0) {
		int32_t off[1];
		int64_t val[1];
		if (!ast_eval_ladder_point(a, aroot, b, broot, off, val, 0, res)) {
			res->verdict = 0;
			res->rung = AST_LADDER_RUNG_CONST;
			res->diff_width = 0;
			return;
		}
		if (res->informative == 0) {
			res->reason = AST_LADDER_R_VACUOUS;
			return;
		}
		res->verdict = 1;
		res->rung = AST_LADDER_RUNG_CONST;
		res->type_complete = 1;
		return;
	}
	for (i = 0; i < AST_EVAL_LADDER_NRUNG; i++) {
		int full = 0;
		r = ast_eval_ladder_rung(a, aroot, b, broot, in, n, wid[i], budget, &full,
														 res);
		if (r < 0)
			break;
		if (r == 0) {
			res->verdict = 0;
			res->rung = wid[i];
			res->diff_width = wid[i];
			return;
		}
		best = wid[i];
		exact = full;
		if (full)
			break;
	}
	if (best == AST_LADDER_RUNG_NONE) {
		r = ast_eval_ladder_observed(a, aroot, b, broot, in, n, res);
		if (r < 0) {
			res->reason =
					ast_eval_ladder_obs_fn ? AST_LADDER_R_OBS : AST_LADDER_R_BUDGET;
			return;
		}
		if (r == 0) {
			res->verdict = 0;
			res->rung = AST_LADDER_RUNG_OBSERVED;
			return;
		}
		if (res->informative == 0) {
			res->reason = AST_LADDER_R_VACUOUS;
			return;
		}
		res->verdict = 1;
		res->rung = AST_LADDER_RUNG_OBSERVED;
		return;
	}
	if (res->informative == 0) {
		res->reason = AST_LADDER_R_VACUOUS;
		return;
	}
	if (!exact) {
		r = ast_eval_ladder_corner_sweep(a, aroot, b, broot, in, n, budget, res);
		if (r == 0) {
			res->verdict = 0;
			res->rung = AST_LADDER_RUNG_CORNERS;
			return;
		}
	}
	res->verdict = 1;
	res->rung = best;
	res->type_complete = exact;
}

static int ast_eval_slice_ladder_equiv(AstArena *a, AstLocal aroot, AstArena *b,
																			 AstLocal broot) {
	AstEvalLadderRes res;
	ast_eval_slice_ladder(a, aroot, b, broot, &res);
	return res.verdict == 1;
}

static int ast_eval_slice_equiv(AstArena *a, AstLocal aroot, AstArena *b,
																AstLocal broot) {
	if (ast_eval_ladder_on())
		return ast_eval_slice_ladder_equiv(a, aroot, b, broot);
	return ast_eval_slice_equiv_seed(a, aroot, b, broot);
}

#endif

#endif
