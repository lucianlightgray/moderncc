#ifndef MCC_SPV_PROVIDED
#define MCC_SPV_PROVIDED 1

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SPV_MAGIC 0x07230203u
#define SPV_VERSION 0x00010300u

enum {
	SpvOpEntryPoint = 15, SpvOpExecutionMode = 16, SpvOpCapability = 17,
	SpvOpMemoryModel = 14, SpvOpTypeVoid = 19, SpvOpTypeBool = 20,
	SpvOpTypeInt = 21, SpvOpTypeVector = 23, SpvOpTypeRuntimeArray = 29,
	SpvOpTypeStruct = 30, SpvOpTypePointer = 32, SpvOpTypeFunction = 33,
	SpvOpConstant = 43, SpvOpFunction = 54, SpvOpFunctionEnd = 56,
	SpvOpVariable = 59, SpvOpLoad = 61, SpvOpStore = 62, SpvOpAccessChain = 65,
	SpvOpDecorate = 71, SpvOpMemberDecorate = 72, SpvOpCompositeExtract = 81,
	SpvOpSNegate = 126, SpvOpIAdd = 128, SpvOpISub = 130, SpvOpIMul = 132,
	SpvOpUDiv = 134, SpvOpSDiv = 135, SpvOpUMod = 137, SpvOpSRem = 138,
	SpvOpSelect = 169, SpvOpIEqual = 170, SpvOpINotEqual = 171,
	SpvOpUGreaterThan = 172, SpvOpSGreaterThan = 173,
	SpvOpUGreaterThanEqual = 174, SpvOpSGreaterThanEqual = 175,
	SpvOpULessThan = 176, SpvOpSLessThan = 177, SpvOpULessThanEqual = 178,
	SpvOpSLessThanEqual = 179, SpvOpShiftRightLogical = 194,
	SpvOpShiftRightArithmetic = 195, SpvOpShiftLeftLogical = 196,
	SpvOpBitwiseOr = 197, SpvOpBitwiseXor = 198, SpvOpBitwiseAnd = 199,
	SpvOpBitcast = 124,
	SpvOpNot = 200, SpvOpPhi = 245, SpvOpSelectionMerge = 247, SpvOpLabel = 248,
	SpvOpBranch = 249, SpvOpBranchConditional = 250, SpvOpReturn = 253
};

enum {
	SpvDecBlock = 2, SpvDecArrayStride = 6, SpvDecBuiltIn = 11,
	SpvDecBinding = 33, SpvDecDescriptorSet = 34, SpvDecOffset = 35
};

enum { SpvStorageInput = 1, SpvStorageStorageBuffer = 12, SpvStorageFunction = 7 };
enum { SpvBuiltInGlobalInvocationId = 28 };
enum { SpvExecModelGLCompute = 5, SpvExecModeLocalSize = 17 };
enum { SpvCapShader = 1 };

#define SPV_LOCAL_SIZE 64
#define SPV_MAX_CONST 512

typedef struct SpvWords {
	uint32_t *w;
	int n, cap;
} SpvWords;

typedef struct SpvMod {
	SpvWords pre, types, body;
	uint32_t next_id;
	uint32_t id_void, id_fnvoid, id_bool, id_int, id_uint, id_v3uint;
	uint32_t id_ptr_in_v3uint, id_gid;
	uint32_t id_rt, id_buf, id_ptr_buf, id_ptr_sb_int;
	uint32_t id_in, id_out, id_main, id_nlive;
	uint32_t cur_label;
	int32_t cval[SPV_MAX_CONST];
	uint32_t cid[SPV_MAX_CONST];
	int ncached;
	int failed;
} SpvMod;

static void spvw_put(SpvWords *b, uint32_t v) {
	if (b->n == b->cap) {
		b->cap = b->cap ? b->cap * 2 : 256;
		b->w = (uint32_t *)realloc(b->w, (size_t)b->cap * sizeof *b->w);
	}
	b->w[b->n++] = v;
}

static void spvw_op(SpvWords *b, int opcode, int nwords) {
	spvw_put(b, ((uint32_t)nwords << 16) | (uint32_t)opcode);
}

static void spvw_str(SpvWords *b, const char *s) {
	size_t len = strlen(s), i = 0;
	uint32_t acc = 0;
	int k = 0;
	for (i = 0; i <= len; i++) {
		acc |= (uint32_t)(unsigned char)s[i] << (8 * k);
		if (++k == 4) {
			spvw_put(b, acc);
			acc = 0;
			k = 0;
		}
	}
	if (k)
		spvw_put(b, acc);
}

static int spv_str_words(const char *s) {
	return (int)(strlen(s) / 4 + 1);
}

static uint32_t spv_id(SpvMod *m) { return m->next_id++; }

static uint32_t spv_const(SpvMod *m, int32_t v) {
	int i;
	for (i = 0; i < m->ncached; i++)
		if (m->cval[i] == v)
			return m->cid[i];
	if (m->ncached == SPV_MAX_CONST) {
		m->failed = 1;
		return m->cid[0];
	}
	uint32_t id = spv_id(m);
	spvw_op(&m->types, SpvOpConstant, 4);
	spvw_put(&m->types, m->id_int);
	spvw_put(&m->types, id);
	spvw_put(&m->types, (uint32_t)v);
	m->cval[m->ncached] = v;
	m->cid[m->ncached] = id;
	m->ncached++;
	return id;
}

static uint32_t spv_uconst(SpvMod *m, uint32_t v) {
	return spv_const(m, (int32_t)v);
}

static uint32_t spv_emit2(SpvMod *m, int opcode, uint32_t rtype, uint32_t a) {
	uint32_t id = spv_id(m);
	spvw_op(&m->body, opcode, 4);
	spvw_put(&m->body, rtype);
	spvw_put(&m->body, id);
	spvw_put(&m->body, a);
	return id;
}

static uint32_t spv_emit3(SpvMod *m, int opcode, uint32_t rtype, uint32_t a,
													uint32_t b) {
	uint32_t id = spv_id(m);
	spvw_op(&m->body, opcode, 5);
	spvw_put(&m->body, rtype);
	spvw_put(&m->body, id);
	spvw_put(&m->body, a);
	spvw_put(&m->body, b);
	return id;
}

static uint32_t spv_label(SpvMod *m) {
	uint32_t id = spv_id(m);
	spvw_op(&m->body, SpvOpLabel, 2);
	spvw_put(&m->body, id);
	m->cur_label = id;
	return id;
}

static void spv_label_at(SpvMod *m, uint32_t id) {
	spvw_op(&m->body, SpvOpLabel, 2);
	spvw_put(&m->body, id);
	m->cur_label = id;
}

static uint32_t spv_bool_of(SpvMod *m, uint32_t v) {
	return spv_emit3(m, SpvOpINotEqual, m->id_bool, v, spv_const(m, 0));
}

static uint32_t spv_int_of_bool(SpvMod *m, uint32_t b) {
	uint32_t id = spv_id(m);
	spvw_op(&m->body, SpvOpSelect, 6);
	spvw_put(&m->body, m->id_int);
	spvw_put(&m->body, id);
	spvw_put(&m->body, b);
	spvw_put(&m->body, spv_const(m, 1));
	spvw_put(&m->body, spv_const(m, 0));
	return id;
}

static void spv_module_begin(SpvMod *m, int nlive) {
	memset(m, 0, sizeof *m);
	m->next_id = 1;
	m->id_void = spv_id(m);
	m->id_fnvoid = spv_id(m);
	m->id_bool = spv_id(m);
	m->id_int = spv_id(m);
	m->id_uint = spv_id(m);
	m->id_v3uint = spv_id(m);
	m->id_ptr_in_v3uint = spv_id(m);
	m->id_gid = spv_id(m);
	m->id_rt = spv_id(m);
	m->id_buf = spv_id(m);
	m->id_ptr_buf = spv_id(m);
	m->id_ptr_sb_int = spv_id(m);
	m->id_in = spv_id(m);
	m->id_out = spv_id(m);
	m->id_main = spv_id(m);
	m->id_nlive = (uint32_t)nlive;

	spvw_op(&m->pre, SpvOpCapability, 2);
	spvw_put(&m->pre, SpvCapShader);
	spvw_op(&m->pre, SpvOpMemoryModel, 3);
	spvw_put(&m->pre, 0);
	spvw_put(&m->pre, 1);
	spvw_op(&m->pre, SpvOpEntryPoint, 4 + spv_str_words("main"));
	spvw_put(&m->pre, SpvExecModelGLCompute);
	spvw_put(&m->pre, m->id_main);
	spvw_str(&m->pre, "main");
	spvw_put(&m->pre, m->id_gid);
	spvw_op(&m->pre, SpvOpExecutionMode, 6);
	spvw_put(&m->pre, m->id_main);
	spvw_put(&m->pre, SpvExecModeLocalSize);
	spvw_put(&m->pre, SPV_LOCAL_SIZE);
	spvw_put(&m->pre, 1);
	spvw_put(&m->pre, 1);

	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_gid);
	spvw_put(&m->pre, SpvDecBuiltIn);
	spvw_put(&m->pre, SpvBuiltInGlobalInvocationId);
	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_rt);
	spvw_put(&m->pre, SpvDecArrayStride);
	spvw_put(&m->pre, 4);
	spvw_op(&m->pre, SpvOpDecorate, 3);
	spvw_put(&m->pre, m->id_buf);
	spvw_put(&m->pre, SpvDecBlock);
	spvw_op(&m->pre, SpvOpMemberDecorate, 5);
	spvw_put(&m->pre, m->id_buf);
	spvw_put(&m->pre, 0);
	spvw_put(&m->pre, SpvDecOffset);
	spvw_put(&m->pre, 0);
	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_in);
	spvw_put(&m->pre, SpvDecDescriptorSet);
	spvw_put(&m->pre, 0);
	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_in);
	spvw_put(&m->pre, SpvDecBinding);
	spvw_put(&m->pre, 0);
	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_out);
	spvw_put(&m->pre, SpvDecDescriptorSet);
	spvw_put(&m->pre, 0);
	spvw_op(&m->pre, SpvOpDecorate, 4);
	spvw_put(&m->pre, m->id_out);
	spvw_put(&m->pre, SpvDecBinding);
	spvw_put(&m->pre, 1);

	spvw_op(&m->types, SpvOpTypeVoid, 2);
	spvw_put(&m->types, m->id_void);
	spvw_op(&m->types, SpvOpTypeFunction, 3);
	spvw_put(&m->types, m->id_fnvoid);
	spvw_put(&m->types, m->id_void);
	spvw_op(&m->types, SpvOpTypeBool, 2);
	spvw_put(&m->types, m->id_bool);
	spvw_op(&m->types, SpvOpTypeInt, 4);
	spvw_put(&m->types, m->id_int);
	spvw_put(&m->types, 32);
	spvw_put(&m->types, 1);
	spvw_op(&m->types, SpvOpTypeInt, 4);
	spvw_put(&m->types, m->id_uint);
	spvw_put(&m->types, 32);
	spvw_put(&m->types, 0);
	spvw_op(&m->types, SpvOpTypeVector, 4);
	spvw_put(&m->types, m->id_v3uint);
	spvw_put(&m->types, m->id_uint);
	spvw_put(&m->types, 3);
	spvw_op(&m->types, SpvOpTypePointer, 4);
	spvw_put(&m->types, m->id_ptr_in_v3uint);
	spvw_put(&m->types, SpvStorageInput);
	spvw_put(&m->types, m->id_v3uint);
	spvw_op(&m->types, SpvOpVariable, 4);
	spvw_put(&m->types, m->id_ptr_in_v3uint);
	spvw_put(&m->types, m->id_gid);
	spvw_put(&m->types, SpvStorageInput);
	spvw_op(&m->types, SpvOpTypeRuntimeArray, 3);
	spvw_put(&m->types, m->id_rt);
	spvw_put(&m->types, m->id_int);
	spvw_op(&m->types, SpvOpTypeStruct, 3);
	spvw_put(&m->types, m->id_buf);
	spvw_put(&m->types, m->id_rt);
	spvw_op(&m->types, SpvOpTypePointer, 4);
	spvw_put(&m->types, m->id_ptr_buf);
	spvw_put(&m->types, SpvStorageStorageBuffer);
	spvw_put(&m->types, m->id_buf);
	spvw_op(&m->types, SpvOpVariable, 4);
	spvw_put(&m->types, m->id_ptr_buf);
	spvw_put(&m->types, m->id_in);
	spvw_put(&m->types, SpvStorageStorageBuffer);
	spvw_op(&m->types, SpvOpVariable, 4);
	spvw_put(&m->types, m->id_ptr_buf);
	spvw_put(&m->types, m->id_out);
	spvw_put(&m->types, SpvStorageStorageBuffer);
	spvw_op(&m->types, SpvOpTypePointer, 4);
	spvw_put(&m->types, m->id_ptr_sb_int);
	spvw_put(&m->types, SpvStorageStorageBuffer);
	spvw_put(&m->types, m->id_int);
}

static uint32_t spv_load_live(SpvMod *m, uint32_t base, int k) {
	uint32_t idx = base;
	if (k) {
		idx = spv_emit3(m, SpvOpIAdd, m->id_int, base, spv_const(m, k));
	}
	uint32_t p = spv_id(m);
	spvw_op(&m->body, SpvOpAccessChain, 5 + 1);
	spvw_put(&m->body, m->id_ptr_sb_int);
	spvw_put(&m->body, p);
	spvw_put(&m->body, m->id_in);
	spvw_put(&m->body, spv_const(m, 0));
	spvw_put(&m->body, idx);
	return spv_emit2(m, SpvOpLoad, m->id_int, p);
}

static uint32_t spv_fit(SpvMod *m, uint32_t v, int t) {
	int bt = t & VT_BTYPE;
	int uns = (t & VT_UNSIGNED) != 0;
	switch (bt) {
	case VT_BOOL:
		return spv_int_of_bool(m, spv_bool_of(m, v));
	case VT_BYTE:
		if (uns)
			return spv_emit3(m, SpvOpBitwiseAnd, m->id_int, v, spv_const(m, 0xFF));
		{
			uint32_t s = spv_emit3(m, SpvOpShiftLeftLogical, m->id_int, v,
														 spv_uconst(m, 24));
			return spv_emit3(m, SpvOpShiftRightArithmetic, m->id_int, s,
											 spv_uconst(m, 24));
		}
	case VT_SHORT:
		if (uns)
			return spv_emit3(m, SpvOpBitwiseAnd, m->id_int, v, spv_const(m, 0xFFFF));
		{
			uint32_t s = spv_emit3(m, SpvOpShiftLeftLogical, m->id_int, v,
														 spv_uconst(m, 16));
			return spv_emit3(m, SpvOpShiftRightArithmetic, m->id_int, s,
											 spv_uconst(m, 16));
		}
	default:
		return v;
	}
}

static uint32_t spv_main_begin(SpvMod *m, int nlive) {
	spvw_op(&m->body, SpvOpFunction, 5);
	spvw_put(&m->body, m->id_void);
	spvw_put(&m->body, m->id_main);
	spvw_put(&m->body, 0);
	spvw_put(&m->body, m->id_fnvoid);
	spv_label(m);
	uint32_t g = spv_emit2(m, SpvOpLoad, m->id_v3uint, m->id_gid);
	uint32_t gx = spv_id(m);
	spvw_op(&m->body, SpvOpCompositeExtract, 5);
	spvw_put(&m->body, m->id_uint);
	spvw_put(&m->body, gx);
	spvw_put(&m->body, g);
	spvw_put(&m->body, 0);
	uint32_t gi = spv_id(m);
	spvw_op(&m->body, SpvOpBitwiseOr, 5);
	spvw_put(&m->body, m->id_int);
	spvw_put(&m->body, gi);
	spvw_put(&m->body, gx);
	spvw_put(&m->body, spv_const(m, 0));
	return spv_emit3(m, SpvOpIMul, m->id_int, gi, spv_const(m, nlive));
}

static void spv_main_end(SpvMod *m, uint32_t lane, uint32_t val) {
	uint32_t p = spv_id(m);
	spvw_op(&m->body, SpvOpAccessChain, 6);
	spvw_put(&m->body, m->id_ptr_sb_int);
	spvw_put(&m->body, p);
	spvw_put(&m->body, m->id_out);
	spvw_put(&m->body, spv_const(m, 0));
	spvw_put(&m->body, lane);
	spvw_op(&m->body, SpvOpStore, 3);
	spvw_put(&m->body, p);
	spvw_put(&m->body, val);
	spvw_op(&m->body, SpvOpReturn, 1);
	spvw_op(&m->body, SpvOpFunctionEnd, 1);
}

static uint32_t spv_unsigned_binop(SpvMod *m, int opcode, uint32_t a,
																	 uint32_t b) {
	uint32_t ua = spv_emit2(m, SpvOpBitcast, m->id_uint, a);
	uint32_t ub = spv_emit2(m, SpvOpBitcast, m->id_uint, b);
	uint32_t r = spv_emit3(m, opcode, m->id_uint, ua, ub);
	return spv_emit2(m, SpvOpBitcast, m->id_int, r);
}

static uint32_t spv_signed_rem(SpvMod *m, uint32_t a, uint32_t b) {
	uint32_t q = spv_emit3(m, SpvOpSDiv, m->id_int, a, b);
	uint32_t p = spv_emit3(m, SpvOpIMul, m->id_int, b, q);
	return spv_emit3(m, SpvOpISub, m->id_int, a, p);
}

static int spv_binop_code(int op, int uns, int *is_cmp) {
	*is_cmp = 0;
	switch (op) {
	case '+': return SpvOpIAdd;
	case '-': return SpvOpISub;
	case '*': return SpvOpIMul;
	case '/': case TOK_PDIV: return uns ? SpvOpUDiv : SpvOpSDiv;
	case '%': return uns ? SpvOpUMod : SpvOpSRem;
	case TOK_UDIV: return SpvOpUDiv;
	case TOK_UMOD: return SpvOpUMod;
	case TOK_SHL: return SpvOpShiftLeftLogical;
	case TOK_SHR: return SpvOpShiftRightLogical;
	case TOK_SAR: return SpvOpShiftRightArithmetic;
	case '&': return SpvOpBitwiseAnd;
	case '|': return SpvOpBitwiseOr;
	case '^': return SpvOpBitwiseXor;
	default: break;
	}
	*is_cmp = 1;
	switch (op) {
	case TOK_EQ: return SpvOpIEqual;
	case TOK_NE: return SpvOpINotEqual;
	case TOK_ULT: return SpvOpULessThan;
	case TOK_UGE: return SpvOpUGreaterThanEqual;
	case TOK_ULE: return SpvOpULessThanEqual;
	case TOK_UGT: return SpvOpUGreaterThan;
	case '<': return uns ? SpvOpULessThan : SpvOpSLessThan;
	case '>': return uns ? SpvOpUGreaterThan : SpvOpSGreaterThan;
	case TOK_LE: return uns ? SpvOpULessThanEqual : SpvOpSLessThanEqual;
	case TOK_GE: return uns ? SpvOpUGreaterThanEqual : SpvOpSGreaterThanEqual;
	case TOK_LT: return SpvOpSLessThan;
	case TOK_GT: return SpvOpSGreaterThan;
	default: break;
	}
	*is_cmp = -1;
	return 0;
}

static int spv_env_index(const int32_t *off, int nenv, int32_t want, int *out) {
	int i;
	for (i = 0; i < nenv; i++)
		if (off[i] == want) {
			*out = i;
			return 1;
		}
	return 0;
}

static int spv_expr(SpvMod *m, AstArena *a, AstLocal n, const int32_t *off,
										int nenv, uint32_t base, uint32_t *out);

static int spv_branch_pair(SpvMod *m, AstArena *a, AstLocal cn, AstLocal tn,
													 AstLocal en, const int32_t *off, int nenv,
													 uint32_t base, uint32_t *out) {
	uint32_t cv;
	if (!spv_expr(m, a, cn, off, nenv, base, &cv))
		return 0;
	uint32_t cb = spv_bool_of(m, cv);
	uint32_t l_then = spv_id(m), l_else = spv_id(m), l_merge = spv_id(m);
	spvw_op(&m->body, SpvOpSelectionMerge, 3);
	spvw_put(&m->body, l_merge);
	spvw_put(&m->body, 0);
	spvw_op(&m->body, SpvOpBranchConditional, 4);
	spvw_put(&m->body, cb);
	spvw_put(&m->body, l_then);
	spvw_put(&m->body, l_else);

	spv_label_at(m, l_then);
	uint32_t tv;
	if (!spv_expr(m, a, tn, off, nenv, base, &tv))
		return 0;
	uint32_t from_then = m->cur_label;
	spvw_op(&m->body, SpvOpBranch, 2);
	spvw_put(&m->body, l_merge);

	spv_label_at(m, l_else);
	uint32_t ev;
	if (!spv_expr(m, a, en, off, nenv, base, &ev))
		return 0;
	uint32_t from_else = m->cur_label;
	spvw_op(&m->body, SpvOpBranch, 2);
	spvw_put(&m->body, l_merge);

	spv_label_at(m, l_merge);
	uint32_t phi = spv_id(m);
	spvw_op(&m->body, SpvOpPhi, 7);
	spvw_put(&m->body, m->id_int);
	spvw_put(&m->body, phi);
	spvw_put(&m->body, tv);
	spvw_put(&m->body, from_then);
	spvw_put(&m->body, ev);
	spvw_put(&m->body, from_else);
	*out = phi;
	return 1;
}

static int spv_logical(SpvMod *m, AstArena *a, AstLocal n, int want,
											 const int32_t *off, int nenv, uint32_t base,
											 uint32_t *out, uint32_t k) {
	uint32_t nc = ast_nchild(a, n);
	if (k == nc) {
		*out = spv_const(m, want ? 1 : 0);
		return 1;
	}
	uint32_t cv;
	if (!spv_expr(m, a, ast_child(a, n, k), off, nenv, base, &cv))
		return 0;
	uint32_t cb = spv_bool_of(m, cv);
	uint32_t l_cont = spv_id(m), l_stop = spv_id(m), l_merge = spv_id(m);
	spvw_op(&m->body, SpvOpSelectionMerge, 3);
	spvw_put(&m->body, l_merge);
	spvw_put(&m->body, 0);
	spvw_op(&m->body, SpvOpBranchConditional, 4);
	spvw_put(&m->body, cb);
	spvw_put(&m->body, want ? l_cont : l_stop);
	spvw_put(&m->body, want ? l_stop : l_cont);

	spv_label_at(m, l_cont);
	uint32_t rest;
	if (!spv_logical(m, a, n, want, off, nenv, base, &rest, k + 1))
		return 0;
	uint32_t from_cont = m->cur_label;
	spvw_op(&m->body, SpvOpBranch, 2);
	spvw_put(&m->body, l_merge);

	spv_label_at(m, l_stop);
	uint32_t stopv = spv_const(m, want ? 0 : 1);
	uint32_t from_stop = m->cur_label;
	spvw_op(&m->body, SpvOpBranch, 2);
	spvw_put(&m->body, l_merge);

	spv_label_at(m, l_merge);
	uint32_t phi = spv_id(m);
	spvw_op(&m->body, SpvOpPhi, 7);
	spvw_put(&m->body, m->id_int);
	spvw_put(&m->body, phi);
	spvw_put(&m->body, rest);
	spvw_put(&m->body, from_cont);
	spvw_put(&m->body, stopv);
	spvw_put(&m->body, from_stop);
	*out = phi;
	return 1;
}

static int spv_expr(SpvMod *m, AstArena *a, AstLocal n, const int32_t *off,
										int nenv, uint32_t base, uint32_t *out) {
	if (n == AST_NONE || m->failed)
		return 0;
	switch (ast_kind(a, n)) {
	case AST_Literal: {
		int t = ast_type_t(a, n);
		if (ast_bad_type(t) || is_float(t) || !ast_eval_slice_intt(t))
			return 0;
		if (ast_eval_slice_is64(t))
			return 0;
		if ((ast_op(a, n) & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
			return 0;
		*out = spv_const(m, (int32_t)ast_eval_slice_fit((int64_t)ast_ival(a, n), t));
		return 1;
	}
	case AST_Ref: {
		int r = ast_op(a, n);
		int t = ast_type_t(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
			int k;
			if (!ast_eval_slice_intt(t) || is_float(t) || ast_eval_slice_is64(t))
				return 0;
			if (!spv_env_index(off, nenv, (int32_t)(int64_t)ast_ival(a, n), &k))
				return 0;
			*out = spv_load_live(m, base, k);
			return 1;
		}
		if ((r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
			if (ast_bad_type(t) || is_float(t) || !ast_eval_slice_intt(t))
				return 0;
			if (ast_eval_slice_is64(t))
				return 0;
			*out = spv_const(m, (int32_t)ast_eval_slice_fit((int64_t)ast_ival(a, n), t));
			return 1;
		}
		return 0;
	}
	case AST_Load: {
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
			return 0;
		int r = ast_op(a, c);
		int t = ast_type_t(a, n);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
			return 0;
		if (!ast_eval_slice_intt(t) || is_float(t) || ast_eval_slice_is64(t))
			return 0;
		int k;
		if (!spv_env_index(off, nenv, (int32_t)(int64_t)ast_ival(a, c), &k))
			return 0;
		*out = spv_load_live(m, base, k);
		return 1;
	}
	case AST_Convert: {
		int t = ast_type_t(a, n);
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || is_float(t) || is_float(ast_type_t(a, c)))
			return 0;
		if (ast_bad_type(t) || !ast_eval_slice_intt(t) || ast_eval_slice_is64(t))
			return 0;
		uint32_t v;
		if (!spv_expr(m, a, c, off, nenv, base, &v))
			return 0;
		*out = spv_fit(m, v, t);
		return 1;
	}
	case AST_Unary: {
		int uop = ast_op(a, n);
		int t = ast_eval_slice_wtype(a, n);
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || !t || ast_eval_slice_is64(t))
			return 0;
		if (uop != '-' && uop != TOK_NEG && uop != '~' && uop != '!')
			return 0;
		uint32_t v;
		if (!spv_expr(m, a, c, off, nenv, base, &v))
			return 0;
		if (uop == '!') {
			uint32_t z = spv_emit3(m, SpvOpIEqual, m->id_bool, v, spv_const(m, 0));
			*out = spv_int_of_bool(m, z);
			return 1;
		}
		if (uop == '~') {
			*out = spv_fit(m, spv_emit2(m, SpvOpNot, m->id_int, v), t);
			return 1;
		}
		*out = spv_emit2(m, SpvOpSNegate, m->id_int, v);
		return 1;
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		if (bop == TOK_LAND || bop == TOK_LOR)
			return spv_logical(m, a, n, bop == TOK_LAND, off, nenv, base, out, 0);
		if (ast_nchild(a, n) != 2)
			return 0;
		AstLocal x = ast_child(a, n, 0), y = ast_child(a, n, 1);
		int xt = ast_eval_slice_wtype(a, x);
		if (!xt || is_float(ast_type_t(a, x)) || is_float(ast_type_t(a, y)))
			return 0;
		if (ast_eval_slice_is64(xt))
			return 0;
		uint32_t lv, rv;
		if (!spv_expr(m, a, x, off, nenv, base, &lv))
			return 0;
		if (!spv_expr(m, a, y, off, nenv, base, &rv))
			return 0;
		int uns = (xt & VT_UNSIGNED) != 0;
		int is_cmp;
		int code = spv_binop_code(bop, uns, &is_cmp);
		if (is_cmp < 0)
			return 0;
		if (is_cmp) {
			*out = spv_int_of_bool(m, spv_emit3(m, code, m->id_bool, lv, rv));
			return 1;
		}
		if (code == SpvOpUDiv || code == SpvOpUMod) {
			*out = spv_unsigned_binop(m, code, lv, rv);
			return 1;
		}
		if (code == SpvOpSRem) {
			*out = spv_signed_rem(m, lv, rv);
			return 1;
		}
		*out = spv_emit3(m, code, m->id_int, lv, rv);
		return 1;
	}
	case AST_If: {
		if (ast_nchild(a, n) != 3)
			return 0;
		return spv_branch_pair(m, a, ast_child(a, n, 0), ast_child(a, n, 1),
													 ast_child(a, n, 2), off, nenv, base, out);
	}
	default:
		return 0;
	}
}

static uint32_t *spv_module_finish(SpvMod *m, int *nwords) {
	int total = 5 + m->pre.n + m->types.n + m->body.n;
	uint32_t *w = (uint32_t *)malloc((size_t)total * sizeof *w);
	int i = 0;
	w[i++] = SPV_MAGIC;
	w[i++] = SPV_VERSION;
	w[i++] = 0;
	w[i++] = m->next_id;
	w[i++] = 0;
	memcpy(w + i, m->pre.w, (size_t)m->pre.n * sizeof *w);
	i += m->pre.n;
	memcpy(w + i, m->types.w, (size_t)m->types.n * sizeof *w);
	i += m->types.n;
	memcpy(w + i, m->body.w, (size_t)m->body.n * sizeof *w);
	i += m->body.n;
	*nwords = total;
	return w;
}

static void spv_module_free(SpvMod *m) {
	free(m->pre.w);
	free(m->types.w);
	free(m->body.w);
}

#endif
