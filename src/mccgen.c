#define USING_GLOBALS
#include "mcc.h"

#include "mccforecast.h"

ST_DATA int rsym, anon_sym, ind, loc;

ST_DATA Sym *global_stack;
ST_DATA Sym *local_stack;
ST_DATA Sym *define_stack;
ST_DATA Sym *global_label_stack;
ST_DATA Sym *local_label_stack;

#define sym_free_first (mcc_state->sym_free_first)
#define sym_pools (mcc_state->sym_pools)
#define nb_sym_pools (mcc_state->nb_sym_pools)

#define all_cleanups (mcc_state->all_cleanups)
#define pending_gotos (mcc_state->pending_gotos)
#define local_scope (mcc_state->local_scope)
ST_DATA char debug_modes;

ST_DATA SValue *vtop;
#define vstack (mcc_state->gen_vstack + 1)

ST_DATA int nocode_wanted;
#define NODATA_WANTED (nocode_wanted > 0)

ST_DATA int asm_lvalue_cast;
#define DATA_ONLY_WANTED 0x80000000

#define CODE_OFF_BIT 0x20000000
#define CODE_OFF()    \
	if (!nocode_wanted) \
	(nocode_wanted |= CODE_OFF_BIT)
#define CODE_ON() (nocode_wanted &= ~CODE_OFF_BIT)

#define NOEVAL_MASK 0x0000FFFF
#define NOEVAL_WANTED (nocode_wanted & NOEVAL_MASK)

#define CONST_WANTED_BIT 0x00010000
#define CONST_WANTED_MASK 0x0FFF0000
#define CONST_WANTED (nocode_wanted & CONST_WANTED_MASK)

ST_DATA int global_expr;
ST_DATA CType func_vt;
ST_DATA int func_var;
ST_DATA int func_vc;
ST_DATA int func_ind;
#define func_old (mcc_state->func_old)
#define cur_func_noreturn (mcc_state->cur_func_noreturn)
#define cur_func_last_param (mcc_state->cur_func_last_param)
#define expr_was_assign (mcc_state->expr_was_assign)
#define expr_has_effect (mcc_state->expr_has_effect)
#define cur_func_inline_extern (mcc_state->cur_func_inline_extern)
#define ice_float_op (mcc_state->ice_float_op)
#define ice_nonconst (mcc_state->ice_nonconst)
ST_DATA const char *funcname;
ST_DATA CType int_type, func_old_type, char_type, char_pointer_type;
#define initstr (mcc_state->initstr)

#if MCC_PTR_SIZE == 4
#define VT_SIZE_T (VT_INT | VT_UNSIGNED)
#define VT_PTRDIFF_T VT_INT
#elif LONG_SIZE == 4
#define VT_SIZE_T (VT_LLONG | VT_UNSIGNED)
#define VT_PTRDIFF_T VT_LLONG
#else
#define VT_SIZE_T (VT_LONG | VT_LLONG | VT_UNSIGNED)
#define VT_PTRDIFF_T (VT_LONG | VT_LLONG)
#endif

#define cur_switch (mcc_state->cur_switch)

#define atomic_lowering (mcc_state->atomic_lowering)

#define in_for_init (mcc_state->in_for_init)

#define vla_seq (mcc_state->vla_seq)
#define vla_open_birth (mcc_state->vla_open_birth)
#define nb_vla_open (mcc_state->nb_vla_open)
#define vla_track_ovf (mcc_state->vla_track_ovf)

#define arr_temp_local_vars (mcc_state->arr_temp_local_vars)
#define nb_temp_local_vars (mcc_state->nb_temp_local_vars)

#define cur_scope (mcc_state->cur_scope)
#define loop_scope (mcc_state->loop_scope)
#define root_scope (mcc_state->root_scope)

typedef struct
{
	Section *sec;
	int local_offset;
	Sym *flex_array_ref;
	char flex_is_member;
	char flex_warned;
	int llocal;
} init_params;

static void init_prec(void);

static void block(int flags);
#define STMT_EXPR 1
#define STMT_COMPOUND 2

enum {
	SEQP_READ,
	SEQP_WRITE
};

#define seqp_ev (mcc_state->seqp_ev)
#define nb_seqp (mcc_state->nb_seqp)
#define seqp_overflow (mcc_state->seqp_overflow)

static void seqp_reset(void) { MCC_TRACE("enter\n");
	nb_seqp = 0;
	seqp_overflow = 0;
}

static int seqp_key(SValue *sv, Sym **obj, unsigned long long *off) { MCC_TRACE("enter\n");
	int r = sv->r;
	if (!(r & VT_LVAL) || !sv->sym || (sv->type.t & VT_BITFIELD))
		{ MCC_TRACE("br\n"); return 0; }
	if ((r & VT_VALMASK) != VT_LOCAL && (r & VT_VALMASK) != VT_CONST)
		{ MCC_TRACE("br\n"); return 0; }
	*obj = sv->sym;
	*off = (unsigned long long)sv->c.i;
	return 1;
}

static void seqp_record_sv(SValue *sv, int kind) { MCC_TRACE("enter\n");
	Sym *obj;
	unsigned long long off;
	if (nocode_wanted)
		{ MCC_TRACE("br\n"); return; }
	if (!seqp_key(sv, &obj, &off))
		{ MCC_TRACE("br\n"); return; }
	if ((mcc_state->warn_uninitialized & WARN_ON) && (obj->r & VT_VALMASK) == VT_LOCAL && obj->v >= TOK_IDENT && obj->v < SYM_FIRST_ANOM) { MCC_TRACE("br\n");
		if (kind == SEQP_WRITE)
			{ MCC_TRACE("br\n"); obj->a.inited = 1; }
		else if (!obj->a.inited && !obj->a.addrtaken) { MCC_TRACE("br\n");
			mcc_warning_c(warn_uninitialized)(
					"'%s' is used uninitialized", get_tok_str(obj->v, NULL));
			obj->a.inited = 1;
		}
	}
	if (!mcc_state->warn_sequence_point)
		{ MCC_TRACE("br\n"); return; }
	if (nb_seqp >= SEQP_MAX) { MCC_TRACE("br\n");
		seqp_overflow = 1;
		return;
	}
	seqp_ev[nb_seqp].obj = obj;
	seqp_ev[nb_seqp].off = off;
	seqp_ev[nb_seqp].kind = kind;
	nb_seqp++;
}

static void seqp_check(void) { MCC_TRACE("enter\n");
	int i, j;
	if (seqp_overflow || !mcc_state->warn_sequence_point)
		{ MCC_TRACE("br\n"); return; }
	for (i = 0; i < nb_seqp; i++) { MCC_TRACE("br\n");
		Sym *o = seqp_ev[i].obj;
		unsigned long long ooff = seqp_ev[i].off;
		int writes = 0;
		if (!o || seqp_ev[i].kind != SEQP_WRITE)
			{ MCC_TRACE("br\n"); continue; }
		for (j = i; j < nb_seqp; j++)
			{ MCC_TRACE("br\n"); if (seqp_ev[j].obj == o && seqp_ev[j].off == ooff && seqp_ev[j].kind == SEQP_WRITE)
				{ MCC_TRACE("br\n"); writes++; } }
		for (j = i + 1; j < nb_seqp; j++)
			{ MCC_TRACE("br\n"); if (seqp_ev[j].obj == o && seqp_ev[j].off == ooff)
				{ MCC_TRACE("br\n"); seqp_ev[j].obj = NULL; } }
		if (writes >= 2)
			{ MCC_TRACE("br\n"); mcc_warning_c(warn_sequence_point)(
					"operation on '%s' may be undefined", get_tok_str(o->v, NULL)); }
	}
}

static void seqp_flush(void) { MCC_TRACE("enter\n");
	seqp_check();
	seqp_reset();
}

static void gen_cast(CType *type);
static void gen_cast_s(int t);
static int atomic_rmw_size(SValue *sv, int op);
static void gen_atomic_rmw(int op, int ret_new);
static int atomic_cas_size(SValue *sv);
static void gen_atomic_cas_rmw(int op, int ret_new);
static int atomic_store_needs_libcall(SValue *sv);
static void gen_atomic_store_scalar(void);
static void gen_atomic_load_scalar(void);
static int atomic_store_needs_generic(SValue *sv);
static void gen_atomic_store_aggregate(void);
static void gen_atomic_load_aggregate(void);
static inline CType *pointed_type(CType *type);
static int is_compatible_types(CType *type1, CType *type2);
static int parse_btype(CType *type, AttributeDef *ad, int ignore_label);
static CType *type_decl(CType *type, AttributeDef *ad, int *v, int td);
static void parse_expr_type(CType *type);
static void init_putv(init_params *p, CType *type, unsigned long c);
static void decl_initializer(init_params *p, CType *type, unsigned long c, int flags);
static void decl_initializer_alloc(CType *type, AttributeDef *ad, int r, int has_init, int v, int scope);
static int decl(int l);
static void expr_eq(void);
static void vpush_type_size(CType *type, int *a);
static void gen_complex_op(int op);
static void gen_complex_cast(CType *type);
static int is_compatible_unqualified_types(CType *type1, CType *type2);
static inline int64_t expr_const64(void);
static void vpush64(int ty, unsigned long long v);
static void vpush(CType *type);
static int gvtst(int inv, int t);
static void gen_inline_functions(MCCState *s);
static void resolve_alias_fixups(MCCState *s);
static void free_inline_functions(MCCState *s);
static void finalize_tentative_arrays(void);
static void skip_or_save_block(TokenString **str);
static void gv_dup(void);
static int get_temp_local_var(int size, int align, int *r2);
static void cast_error(CType *st, CType *dt);
static void write_ldouble(unsigned char *d, void *s);
static void end_switch(void);
static void do_Static_assert(void);

static void mcc_pedantic(const char *msg) { MCC_TRACE("enter\n");
	if (!mcc_state->warn_pedantic)
		{ MCC_TRACE("br\n"); return; }
	if (mcc_state->error_set_jmp_enabled) { MCC_TRACE("br\n");
		BufferedFile *wf;
		for (wf = file; wf && wf->filename[0] == ':'; wf = wf->prev)
			;
		if (wf && wf->system_header)
			{ MCC_TRACE("br\n"); return; }
	}
	if (mcc_state->pedantic_errors)
		{ MCC_TRACE("br\n"); mcc_error("%s", msg); }
	else
		{ MCC_TRACE("br\n"); mcc_warning_c(warn_pedantic)("%s", msg); }
}

static int struct_has_flexible_member(CType *type) { MCC_TRACE("enter\n");
	Sym *f, *last = NULL;
	int align;
	if ((type->t & VT_BTYPE) != VT_STRUCT)
		{ MCC_TRACE("br\n"); return 0; }
	for (f = type->ref->next; f; f = f->next)
		{ MCC_TRACE("br\n"); last = f; }
	return last && (last->type.t & VT_ARRAY) && type_size(&last->type, &align) < 0;
}

ST_FUNC void g(int c) { MCC_TRACE("enter\n");
	int ind1;
	if (nocode_wanted)
		{ MCC_TRACE("br\n"); return; }
	ind1 = ind + 1;
	if (ind1 > cur_text_section->data_allocated)
		{ MCC_TRACE("br\n"); section_realloc(cur_text_section, ind1); }
	cur_text_section->data[ind] = c;
	ind = ind1;
}

ST_FUNC void gen_le16(int c) { MCC_TRACE("enter\n");
	g(c);
	g(c >> 8);
}

#ifndef MCC_TARGET_ARM
ST_FUNC int gjmp_append(int n, int t) { MCC_TRACE("enter\n");
	void *p;
	if (n) { MCC_TRACE("br\n");
		uint32_t n1 = n, n2;
		while ((n2 = read32le(p = cur_text_section->data + n1)))
			{ MCC_TRACE("br\n"); n1 = n2; }
		write32le(p, t);
		t = n;
	}
	return t;
}
#endif

#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
ST_FUNC int oad(int c, int s) { MCC_TRACE("enter\n");
	int t;
	if (nocode_wanted)
		{ MCC_TRACE("br\n"); return s; }
	o(c);
	t = ind;
	gen_le32(s);
	return t;
}
#endif

#if MCC_CONFIG_DIAG_RT >= 2

ST_FUNC MAYBE_UNUSED int gen_bounds_epilog_head(addr_t func_bound_offset,
																								Sym **psym_data, int *poffset_modified) { MCC_TRACE("enter\n");
	addr_t *bounds_ptr;
	int offset_modified = func_bound_offset != lbounds_section->data_offset;

	*poffset_modified = offset_modified;
	if (!offset_modified && !func_bound_add_epilog)
		{ MCC_TRACE("br\n"); return 0; }

	bounds_ptr = section_ptr_add(lbounds_section, sizeof(addr_t));
	*bounds_ptr = 0;

	*psym_data = get_sym_ref(&char_pointer_type, lbounds_section,
													 func_bound_offset, MCC_PTR_SIZE);
	return 1;
}
#endif

ST_FUNC void gsym(int t) { MCC_TRACE("enter\n");
	if (t) { MCC_TRACE("br\n");
		gsym_addr(t, ind);
		CODE_ON();
	}
}

static int gind() { MCC_TRACE("enter\n");
	int t = ind;
	CODE_ON();
	if (debug_modes)
		{ MCC_TRACE("br\n"); mcc_tcov_block_begin(mcc_state); }
	return t;
}

static void gjmp_addr_acs(int t) { MCC_TRACE("enter\n");
	gjmp_addr(t);
	CODE_OFF();
}

static int gjmp_acs(int t) { MCC_TRACE("enter\n");
	t = gjmp(t);
	CODE_OFF();
	return t;
}

#define gjmp_addr gjmp_addr_acs
#define gjmp gjmp_acs

ST_INLN int is_float(int t) { MCC_TRACE("enter\n");
	int bt = t & VT_BTYPE;
	return bt == VT_LDOUBLE || bt == VT_DOUBLE || bt == VT_FLOAT || bt == VT_QFLOAT;
}

static inline int is_complex_type(CType *type) { MCC_TRACE("enter\n");
	return (type->t & VT_BTYPE) == VT_STRUCT && type->ref->a.is_complex;
}

static inline int is_integer_btype(int bt) { MCC_TRACE("enter\n");
	return bt == VT_BYTE || bt == VT_BOOL || bt == VT_SHORT || bt == VT_INT || bt == VT_LLONG;
}

static int type_is_vm(CType *type) { MCC_TRACE("enter\n");
	CType *t = type;
	while ((t->t & VT_BTYPE) == VT_PTR) { MCC_TRACE("br\n");
		if (t->t & VT_VLA)
			{ MCC_TRACE("br\n"); return 1; }
		t = &t->ref->type;
	}
	return 0;
}

static int btype_size(int bt) { MCC_TRACE("enter\n");
	return bt == VT_BYTE || bt == VT_BOOL
						 ? 1
				 : bt == VT_SHORT
						 ? 2
				 : bt == VT_INT
						 ? 4
				 : bt == VT_LLONG
						 ? 8
				 : bt == VT_PTR
						 ? MCC_PTR_SIZE
						 : 0;
}

static int R_RET(int t) { MCC_TRACE("enter\n");
	if (!is_float(t))
		{ MCC_TRACE("br\n"); return REG_IRET; }
#ifdef MCC_TARGET_X86_64
	if ((t & VT_BTYPE) == VT_LDOUBLE)
		{ MCC_TRACE("br\n"); return MCC_TREG_ST0; }
#elif defined MCC_TARGET_RISCV64
	if ((t & VT_BTYPE) == VT_LDOUBLE)
		{ MCC_TRACE("br\n"); return REG_IRET; }
#endif
	return REG_FRET;
}

static int R2_RET(int t) { MCC_TRACE("enter\n");
	t &= VT_BTYPE;
	(void)t;
#if MCC_PTR_SIZE == 4
	if (t == VT_LLONG)
		{ MCC_TRACE("br\n"); return REG_IRE2; }
#elif defined MCC_TARGET_X86_64
	if (t == VT_QLONG)
		{ MCC_TRACE("br\n"); return REG_IRE2; }
	if (t == VT_QFLOAT)
		{ MCC_TRACE("br\n"); return REG_FRE2; }
#elif defined MCC_TARGET_RISCV64
	if (t == VT_LDOUBLE)
		{ MCC_TRACE("br\n"); return REG_IRE2; }
#endif
	return VT_CONST;
}

#define USING_TWO_WORDS(t) (R2_RET(t) != VT_CONST)

static void PUT_R_RET(SValue *sv, int t) { MCC_TRACE("enter\n");
	sv->r = R_RET(t), sv->r2 = R2_RET(t);
}

enum {
	BB_FFS,
	BB_CLZ,
	BB_CTZ,
	BB_CLRSB,
	BB_POPCOUNT,
	BB_PARITY
};

static int fold_bit_builtin(int op, int W, int64_t s) { MCC_TRACE("enter\n");
	uint64_t u = (uint64_t)s;
	int i, n = 0;
	if (W < 64)
		{ MCC_TRACE("br\n"); u &= ((uint64_t)1 << W) - 1; }
	switch (op) { MCC_TRACE("br\n");
	case BB_CLZ:
		for (i = W - 1; i >= 0 && !((u >> i) & 1); i--)
			{ MCC_TRACE("br\n"); n++; }
		return n;
	case BB_CTZ:
		if (u == 0)
			{ MCC_TRACE("br\n"); return W; }
		for (i = 0; i < W && !((u >> i) & 1); i++)
			{ MCC_TRACE("br\n"); n++; }
		return n;
	case BB_FFS:
		if (u == 0)
			{ MCC_TRACE("br\n"); return 0; }
		for (i = 0; i < W && !((u >> i) & 1); i++)
			;
		return i + 1;
	case BB_POPCOUNT:
		for (i = 0; i < W; i++)
			{ MCC_TRACE("br\n"); n += (u >> i) & 1; }
		return n;
	case BB_PARITY:
		for (i = 0; i < W; i++)
			{ MCC_TRACE("br\n"); n += (u >> i) & 1; }
		return n & 1;
	case BB_CLRSB: {
		int sign = (u >> (W - 1)) & 1;
		for (i = W - 2; i >= 0 && (int)((u >> i) & 1) == sign; i--)
			{ MCC_TRACE("br\n"); n++; }
		return n;
	}
	}
	return 0;
}

static int MCC_RC_RET(int t) { MCC_TRACE("enter\n");
	return reg_classes[R_RET(t)] & ~(MCC_RC_FLOAT | MCC_RC_INT);
}

static int MCC_RC_TYPE(int t) { MCC_TRACE("enter\n");
	if (!is_float(t))
		{ MCC_TRACE("br\n"); return MCC_RC_INT; }
#ifdef MCC_TARGET_X86_64
	if ((t & VT_BTYPE) == VT_LDOUBLE)
		{ MCC_TRACE("br\n"); return MCC_RC_ST0; }
	if ((t & VT_BTYPE) == VT_QFLOAT)
		{ MCC_TRACE("br\n"); return MCC_RC_FRET; }
#elif defined MCC_TARGET_RISCV64
	if ((t & VT_BTYPE) == VT_LDOUBLE)
		{ MCC_TRACE("br\n"); return MCC_RC_INT; }
#endif
	return MCC_RC_FLOAT;
}

static int RC2_TYPE(int t, int rc) { MCC_TRACE("enter\n");
	if (!USING_TWO_WORDS(t))
		{ MCC_TRACE("br\n"); return 0; }
#ifdef MCC_RC_IRE2
	if (rc == MCC_RC_IRET)
		{ MCC_TRACE("br\n"); return MCC_RC_IRE2; }
#endif
#ifdef MCC_RC_FRE2
	if (rc == MCC_RC_FRET)
		{ MCC_TRACE("br\n"); return MCC_RC_FRE2; }
#endif
	if (rc & MCC_RC_FLOAT)
		{ MCC_TRACE("br\n"); return MCC_RC_FLOAT; }
	return MCC_RC_INT;
}

ST_FUNC int ieee_finite(double d) { MCC_TRACE("enter\n");
	int p[4];
	memcpy(p, &d, sizeof(double));
	return ((unsigned)((p[1] | 0x800fffff) + 1)) >> 31;
}

ST_FUNC void test_lvalue(void) { MCC_TRACE("enter\n");
	if (!(vtop->r & VT_LVAL))
		{ MCC_TRACE("br\n"); expect("lvalue"); }
}

ST_FUNC void check_vstack(void) { MCC_TRACE("enter\n");
	if (vtop != vstack - 1)
		{ MCC_TRACE("br\n"); mcc_error("internal compiler error: vstack leak (%d)",
							(int)(vtop - vstack + 1)); }
}

MAYBE_UNUSED void pv(const char *lbl, int a, int b) { MCC_TRACE("enter\n");
	for (int i = a; i < a + b; ++i) { MCC_TRACE("br\n");
		SValue *p = &vtop[-i];
		printf("%s vtop[-%d] : type.t:%04x  r:%04x  r2:%04x  c.i:%d\n",
					 lbl, i, p->type.t, p->r, p->r2, (int)p->c.i);
	}
}

MAYBE_UNUSED static void psyms(const char *msg, Sym *s, Sym *last) { MCC_TRACE("enter\n");
	printf("%-8s scope         v        c        r   type.t\n", msg);
	while (s && s != last) { MCC_TRACE("br\n");
		printf("      %8x  %08x %08x %08x %08x %s\n",
					 s->sym_scope, s->v, s->c, s->r, s->type.t, get_tok_str(s->v, 0));
		s = s->prev;
	}
}

static void type_to_str(char *buf, int buf_size, CType *type, const char *varstr);

MAYBE_UNUSED static void ptype(const char *msg, CType *type, int v) { MCC_TRACE("enter\n");
	char buf[500];
	type_to_str(buf, sizeof(buf), type,
							(v & ~SYM_FIELD) ? get_tok_str(v, NULL) : NULL);
	printf("%s : %s;\n", msg, buf);
}

static int assign_ctx_is_init;

ST_FUNC void mccgen_init(MCCState *s1) { MCC_TRACE("enter\n");
	vtop = vstack - 1;
	memset(vtop, 0, sizeof *vtop);

	int_type.t = VT_INT;

	char_type.t = VT_BYTE;
	if (s1->char_is_unsigned)
		{ MCC_TRACE("br\n"); char_type.t |= VT_UNSIGNED; }
	char_pointer_type = char_type;
	mk_pointer(&char_pointer_type);

	func_old_type.t = VT_FUNC;
	func_old_type.ref = sym_push(SYM_FIELD, &int_type, 0, 0);
	func_old_type.ref->f.func_call = FUNC_CDECL;
	func_old_type.ref->f.func_type = FUNC_OLD;
	init_prec();
	cstr_new(&initstr);
}

ST_FUNC int mccgen_compile(MCCState *s1) { MCC_TRACE("enter\n");
	funcname = "";
	func_ind = -1;
	anon_sym = SYM_FIRST_ANOM;
	assign_ctx_is_init = 0;
	nocode_wanted = DATA_ONLY_WANTED;
	debug_modes = (s1->do_debug ? 1 : 0) | s1->test_coverage << 1;
	global_expr = 0;

#if MCC_CONFIG_OPTIMIZER
	ast_configure(s1);
#endif

	mcc_debug_start(s1);
	mcc_tcov_start(s1);
#ifdef MCC_TARGET_ARM
	arm_init(s1);
#endif
	parse_flags = PARSE_FLAG_PREPROCESS | PARSE_FLAG_TOK_NUM | PARSE_FLAG_TOK_STR;
	next();
	decl(VT_CONST);
#if MCC_CONFIG_OPTIMIZER
	ast_reemit_forward_inlines();
#endif
	finalize_tentative_arrays();
	gen_inline_functions(s1);
	resolve_alias_fixups(s1);
	if (mcc_state->warn_unused_function & WARN_ON) { MCC_TRACE("br\n");
		Sym *fs;
		for (fs = global_stack; fs; fs = fs->prev) { MCC_TRACE("br\n");
			ElfSym *es;
			if ((fs->type.t & VT_BTYPE) != VT_FUNC || !(fs->type.t & VT_STATIC) || (fs->type.t & VT_INLINE) || fs->a.used || fs->v < TOK_IDENT || fs->v >= SYM_FIRST_ANOM)
				{ MCC_TRACE("br\n"); continue; }
			es = elfsym(fs);
			if (!es || es->st_shndx == SHN_UNDEF)
				{ MCC_TRACE("br\n"); continue; }
			mcc_warning_c(warn_unused_function)(
					"%i:'%s' defined but not used",
					fs->vla_inner_id, get_tok_str(fs->v, NULL));
		}
	}
	if (mcc_state->warn_all & WARN_ON) { MCC_TRACE("br\n");
		Sym *fs;
		for (fs = global_stack; fs; fs = fs->prev) { MCC_TRACE("br\n");
			ElfSym *es;
			if ((fs->type.t & VT_BTYPE) != VT_FUNC || !(fs->type.t & VT_STATIC) || (fs->type.t & VT_INLINE) || !fs->a.used || fs->v < TOK_IDENT || fs->v >= SYM_FIRST_ANOM)
				{ MCC_TRACE("br\n"); continue; }
			es = elfsym(fs);
			if (es && es->st_shndx != SHN_UNDEF)
				{ MCC_TRACE("br\n"); continue; }
			mcc_warning_c(warn_all)("'%s' used but never defined",
															get_tok_str(fs->v, NULL));
		}
	}
	check_vstack();
#if MCC_EH_FRAME
	mcc_eh_frame_end(s1);
#endif
	mcc_debug_end(s1);
	mcc_tcov_end(s1);
	return 0;
}

static void finalize_tentative_arrays(void) { MCC_TRACE("enter\n");
	Sym *sym;
	for (sym = global_stack; sym; sym = sym->prev) { MCC_TRACE("br\n");
		ElfSym *esym;
		int size, align;
		if (!sym->a.tentative_array)
			{ MCC_TRACE("br\n"); continue; }
		if (!(sym->type.t & VT_ARRAY) || sym->type.ref->c >= 0)
			{ MCC_TRACE("br\n"); continue; }
		esym = elfsym(sym);
		if (esym && esym->st_shndx != SHN_UNDEF)
			{ MCC_TRACE("br\n"); continue; }
		mcc_warning_c(warn_all)("array '%s' assumed to have one element",
														get_tok_str(sym->v, NULL));
		sym->type.ref->c = 1;
		sym->type.t &= ~VT_EXTERN;
		size = type_size(&sym->type, &align);
		put_extern_sym(sym, common_section, align, size);
	}
}

ST_FUNC void mccgen_finish(MCCState *s1) { MCC_TRACE("enter\n");
	mcc_debug_end(s1);
	free_inline_functions(s1);
	sym_pop(&global_stack, NULL, 0);
	memset(s1->gen_complex_type_cache, 0, sizeof s1->gen_complex_type_cache);
	memset(s1->gen_complex_call_ftype, 0, sizeof s1->gen_complex_call_ftype);
	s1->gen_complex_re_tok = s1->gen_complex_im_tok = 0;
	sym_pop(&local_stack, NULL, 0);
	free_defines(NULL);
	dynarray_reset(&sym_pools, &nb_sym_pools);
	cstr_free(&initstr);
	dynarray_reset(&stk_data, &nb_stk_data);
	while (cur_switch)
		{ MCC_TRACE("br\n"); end_switch(); }
	local_scope = 0;
	loop_scope = NULL;
	all_cleanups = NULL;
	pending_gotos = NULL;
	nb_temp_local_vars = 0;
	global_label_stack = NULL;
	local_label_stack = NULL;
	cur_text_section = NULL;
	sym_free_first = NULL;
}

ST_FUNC ElfSym *elfsym(Sym *s) { MCC_TRACE("enter\n");
	if (!s || !s->c)
		{ MCC_TRACE("br\n"); return NULL; }
	return &((ElfSym *)symtab_section->data)[s->c];
}

ST_FUNC void update_storage(Sym *sym) { MCC_TRACE("enter\n");
	ElfSym *esym;
	int sym_bind, old_sym_bind;

	esym = elfsym(sym);
	if (!esym)
		{ MCC_TRACE("br\n"); return; }

	if (sym->a.visibility_set)
		{ MCC_TRACE("br\n"); esym->st_other = (esym->st_other & ~ELFW(ST_VISIBILITY)(-1)) | sym->a.visibility; }
	else if (mcc_state->visibility && esym->st_shndx != SHN_UNDEF)
		{ MCC_TRACE("br\n"); esym->st_other = (esym->st_other & ~ELFW(ST_VISIBILITY)(-1)) | mcc_state->visibility; }

	if (sym->type.t & (VT_STATIC | VT_INLINE))
		{ MCC_TRACE("br\n"); sym_bind = STB_LOCAL; }
	else if (sym->a.weak)
		{ MCC_TRACE("br\n"); sym_bind = STB_WEAK; }
	else
		{ MCC_TRACE("br\n"); sym_bind = STB_GLOBAL; }
	old_sym_bind = ELFW(ST_BIND)(esym->st_info);
	if (sym_bind != old_sym_bind) { MCC_TRACE("br\n");
		esym->st_info = ELFW(ST_INFO)(sym_bind, ELFW(ST_TYPE)(esym->st_info));
	}

#ifdef MCC_TARGET_PE
	if (sym->a.dllimport)
		{ MCC_TRACE("br\n"); esym->st_other |= ST_PE_IMPORT; }
	if (sym->a.dllexport)
		{ MCC_TRACE("br\n"); esym->st_other |= ST_PE_EXPORT; }
#endif

}

ST_FUNC void put_extern_sym2(Sym *sym, int sh_num,
														 addr_t value, unsigned long size,
														 int can_add_underscore) { MCC_TRACE("enter\n");
	int sym_type, sym_bind, info, other, t;
	ElfSym *esym;
	const char *name;
	char buf1[256];

	if (!sym->c) { MCC_TRACE("br\n");
		name = get_tok_str(sym->v, NULL);
		t = sym->type.t;
		if ((t & VT_BTYPE) == VT_FUNC) { MCC_TRACE("br\n");
			sym_type = STT_FUNC;
		} else if ((t & VT_BTYPE) == VT_VOID) { MCC_TRACE("br\n");
			sym_type = STT_NOTYPE;
			if (IS_ASM_FUNC(t))
				{ MCC_TRACE("br\n"); sym_type = STT_FUNC; }
		} else if (t & VT_TLS) { MCC_TRACE("br\n");
			sym_type = STT_TLS;
		} else { MCC_TRACE("br\n");
			sym_type = STT_OBJECT;
		}
		if (t & (VT_STATIC | VT_INLINE))
			{ MCC_TRACE("br\n"); sym_bind = STB_LOCAL; }
		else
			{ MCC_TRACE("br\n"); sym_bind = STB_GLOBAL; }
		other = 0;

#ifdef MCC_TARGET_PE
		if (sym_type == STT_FUNC && sym->type.ref) { MCC_TRACE("br\n");
			Sym *ref = sym->type.ref;
			if (ref->a.nodecorate) { MCC_TRACE("br\n");
				can_add_underscore = 0;
			}
			if (ref->f.func_call == FUNC_STDCALL && can_add_underscore) { MCC_TRACE("br\n");
				snprintf(buf1, sizeof(buf1), "_%s@%d", name, ref->f.func_args * MCC_PTR_SIZE);
				name = buf1;
				other |= ST_PE_STDCALL;
				can_add_underscore = 0;
			}
		}
#endif

		if (sym->asm_label) { MCC_TRACE("br\n");
			name = get_tok_str(sym->asm_label, NULL);
			can_add_underscore = 0;
		}

		if (mcc_state->leading_underscore && can_add_underscore) { MCC_TRACE("br\n");
			buf1[0] = '_';
			pstrcpy(buf1 + 1, sizeof(buf1) - 1, name);
			name = buf1;
		}

		info = ELFW(ST_INFO)(sym_bind, sym_type);
		sym->c = put_elf_sym(symtab_section, value, size, info, other, sh_num, name);

		if (debug_modes)
			{ MCC_TRACE("br\n"); mcc_debug_extern_sym(mcc_state, sym, sh_num, sym_bind, sym_type); }
	} else { MCC_TRACE("br\n");
		esym = elfsym(sym);
		esym->st_value = value;
		esym->st_size = size;
		esym->st_shndx = sh_num;
	}
	update_storage(sym);
}

ST_FUNC void put_extern_sym(Sym *sym, Section *s, addr_t value, unsigned long size) { MCC_TRACE("enter\n");
	if (nocode_wanted && (NODATA_WANTED || (s && s == cur_text_section)))
		{ MCC_TRACE("br\n"); return; }
	put_extern_sym2(sym, s ? s->sh_num : SHN_UNDEF, value, size, 1);
}

ST_FUNC void greloca(Section *s, Sym *sym, unsigned long offset, int type,
										 addr_t addend) { MCC_TRACE("enter\n");
	int c = 0;

	if (nocode_wanted && s == cur_text_section)
		{ MCC_TRACE("br\n"); return; }

	if (sym) { MCC_TRACE("br\n");
		if (0 == sym->c) { MCC_TRACE("br\n");
			put_extern_sym(sym, NULL, 0, 0);
			if (sym->sym_scope && (sym->type.t & (VT_STATIC | VT_EXTERN)) == (VT_STATIC | VT_EXTERN)) { MCC_TRACE("br\n");
				Sym *s = sym;
				while (s->prev_tok)
					{ MCC_TRACE("br\n"); s = s->prev_tok; }
				s->c = sym->c;
			}
		}
		c = sym->c;
	}

	put_elf_reloca(symtab_section, s, offset, type, c, addend);
}

#if MCC_PTR_SIZE == 4
ST_FUNC void greloc(Section *s, Sym *sym, unsigned long offset, int type) { MCC_TRACE("enter\n");
	greloca(s, sym, offset, type, 0);
}
#endif

static Sym *__sym_malloc(void) { MCC_TRACE("enter\n");
	Sym *sym_pool, *sym, *last_sym;

	sym_pool = mcc_malloc(SYM_POOL_NB * sizeof(Sym));
	dynarray_add(&sym_pools, &nb_sym_pools, sym_pool);

	last_sym = sym_free_first;
	sym = sym_pool;
	for (int i = 0; i < SYM_POOL_NB; i++) { MCC_TRACE("br\n");
		sym->next = last_sym;
		last_sym = sym;
		sym++;
	}
	sym_free_first = last_sym;
	return last_sym;
}

static inline Sym *sym_malloc(void) { MCC_TRACE("enter\n");
	Sym *sym;
#ifndef MCC_SYM_DEBUG
	sym = sym_free_first;
	if (!sym)
		{ MCC_TRACE("br\n"); sym = __sym_malloc(); }
	sym_free_first = sym->next;
	return sym;
#else
	sym = mcc_malloc(sizeof(Sym));
	return sym;
#endif
}

ST_INLN void sym_free(Sym *sym) { MCC_TRACE("enter\n");
#if MCC_CONFIG_OPTIMIZER
	if (ast_sym_defer(sym))
		{ MCC_TRACE("br\n"); return; }
#endif
#ifndef MCC_SYM_DEBUG
	sym->next = sym_free_first;
	sym_free_first = sym;
#else
	mcc_free(sym);
#endif
}

ST_FUNC Sym *sym_push2(Sym **ps, int v, int t, int c) { MCC_TRACE("enter\n");
	Sym *s;

	s = sym_malloc();
	memset(s, 0, sizeof *s);
	s->v = v;
	s->type.t = t;
	s->c = c;
	s->prev = *ps;
	*ps = s;
	return s;
}

ST_FUNC Sym *sym_find2(Sym *s, int v) { MCC_TRACE("enter\n");
	while (s) { MCC_TRACE("br\n");
		if (s->v == v)
			{ MCC_TRACE("br\n"); return s; }
		s = s->prev;
	}
	return NULL;
}

ST_INLN Sym *struct_find(int v) { MCC_TRACE("enter\n");
	v -= TOK_IDENT;
	if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
		{ MCC_TRACE("br\n"); return NULL; }
	return table_ident[v]->sym_struct;
}

ST_INLN Sym *sym_find(int v) { MCC_TRACE("enter\n");
	v -= TOK_IDENT;
	if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
		{ MCC_TRACE("br\n"); return NULL; }
	return table_ident[v]->sym_identifier;
}

static inline void sym_link(Sym *s, int yes) { MCC_TRACE("enter\n");
	TokenSym *ts = table_ident[(s->v & ~SYM_STRUCT) - TOK_IDENT];
	Sym **ps;
	if (s->v & SYM_STRUCT)
		{ MCC_TRACE("br\n"); ps = &ts->sym_struct; }
	else
		{ MCC_TRACE("br\n"); ps = &ts->sym_identifier; }
	if (yes) { MCC_TRACE("br\n");
		s->prev_tok = *ps, *ps = s;
		s->sym_scope = local_scope;
	} else { MCC_TRACE("br\n");
		*ps = s->prev_tok;
	}
}

static inline int sym_scope_ex(Sym *s) { MCC_TRACE("enter\n");
	return IS_ENUM_VAL(s->type.t)
						 ? s->type.ref->sym_scope
						 : s->sym_scope;
}

ST_FUNC Sym *sym_push(int v, CType *type, int r, int c) { MCC_TRACE("enter\n");
	Sym *s, **ps;
	if (local_stack)
		{ MCC_TRACE("br\n"); ps = &local_stack; }
	else
		{ MCC_TRACE("br\n"); ps = &global_stack; }
	s = sym_push2(ps, v, type->t, c);
	s->type.ref = type->ref;
	s->r = r;
	if ((v & ~SYM_STRUCT) < SYM_FIRST_ANOM) { MCC_TRACE("br\n");
		sym_link(s, 1);
		if (s->prev_tok && sym_scope_ex(s->prev_tok) == local_scope)
			{ MCC_TRACE("br\n"); mcc_error("redeclaration of '%s'", get_tok_str(s->v, NULL)); }
	}
	return s;
}

ST_FUNC Sym *global_identifier_push(int v, int t, int c) { MCC_TRACE("enter\n");
	Sym *s, **ps;
	s = sym_push2(&global_stack, v, t, c);
	s->r = VT_CONST | VT_SYM;
	if (v < SYM_FIRST_ANOM) { MCC_TRACE("br\n");
		ps = &table_ident[v - TOK_IDENT]->sym_identifier;
		while (*ps != NULL && (*ps)->sym_scope)
			{ MCC_TRACE("br\n"); ps = &(*ps)->prev_tok; }
		s->prev_tok = *ps;
		*ps = s;
	}
	return s;
}

ST_FUNC void sym_pop(Sym **ptop, Sym *b, int keep) { MCC_TRACE("enter\n");
	Sym *s, *ss;
	int v;

	s = *ptop;
	while (s != b) { MCC_TRACE("br\n");
		ss = s->prev;
		v = s->v;
		if ((v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
			{ MCC_TRACE("br\n"); sym_link(s, 0); }
		if (!keep)
			{ MCC_TRACE("br\n"); sym_free(s); }
		s = ss;
	}
	if (!keep)
		{ MCC_TRACE("br\n"); *ptop = b; }
}

ST_FUNC Sym *label_find(int v) { MCC_TRACE("enter\n");
	v -= TOK_IDENT;
	if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
		{ MCC_TRACE("br\n"); return NULL; }
	return table_ident[v]->sym_label;
}

ST_FUNC Sym *label_push(Sym **ptop, int v, int flags) { MCC_TRACE("enter\n");
	Sym *s, **ps;
	s = sym_push2(ptop, v, VT_STATIC, 0);
	s->r = flags;
	s->vla_inner_id = 0;
	s->vla_min_goto_gpp = INT_MAX;
	ps = &table_ident[v - TOK_IDENT]->sym_label;
	if (ptop == &global_label_stack) { MCC_TRACE("br\n");
		while (*ps != NULL)
			{ MCC_TRACE("br\n"); ps = &(*ps)->prev_tok; }
	}
	s->prev_tok = *ps;
	*ps = s;
	return s;
}

ST_FUNC void label_pop(Sym **ptop, Sym *slast, int keep) { MCC_TRACE("enter\n");
	Sym *s, *s1;
	for (s = *ptop; s != slast; s = s1) { MCC_TRACE("br\n");
		s1 = s->prev;
		if (s->r == LABEL_DECLARED) { MCC_TRACE("br\n");
			mcc_warning_c(warn_all)("label '%s' declared but not used", get_tok_str(s->v, NULL));
		} else if (s->r == LABEL_FORWARD) { MCC_TRACE("br\n");
			mcc_error("label '%s' used but not defined",
								get_tok_str(s->v, NULL));
		} else { MCC_TRACE("br\n");
			if (s->c) { MCC_TRACE("br\n");
				put_extern_sym(s, cur_text_section, s->jnext, 1);
			}
		}
		if (s->r != LABEL_GONE)
			{ MCC_TRACE("br\n"); table_ident[s->v - TOK_IDENT]->sym_label = s->prev_tok; }
		if (!keep)
			{ MCC_TRACE("br\n"); sym_free(s); }
		else
			{ MCC_TRACE("br\n"); s->r = LABEL_GONE; }
	}
	if (!keep)
		{ MCC_TRACE("br\n"); *ptop = slast; }
}

static void vcheck_cmp(void) { MCC_TRACE("enter\n");
	if (vtop->r == VT_CMP && 0 == (nocode_wanted & ~CODE_OFF_BIT)) { MCC_TRACE("br\n");
#if MCC_CONFIG_OPTIMIZER
		int sup = ast_active && !ast_replaying;
		if (sup)
			{ MCC_TRACE("br\n"); ast_in_op++; }
		gv(MCC_RC_INT);
		if (sup)
			{ MCC_TRACE("br\n"); ast_in_op--; }
#else
		gv(MCC_RC_INT);
#endif
	}
}

static void vsetc(CType *type, int r, CValue *vc) { MCC_TRACE("enter\n");
	if (vtop >= vstack + (VSTACK_SIZE - 1))
		{ MCC_TRACE("br\n"); mcc_error("memory full (vstack)"); }
	vcheck_cmp();
	vtop++;
	vtop->type = *type;
	vtop->r = r;
	vtop->r2 = VT_CONST;
	vtop->c = *vc;
	vtop->sym = NULL;
#if MCC_CONFIG_OPTIMIZER
	ast_hook_vpush();
#endif
}

ST_FUNC void vswap(void) { MCC_TRACE("enter\n");
	SValue tmp;
#if MCC_CONFIG_OPTIMIZER
	ast_hook_vswap();
#endif

	vcheck_cmp();
	tmp = vtop[0];
	vtop[0] = vtop[-1];
	vtop[-1] = tmp;
}

ST_FUNC void vpop(void) { MCC_TRACE("enter\n");
	int v;
#if MCC_CONFIG_OPTIMIZER
	ast_hook_vpop();
#endif
	v = vtop->r & VT_VALMASK;
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
	if (v == MCC_TREG_ST0) { MCC_TRACE("br\n");
		o(0xd8dd);
	} else
#endif
			if (v == VT_CMP) { MCC_TRACE("br\n");
		gsym(vtop->jtrue);
		gsym(vtop->jfalse);
	}
	vtop--;
}

static void vpush(CType *type) { MCC_TRACE("enter\n");
	vset(type, VT_CONST, 0);
}

static void vpush64(int ty, unsigned long long v) { MCC_TRACE("enter\n");
	vsetc(&(CType){.t = ty, .ref = NULL}, VT_CONST, &(CValue){.i = v});
}

ST_FUNC void vpushi(int v) { MCC_TRACE("enter\n");
	vpush64(VT_INT, v);
}

static void vpushs(addr_t v) { MCC_TRACE("enter\n");
	vpush64(VT_SIZE_T, v);
}

static inline void vpushll(long long v) { MCC_TRACE("enter\n");
	vpush64(VT_LLONG, v);
}

ST_FUNC void vset(CType *type, int r, int v) { MCC_TRACE("enter\n");
	vsetc(type, r, &(CValue){.i = v});
}

static void vseti(int r, int v) { MCC_TRACE("enter\n");
	vset(&(CType){.t = VT_INT, .ref = NULL}, r, v);
}

ST_FUNC void vpushv(SValue *v) { MCC_TRACE("enter\n");
	if (vtop >= vstack + (VSTACK_SIZE - 1))
		{ MCC_TRACE("br\n"); mcc_error("memory full (vstack)"); }
	vtop++;
	*vtop = *v;
#if MCC_CONFIG_OPTIMIZER
	ast_hook_vpush();
#endif
}

static void vdup(void) { MCC_TRACE("enter\n");
	vpushv(vtop);
}

ST_FUNC void vrotb(int n) { MCC_TRACE("enter\n");
	SValue tmp;
	if (--n < 1)
		{ MCC_TRACE("br\n"); return; }
	vcheck_cmp();
	tmp = vtop[-n];
	memmove(vtop - n, vtop - n + 1, sizeof *vtop * n);
	vtop[0] = tmp;
}

ST_FUNC void vrott(int n) { MCC_TRACE("enter\n");
	SValue tmp;
	if (--n < 1)
		{ MCC_TRACE("br\n"); return; }
	vcheck_cmp();
	tmp = vtop[0];
	memmove(vtop - n + 1, vtop - n, sizeof *vtop * n);
	vtop[-n] = tmp;
}

ST_FUNC void vrev(int n) { MCC_TRACE("enter\n");
	int i;
	SValue tmp;
	vcheck_cmp();
	for (i = 0, n = -n; i > ++n; --i)
		{ MCC_TRACE("br\n"); tmp = vtop[i], vtop[i] = vtop[n], vtop[n] = tmp; }
}

ST_FUNC void vset_VT_CMP(int op) { MCC_TRACE("enter\n");
	vtop->r = VT_CMP;
	vtop->cmp_op = op;
	vtop->jfalse = 0;
	vtop->jtrue = 0;
}

static void vset_VT_JMP(void) { MCC_TRACE("enter\n");
	int op = vtop->cmp_op;

	if (vtop->jtrue || vtop->jfalse) { MCC_TRACE("br\n");
		int origt = vtop->type.t;
		int inv = op & (op < 2);
		vseti(VT_JMP + inv, gvtst(inv, 0));
		vtop->type.t |= origt & (VT_UNSIGNED | VT_DEFSIGN);
	} else { MCC_TRACE("br\n");
		vtop->c.i = op;
		if (op < 2)
			{ MCC_TRACE("br\n"); vtop->r = VT_CONST; }
	}
}

static void gvtst_set(int inv, int t) { MCC_TRACE("enter\n");
	int *p;

	if (vtop->r != VT_CMP) { MCC_TRACE("br\n");
		vpushi(0);
		gen_op(TOK_NE);
		if (vtop->r != VT_CMP)
			{ MCC_TRACE("br\n"); vset_VT_CMP(vtop->c.i != 0); }
	}

	p = inv ? &vtop->jfalse : &vtop->jtrue;
	*p = gjmp_append(*p, t);
}

static int gvtst(int inv, int t) { MCC_TRACE("enter\n");
	int op, x, u;

	gvtst_set(inv, t);
	t = vtop->jtrue, u = vtop->jfalse;
	if (inv)
		{ MCC_TRACE("br\n"); x = u, u = t, t = x; }
	op = vtop->cmp_op;

	if (op > 1)
		{ MCC_TRACE("br\n"); t = gjmp_cond(op ^ inv, t); }
	else if (op != inv)
		{ MCC_TRACE("br\n"); t = gjmp(t); }
	gsym(u);

	vtop--;
	return t;
}

static void gen_test_zero(int op) { MCC_TRACE("enter\n");
	if (vtop->r == VT_CMP) { MCC_TRACE("br\n");
		int j;
		if (op == TOK_EQ) { MCC_TRACE("br\n");
			j = vtop->jfalse;
			vtop->jfalse = vtop->jtrue;
			vtop->jtrue = j;
			vtop->cmp_op ^= 1;
		}
	} else { MCC_TRACE("br\n");
		vpushi(0);
		gen_op(op);
	}
}

ST_FUNC void vpushsym(CType *type, Sym *sym) { MCC_TRACE("enter\n");
	CValue cval;
	cval.i = 0;
	vsetc(type, VT_CONST | VT_SYM, &cval);
	vtop->sym = sym;
}

ST_FUNC Sym *get_sym_ref(CType *type, Section *sec, unsigned long offset, unsigned long size) { MCC_TRACE("enter\n");
	int v;
	Sym *sym;

	v = anon_sym++;
	sym = sym_push(v, type, VT_CONST | VT_SYM, 0);
	sym->type.t |= VT_STATIC;
	put_extern_sym(sym, sec, offset, size);
	return sym;
}

static void vpush_ref(CType *type, Section *sec, unsigned long offset, unsigned long size) { MCC_TRACE("enter\n");
	vpushsym(type, get_sym_ref(type, sec, offset, size));
}

ST_FUNC Sym *external_global_sym(int v, CType *type) { MCC_TRACE("enter\n");
	Sym *s;

	s = sym_find(v);
	if (!s) { MCC_TRACE("br\n");
		s = global_identifier_push(v, type->t | VT_EXTERN, 0);
		s->type.ref = type->ref;
	} else if (IS_ASM_SYM(s)) { MCC_TRACE("br\n");
		s->type.t = type->t | (s->type.t & VT_EXTERN);
		s->type.ref = type->ref;
		update_storage(s);
	}
	return s;
}

ST_FUNC Sym *external_helper_sym(int v) { MCC_TRACE("enter\n");
	CType ct = {VT_ASM_FUNC, NULL};
	return external_global_sym(v, &ct);
}

ST_FUNC void vpush_helper_func(int v) { MCC_TRACE("enter\n");
	vpushsym(&func_old_type, external_helper_sym(v));
}

static void merge_symattr(struct SymAttr *sa, struct SymAttr *sa1) { MCC_TRACE("enter\n");
	if (sa1->aligned && !sa->aligned)
		{ MCC_TRACE("br\n"); sa->aligned = sa1->aligned; }
	sa->packed |= sa1->packed;
	sa->weak |= sa1->weak;
	sa->nodebug |= sa1->nodebug;
	if (sa1->visibility != STV_DEFAULT) { MCC_TRACE("br\n");
		int vis = sa->visibility;
		if (vis == STV_DEFAULT || vis > sa1->visibility)
			{ MCC_TRACE("br\n"); vis = sa1->visibility; }
		sa->visibility = vis;
	}
	sa->visibility_set |= sa1->visibility_set;
	sa->dllexport |= sa1->dllexport;
	sa->nodecorate |= sa1->nodecorate;
	sa->dllimport |= sa1->dllimport;
}

static void merge_funcattr(struct FuncAttr *fa, struct FuncAttr *fa1) { MCC_TRACE("enter\n");
	if (fa1->func_call && !fa->func_call)
		{ MCC_TRACE("br\n"); fa->func_call = fa1->func_call; }
	if (fa1->func_type && !fa->func_type)
		{ MCC_TRACE("br\n"); fa->func_type = fa1->func_type; }
	if (fa1->func_args && !fa->func_args)
		{ MCC_TRACE("br\n"); fa->func_args = fa1->func_args; }
	if (fa1->func_noreturn)
		{ MCC_TRACE("br\n"); fa->func_noreturn = 1; }
	if (fa1->func_ctor)
		{ MCC_TRACE("br\n"); fa->func_ctor = 1; }
	if (fa1->func_dtor)
		{ MCC_TRACE("br\n"); fa->func_dtor = 1; }
}

static void merge_attr(AttributeDef *ad, AttributeDef *ad1) { MCC_TRACE("enter\n");
	merge_symattr(&ad->a, &ad1->a);
	merge_funcattr(&ad->f, &ad1->f);

	if (ad1->section)
		{ MCC_TRACE("br\n"); ad->section = ad1->section; }
	if (ad1->alias_target)
		{ MCC_TRACE("br\n"); ad->alias_target = ad1->alias_target; }
	if (ad1->asm_label)
		{ MCC_TRACE("br\n"); ad->asm_label = ad1->asm_label; }
	if (ad1->attr_mode)
		{ MCC_TRACE("br\n"); ad->attr_mode = ad1->attr_mode; }
}

static void patch_type(Sym *sym, CType *type) { MCC_TRACE("enter\n");
	if (!(type->t & VT_EXTERN) || IS_ENUM_VAL(sym->type.t)) { MCC_TRACE("br\n");
		if (!(sym->type.t & VT_EXTERN))
			{ MCC_TRACE("br\n"); mcc_error("redefinition of '%s'", get_tok_str(sym->v, NULL)); }
		sym->type.t &= ~VT_EXTERN;
	}

	if (IS_ASM_SYM(sym)) { MCC_TRACE("br\n");
		sym->type.t = type->t & (sym->type.t | ~VT_STATIC);
		sym->type.ref = type->ref;
		if ((type->t & VT_BTYPE) != VT_FUNC && !(type->t & VT_ARRAY))
			{ MCC_TRACE("br\n"); sym->r |= VT_LVAL; }
	}

	if (!is_compatible_types(&sym->type, type)) { MCC_TRACE("br\n");
		mcc_error("incompatible types for redefinition of '%s'",
							get_tok_str(sym->v, NULL));
	} else if ((sym->type.t & VT_BTYPE) == VT_FUNC) { MCC_TRACE("br\n");
		int static_proto = sym->type.t & VT_STATIC;
		int ft1 = sym->type.ref->f.func_type;
		int ft2 = type->ref->f.func_type;

		if ((type->t & VT_STATIC) && !static_proto && !((type->t | sym->type.t) & VT_INLINE))
			{ MCC_TRACE("br\n"); mcc_warning("static storage ignored for redefinition of '%s'",
									get_tok_str(sym->v, NULL)); }

		if ((type->t | sym->type.t) & VT_INLINE) { MCC_TRACE("br\n");
			if (!((type->t ^ sym->type.t) & VT_INLINE) || ((type->t | sym->type.t) & VT_STATIC))
				{ MCC_TRACE("br\n"); static_proto |= VT_INLINE | (type->t & VT_STATIC); }
		}

		if (0 == (type->t & VT_EXTERN)) { MCC_TRACE("br\n");
			struct FuncAttr f = sym->type.ref->f;
			sym->type.t = (type->t & ~(VT_STATIC | VT_INLINE)) | static_proto;
			if (ft1 != FUNC_OLD)
				{ MCC_TRACE("br\n"); type->ref->f.func_type = ft1; }
			sym->type.ref = type->ref;
			merge_funcattr(&sym->type.ref->f, &f);
		} else { MCC_TRACE("br\n");
			sym->type.t &= ~VT_INLINE | static_proto;
			if (ft1 == FUNC_OLD && ft2 != FUNC_OLD)
				{ MCC_TRACE("br\n"); sym->type.ref = type->ref; }
		}
	} else { MCC_TRACE("br\n");
		if ((sym->type.t & VT_ARRAY) && type->ref->c >= 0) { MCC_TRACE("br\n");
			sym->type.ref->c = type->ref->c;
		}
		if ((type->t ^ sym->type.t) & VT_STATIC) { MCC_TRACE("br\n");
			if ((type->t & VT_STATIC) && !(sym->type.t & VT_STATIC))
				{ MCC_TRACE("br\n"); mcc_error("static declaration of '%s' follows non-static "
									"declaration",
									get_tok_str(sym->v, NULL)); }
			else if (!(type->t & (VT_STATIC | VT_EXTERN)) && (sym->type.t & VT_STATIC))
				{ MCC_TRACE("br\n"); mcc_error("non-static declaration of '%s' follows static "
									"declaration",
									get_tok_str(sym->v, NULL)); }
		}
	}
}

static void patch_storage(Sym *sym, AttributeDef *ad, CType *type) { MCC_TRACE("enter\n");
	if (type)
		{ MCC_TRACE("br\n"); patch_type(sym, type); }

#ifdef MCC_TARGET_PE
	if (sym->a.dllimport != ad->a.dllimport)
		{ MCC_TRACE("br\n"); mcc_error("incompatible dll linkage for redefinition of '%s'",
							get_tok_str(sym->v, NULL)); }
#endif
	merge_symattr(&sym->a, &ad->a);
	if (ad->asm_label)
		{ MCC_TRACE("br\n"); sym->asm_label = ad->asm_label; }
	update_storage(sym);
}

static Sym *sym_copy(Sym *s0, Sym **ps) { MCC_TRACE("enter\n");
	Sym *s;
	s = sym_malloc(), *s = *s0;
	s->prev = *ps, *ps = s;
	if ((s->v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
		{ MCC_TRACE("br\n"); sym_link(s, 1); }
	return s;
}

static void move_ref_to_global(Sym *s) { MCC_TRACE("enter\n");
	Sym *l, **lp;
	int n, bt;

	bt = s->type.t & VT_BTYPE;
	if (!(bt == VT_PTR || bt == VT_FUNC || bt == VT_STRUCT || IS_ENUM(s->type.t)))
		{ MCC_TRACE("br\n"); return; }

	for (s = s->type.ref, n = 0; s; s = s->next) { MCC_TRACE("br\n");
		for (lp = &local_stack; !!(l = *lp); lp = &l->prev) { MCC_TRACE("br\n");
			if (l == s) { MCC_TRACE("br\n");
				*lp = s->prev;
				s->prev = global_stack, global_stack = s;
				if (n || bt == VT_PTR || bt == VT_FUNC) { MCC_TRACE("br\n");
					move_ref_to_global(s);
				} else { MCC_TRACE("br\n");
					if ((s->v & ~SYM_STRUCT) < SYM_FIRST_ANOM) { MCC_TRACE("br\n");
						s->v |= SYM_FIELD;
						l = sym_copy(s, lp);
						l->v &= ~SYM_FIELD;
					}
				}
				if (bt != VT_PTR)
					{ MCC_TRACE("br\n"); n = 1; }
				break;
			}
		}
		if (n == 0)
			{ MCC_TRACE("br\n"); break; }
	}
}

static Sym *external_sym(int v, CType *type, int r, AttributeDef *ad) { MCC_TRACE("enter\n");
	Sym *s;

	s = sym_find(v);
	while (s && s->sym_scope)
		{ MCC_TRACE("br\n"); s = s->prev_tok; }

	if (!s) { MCC_TRACE("br\n");
		s = global_identifier_push(v, type->t, 0);
		s->r |= r;
		s->a = ad->a;
		s->asm_label = ad->asm_label;
		s->type.ref = type->ref;
	} else { MCC_TRACE("br\n");
		patch_storage(s, ad, type);
	}
	if (local_stack) { MCC_TRACE("br\n");
		move_ref_to_global(s);
		sym_copy(s, &local_stack);
	}
	return s;
}

ST_FUNC void save_regs(int n) { MCC_TRACE("enter\n");
	SValue *p, *p1;
	for (p = vstack, p1 = vtop - n; p <= p1; p++)
		{ MCC_TRACE("br\n"); save_reg(p->r); }
}

ST_FUNC void save_reg(int r) { MCC_TRACE("enter\n");
	save_reg_upstack(r, 0);
}

ST_FUNC void save_reg_upstack(int r, int n) { MCC_TRACE("enter\n");
	int l, size, align, bt, r2;
	SValue *p, *p1, sv;

	if ((r &= VT_VALMASK) >= VT_CONST)
		{ MCC_TRACE("br\n"); return; }
	if (nocode_wanted)
		{ MCC_TRACE("br\n"); return; }
	l = r2 = 0;
	for (p = vstack, p1 = vtop - n; p <= p1; p++) { MCC_TRACE("br\n");
		if ((p->r & VT_VALMASK) == r || p->r2 == r) { MCC_TRACE("br\n");
			if (!l) { MCC_TRACE("br\n");
				bt = p->type.t & VT_BTYPE;
				if (bt == VT_VOID)
					{ MCC_TRACE("br\n"); continue; }
				if ((p->r & VT_LVAL) || bt == VT_FUNC)
					{ MCC_TRACE("br\n"); bt = VT_PTR; }
				sv.type.t = bt;
				size = type_size(&sv.type, &align);
				l = get_temp_local_var(size, align, &r2);
				sv.r = VT_LOCAL | VT_LVAL;
				sv.c.i = l;
				sv.sym = NULL;
				store(p->r & VT_VALMASK, &sv);
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
				if (r == MCC_TREG_ST0) { MCC_TRACE("br\n");
					o(0xd8dd);
				}
#endif
				if (p->r2 < VT_CONST && USING_TWO_WORDS(bt)) { MCC_TRACE("br\n");
					sv.c.i += MCC_PTR_SIZE;
					store(p->r2, &sv);
				}
			}
			if (p->r & VT_LVAL) { MCC_TRACE("br\n");
				p->r = (p->r & ~(VT_VALMASK | VT_BOUNDED)) | VT_LLOCAL;
			} else { MCC_TRACE("br\n");
				p->r = VT_LVAL | VT_LOCAL;
				p->type.t &= ~VT_ARRAY;
			}
			p->sym = NULL;
			p->r2 = r2;
			p->c.i = l;
		}
	}
}

#ifdef MCC_TARGET_ARM
ST_FUNC int get_reg_ex(int rc, int rc2) { MCC_TRACE("enter\n");
	int r;
	SValue *p;

	for (r = 0; r < MCC_NB_REGS; r++) { MCC_TRACE("br\n");
		if (reg_classes[r] & rc2) { MCC_TRACE("br\n");
#if MCC_CONFIG_OPTIMIZER
			if (ast_pinned_regs & ((uint64_t)1 << r))
				{ MCC_TRACE("br\n"); continue; }
#endif
			int n;
			n = 0;
			for (p = vstack; p <= vtop; p++) { MCC_TRACE("br\n");
				if ((p->r & VT_VALMASK) == r ||
						p->r2 == r)
					{ MCC_TRACE("br\n"); n++; }
			}
			if (n <= 1)
				{ MCC_TRACE("br\n"); return r; }
		}
	}
	return get_reg(rc);
}
#endif

ST_FUNC int get_reg(int rc) { MCC_TRACE("enter\n");
	int r;
	SValue *p;

	for (r = 0; r < MCC_NB_REGS; r++) { MCC_TRACE("br\n");
		if (reg_classes[r] & rc) { MCC_TRACE("br\n");
#if MCC_CONFIG_OPTIMIZER
			if (ast_pinned_regs & ((uint64_t)1 << r))
				{ MCC_TRACE("br\n"); continue; }
#endif
			if (nocode_wanted)
				{ MCC_TRACE("br\n"); return r; }
			for (p = vstack; p <= vtop; p++) { MCC_TRACE("br\n");
				if ((p->r & VT_VALMASK) == r ||
						p->r2 == r)
					{ MCC_TRACE("br\n"); goto notfound; }
			}
			return r;
		}
	notfound:;
	}

	for (p = vstack; p <= vtop; p++) { MCC_TRACE("br\n");
		r = p->r2;
#if MCC_CONFIG_OPTIMIZER
		if (r < VT_CONST && (ast_pinned_regs & ((uint64_t)1 << r)))
			{ MCC_TRACE("br\n"); r = VT_CONST; }
#endif
		if (r < VT_CONST && (reg_classes[r] & rc))
			{ MCC_TRACE("br\n"); goto save_found; }
		r = p->r & VT_VALMASK;
#if MCC_CONFIG_OPTIMIZER
		if (r < VT_CONST && (ast_pinned_regs & ((uint64_t)1 << r)))
			{ MCC_TRACE("br\n"); r = VT_CONST; }
#endif
		if (r < VT_CONST && (reg_classes[r] & rc)) { MCC_TRACE("br\n");
		save_found:
			save_reg(r);
			return r;
		}
	}
	return -1;
}

static int get_temp_local_var(int size, int align, int *r2) { MCC_TRACE("enter\n");
	int i;
	struct temp_local_variable *temp_var;
	SValue *p;
	int r;
	unsigned used = 0;

	for (p = vstack; p <= vtop; p++) { MCC_TRACE("br\n");
		r = p->r & VT_VALMASK;
		if (r == VT_LOCAL || r == VT_LLOCAL) { MCC_TRACE("br\n");
			r = p->r2 - (VT_CONST + 1);
			if (r >= 0 && r < MAX_TEMP_LOCAL_VARIABLE_NUMBER)
				{ MCC_TRACE("br\n"); used |= 1 << r; }
		}
	}
	for (i = 0; i < nb_temp_local_vars; i++) { MCC_TRACE("br\n");
		temp_var = &arr_temp_local_vars[i];
		if (!(used & 1 << i) && temp_var->size >= size && temp_var->align >= align) { MCC_TRACE("br\n");
		ret_tmp:
			*r2 = (VT_CONST + 1) + i;
			return temp_var->location;
		}
	}
	loc = (loc - size) & -align;
	if (nb_temp_local_vars < MAX_TEMP_LOCAL_VARIABLE_NUMBER) { MCC_TRACE("br\n");
		temp_var = &arr_temp_local_vars[i];
		temp_var->location = loc;
		temp_var->size = size;
		temp_var->align = align;
		nb_temp_local_vars++;
		goto ret_tmp;
	}
	*r2 = VT_CONST;
	return loc;
}

static void move_reg(int r, int s, int t) { MCC_TRACE("enter\n");
	SValue sv;

	if (r != s) { MCC_TRACE("br\n");
		save_reg(r);
		sv.type.t = t;
		sv.type.ref = NULL;
		sv.r = s;
		sv.c.i = 0;
		load(r, &sv);
	}
}

ST_FUNC void gaddrof(void) { MCC_TRACE("enter\n");
#if MCC_CONFIG_OPTIMIZER
	ast_hook_gaddrof();
#endif
	vtop->r &= ~VT_LVAL;
	if ((vtop->r & VT_VALMASK) == VT_LLOCAL)
		{ MCC_TRACE("br\n"); vtop->r = (vtop->r & ~VT_VALMASK) | VT_LOCAL | VT_LVAL; }
}

#if MCC_CONFIG_DIAG_RT >= 2
static void gen_bounded_ptr_add(void) { MCC_TRACE("enter\n");
	int save = (vtop[-1].r & VT_VALMASK) == VT_LOCAL;
	if (save) { MCC_TRACE("br\n");
		vpushv(&vtop[-1]);
		vrott(3);
	}
	vpush_helper_func(TOK___bound_ptr_add);
	vrott(3);
	gfunc_call(2);
	vtop -= save;
	vpushi(0);
	vtop->r = REG_IRET | VT_BOUNDED;
	if (nocode_wanted)
		{ MCC_TRACE("br\n"); return; }
	vtop->c.i = (cur_text_section->reloc->data_offset - sizeof(ElfW_Rel));
}

static void gen_bounded_ptr_deref(void) { MCC_TRACE("enter\n");
	addr_t func;
	int size, align;
	ElfW_Rel *rel;
	Sym *sym;

	if (nocode_wanted)
		{ MCC_TRACE("br\n"); return; }

	size = type_size(&vtop->type, &align);
	switch (size) { MCC_TRACE("br\n");
	case 1:
		func = TOK___bound_ptr_indir1;
		break;
	case 2:
		func = TOK___bound_ptr_indir2;
		break;
	case 4:
		func = TOK___bound_ptr_indir4;
		break;
	case 8:
		func = TOK___bound_ptr_indir8;
		break;
	case 12:
		func = TOK___bound_ptr_indir12;
		break;
	case 16:
		func = TOK___bound_ptr_indir16;
		break;
	default:
		return;
	}
	sym = external_helper_sym(func);
	if (!sym->c)
		{ MCC_TRACE("br\n"); put_extern_sym(sym, NULL, 0, 0); }
	rel = (ElfW_Rel *)(cur_text_section->reloc->data + vtop->c.i);
	rel->r_info = ELFW(R_INFO)(sym->c, ELFW(R_TYPE)(rel->r_info));
}

static void gbound(void) { MCC_TRACE("enter\n");
	CType type1;

	vtop->r &= ~VT_MUSTBOUND;
	if (vtop->r & VT_LVAL) { MCC_TRACE("br\n");
		if (!(vtop->r & VT_BOUNDED)) { MCC_TRACE("br\n");
			type1 = vtop->type;
			vtop->type.t = VT_PTR;
			gaddrof();
			vpushi(0);
			gen_bounded_ptr_add();
			vtop->r |= VT_LVAL;
			vtop->type = type1;
		}
		gen_bounded_ptr_deref();
	}
}

ST_FUNC void gbound_args(int nb_args) { MCC_TRACE("enter\n");
	int i, v;
	SValue *sv;

	for (i = 1; i <= nb_args; ++i)
		{ MCC_TRACE("br\n"); if (vtop[1 - i].r & VT_MUSTBOUND) { MCC_TRACE("br\n");
			vrotb(i);
			gbound();
			vrott(i);
		} }

	sv = vtop - nb_args;
	if (sv->r & VT_SYM) { MCC_TRACE("br\n");
		v = sv->sym->v;
		if (v == TOK_setjmp || v == TOK__setjmp
#ifndef MCC_TARGET_PE
				|| v == TOK_sigsetjmp || v == TOK___sigsetjmp
#endif
		) { MCC_TRACE("br\n");
			vpush_helper_func(TOK___bound_setjmp);
			vpushv(sv + 1);
			gfunc_call(1);
			func_bound_add_epilog = 1;
		}
		if (v == TOK_alloca)
			{ MCC_TRACE("br\n"); func_bound_add_epilog = 1; }
#if MCC_TARGETOS_NetBSD
		if (v == TOK_longjmp)
			{ MCC_TRACE("br\n"); sv->sym->asm_label = TOK___bound_longjmp; }
#endif
	}
}

static void add_local_bounds(Sym *s, Sym *e) { MCC_TRACE("enter\n");
	for (; s != e; s = s->prev) { MCC_TRACE("br\n");
		if (!s->v || (s->r & VT_VALMASK) != VT_LOCAL)
			{ MCC_TRACE("br\n"); continue; }
		if ((s->type.t & VT_ARRAY) || (s->type.t & VT_BTYPE) == VT_STRUCT || s->a.addrtaken) { MCC_TRACE("br\n");
			int align, size = type_size(&s->type, &align);
			addr_t *bounds_ptr = section_ptr_add(lbounds_section,
																					 2 * sizeof(addr_t));
			bounds_ptr[0] = s->c;
			bounds_ptr[1] = size;
		}
	}
}
#endif

static void add_asan_locals(Sym *s, Sym *e) { MCC_TRACE("enter\n");
	if (!asan_lstack_section)
		{ MCC_TRACE("br\n"); return; }
	for (; s != e; s = s->prev) { MCC_TRACE("br\n");
		if (!s->v || (s->r & VT_VALMASK) != VT_LOCAL)
			{ MCC_TRACE("br\n"); continue; }
		if ((s->type.t & VT_ARRAY) || (s->type.t & VT_BTYPE) == VT_STRUCT
			|| s->a.addrtaken) { MCC_TRACE("br\n");
			int align, size = type_size(&s->type, &align);
			addr_t *p = section_ptr_add(asan_lstack_section, 2 * sizeof(addr_t));
			p[0] = s->c;
			p[1] = size;
		}
	}
}

ST_FUNC int gen_asan_stack_epilog_head(addr_t off, Sym **psym) { MCC_TRACE("enter\n");
	addr_t *p;
	if (!asan_lstack_section || off == asan_lstack_section->data_offset)
		{ MCC_TRACE("br\n"); return 0; }
	p = section_ptr_add(asan_lstack_section, sizeof(addr_t));
	*p = 0;
	*psym = get_sym_ref(&char_pointer_type, asan_lstack_section, off, MCC_PTR_SIZE);
	return 1;
}

static void mcc_debug_end_scope(Sym *b, int bounds) { MCC_TRACE("enter\n");
#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check && bounds)
		{ MCC_TRACE("br\n"); add_local_bounds(local_stack, b); }
#endif
	if (mcc_state->do_asan_shadow && bounds)
		{ MCC_TRACE("br\n"); add_asan_locals(local_stack, b); }
	mcc_add_debug_info(mcc_state, local_stack, b);
}

static void incr_offset(int offset) { MCC_TRACE("enter\n");
	int t = vtop->type.t;
	gaddrof();
	vtop->type.t = VT_PTRDIFF_T;
	vpushs(offset);
	gen_op('+');
	vtop->r |= VT_LVAL;
	vtop->type.t = t;
}

static void incr_bf_adr(int o) { MCC_TRACE("enter\n");
	vtop->type.t = VT_BYTE | VT_UNSIGNED;
	incr_offset(o);
}

static void load_packed_bf(CType *type, int bit_pos, int bit_size) { MCC_TRACE("enter\n");
	int n, o, bits;
	save_reg_upstack(vtop->r, 1);
	vpush64(type->t & VT_BTYPE, 0);
	bits = 0, o = bit_pos >> 3, bit_pos &= 7;
	do { MCC_TRACE("br\n");
		vswap();
		incr_bf_adr(o);
		vdup();
		n = 8 - bit_pos;
		if (n > bit_size)
			{ MCC_TRACE("br\n"); n = bit_size; }
		if (bit_pos)
			{ MCC_TRACE("br\n"); vpushi(bit_pos), gen_op(TOK_SHR), bit_pos = 0; }
		if (n < 8)
			{ MCC_TRACE("br\n"); vpushi((1 << n) - 1), gen_op('&'); }
		gen_cast(type);
		if (bits)
			{ MCC_TRACE("br\n"); vpushi(bits), gen_op(TOK_SHL); }
		vrotb(3);
		gen_op('|');
		bits += n, bit_size -= n, o = 1;
	} while (bit_size);
	vswap(), vpop();
	if (!(type->t & VT_UNSIGNED)) { MCC_TRACE("br\n");
		n = ((type->t & VT_BTYPE) == VT_LLONG ? 64 : 32) - bits;
		vpushi(n), gen_op(TOK_SHL);
		vpushi(n), gen_op(TOK_SAR);
	}
}

static void store_packed_bf(int bit_pos, int bit_size) { MCC_TRACE("enter\n");
	int bits, n, o, m, c;
	c = (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
	vswap();
	save_reg_upstack(vtop->r, 1);
	bits = 0, o = bit_pos >> 3, bit_pos &= 7;
	do { MCC_TRACE("br\n");
		incr_bf_adr(o);
		vswap();
		c ? vdup() : gv_dup();
		vrott(3);
		if (bits)
			{ MCC_TRACE("br\n"); vpushi(bits), gen_op(TOK_SHR); }
		if (bit_pos)
			{ MCC_TRACE("br\n"); vpushi(bit_pos), gen_op(TOK_SHL); }
		n = 8 - bit_pos;
		if (n > bit_size)
			{ MCC_TRACE("br\n"); n = bit_size; }
		if (n < 8) { MCC_TRACE("br\n");
			m = ((1 << n) - 1) << bit_pos;
			vpushi(m), gen_op('&');
			vpushv(vtop - 1);
			vpushi(m & 0x80 ? ~m & 0x7f : ~m);
			gen_op('&');
			gen_op('|');
		}
		vdup(), vtop[-1] = vtop[-2];
		vstore(), vpop();
		bits += n, bit_size -= n, bit_pos = 0, o = 1;
	} while (bit_size);
	vpop(), vpop();
}

static int adjust_bf(SValue *sv, int bit_pos, int bit_size) { MCC_TRACE("enter\n");
	int t;
	if (0 == sv->type.ref)
		{ MCC_TRACE("br\n"); return 0; }
	t = sv->type.ref->auxtype;
	if (t != -1 && t != VT_STRUCT) { MCC_TRACE("br\n");
		sv->type.t = (sv->type.t & ~(VT_BTYPE | VT_LONG)) | t;
		sv->r |= VT_LVAL;
	}
	return t;
}

ST_FUNC int gv(int rc) { MCC_TRACE("enter\n");
	int r, r2, r_ok, r2_ok, rc2, bt;
	int bit_pos, bit_size, size, align;

	seqp_record_sv(vtop, SEQP_READ);

	if (vtop->type.t & VT_BITFIELD) { MCC_TRACE("br\n");
		CType type;

		bit_pos = BIT_POS(vtop->type.t);
		bit_size = BIT_SIZE(vtop->type.t);
		vtop->type.t &= ~VT_STRUCT_MASK;

		type.ref = NULL;
		type.t = vtop->type.t & VT_UNSIGNED;
		if ((vtop->type.t & VT_BTYPE) == VT_BOOL)
			{ MCC_TRACE("br\n"); type.t |= VT_UNSIGNED; }

		r = adjust_bf(vtop, bit_pos, bit_size);

		if ((vtop->type.t & VT_BTYPE) == VT_LLONG)
			{ MCC_TRACE("br\n"); type.t |= VT_LLONG; }
		else
			{ MCC_TRACE("br\n"); type.t |= VT_INT; }

		if (r == VT_STRUCT) { MCC_TRACE("br\n");
			load_packed_bf(&type, bit_pos, bit_size);
		} else { MCC_TRACE("br\n");
			int bits = (type.t & VT_BTYPE) == VT_LLONG ? 64 : 32;
			gen_cast(&type);
			vpushi(bits - (bit_pos + bit_size));
			gen_op(TOK_SHL);
			vpushi(bits - bit_size);
			gen_op(TOK_SAR);
		}
		r = gv(rc);
	} else { MCC_TRACE("br\n");
		if (is_float(vtop->type.t) &&
				(vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) { MCC_TRACE("br\n");
			init_params p = {.sec = rodata_section};
			unsigned long offset;
			CType ltype = vtop->type;
			ltype.t &= ~VT_TLS;
			size = type_size(&vtop->type, &align);
			if (NODATA_WANTED)
				{ MCC_TRACE("br\n"); size = 0, align = 1; }
#if MCC_CONFIG_OPTIMIZER
			int fc = ast_fconst_reuse();
			if (fc) { MCC_TRACE("br\n");
				vpop();
				ast_fconst_push_ref(&ltype, fc);
			} else
#endif
			{ MCC_TRACE("br\n");
				offset = section_add(p.sec, size, align);
				vpush_ref(&ltype, p.sec, offset, size);
#if MCC_CONFIG_OPTIMIZER
				ast_fconst_record(vtop->sym->c);
#endif
				vswap();
				init_putv(&p, &vtop->type, offset);
				vtop->r |= VT_LVAL;
			}
		}
#if MCC_CONFIG_DIAG_RT >= 2
		if (vtop->r & VT_MUSTBOUND)
			{ MCC_TRACE("br\n"); gbound(); }
#endif

		bt = vtop->type.t & VT_BTYPE;
		if (bt == VT_VOID || bt == VT_STRUCT)
			{ MCC_TRACE("br\n"); return vtop->r; }

#ifdef MCC_TARGET_RISCV64
		if (bt == VT_LDOUBLE && rc == MCC_RC_FLOAT)
			{ MCC_TRACE("br\n"); rc = MCC_RC_INT; }
#endif
		rc2 = RC2_TYPE(bt, rc);

		r = vtop->r & VT_VALMASK;
		r_ok = !(vtop->r & VT_LVAL) && (r < VT_CONST) && (reg_classes[r] & rc);
		r2_ok = !rc2 || ((vtop->r2 < VT_CONST) && (reg_classes[vtop->r2] & rc2));

		if (!r_ok || !r2_ok) { MCC_TRACE("br\n");
			if (!r_ok) { MCC_TRACE("br\n");
				if (1 && r < VT_CONST && (reg_classes[r] & rc) && !rc2)
					{ MCC_TRACE("br\n"); save_reg_upstack(r, 1); }
				else
					{ MCC_TRACE("br\n"); r = get_reg(rc); }
			}

			if (rc2) { MCC_TRACE("br\n");
				int load_type = (bt == VT_QFLOAT) ? VT_DOUBLE : VT_PTRDIFF_T;
				int original_type = vtop->type.t;

				if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) { MCC_TRACE("br\n");
					unsigned long long ll = vtop->c.i;
					vtop->c.i = ll;
					load(r, vtop);
					vtop->r = r;
					vpushi(ll >> 32);
				} else if (vtop->r & VT_LVAL) { MCC_TRACE("br\n");
					save_reg_upstack(vtop->r, 1);
					vtop->type.t = load_type;
					load(r, vtop);
					vdup();
					vtop[-1].r = r;
					incr_offset(MCC_PTR_SIZE);
				} else { MCC_TRACE("br\n");
					if (!r_ok)
						{ MCC_TRACE("br\n"); load(r, vtop); }
					if (r2_ok && vtop->r2 < VT_CONST)
						{ MCC_TRACE("br\n"); goto done; }
					vdup();
					vtop[-1].r = r;
					vtop->r = vtop[-1].r2;
				}
				r2 = get_reg(rc2);
				load(r2, vtop);
				vpop();
				vtop->r2 = r2;
			done:
				vtop->type.t = original_type;
			} else { MCC_TRACE("br\n");
				if (vtop->r == VT_CMP)
					{ MCC_TRACE("br\n"); vset_VT_JMP(); }
				load(r, vtop);
			}
		}
		vtop->r = r;
	}
	return r;
}

ST_FUNC void gv2(int rc1, int rc2) { MCC_TRACE("enter\n");
	if (vtop->r != VT_CMP && rc1 <= rc2) { MCC_TRACE("br\n");
		vswap();
		gv(rc1);
		vswap();
		gv(rc2);
		if ((vtop[-1].r & VT_VALMASK) >= VT_CONST) { MCC_TRACE("br\n");
			vswap();
			gv(rc1);
			vswap();
		}
	} else { MCC_TRACE("br\n");
		gv(rc2);
		vswap();
		gv(rc1);
		vswap();
		if ((vtop[0].r & VT_VALMASK) >= VT_CONST) { MCC_TRACE("br\n");
			gv(rc2);
		}
	}
}

#if MCC_PTR_SIZE == 4
ST_FUNC void lexpand(void) { MCC_TRACE("enter\n");
	int u, v;
	u = vtop->type.t & (VT_DEFSIGN | VT_UNSIGNED);
	v = vtop->r & (VT_VALMASK | VT_LVAL);
	if (v == VT_CONST) { MCC_TRACE("br\n");
		vdup();
		vtop[0].c.i >>= 32;
	} else if (v == (VT_LVAL | VT_CONST) || v == (VT_LVAL | VT_LOCAL)) { MCC_TRACE("br\n");
		vdup();
		vtop[0].c.i += 4;
	} else { MCC_TRACE("br\n");
		gv(MCC_RC_INT);
		vdup();
		vtop[0].r = vtop[-1].r2;
		vtop[0].r2 = vtop[-1].r2 = VT_CONST;
	}
	vtop[0].type.t = vtop[-1].type.t = VT_INT | u;
}
#endif

#if MCC_PTR_SIZE == 4
static void lbuild(int t) { MCC_TRACE("enter\n");
	gv2(MCC_RC_INT, MCC_RC_INT);
	vtop[-1].r2 = vtop[0].r;
	vtop[-1].type.t = t;
	vpop();
}
#endif

static void gv_dup(void) { MCC_TRACE("enter\n");
	int t, rc, r;

	t = vtop->type.t;
#if MCC_PTR_SIZE == 4
	if ((t & VT_BTYPE) == VT_LLONG) { MCC_TRACE("br\n");
		if (t & VT_BITFIELD) { MCC_TRACE("br\n");
			gv(MCC_RC_INT);
			t = vtop->type.t;
		}
		lexpand();
		gv_dup();
		vswap();
		vrotb(3);
		gv_dup();
		vrotb(4);
		lbuild(t);
		vrotb(3);
		vrotb(3);
		vswap();
		lbuild(t);
		vswap();
		return;
	}
#endif
	rc = MCC_RC_TYPE(t);
	gv(rc);
	r = get_reg(rc);
	vdup();
	load(r, vtop);
	vtop->r = r;
}

#if MCC_PTR_SIZE == 4
static void gen_opl(int op) { MCC_TRACE("enter\n");
	int t, a, b, op1, c, i;
	int func;
	unsigned short reg_iret = REG_IRET;
	unsigned short reg_lret = REG_IRE2;
	SValue tmp;

	switch (op) { MCC_TRACE("br\n");
	case '/':
	case TOK_PDIV:
		func = TOK___divdi3;
		goto gen_func;
	case TOK_UDIV:
		func = TOK___udivdi3;
		goto gen_func;
	case '%':
		func = TOK___moddi3;
		goto gen_mod_func;
	case TOK_UMOD:
		func = TOK___umoddi3;
	gen_mod_func:
#ifdef MCC_ARM_EABI
		reg_iret = MCC_TREG_R2;
		reg_lret = MCC_TREG_R3;
#endif
	gen_func:
		vpush_helper_func(func);
		vrott(3);
		gfunc_call(2);
		vpushi(0);
		vtop->r = reg_iret;
		vtop->r2 = reg_lret;
		break;
	case '^':
	case '&':
	case '|':
	case '*':
	case '+':
	case '-':
		t = vtop->type.t;
		vswap();
		lexpand();
		vrotb(3);
		lexpand();
		tmp = vtop[0];
		vtop[0] = vtop[-3];
		vtop[-3] = tmp;
		tmp = vtop[-2];
		vtop[-2] = vtop[-3];
		vtop[-3] = tmp;
		vswap();
		if (op == '*') { MCC_TRACE("br\n");
			vpushv(vtop - 1);
			vpushv(vtop - 1);
			gen_op(TOK_UMULL);
			lexpand();
			for (i = 0; i < 4; i++)
				{ MCC_TRACE("br\n"); vrotb(6); }
			tmp = vtop[0];
			vtop[0] = vtop[-2];
			vtop[-2] = tmp;
			gen_op('*');
			vrotb(3);
			vrotb(3);
			gen_op('*');
			gen_op('+');
			gen_op('+');
		} else if (op == '+' || op == '-') { MCC_TRACE("br\n");
			if (op == '+')
				{ MCC_TRACE("br\n"); op1 = TOK_ADDC1; }
			else
				{ MCC_TRACE("br\n"); op1 = TOK_SUBC1; }
			gen_op(op1);
			vrotb(3);
			vrotb(3);
			gen_op(op1 + 1);
		} else { MCC_TRACE("br\n");
			gen_op(op);
			vrotb(3);
			vrotb(3);
			gen_op(op);
		}
		lbuild(t);
		break;
	case TOK_SAR:
	case TOK_SHR:
	case TOK_SHL:
		if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
			t = vtop[-1].type.t;
			vswap();
			lexpand();
			vrotb(3);
			c = (int)vtop->c.i;
			vpop();
			if (op != TOK_SHL)
				{ MCC_TRACE("br\n"); vswap(); }
			if (c >= 32) { MCC_TRACE("br\n");
				vpop();
				if (c > 32) { MCC_TRACE("br\n");
					vpushi(c - 32);
					gen_op(op);
				}
				if (op != TOK_SAR) { MCC_TRACE("br\n");
					vpushi(0);
				} else { MCC_TRACE("br\n");
					gv_dup();
					vpushi(31);
					gen_op(TOK_SAR);
				}
				vswap();
			} else { MCC_TRACE("br\n");
				vswap();
				gv_dup();
				vpushi(c);
				gen_op(op);
				vswap();
				vpushi(32 - c);
				if (op == TOK_SHL)
					{ MCC_TRACE("br\n"); gen_op(TOK_SHR); }
				else
					{ MCC_TRACE("br\n"); gen_op(TOK_SHL); }
				vrotb(3);
				vpushi(c);
				if (op == TOK_SHL)
					{ MCC_TRACE("br\n"); gen_op(TOK_SHL); }
				else
					{ MCC_TRACE("br\n"); gen_op(TOK_SHR); }
				gen_op('|');
			}
			if (op != TOK_SHL)
				{ MCC_TRACE("br\n"); vswap(); }
			lbuild(t);
		} else { MCC_TRACE("br\n");
			switch (op) { MCC_TRACE("br\n");
			case TOK_SAR:
				func = TOK___ashrdi3;
				goto gen_func;
			case TOK_SHR:
				func = TOK___lshrdi3;
				goto gen_func;
			case TOK_SHL:
				func = TOK___ashldi3;
				goto gen_func;
			}
		}
		break;
	default:
		t = vtop->type.t;
		vswap();
		lexpand();
		vrotb(3);
		lexpand();
		tmp = vtop[-1];
		vtop[-1] = vtop[-2];
		vtop[-2] = tmp;
		if (!cur_switch || cur_switch->bsym) { MCC_TRACE("br\n");
			save_regs(4);
		}
		op1 = op;
		if (op1 == TOK_LT)
			{ MCC_TRACE("br\n"); op1 = TOK_LE; }
		else if (op1 == TOK_GT)
			{ MCC_TRACE("br\n"); op1 = TOK_GE; }
		else if (op1 == TOK_ULT)
			{ MCC_TRACE("br\n"); op1 = TOK_ULE; }
		else if (op1 == TOK_UGT)
			{ MCC_TRACE("br\n"); op1 = TOK_UGE; }
		a = 0;
		b = 0;
		gen_op(op1);
		if (op == TOK_NE) { MCC_TRACE("br\n");
			b = gvtst(0, 0);
		} else { MCC_TRACE("br\n");
			a = gvtst(1, 0);
			if (op != TOK_EQ) { MCC_TRACE("br\n");
				vpushi(0);
				vset_VT_CMP(TOK_NE);
				b = gvtst(0, 0);
			}
		}
		op1 = op;
		if (op1 == TOK_LT)
			{ MCC_TRACE("br\n"); op1 = TOK_ULT; }
		else if (op1 == TOK_LE)
			{ MCC_TRACE("br\n"); op1 = TOK_ULE; }
		else if (op1 == TOK_GT)
			{ MCC_TRACE("br\n"); op1 = TOK_UGT; }
		else if (op1 == TOK_GE)
			{ MCC_TRACE("br\n"); op1 = TOK_UGE; }
		gen_op(op1);
		gvtst_set(1, a);
		gvtst_set(0, b);
		break;
	}
}
#endif

static uint64_t value64(uint64_t l1, int t) { MCC_TRACE("enter\n");
	if ((t & VT_BTYPE) == VT_LLONG || (MCC_PTR_SIZE == 8 && (t & VT_BTYPE) == VT_PTR))
		{ MCC_TRACE("br\n"); return l1; }
	else if (t & VT_UNSIGNED)
		{ MCC_TRACE("br\n"); return (uint32_t)l1; }
	else
		{ MCC_TRACE("br\n"); return (uint32_t)l1 | -(l1 & 0x80000000); }
}

static uint64_t gen_opic_sdiv(uint64_t a, uint64_t b) { MCC_TRACE("enter\n");
	uint64_t x = (a >> 63 ? -a : a) / (b >> 63 ? -b : b);
	return (a ^ b) >> 63 ? -x : x;
}

static int gen_opic_lt(uint64_t a, uint64_t b) { MCC_TRACE("enter\n");
	return (a ^ (uint64_t)1 << 63) < (b ^ (uint64_t)1 << 63);
}

static int pp_signed_ovf(int op, uint64_t l1, uint64_t l2) { MCC_TRACE("enter\n");
	int64_t a = (int64_t)l1, b = (int64_t)l2, r;
	switch (op) { MCC_TRACE("br\n");
	case '+':
		r = (int64_t)((uint64_t)a + (uint64_t)b);
		return ((a ^ r) & (b ^ r)) < 0;
	case '-':
		r = (int64_t)((uint64_t)a - (uint64_t)b);
		return ((a ^ b) & (a ^ r)) < 0;
	case '*':
		if (a == 0 || b == 0)
			{ MCC_TRACE("br\n"); return 0; }
		r = (int64_t)((uint64_t)a * (uint64_t)b);
		return r / a != b || (a == (int64_t)0x8000000000000000ULL && b == -1) || (b == (int64_t)0x8000000000000000ULL && a == -1);
	}
	return 0;
}

static void gen_opic(int op) { MCC_TRACE("enter\n");
	SValue *v1 = vtop - 1;
	SValue *v2 = vtop;
	int t1 = v1->type.t & VT_BTYPE;
	int t2 = v2->type.t & VT_BTYPE;
	int c1 = (v1->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
	int c2 = (v2->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
	uint64_t l1 = c1 ? value64(v1->c.i, v1->type.t) : 0;
	uint64_t l2 = c2 ? value64(v2->c.i, v2->type.t) : 0;
	int shm = (t1 == VT_LLONG) ? 63 : 31;
	int r;

	if (c1 && c2) { MCC_TRACE("br\n");
		if (pp_expr && t1 == VT_LLONG && !(v1->type.t & VT_UNSIGNED) && (op == '+' || op == '-' || op == '*') &&
				pp_signed_ovf(op, l1, l2)) { MCC_TRACE("br\n");
			int pp_save = pp_expr;
			pp_expr = 0;
			mcc_warning("integer overflow in preprocessor expression");
			pp_expr = pp_save;
		} else if (!pp_expr && CONST_WANTED && !NOEVAL_WANTED && !(v1->type.t & VT_UNSIGNED) && (op == '+' || op == '-' || op == '*') && (t1 == VT_INT || t1 == VT_LLONG)) { MCC_TRACE("br\n");
			int ovf;
			if (t1 == VT_LLONG) { MCC_TRACE("br\n");
				ovf = pp_signed_ovf(op, l1, l2);
			} else { MCC_TRACE("br\n");
				int64_t a = (int32_t)l1, b = (int32_t)l2, r;
				switch (op) { MCC_TRACE("br\n");
				case '+':
					r = a + b;
					break;
				case '-':
					r = a - b;
					break;
				default:
					r = a * b;
					break;
				}
				ovf = r < (-2147483647LL - 1) || r > 2147483647LL;
			}
			if (ovf)
				{ MCC_TRACE("br\n"); mcc_pedantic("integer overflow in constant expression"); }
		}
		switch (op) { MCC_TRACE("br\n");
		case '+':
			l1 += l2;
			break;
		case '-':
			l1 -= l2;
			break;
		case '&':
			l1 &= l2;
			break;
		case '^':
			l1 ^= l2;
			break;
		case '|':
			l1 |= l2;
			break;
		case '*':
			l1 *= l2;
			break;

		case TOK_PDIV:
		case '/':
		case '%':
		case TOK_UDIV:
		case TOK_UMOD:
			if (l2 == 0) { MCC_TRACE("br\n");
				if (CONST_WANTED && !NOEVAL_WANTED)
					{ MCC_TRACE("br\n"); mcc_error("division by zero in constant"); }
				goto general_case;
			}
			switch (op) { MCC_TRACE("br\n");
			default:
				l1 = gen_opic_sdiv(l1, l2);
				break;
			case '%':
				l1 = l1 - l2 * gen_opic_sdiv(l1, l2);
				break;
			case TOK_UDIV:
				l1 = l1 / l2;
				break;
			case TOK_UMOD:
				l1 = l1 % l2;
				break;
			}
			break;
		case TOK_SHL:
			l1 <<= (l2 & shm);
			break;
		case TOK_SHR:
			l1 >>= (l2 & shm);
			break;
		case TOK_SAR:
			l1 = (l1 >> 63) ? ~(~l1 >> (l2 & shm)) : l1 >> (l2 & shm);
			break;
		case TOK_ULT:
			l1 = l1 < l2;
			break;
		case TOK_UGE:
			l1 = l1 >= l2;
			break;
		case TOK_EQ:
			l1 = l1 == l2;
			break;
		case TOK_NE:
			l1 = l1 != l2;
			break;
		case TOK_ULE:
			l1 = l1 <= l2;
			break;
		case TOK_UGT:
			l1 = l1 > l2;
			break;
		case TOK_LT:
			l1 = gen_opic_lt(l1, l2);
			break;
		case TOK_GE:
			l1 = !gen_opic_lt(l1, l2);
			break;
		case TOK_LE:
			l1 = !gen_opic_lt(l2, l1);
			break;
		case TOK_GT:
			l1 = gen_opic_lt(l2, l1);
			break;
		case TOK_LAND:
			l1 = l1 && l2;
			break;
		case TOK_LOR:
			l1 = l1 || l2;
			break;
		default:
			goto general_case;
		}
		v1->c.i = value64(l1, v1->type.t);
		v1->r |= v2->r & VT_NONCONST;
		vtop--;
	} else { MCC_TRACE("br\n");
		if (c1 && (op == '+' || op == '&' || op == '^' ||
							 op == '|' || op == '*' || op == TOK_EQ || op == TOK_NE)) { MCC_TRACE("br\n");
			vswap();
			c2 = c1;
			l2 = l1;
		}
		if (c1 && ((l1 == 0 &&
								(op == TOK_SHL || op == TOK_SHR || op == TOK_SAR)) ||
							 (l1 == -1 && op == TOK_SAR))) { MCC_TRACE("br\n");
			vpop();
		} else if (c2 && ((l2 == 0 && (op == '&' || op == '*')) ||
											(op == '|' &&
											 (l2 == -1 || (l2 == 0xFFFFFFFF && t2 != VT_LLONG))) ||
											(l2 == 1 && (op == '%' || op == TOK_UMOD)))) { MCC_TRACE("br\n");
			if (l2 == 1)
				{ MCC_TRACE("br\n"); vtop->c.i = 0; }
			vswap();
			vtop--;
		} else if (c2 && (((op == '*' || op == '/' || op == TOK_UDIV ||
												op == TOK_PDIV) &&
											 l2 == 1) ||
											((op == '+' || op == '-' || op == '|' || op == '^' ||
												op == TOK_SHL || op == TOK_SHR || op == TOK_SAR) &&
											 l2 == 0) ||
											(op == '&' &&
											 (l2 == -1 || (l2 == 0xFFFFFFFF && t2 != VT_LLONG))))) { MCC_TRACE("br\n");
			vtop--;
		} else if (c2 && (op == '*' || op == TOK_PDIV || op == TOK_UDIV || op == TOK_UMOD)) { MCC_TRACE("br\n");
			if (l2 > 0 && (l2 & (l2 - 1)) == 0) { MCC_TRACE("br\n");
				int n = -1;
				if (op == TOK_UMOD) { MCC_TRACE("br\n");
					vtop->c.i = l2 - 1;
					op = '&';
					goto general_case;
				}
				while (l2) { MCC_TRACE("br\n");
					l2 >>= 1;
					n++;
				}
				vtop->c.i = n;
				if (op == '*')
					{ MCC_TRACE("br\n"); op = TOK_SHL; }
				else if (op == TOK_PDIV)
					{ MCC_TRACE("br\n"); op = TOK_SAR; }
				else
					{ MCC_TRACE("br\n"); op = TOK_SHR; }
			}
			goto general_case;
		} else if (c2 && (op == '+' || op == '-') &&
							 (r = vtop[-1].r & (VT_VALMASK | VT_LVAL | VT_SYM),
								r == (VT_CONST | VT_SYM) || r == VT_LOCAL)) { MCC_TRACE("br\n");
			if (op == '-')
				{ MCC_TRACE("br\n"); l2 = -l2; }
			l2 += vtop[-1].c.i;
			if ((int)l2 != l2)
				{ MCC_TRACE("br\n"); goto general_case; }
			vtop--;
			vtop->c.i = l2;
		} else { MCC_TRACE("br\n");
		general_case:
			if (t1 == VT_LLONG || t2 == VT_LLONG ||
					(MCC_PTR_SIZE == 8 && (t1 == VT_PTR || t2 == VT_PTR)))
				{ MCC_TRACE("br\n"); gen_opl(op); }
			else
				{ MCC_TRACE("br\n"); gen_opi(op); }
		}
		if (vtop->r == VT_CONST)
			{ MCC_TRACE("br\n"); vtop->r |= VT_NONCONST; }
	}
}

#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_I386 || defined MCC_TARGET_ARM64
#define gen_negf gen_opf
#elif defined MCC_TARGET_ARM
void gen_negf(int op) { MCC_TRACE("enter\n");
	vpushi(0), vswap(), gen_op('-');
}
#else
void gen_negf(int op) { MCC_TRACE("enter\n");
	int align, size, bt;
	size = type_size(&vtop->type, &align);
	bt = vtop->type.t & VT_BTYPE;
#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_I386
	if (bt == VT_LDOUBLE)
		{ MCC_TRACE("br\n"); size = 10; }
#endif
	if (nocode_wanted)
		{ MCC_TRACE("br\n"); goto gv2; }
	save_reg(gv(MCC_RC_TYPE(bt)));
	vdup();
	incr_bf_adr(size - 1);
	vdup();
	vpushi(0x80);
	gen_op('^');
	vstore();
	vpop();
gv2:
	gv(MCC_RC_TYPE(bt));
}
#endif

static void gen_opif(int op) { MCC_TRACE("enter\n");
	int c1, c2, i, bt;
	SValue *v1, *v2;
	HOST_VOLATILE_LDOUBLE long double f1, f2;

	v1 = vtop - 1;
	v2 = vtop;
	if (op == TOK_NEG)
		{ MCC_TRACE("br\n"); v1 = v2; }
	bt = v1->type.t & VT_BTYPE;

	c1 = (v1->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
	c2 = (v2->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
	if (CONST_WANTED)
		{ MCC_TRACE("br\n"); ice_float_op = 1; }
	if (c1 && c2 && !CONST_WANTED && stdc_fenv_access(mcc_state) && (op == '+' || op == '-' || op == '*' || op == '/'))
		{ MCC_TRACE("br\n"); goto general_case; }
	if (c1 && c2) { MCC_TRACE("br\n");
		if (bt == VT_FLOAT) { MCC_TRACE("br\n");
			f1 = v1->c.f;
			f2 = v2->c.f;
		} else if (bt == VT_DOUBLE) { MCC_TRACE("br\n");
			f1 = v1->c.d;
			f2 = v2->c.d;
		} else { MCC_TRACE("br\n");
			f1 = v1->c.ld;
			f2 = v2->c.ld;
		}
		if (!(ieee_finite(f1) || !ieee_finite(f2)) && !CONST_WANTED)
			{ MCC_TRACE("br\n"); goto general_case; }
		switch (op) { MCC_TRACE("br\n");
		case '+':
			f1 += f2;
			break;
		case '-':
			f1 -= f2;
			break;
		case '*':
			f1 *= f2;
			break;
		case '/':
			if (f2 == 0.0) { MCC_TRACE("br\n");
				union {
					float f;
					unsigned u;
				} x1, x2, y;
				if (!CONST_WANTED)
					{ MCC_TRACE("br\n"); goto general_case; }
				x1.f = f1, x2.f = f2;
				if (f1 == 0.0)
					{ MCC_TRACE("br\n"); y.u = 0x7fc00000; }
				else
					{ MCC_TRACE("br\n"); y.u = 0x7f800000; }
				y.u |= (x1.u ^ x2.u) & 0x80000000;
				f1 = y.f;
				break;
			}
			f1 /= f2;
			break;
		case TOK_NEG:
			f1 = -f1;
			goto unary_result;
		case TOK_EQ:
			i = f1 == f2;
		make_int:
			vtop -= 2;
			vpushi(i);
			return;
		case TOK_NE:
			i = f1 != f2;
			goto make_int;
		case TOK_LT:
			i = f1 < f2;
			goto make_int;
		case TOK_GE:
			i = f1 >= f2;
			goto make_int;
		case TOK_LE:
			i = f1 <= f2;
			goto make_int;
		case TOK_GT:
			i = f1 > f2;
			goto make_int;
		default:
			goto general_case;
		}
		vtop--;
	unary_result:
		if (bt == VT_FLOAT) { MCC_TRACE("br\n");
			v1->c.f = f1;
		} else if (bt == VT_DOUBLE) { MCC_TRACE("br\n");
			v1->c.d = f1;
		} else { MCC_TRACE("br\n");
			v1->c.ld = f1;
		}
	} else { MCC_TRACE("br\n");
	general_case:
		if (op == TOK_NEG) { MCC_TRACE("br\n");
			gen_negf(op);
		} else { MCC_TRACE("br\n");
			gen_opf(op);
		}
	}
}

static void type_to_str(char *buf, int buf_size,
												CType *type, const char *varstr) { MCC_TRACE("enter\n");
	int bt, v, t;
	Sym *s, *sa;
	char buf1[256];
	const char *tstr;

	t = type->t;
	bt = t & VT_BTYPE;
	buf[0] = '\0';

	if (t & VT_EXTERN)
		{ MCC_TRACE("br\n"); pstrcat(buf, buf_size, "extern "); }
	if (t & VT_STATIC)
		{ MCC_TRACE("br\n"); pstrcat(buf, buf_size, "static "); }
	if (t & VT_TYPEDEF)
		{ MCC_TRACE("br\n"); pstrcat(buf, buf_size, "typedef "); }
	if (t & VT_INLINE)
		{ MCC_TRACE("br\n"); pstrcat(buf, buf_size, "inline "); }
	if (bt != VT_PTR) { MCC_TRACE("br\n");
		if (t & VT_VOLATILE)
			{ MCC_TRACE("br\n"); pstrcat(buf, buf_size, "volatile "); }
		if (t & VT_CONSTANT)
			{ MCC_TRACE("br\n"); pstrcat(buf, buf_size, "const "); }
	}
	if (((t & VT_DEFSIGN) && bt == VT_BYTE) || ((t & VT_UNSIGNED) && (bt == VT_SHORT || bt == VT_INT || bt == VT_LLONG) && !IS_ENUM(t)))
		{ MCC_TRACE("br\n"); pstrcat(buf, buf_size, (t & VT_UNSIGNED) ? "unsigned " : "signed "); }

	buf_size -= strlen(buf);
	buf += strlen(buf);

	switch (bt) { MCC_TRACE("br\n");
	case VT_VOID:
		tstr = "void";
		goto add_tstr;
	case VT_BOOL:
		tstr = "_Bool";
		goto add_tstr;
	case VT_BYTE:
		tstr = "char";
		goto add_tstr;
	case VT_SHORT:
		tstr = "short";
		goto add_tstr;
	case VT_INT:
		tstr = "int";
		goto maybe_long;
	case VT_LLONG:
		tstr = "long long";
	maybe_long:
		if (t & VT_LONG)
			{ MCC_TRACE("br\n"); tstr = "long"; }
		if (!IS_ENUM(t))
			{ MCC_TRACE("br\n"); goto add_tstr; }
		tstr = "enum ";
		goto tstruct;
	case VT_FLOAT:
		tstr = "float";
		goto add_tstr;
	case VT_DOUBLE:
		tstr = "double";
		if (!(t & VT_LONG))
			{ MCC_TRACE("br\n"); goto add_tstr; }
		FALLTHROUGH;
	case VT_LDOUBLE:
		tstr = "long double";
	add_tstr:
		pstrcat(buf, buf_size, tstr);
		break;
	case VT_STRUCT:
		tstr = "struct ";
		if (IS_UNION(t))
			{ MCC_TRACE("br\n"); tstr = "union "; }
	tstruct:
		pstrcat(buf, buf_size, tstr);
		v = type->ref->v & ~SYM_STRUCT;
		if (v >= SYM_FIRST_ANOM)
			{ MCC_TRACE("br\n"); pstrcat(buf, buf_size, "<anonymous>"); }
		else
			{ MCC_TRACE("br\n"); pstrcat(buf, buf_size, get_tok_str(v, NULL)); }
		break;
	case VT_FUNC:
		s = type->ref;
		buf1[0] = 0;
		if (varstr && '*' == *varstr) { MCC_TRACE("br\n");
			pstrcat(buf1, sizeof(buf1), "(");
			pstrcat(buf1, sizeof(buf1), varstr);
			pstrcat(buf1, sizeof(buf1), ")");
		}
		pstrcat(buf1, buf_size, "(");
		sa = s->next;
		while (sa != NULL) { MCC_TRACE("br\n");
			char buf2[256];
			type_to_str(buf2, sizeof(buf2), &sa->type, NULL);
			pstrcat(buf1, sizeof(buf1), buf2);
			sa = sa->next;
			if (sa)
				{ MCC_TRACE("br\n"); pstrcat(buf1, sizeof(buf1), ", "); }
		}
		if (s->f.func_type == FUNC_ELLIPSIS)
			{ MCC_TRACE("br\n"); pstrcat(buf1, sizeof(buf1), ", ..."); }
		pstrcat(buf1, sizeof(buf1), ")");
		type_to_str(buf, buf_size, &s->type, buf1);
		goto no_var;
	case VT_PTR:
		s = type->ref;
		if (t & (VT_ARRAY | VT_VLA)) { MCC_TRACE("br\n");
			if (varstr && '*' == *varstr)
				{ MCC_TRACE("br\n"); snprintf(buf1, sizeof(buf1), "(%s)[%d]", varstr, s->c); }
			else
				{ MCC_TRACE("br\n"); snprintf(buf1, sizeof(buf1), "%s[%d]", varstr ? varstr : "", s->c); }
			type_to_str(buf, buf_size, &s->type, buf1);
			goto no_var;
		}
		pstrcpy(buf1, sizeof(buf1), "*");
		if (t & VT_CONSTANT)
			{ MCC_TRACE("br\n"); pstrcat(buf1, buf_size, "const "); }
		if (t & VT_VOLATILE)
			{ MCC_TRACE("br\n"); pstrcat(buf1, buf_size, "volatile "); }
		if (varstr)
			{ MCC_TRACE("br\n"); pstrcat(buf1, sizeof(buf1), varstr); }
		type_to_str(buf, buf_size, &s->type, buf1);
		goto no_var;
	}
	if (varstr) { MCC_TRACE("br\n");
		pstrcat(buf, buf_size, " ");
		pstrcat(buf, buf_size, varstr);
	}
no_var:;
}

static void type_incompatibility_error(CType *st, CType *dt, const char *fmt) { MCC_TRACE("enter\n");
	char buf1[256], buf2[256];
	type_to_str(buf1, sizeof(buf1), st, NULL);
	type_to_str(buf2, sizeof(buf2), dt, NULL);
	mcc_error(fmt, buf1, buf2);
}

static void type_incompatibility_warning(CType *st, CType *dt, const char *fmt) { MCC_TRACE("enter\n");
	char buf1[256], buf2[256];
	type_to_str(buf1, sizeof(buf1), st, NULL);
	type_to_str(buf2, sizeof(buf2), dt, NULL);
	if (mcc_state->pedantic_errors)
		{ MCC_TRACE("br\n"); mcc_error(fmt, buf1, buf2); }
	else
		{ MCC_TRACE("br\n"); mcc_warning(fmt, buf1, buf2); }
}

static int pointed_size(CType *type) { MCC_TRACE("enter\n");
	int align;
	return type_size(pointed_type(type), &align);
}

static inline int is_null_pointer(SValue *p) { MCC_TRACE("enter\n");
	if ((p->r & (VT_VALMASK | VT_LVAL | VT_SYM | VT_NONCONST)) != VT_CONST)
		{ MCC_TRACE("br\n"); return 0; }
	return ((p->type.t & VT_BTYPE) == VT_INT && (uint32_t)p->c.i == 0) ||
				 ((p->type.t & VT_BTYPE) == VT_LLONG && p->c.i == 0) ||
				 ((p->type.t & VT_BTYPE) == VT_PTR &&
					(MCC_PTR_SIZE == 4 ? (uint32_t)p->c.i == 0 : p->c.i == 0) &&
					((pointed_type(&p->type)->t & VT_BTYPE) == VT_VOID) &&
					0 == (pointed_type(&p->type)->t & VT_QUALIFY));
}

static int is_compatible_func(CType *type1, CType *type2) { MCC_TRACE("enter\n");
	Sym *s1, *s2;

	s1 = type1->ref;
	s2 = type2->ref;
	if (s1->f.func_call != s2->f.func_call)
		{ MCC_TRACE("br\n"); return 0; }
	if (s1->f.func_type != s2->f.func_type && s1->f.func_type != FUNC_OLD && s2->f.func_type != FUNC_OLD)
		{ MCC_TRACE("br\n"); return 0; }
	for (;;) { MCC_TRACE("br\n");
		if (!is_compatible_unqualified_types(&s1->type, &s2->type))
			{ MCC_TRACE("br\n"); return 0; }
		if (s1->f.func_type == FUNC_OLD || s2->f.func_type == FUNC_OLD)
			{ MCC_TRACE("br\n"); return 1; }
		s1 = s1->next;
		s2 = s2->next;
		if (!s1)
			{ MCC_TRACE("br\n"); return !s2; }
		if (!s2)
			{ MCC_TRACE("br\n"); return 0; }
	}
}

static int compare_types(CType *type1, CType *type2, int unqualified) { MCC_TRACE("enter\n");
	int bt1, t1, t2;

	if (IS_ENUM(type1->t)) { MCC_TRACE("br\n");
		if (IS_ENUM(type2->t))
			{ MCC_TRACE("br\n"); return type1->ref == type2->ref; }
		type1 = &type1->ref->type;
	} else if (IS_ENUM(type2->t))
		{ MCC_TRACE("br\n"); type2 = &type2->ref->type; }

	t1 = type1->t & VT_TYPE;
	t2 = type2->t & VT_TYPE;
	if (unqualified) { MCC_TRACE("br\n");
		t1 &= ~VT_QUALIFY;
		t2 &= ~VT_QUALIFY;
	}

	if ((t1 & VT_BTYPE) != VT_BYTE) { MCC_TRACE("br\n");
		t1 &= ~VT_DEFSIGN;
		t2 &= ~VT_DEFSIGN;
	}
	if (t1 != t2)
		{ MCC_TRACE("br\n"); return 0; }

	if ((t1 & VT_ARRAY) && !(type1->ref->c < 0 || type2->ref->c < 0 || type1->ref->c == type2->ref->c))
		{ MCC_TRACE("br\n"); return 0; }

	bt1 = t1 & VT_BTYPE;
	if (bt1 == VT_PTR) { MCC_TRACE("br\n");
		type1 = pointed_type(type1);
		type2 = pointed_type(type2);
		return is_compatible_types(type1, type2);
	} else if (bt1 == VT_STRUCT) { MCC_TRACE("br\n");
		return (type1->ref == type2->ref);
	} else if (bt1 == VT_FUNC) { MCC_TRACE("br\n");
		return is_compatible_func(type1, type2);
	} else { MCC_TRACE("br\n");
		return 1;
	}
}

#define CMP_OP 'C'
#define SHIFT_OP 'S'

static int combine_types(CType *dest, SValue *op1, SValue *op2, int op) { MCC_TRACE("enter\n");
	CType *type1, *type2, type;
	int t1, t2, bt1, bt2;
	int ret = 1;

	if (op == SHIFT_OP)
		{ MCC_TRACE("br\n"); op2 = op1; }

	type1 = &op1->type, type2 = &op2->type;
	t1 = type1->t, t2 = type2->t;
	bt1 = t1 & VT_BTYPE, bt2 = t2 & VT_BTYPE;

	type.t = VT_VOID;
	type.ref = NULL;

	if (bt1 == VT_VOID || bt2 == VT_VOID) { MCC_TRACE("br\n");
		if (op != '?')
			{ MCC_TRACE("br\n"); mcc_error("operation on void value"); }
		type.t = VT_VOID;
	} else if (bt1 == VT_PTR || bt2 == VT_PTR) { MCC_TRACE("br\n");
		if (op == '+') { MCC_TRACE("br\n");
			if (!is_integer_btype(bt1 == VT_PTR ? bt2 : bt1))
				{ MCC_TRACE("br\n"); ret = 0; }
		} else if (is_null_pointer(op2))
			{ MCC_TRACE("br\n"); type = *type1; }
		else if (is_null_pointer(op1))
			{ MCC_TRACE("br\n"); type = *type2; }
		else if (bt1 != bt2) { MCC_TRACE("br\n");
			if ((op == '?' || op == CMP_OP) && (is_integer_btype(bt1) || is_integer_btype(bt2)))
				{ MCC_TRACE("br\n"); mcc_warning("pointer/integer mismatch in %s",
										op == '?' ? "conditional expression" : "comparison"); }
			else if (op != '-' || !is_integer_btype(bt2))
				{ MCC_TRACE("br\n"); ret = 0; }
			type = *(bt1 == VT_PTR ? type1 : type2);
		} else { MCC_TRACE("br\n");
			CType *pt1 = pointed_type(type1);
			CType *pt2 = pointed_type(type2);
			int pbt1 = pt1->t & VT_BTYPE;
			int pbt2 = pt2->t & VT_BTYPE;
			int newquals, copied = 0;
			if (pbt1 != VT_VOID && pbt2 != VT_VOID && !compare_types(pt1, pt2, 1)) { MCC_TRACE("br\n");
				if (op != '?' && op != CMP_OP)
					{ MCC_TRACE("br\n"); ret = 0; }
				else
					{ MCC_TRACE("br\n"); type_incompatibility_warning(type1, type2,
																			 op == '?'
																					 ? "pointer type mismatch in conditional expression ('%s' and '%s')"
																					 : "pointer type mismatch in comparison('%s' and '%s')"); }
			}
			if (op == '?') { MCC_TRACE("br\n");
				type = *((pbt1 == VT_VOID) ? type1 : type2);
				newquals = ((pt1->t | pt2->t) & VT_QUALIFY);
				if ((~pointed_type(&type)->t & VT_QUALIFY) & newquals) { MCC_TRACE("br\n");
					type.ref = sym_push(SYM_FIELD, &type.ref->type,
															0, type.ref->c);
					copied = 1;
					pointed_type(&type)->t |= newquals;
				}
				if (pt1->t & VT_ARRAY && pt2->t & VT_ARRAY && pointed_type(&type)->ref->c < 0 && (pt1->ref->c > 0 || pt2->ref->c > 0)) { MCC_TRACE("br\n");
					if (!copied)
						{ MCC_TRACE("br\n"); type.ref = sym_push(SYM_FIELD, &type.ref->type,
																0, type.ref->c); }
					pointed_type(&type)->ref =
							sym_push(SYM_FIELD, &pointed_type(&type)->ref->type,
											 0, pointed_type(&type)->ref->c);
					pointed_type(&type)->ref->c =
							0 < pt1->ref->c ? pt1->ref->c : pt2->ref->c;
				}
			}
		}
		if (op == CMP_OP)
			{ MCC_TRACE("br\n"); type.t = VT_SIZE_T; }
	} else if (bt1 == VT_STRUCT || bt2 == VT_STRUCT) { MCC_TRACE("br\n");
		if (op != '?' || !compare_types(type1, type2, 1))
			{ MCC_TRACE("br\n"); ret = 0; }
		type = *type1;
	} else if (is_float(bt1) || is_float(bt2)) { MCC_TRACE("br\n");
		if (bt1 == VT_LDOUBLE || bt2 == VT_LDOUBLE) { MCC_TRACE("br\n");
			type.t = VT_LDOUBLE;
		} else if (bt1 == VT_DOUBLE || bt2 == VT_DOUBLE) { MCC_TRACE("br\n");
			type.t = VT_DOUBLE;
		} else { MCC_TRACE("br\n");
			type.t = VT_FLOAT;
		}
	} else if (bt1 == VT_LLONG || bt2 == VT_LLONG) { MCC_TRACE("br\n");
		type.t = VT_LLONG | VT_LONG;
		if (bt1 == VT_LLONG)
			{ MCC_TRACE("br\n"); type.t &= t1; }
		if (bt2 == VT_LLONG)
			{ MCC_TRACE("br\n"); type.t &= t2; }
		if ((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED) ||
				(t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED))
			{ MCC_TRACE("br\n"); type.t |= VT_UNSIGNED; }
	} else { MCC_TRACE("br\n");
		type.t = VT_INT | (VT_LONG & (t1 | t2));
		if (((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED) && (!(t1 & VT_BITFIELD) || BIT_SIZE(t1) == 32)) || ((t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED) && (!(t2 & VT_BITFIELD) || BIT_SIZE(t2) ==
																																																																																																								32)))
			{ MCC_TRACE("br\n"); type.t |= VT_UNSIGNED; }
	}
	if (dest)
		{ MCC_TRACE("br\n"); *dest = type; }
	return ret;
}

static int bf_operand_bits(int tt) { MCC_TRACE("enter\n");
	if (tt & VT_BITFIELD)
		{ MCC_TRACE("br\n"); return BIT_SIZE(tt); }
	return (tt & VT_BTYPE) == VT_LLONG ? 64 : 32;
}

ST_FUNC void gen_op(int op) { MCC_TRACE("enter\n");
	int t1, t2, bt1, bt2, t;
	int bf_trunc = 0;
#if MCC_CONFIG_OPTIMIZER
	ast_hook_genop(op);
#endif
	CType type1, combtype;
	int op_class = op;

	expr_has_effect = 0;

	if (op == TOK_SHR || op == TOK_SAR || op == TOK_SHL)
		{ MCC_TRACE("br\n"); op_class = SHIFT_OP; }
	else if (TOK_ISCOND(op))
		{ MCC_TRACE("br\n"); op_class = CMP_OP; }

redo:
	t1 = vtop[-1].type.t;
	t2 = vtop[0].type.t;
	bt1 = t1 & VT_BTYPE;
	bt2 = t2 & VT_BTYPE;

	if (is_complex_type(&vtop[-1].type) || is_complex_type(&vtop[0].type)) { MCC_TRACE("br\n");
		gen_complex_op(op);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_genop_end();
#endif
		return;
	}

	if (bt1 == VT_FUNC || bt2 == VT_FUNC) { MCC_TRACE("br\n");
		if (bt2 == VT_FUNC) { MCC_TRACE("br\n");
			mk_pointer(&vtop->type);
			gaddrof();
		}
		if (bt1 == VT_FUNC) { MCC_TRACE("br\n");
			vswap();
			mk_pointer(&vtop->type);
			gaddrof();
			vswap();
		}
		goto redo;
	} else if (!combine_types(&combtype, vtop - 1, vtop, op_class)) { MCC_TRACE("br\n");
	op_err:
		mcc_error("invalid operand types for binary operation");
	} else if (bt1 == VT_PTR || bt2 == VT_PTR) { MCC_TRACE("br\n");
		int align;
		if (op_class == CMP_OP)
			{ MCC_TRACE("br\n"); goto std_op; }
		if (op == '+' || op == '-') { MCC_TRACE("br\n");
			CType *pt = bt1 == VT_PTR
											? pointed_type(&vtop[-1].type)
									: bt2 == VT_PTR
											? pointed_type(&vtop[0].type)
											: NULL;
			if (pt && ((pt->t & VT_BTYPE) == VT_VOID || (pt->t & VT_BTYPE) == VT_FUNC))
				{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids arithmetic on a pointer to "
										 "'void' or to a function"); }
		}
		if (bt1 == VT_PTR && bt2 == VT_PTR) { MCC_TRACE("br\n");
			if (op != '-')
				{ MCC_TRACE("br\n"); goto op_err; }
			vpush_type_size(pointed_type(&vtop[-1].type), &align);
			vtop->type.t &= ~VT_UNSIGNED;
			vrott(3);
			gen_opic(op);
			vtop->type.t = VT_PTRDIFF_T;
			vswap();
			gen_op(TOK_PDIV);
		} else { MCC_TRACE("br\n");
			if (op != '-' && op != '+')
				{ MCC_TRACE("br\n"); goto op_err; }
			if (bt2 == VT_PTR) { MCC_TRACE("br\n");
				vswap();
				t = t1, t1 = t2, t2 = t;
				bt2 = bt1;
			}
#if MCC_PTR_SIZE == 4
			if (bt2 == VT_LLONG)
				{ MCC_TRACE("br\n"); gen_cast_s(VT_INT); }
#endif
			type1 = vtop[-1].type;
			vpush_type_size(pointed_type(&vtop[-1].type), &align);
			vtop->type.t &= ~VT_UNSIGNED;
			gen_op('*');
#if MCC_CONFIG_DIAG_RT >= 2
			if (mcc_state->do_bounds_check && !CONST_WANTED) { MCC_TRACE("br\n");
				if (op == '-') { MCC_TRACE("br\n");
					vpushi(0);
					vswap();
					gen_op('-');
				}
				gen_bounded_ptr_add();
			} else
#endif
			{ MCC_TRACE("br\n");
				gen_opic(op);
			}
			type1.t &= ~(VT_ARRAY | VT_VLA);
			vtop->type = type1;
		}
	} else { MCC_TRACE("br\n");
		if (is_float(combtype.t) && op != '+' && op != '-' && op != '*' && op != '/' && op_class != CMP_OP) { MCC_TRACE("br\n");
			goto op_err;
		}
	std_op:
		if (op_class != CMP_OP && (combtype.t & VT_BTYPE) == VT_LLONG && !is_float(combtype.t)) { MCC_TRACE("br\n");
			int wide1 = (t1 & VT_BITFIELD) && BIT_SIZE(t1) > 32;
			int wide2 = (t2 & VT_BITFIELD) && BIT_SIZE(t2) > 32;
			if (op_class == SHIFT_OP) { MCC_TRACE("br\n");
				if (wide1 && bf_operand_bits(t1) < 64)
					{ MCC_TRACE("br\n"); bf_trunc = bf_operand_bits(t1); }
			} else if (wide1 || wide2) { MCC_TRACE("br\n");
				int w1 = bf_operand_bits(t1), w2 = bf_operand_bits(t2);
				int p = w1 > w2 ? w1 : w2;
				if (p < 64)
					{ MCC_TRACE("br\n"); bf_trunc = p; }
			}
		}
		if (op_class == CMP_OP && (mcc_state->warn_sign_compare & WARN_ON) && !is_float(combtype.t) && (t1 & VT_UNSIGNED) != (t2 & VT_UNSIGNED)) { MCC_TRACE("br\n");
			int ac = (vtop[-1].r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
			int bc = (vtop[0].r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
			if (!ac && !bc)
				{ MCC_TRACE("br\n"); mcc_warning_c(warn_sign_compare)(
						"comparison of integer expressions of different signedness"); }
		}
		t = t2 = combtype.t;
		if (op_class == SHIFT_OP)
			{ MCC_TRACE("br\n"); t2 = VT_INT; }
		if (t & VT_UNSIGNED) { MCC_TRACE("br\n");
			if (op == TOK_SAR)
				{ MCC_TRACE("br\n"); op = TOK_SHR; }
			else if (op == '/')
				{ MCC_TRACE("br\n"); op = TOK_UDIV; }
			else if (op == '%')
				{ MCC_TRACE("br\n"); op = TOK_UMOD; }
			else if (op == TOK_LT)
				{ MCC_TRACE("br\n"); op = TOK_ULT; }
			else if (op == TOK_GT)
				{ MCC_TRACE("br\n"); op = TOK_UGT; }
			else if (op == TOK_LE)
				{ MCC_TRACE("br\n"); op = TOK_ULE; }
			else if (op == TOK_GE)
				{ MCC_TRACE("br\n"); op = TOK_UGE; }
		}
		vswap();
		gen_cast_s(t);
		vswap();
		gen_cast_s(t2);
		if (is_float(t))
			{ MCC_TRACE("br\n"); gen_opif(op); }
		else
			{ MCC_TRACE("br\n"); gen_opic(op); }
		if (op_class == CMP_OP) { MCC_TRACE("br\n");
			vtop->type.t = VT_INT;
		} else { MCC_TRACE("br\n");
			vtop->type.t = t;
			if (bf_trunc) { MCC_TRACE("br\n");
				int sh = 64 - bf_trunc;
				vpushi(sh);
				gen_op(TOK_SHL);
				vpushi(sh);
				gen_op((t & VT_UNSIGNED) ? TOK_SHR : TOK_SAR);
			}
		}
	}
	if (vtop->r & VT_LVAL)
		{ MCC_TRACE("br\n"); gv(MCC_RC_TYPE(vtop->type.t)); }
#if MCC_CONFIG_OPTIMIZER
	ast_hook_genop_end();
#endif
}

#if defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64 || defined MCC_TARGET_ARM
#define gen_cvt_itof1 gen_cvt_itof
#else
static void gen_cvt_itof1(int t) { MCC_TRACE("enter\n");
	if ((vtop->type.t & (VT_BTYPE | VT_UNSIGNED)) ==
			(VT_LLONG | VT_UNSIGNED)) { MCC_TRACE("br\n");
		if (t == VT_FLOAT)
			{ MCC_TRACE("br\n"); vpush_helper_func(TOK___floatundisf); }
#if MCC_LDOUBLE_SIZE != 8
		else if (t == VT_LDOUBLE)
			{ MCC_TRACE("br\n"); vpush_helper_func(TOK___floatundixf); }
#endif
		else
			{ MCC_TRACE("br\n"); vpush_helper_func(TOK___floatundidf); }
		vrott(2);
		gfunc_call(1);
		vpushi(0);
		PUT_R_RET(vtop, t);
	} else { MCC_TRACE("br\n");
		gen_cvt_itof(t);
	}
}
#endif

#if defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64
#define gen_cvt_ftoi1 gen_cvt_ftoi
#else
static void gen_cvt_ftoi1(int t) { MCC_TRACE("enter\n");
	int st;
	if (t == (VT_LLONG | VT_UNSIGNED)) { MCC_TRACE("br\n");
		st = vtop->type.t & VT_BTYPE;
		if (st == VT_FLOAT)
			{ MCC_TRACE("br\n"); vpush_helper_func(TOK___fixunssfdi); }
#if MCC_LDOUBLE_SIZE != 8
		else if (st == VT_LDOUBLE)
			{ MCC_TRACE("br\n"); vpush_helper_func(TOK___fixunsxfdi); }
#endif
		else
			{ MCC_TRACE("br\n"); vpush_helper_func(TOK___fixunsdfdi); }
		vrott(2);
		gfunc_call(1);
		vpushi(0);
		PUT_R_RET(vtop, t);
	} else { MCC_TRACE("br\n");
		gen_cvt_ftoi(t);
	}
}
#endif

static void force_charshort_cast(void) { MCC_TRACE("enter\n");
	int sbt = BFGET(vtop->r, VT_MUSTCAST) == 2 ? VT_LLONG : VT_INT;
	int dbt = vtop->type.t;
	vtop->r &= ~VT_MUSTCAST;
	vtop->type.t = sbt;
	gen_cast_s(dbt == VT_BOOL ? VT_BYTE | VT_UNSIGNED : dbt);
	vtop->type.t = dbt;
}

static void gen_cast_s(int t) { MCC_TRACE("enter\n");
	gen_cast(&(CType){.t = t, .ref = NULL});
}

static void gen_cast(CType *type) { MCC_TRACE("enter\n");
	int sbt, dbt, sf, df, c;
#if MCC_CONFIG_OPTIMIZER
	ast_hook_convert(type);
#endif
	int dbt_bt, sbt_bt, ds, ss, bits, trunc;

	if (!atomic_lowering && (vtop->type.t & VT_ATOMIC_BIT) && (vtop->r & VT_LVAL) && atomic_store_needs_libcall(vtop))
		{ MCC_TRACE("br\n"); gen_atomic_load_scalar(); }

	if (vtop->r & VT_MUSTCAST)
		{ MCC_TRACE("br\n"); force_charshort_cast(); }

	if (is_complex_type(type) || is_complex_type(&vtop->type)) { MCC_TRACE("br\n");
		if (is_complex_type(type) && is_complex_type(&vtop->type) && (type->ref->next->type.t & (VT_BTYPE | VT_LONG)) == (vtop->type.ref->next->type.t & (VT_BTYPE | VT_LONG)))
			{ MCC_TRACE("br\n"); return; }
#if MCC_CONFIG_OPTIMIZER
		int sup = ast_active && !ast_replaying;
		if (sup)
			{ MCC_TRACE("br\n"); ast_in_op++; }
		gen_complex_cast(type);
		if (sup)
			{ MCC_TRACE("br\n"); ast_in_op--; }
#else
		gen_complex_cast(type);
#endif
		return;
	}

	if (vtop->type.t & VT_BITFIELD)
		{ MCC_TRACE("br\n"); gv(MCC_RC_INT); }

	if (IS_ENUM(type->t) && type->ref->c < 0)
		{ MCC_TRACE("br\n"); mcc_error("cast to incomplete type"); }

	dbt = type->t & (VT_BTYPE | VT_UNSIGNED);
	sbt = vtop->type.t & (VT_BTYPE | VT_UNSIGNED);
	if (sbt == VT_FUNC)
		{ MCC_TRACE("br\n"); sbt = VT_PTR; }

again:
	if (sbt != dbt) { MCC_TRACE("br\n");
		sf = is_float(sbt);
		df = is_float(dbt);
		dbt_bt = dbt & VT_BTYPE;
		sbt_bt = sbt & VT_BTYPE;
		if (dbt_bt == VT_VOID) { MCC_TRACE("br\n");
			goto done;
		}
		if (sbt_bt == VT_VOID) { MCC_TRACE("br\n");
		error:
			cast_error(&vtop->type, type);
		}

		if ((sf && dbt_bt == VT_PTR) || (df && sbt_bt == VT_PTR))
			{ MCC_TRACE("br\n"); mcc_error("cannot cast between a floating type and a pointer"); }

		c = (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
		if (c) { MCC_TRACE("br\n");
			if (sbt == VT_FLOAT)
				{ MCC_TRACE("br\n"); vtop->c.ld = vtop->c.f; }
			else if (sbt == VT_DOUBLE)
				{ MCC_TRACE("br\n"); vtop->c.ld = vtop->c.d; }

			if (df) { MCC_TRACE("br\n");
				if (sbt_bt == VT_LLONG) { MCC_TRACE("br\n");
					if ((sbt & VT_UNSIGNED) || !(vtop->c.i >> 63))
						{ MCC_TRACE("br\n"); vtop->c.ld = vtop->c.i; }
					else
						{ MCC_TRACE("br\n"); vtop->c.ld = -(long double)-vtop->c.i; }
				} else if (!sf) { MCC_TRACE("br\n");
					if ((sbt & VT_UNSIGNED) || !(vtop->c.i >> 31))
						{ MCC_TRACE("br\n"); vtop->c.ld = (uint32_t)vtop->c.i; }
					else
						{ MCC_TRACE("br\n"); vtop->c.ld = -(long double)-(uint32_t)vtop->c.i; }
				}

				if (dbt == VT_FLOAT)
					{ MCC_TRACE("br\n"); vtop->c.f = (float)vtop->c.ld; }
				else if (dbt == VT_DOUBLE)
					{ MCC_TRACE("br\n"); vtop->c.d = (double)vtop->c.ld; }
			} else if (sf && dbt == VT_BOOL) { MCC_TRACE("br\n");
				vtop->c.i = (vtop->c.ld != 0);
			} else { MCC_TRACE("br\n");
				if (sf) { MCC_TRACE("br\n");
					if (dbt & VT_UNSIGNED)
						{ MCC_TRACE("br\n"); vtop->c.i = (uint64_t)vtop->c.ld; }
					else
						{ MCC_TRACE("br\n"); vtop->c.i = (int64_t)vtop->c.ld; }
				} else if (sbt_bt == VT_LLONG || (MCC_PTR_SIZE == 8 && sbt == VT_PTR))
					{ MCC_TRACE("br\n"); ; }
				else if (sbt & VT_UNSIGNED)
					{ MCC_TRACE("br\n"); vtop->c.i = (uint32_t)vtop->c.i; }
				else
					{ MCC_TRACE("br\n"); vtop->c.i = ((uint32_t)vtop->c.i | -(vtop->c.i & 0x80000000)); }

				if (dbt_bt == VT_LLONG || (MCC_PTR_SIZE == 8 && dbt == VT_PTR))
					{ MCC_TRACE("br\n"); ; }
				else if (dbt == VT_BOOL)
					{ MCC_TRACE("br\n"); vtop->c.i = (vtop->c.i != 0); }
				else { MCC_TRACE("br\n");
					uint32_t m = dbt_bt == VT_BYTE
													 ? 0xff
											 : dbt_bt == VT_SHORT
													 ? 0xffff
													 : 0xffffffff;
					vtop->c.i &= m;
					if (!(dbt & VT_UNSIGNED))
						{ MCC_TRACE("br\n"); vtop->c.i |= -(vtop->c.i & ((m >> 1) + 1)); }
				}
			}
			goto done;
		} else if (dbt == VT_BOOL && (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == (VT_CONST | VT_SYM)) { MCC_TRACE("br\n");
			vtop->r = VT_CONST;
			vtop->c.i = 1;
			goto done;
		}

		if (nocode_wanted & DATA_ONLY_WANTED) { MCC_TRACE("br\n");
			if (df)
				{ MCC_TRACE("br\n"); vtop->r = get_reg(MCC_RC_FLOAT); }
			goto done;
		}

		if (dbt == VT_BOOL) { MCC_TRACE("br\n");
			gen_test_zero(TOK_NE);
			goto done;
		}

		if (sf || df) { MCC_TRACE("br\n");
			if ((sf && dbt_bt == VT_PTR) || (df && sbt_bt == VT_PTR))
				{ MCC_TRACE("br\n"); mcc_error("cannot cast between a floating type and a pointer"); }
			if (sf && df) { MCC_TRACE("br\n");
				gen_cvt_ftof(dbt);
			} else if (df) { MCC_TRACE("br\n");
				gen_cvt_itof1(dbt);
			} else { MCC_TRACE("br\n");
				sbt = dbt;
				if (dbt_bt != VT_LLONG && dbt_bt != VT_INT)
					{ MCC_TRACE("br\n"); sbt = VT_INT; }
				gen_cvt_ftoi1(sbt);
				goto again;
			}
			goto done;
		}

		ds = btype_size(dbt_bt);
		ss = btype_size(sbt_bt);
		if (ds == 0 || ss == 0)
			{ MCC_TRACE("br\n"); goto error; }

		if (ds == ss && ds >= 4)
			{ MCC_TRACE("br\n"); goto done; }
		if (dbt_bt == VT_PTR || sbt_bt == VT_PTR) { MCC_TRACE("br\n");
			mcc_warning("cast between pointer and integer of different size");
			if (sbt_bt == VT_PTR) { MCC_TRACE("br\n");
				vtop->type.t = (MCC_PTR_SIZE == 8 ? VT_LLONG : VT_INT);
			}
		}

#define ALLOW_SUBTYPE_ACCESS 1

		if (ALLOW_SUBTYPE_ACCESS && (vtop->r & VT_LVAL)) { MCC_TRACE("br\n");
			if (ds <= ss)
				{ MCC_TRACE("br\n"); goto done; }
			if (ds <= 4 && !(dbt == (VT_SHORT | VT_UNSIGNED) && sbt == VT_BYTE)) { MCC_TRACE("br\n");
				gv(MCC_RC_INT);
				goto done;
			}
		}
		gv(MCC_RC_INT);

		trunc = 0;
#if MCC_PTR_SIZE == 4
		if (ds == 8) { MCC_TRACE("br\n");
			if (sbt & VT_UNSIGNED) { MCC_TRACE("br\n");
				vpushi(0);
				gv(MCC_RC_INT);
			} else { MCC_TRACE("br\n");
				gv_dup();
				vpushi(31);
				gen_op(TOK_SAR);
			}
			lbuild(dbt);
		} else if (ss == 8) { MCC_TRACE("br\n");
			lexpand();
			vpop();
		}
		ss = 4;

#elif MCC_PTR_SIZE == 8
		if (ds == 8) { MCC_TRACE("br\n");
			if (sbt & VT_UNSIGNED) { MCC_TRACE("br\n");
#if defined(MCC_TARGET_RISCV64)
				trunc = 32;
#else
				goto done;
#endif
			} else { MCC_TRACE("br\n");
				gen_cvt_sxtw();
				goto done;
			}
			ss = ds, ds = 4, dbt = sbt;
		} else if (ss == 8) { MCC_TRACE("br\n");
#if !defined(MCC_TARGET_RISCV64)
			trunc = 32;
#endif
		} else { MCC_TRACE("br\n");
			ss = 4;
		}
#endif

		if (ds >= ss)
			{ MCC_TRACE("br\n"); goto done; }
#if defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64 || defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64
		if (ss == 4) { MCC_TRACE("br\n");
			gen_cvt_csti(dbt);
			goto done;
		}
#endif
	{
#if MCC_CONFIG_OPTIMIZER
			int sup = ast_active && !ast_replaying;
			if (sup)
				{ MCC_TRACE("br\n"); ast_in_op++; }
#endif
			bits = (ss - ds) * 8;
			vtop->type.t = (ss == 8 ? VT_LLONG : VT_INT) | (dbt & VT_UNSIGNED);
			vpushi(bits);
			gen_op(TOK_SHL);
			vpushi(bits - trunc);
			gen_op(TOK_SAR);
			vpushi(trunc);
			gen_op(TOK_SHR);
#if MCC_CONFIG_OPTIMIZER
			if (sup)
				{ MCC_TRACE("br\n"); ast_in_op--; }
#endif
		}
	}
done:
	vtop->type = *type;
	vtop->type.t &= ~(VT_QUALIFY | VT_ARRAY);
}

ST_FUNC int type_size(CType *type, int *a) { MCC_TRACE("enter\n");
	Sym *s;
	int bt;

	bt = type->t & VT_BTYPE;
	if (bt == VT_STRUCT) { MCC_TRACE("br\n");
		s = type->ref;
		*a = s->r;
		return s->c;
	} else if (bt == VT_PTR) { MCC_TRACE("br\n");
		if (type->t & VT_ARRAY) { MCC_TRACE("br\n");
			int ts;
			s = type->ref;
			ts = type_size(&s->type, a);
			if (s->c < 0)
				{ MCC_TRACE("br\n"); return s->c; }
			return ts * s->c;
		} else { MCC_TRACE("br\n");
			*a = MCC_PTR_SIZE;
			return MCC_PTR_SIZE;
		}
	} else if (IS_ENUM(type->t) && type->ref->c < 0) { MCC_TRACE("br\n");
		*a = 0;
		return -1;
	} else if (bt == VT_LDOUBLE) { MCC_TRACE("br\n");
		*a = MCC_LDOUBLE_ALIGN;
		return MCC_LDOUBLE_SIZE;
	} else if (bt == VT_DOUBLE || bt == VT_LLONG) { MCC_TRACE("br\n");
#if (defined MCC_TARGET_I386 && !defined MCC_TARGET_PE) || (defined MCC_TARGET_ARM && !defined MCC_ARM_EABI)
		*a = 4;
#else
		*a = 8;
#endif
		return 8;
	} else if (bt == VT_INT || bt == VT_FLOAT) { MCC_TRACE("br\n");
		*a = 4;
		return 4;
	} else if (bt == VT_SHORT) { MCC_TRACE("br\n");
		*a = 2;
		return 2;
	} else if (bt == VT_QLONG || bt == VT_QFLOAT) { MCC_TRACE("br\n");
		*a = 8;
		return 16;
	} else { MCC_TRACE("br\n");
		*a = 1;
		return 1;
	}
}

static void vpush_type_size(CType *type, int *a) { MCC_TRACE("enter\n");
	if (type->t & VT_VLA) { MCC_TRACE("br\n");
		type_size(&type->ref->type, a);
		vset(&int_type, VT_LOCAL | VT_LVAL, type->ref->c);
	} else { MCC_TRACE("br\n");
		int size = type_size(type, a);
		if (size < 0)
			{ MCC_TRACE("br\n"); mcc_error("unknown type size"); }
		vpushs(size);
	}
}

static inline CType *pointed_type(CType *type) { MCC_TRACE("enter\n");
	return &type->ref->type;
}

ST_FUNC void mk_pointer(CType *type) { MCC_TRACE("enter\n");
	Sym *s;
	s = sym_push(SYM_FIELD, type, 0, -1);
	type->t = VT_PTR | (type->t & VT_STORAGE);
	type->ref = s;
}

static int is_compatible_types(CType *type1, CType *type2) { MCC_TRACE("enter\n");
	return compare_types(type1, type2, 0);
}

static int is_compatible_unqualified_types(CType *type1, CType *type2) { MCC_TRACE("enter\n");
	return compare_types(type1, type2, 1);
}

static void cast_error(CType *st, CType *dt) { MCC_TRACE("enter\n");
	type_incompatibility_error(st, dt, "cannot convert '%s' to '%s'");
}

static int aggr_has_const_member_rec(CType *type, int depth) { MCC_TRACE("enter\n");
	Sym *f;
	if ((type->t & VT_BTYPE) != VT_STRUCT || depth > 64)
		{ MCC_TRACE("br\n"); return 0; }
	for (f = type->ref->next; f; f = f->next) { MCC_TRACE("br\n");
		if (f->type.t & VT_CONSTANT)
			{ MCC_TRACE("br\n"); return 1; }
		if ((f->type.t & VT_BTYPE) == VT_STRUCT &&
				f->type.ref != type->ref &&
				aggr_has_const_member_rec(&f->type, depth + 1))
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static int aggr_has_const_member(CType *type) { MCC_TRACE("enter\n");
	return aggr_has_const_member_rec(type, 0);
}

static void incompatible_ptr_diag(void) { MCC_TRACE("enter\n");
	const char *what = assign_ctx_is_init ? "initialization" : "assignment";
	if (mcc_state->pedantic_errors)
		{ MCC_TRACE("br\n"); mcc_error("%s from incompatible pointer type", what); }
	else
		{ MCC_TRACE("br\n"); mcc_warning("%s from incompatible pointer type", what); }
}

static void verify_assign_cast(CType *dt) { MCC_TRACE("enter\n");
	CType *st, *type1, *type2;
	int dbt, sbt, qualwarn, lvl, deepqual;

	st = &vtop->type;
	dbt = dt->t & VT_BTYPE;
	sbt = st->t & VT_BTYPE;
	if (dt->t & VT_CONSTANT)
		{ MCC_TRACE("br\n"); mcc_error("assignment of read-only location"); }
	else if (dbt == VT_STRUCT && aggr_has_const_member(dt))
		{ MCC_TRACE("br\n"); mcc_error("assignment of read-only location"); }
	switch (dbt) { MCC_TRACE("br\n");
	case VT_VOID:
		if (sbt != dbt)
			{ MCC_TRACE("br\n"); mcc_error("assignment to void expression"); }
		break;
	case VT_PTR:
		if (is_null_pointer(vtop))
			{ MCC_TRACE("br\n"); break; }
		if (is_integer_btype(sbt)) { MCC_TRACE("br\n");
			mcc_warning("assignment makes pointer from integer without a cast");
			break;
		}
		type1 = pointed_type(dt);
		if (sbt == VT_PTR)
			{ MCC_TRACE("br\n"); type2 = pointed_type(st); }
		else if (sbt == VT_FUNC)
			{ MCC_TRACE("br\n"); type2 = st; }
		else
			{ MCC_TRACE("br\n"); goto error; }
		if (is_compatible_types(type1, type2))
			{ MCC_TRACE("br\n"); break; }
		for (qualwarn = lvl = deepqual = 0;; ++lvl) { MCC_TRACE("br\n");
			if (((type2->t & VT_CONSTANT) && !(type1->t & VT_CONSTANT)) ||
					((type2->t & VT_VOLATILE) && !(type1->t & VT_VOLATILE)))
				{ MCC_TRACE("br\n"); qualwarn = 1; }
			if (lvl > 0 && ((type1->t & ~type2->t) & (VT_CONSTANT | VT_VOLATILE)))
				{ MCC_TRACE("br\n"); deepqual = 1; }
			dbt = type1->t & (VT_BTYPE | VT_LONG);
			sbt = type2->t & (VT_BTYPE | VT_LONG);
			if (dbt != VT_PTR || sbt != VT_PTR)
				{ MCC_TRACE("br\n"); break; }
			type1 = pointed_type(type1);
			type2 = pointed_type(type2);
		}
		if (deepqual) { MCC_TRACE("br\n");
			incompatible_ptr_diag();
			break;
		}
		if (!is_compatible_unqualified_types(type1, type2)) { MCC_TRACE("br\n");
			if ((dbt == VT_VOID || sbt == VT_VOID) && lvl == 0) { MCC_TRACE("br\n");
				if (dbt == VT_FUNC || sbt == VT_FUNC)
					{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids conversion between a function "
											 "pointer and 'void *'"); }
			} else if (dbt == sbt && is_integer_btype(sbt & VT_BTYPE) && IS_ENUM(type1->t) + IS_ENUM(type2->t) + !!((type1->t ^ type2->t) & VT_UNSIGNED) < 2) { MCC_TRACE("br\n");
			} else { MCC_TRACE("br\n");
				incompatible_ptr_diag();
				break;
			}
		}
		if (qualwarn)
			{ MCC_TRACE("br\n"); mcc_warning_c(warn_discarded_qualifiers)("assignment discards qualifiers from pointer target type"); }
		break;
	case VT_BYTE:
	case VT_SHORT:
	case VT_INT:
	case VT_LLONG:
		if (sbt == VT_PTR || sbt == VT_FUNC) { MCC_TRACE("br\n");
			mcc_warning("assignment makes integer from pointer without a cast");
		} else if (sbt == VT_STRUCT) { MCC_TRACE("br\n");
			if (!is_complex_type(st))
				{ MCC_TRACE("br\n"); goto case_VT_STRUCT; }
		}
		break;
	case VT_STRUCT:
	case_VT_STRUCT:
		if (is_complex_type(dt) && (is_complex_type(st) || is_integer_btype(sbt) || is_float(st->t)))
			{ MCC_TRACE("br\n"); break; }
		if (!is_compatible_unqualified_types(dt, st)) { MCC_TRACE("br\n");
		error:
			cast_error(st, dt);
		}
		break;
	}
}

static void gen_assign_cast(CType *dt) { MCC_TRACE("enter\n");
	verify_assign_cast(dt);
	gen_cast(dt);
}

ST_FUNC void vstore(void) { MCC_TRACE("enter\n");
	int sbt, dbt, ft, r, size, align, bit_size, bit_pos, delayed_cast;
#if MCC_CONFIG_OPTIMIZER
	ast_hook_vstore();
#endif

	seqp_record_sv(vtop - 1, SEQP_WRITE);

	if (!atomic_lowering && (vtop->type.t & VT_ATOMIC_BIT) && (vtop->r & VT_LVAL) && atomic_store_needs_generic(vtop))
		{ MCC_TRACE("br\n"); gen_atomic_load_aggregate(); }

	ft = vtop[-1].type.t;
	sbt = vtop->type.t & VT_BTYPE;
	dbt = ft & VT_BTYPE;
	verify_assign_cast(&vtop[-1].type);

	if (is_complex_type(&vtop[-1].type) && !is_complex_type(&vtop->type)) { MCC_TRACE("br\n");
		gen_cast(&vtop[-1].type);
		sbt = vtop->type.t & VT_BTYPE;
	} else if (!is_complex_type(&vtop[-1].type) && is_complex_type(&vtop->type) && dbt != VT_STRUCT) { MCC_TRACE("br\n");
		gen_cast(&vtop[-1].type);
		sbt = vtop->type.t & VT_BTYPE;
	} else if (is_complex_type(&vtop[-1].type) && is_complex_type(&vtop->type) && (vtop[-1].type.ref->next->type.t & (VT_BTYPE | VT_LONG)) != (vtop->type.ref->next->type.t & (VT_BTYPE | VT_LONG))) { MCC_TRACE("br\n");
		gen_cast(&vtop[-1].type);
		sbt = vtop->type.t & VT_BTYPE;
	}

	if (sbt == VT_STRUCT) { MCC_TRACE("br\n");
		size = type_size(&vtop->type, &align);
		vpushv(vtop - 1);
#if MCC_CONFIG_DIAG_RT >= 2
		if (vtop->r & VT_MUSTBOUND)
			{ MCC_TRACE("br\n"); gbound(); }
#endif
		vtop->type.t = VT_PTR;
		gaddrof();
		vswap();
#if MCC_CONFIG_DIAG_RT >= 2
		if (vtop->r & VT_MUSTBOUND)
			{ MCC_TRACE("br\n"); gbound(); }
#endif
		vtop->type.t = VT_PTR;
		gaddrof();

#ifdef MCC_TARGET_NATIVE_STRUCT_COPY
		if (1
#if MCC_CONFIG_DIAG_RT >= 2
				&& !mcc_state->do_bounds_check
#endif
		) { MCC_TRACE("br\n");
			gen_struct_copy(size);
		} else
#endif
		{ MCC_TRACE("br\n");
			vpushi(size);
#ifdef MCC_ARM_EABI
			if (!(align & 7))
				{ MCC_TRACE("br\n"); vpush_helper_func(TOK_memmove8); }
			else if (!(align & 3))
				{ MCC_TRACE("br\n"); vpush_helper_func(TOK_memmove4); }
			else
#endif
				vpush_helper_func(TOK_memmove);
			vrott(4);
			gfunc_call(3);
		}
	} else if (ft & VT_BITFIELD) { MCC_TRACE("br\n");
		vdup(), vtop[-1] = vtop[-2];

		bit_pos = BIT_POS(ft);
		bit_size = BIT_SIZE(ft);
		vtop[-1].type.t = ft & ~VT_STRUCT_MASK;

		if (dbt == VT_BOOL) { MCC_TRACE("br\n");
			gen_cast(&vtop[-1].type);
			vtop[-1].type.t = (vtop[-1].type.t & ~VT_BTYPE) | (VT_BYTE | VT_UNSIGNED);
		}
		r = adjust_bf(vtop - 1, bit_pos, bit_size);
		if (dbt != VT_BOOL) { MCC_TRACE("br\n");
			gen_cast(&vtop[-1].type);
			dbt = vtop[-1].type.t & VT_BTYPE;
		}
		if (r == VT_STRUCT) { MCC_TRACE("br\n");
			store_packed_bf(bit_pos, bit_size);
		} else { MCC_TRACE("br\n");
			unsigned long long mask = (1ULL << bit_size) - 1;
			if (dbt != VT_BOOL) { MCC_TRACE("br\n");
				if (dbt == VT_LLONG)
					{ MCC_TRACE("br\n"); vpushll(mask); }
				else
					{ MCC_TRACE("br\n"); vpushi((unsigned)mask); }
				gen_op('&');
			}
			vpushi(bit_pos);
			gen_op(TOK_SHL);
			vswap();
			vdup();
			vrott(3);
			if (dbt == VT_LLONG)
				{ MCC_TRACE("br\n"); vpushll(~(mask << bit_pos)); }
			else
				{ MCC_TRACE("br\n"); vpushi(~((unsigned)mask << bit_pos)); }
			gen_op('&');
			gen_op('|');
			vstore();
			vpop();
		}
	} else if (dbt == VT_VOID) { MCC_TRACE("br\n");
		--vtop;
	} else { MCC_TRACE("br\n");
		delayed_cast = 0;
		if ((dbt == VT_BYTE || dbt == VT_SHORT) && is_integer_btype(sbt)) { MCC_TRACE("br\n");
			if ((vtop->r & VT_MUSTCAST) && btype_size(dbt) > btype_size(sbt))
				{ MCC_TRACE("br\n"); force_charshort_cast(); }
			delayed_cast = 1;
		} else { MCC_TRACE("br\n");
			gen_cast(&vtop[-1].type);
		}

#if MCC_CONFIG_DIAG_RT >= 2
		if (vtop[-1].r & VT_MUSTBOUND) { MCC_TRACE("br\n");
			vswap();
			gbound();
			vswap();
		}
#endif
		gv(MCC_RC_TYPE(dbt));

		if (delayed_cast) { MCC_TRACE("br\n");
			vtop->r |= BFVAL(VT_MUSTCAST, (sbt == VT_LLONG) + 1);
			vtop->type.t = ft & VT_TYPE;
		}

		if ((vtop[-1].r & VT_VALMASK) == VT_LLOCAL) { MCC_TRACE("br\n");
			SValue sv;
			r = get_reg(MCC_RC_INT);
			sv.type.t = VT_PTRDIFF_T;
			sv.r = VT_LOCAL | VT_LVAL;
			sv.c.i = vtop[-1].c.i;
			sv.sym = NULL;
			load(r, &sv);
			vtop[-1].r = r | VT_LVAL;
		}

		r = vtop->r & VT_VALMASK;
		if (USING_TWO_WORDS(dbt)) { MCC_TRACE("br\n");
			int load_type = (dbt == VT_QFLOAT) ? VT_DOUBLE : VT_PTRDIFF_T;
			vtop[-1].type.t = load_type;
			store(r, vtop - 1);
			vswap();
			incr_offset(MCC_PTR_SIZE);
			vswap();
			store(vtop->r2, vtop - 1);
		} else { MCC_TRACE("br\n");
			store(r, vtop - 1);
		}
		vswap();
		vtop--;
	}
#if MCC_CONFIG_OPTIMIZER
	ast_hook_vstore_end();
#endif
}

ST_FUNC void inc(int post, int c) { MCC_TRACE("enter\n");
	test_lvalue();
	if (vtop->type.t & VT_ATOMIC_BIT) { MCC_TRACE("br\n");
		if (atomic_rmw_size(vtop, '+')) { MCC_TRACE("br\n");
			vpushi(c - TOK_MID);
			gen_atomic_rmw('+', !post);
			return;
		}
		if (atomic_cas_size(vtop)) { MCC_TRACE("br\n");
			vpushi(c - TOK_MID);
			gen_atomic_cas_rmw('+', !post);
			return;
		}
		mcc_error("'++'/'--' on this '_Atomic' object is not supported "
							"(only integer/pointer atomics up to a machine word)");
	}
#if MCC_CONFIG_OPTIMIZER
	ast_hook_inc(post, c);
#endif
	vdup();
	if (post) { MCC_TRACE("br\n");
		gv_dup();
		vrotb(3);
		vrotb(3);
	}
	vpushi(c - TOK_MID);
	gen_op('+');
	vstore();
	if (post)
		{ MCC_TRACE("br\n"); vpop(); }
#if MCC_CONFIG_OPTIMIZER
	ast_hook_inc_end();
#endif
}

ST_FUNC CString *parse_mult_str(const char *msg) { MCC_TRACE("enter\n");
	if (tok != TOK_STR)
		{ MCC_TRACE("br\n"); expect(msg); }
	cstr_reset(&initstr);
	while (tok == TOK_STR) { MCC_TRACE("br\n");
		cstr_cat(&initstr, tokc.str.data, -1);
		next();
	}
	cstr_ccat(&initstr, '\0');
	return &initstr;
}

ST_FUNC int exact_log2p1(int i) { MCC_TRACE("enter\n");
	int ret;
	if (!i)
		{ MCC_TRACE("br\n"); return 0; }
	for (ret = 1; i >= 1 << 8; ret += 8)
		{ MCC_TRACE("br\n"); i >>= 8; }
	if (i >= 1 << 4)
		{ MCC_TRACE("br\n"); ret += 4, i >>= 4; }
	if (i >= 1 << 2)
		{ MCC_TRACE("br\n"); ret += 2, i >>= 2; }
	if (i >= 1 << 1)
		{ MCC_TRACE("br\n"); ret++; }
	return ret;
}

static void parse_attribute(AttributeDef *ad) { MCC_TRACE("enter\n");
	int t, n;
	char *astr;
	AttributeDef ad_tmp;

redo:
	if (tok != TOK_ATTRIBUTE1 && tok != TOK_ATTRIBUTE2)
		{ MCC_TRACE("br\n"); return; }
	if (NULL == ad)
		{ MCC_TRACE("br\n"); ad = &ad_tmp; }

	next();
	skip('(');
	skip('(');
	while (tok != ')') { MCC_TRACE("br\n");
		if (tok < TOK_IDENT)
			{ MCC_TRACE("br\n"); expect("attribute name"); }
		t = tok;
		next();
		switch (t) { MCC_TRACE("br\n");
		case TOK_CLEANUP1:
		case TOK_CLEANUP2: {
			Sym *s;

			skip('(');
			s = sym_find(tok);
			if (!s) { MCC_TRACE("br\n");
				mcc_warning_c(warn_implicit_function_declaration)(
						"implicit declaration of function '%s'", get_tok_str(tok, &tokc));
				s = external_global_sym(tok, &func_old_type);
			} else if ((s->type.t & VT_BTYPE) != VT_FUNC)
				{ MCC_TRACE("br\n"); mcc_error("'%s' is not declared as function", get_tok_str(tok, &tokc)); }
			ad->cleanup_func = s;
			next();
			skip(')');
			break;
		}
		case TOK_CONSTRUCTOR1:
		case TOK_CONSTRUCTOR2:
			ad->f.func_ctor = 1;
			break;
		case TOK_DESTRUCTOR1:
		case TOK_DESTRUCTOR2:
			ad->f.func_dtor = 1;
			break;
		case TOK_ALWAYS_INLINE1:
		case TOK_ALWAYS_INLINE2:
			ad->f.func_alwinl = 1;
			break;
		case TOK_SECTION1:
		case TOK_SECTION2:
			skip('(');
			astr = parse_mult_str("section name")->data;
			ad->section = find_section(mcc_state, astr);
			skip(')');
			break;
		case TOK_ALIAS1:
		case TOK_ALIAS2:
			skip('(');
			astr = parse_mult_str("alias(\"target\")")->data;
			ad->alias_target = tok_alloc_const(astr);
			skip(')');
			break;
		case TOK_VISIBILITY1:
		case TOK_VISIBILITY2:
			skip('(');
			astr = parse_mult_str("visibility(\"default|hidden|internal|protected\")")->data;
			if (!strcmp(astr, "default"))
				{ MCC_TRACE("br\n"); ad->a.visibility = STV_DEFAULT; }
			else if (!strcmp(astr, "hidden"))
				{ MCC_TRACE("br\n"); ad->a.visibility = STV_HIDDEN; }
			else if (!strcmp(astr, "internal"))
				{ MCC_TRACE("br\n"); ad->a.visibility = STV_INTERNAL; }
			else if (!strcmp(astr, "protected"))
				{ MCC_TRACE("br\n"); ad->a.visibility = STV_PROTECTED; }
			else
				{ MCC_TRACE("br\n"); expect("visibility(\"default|hidden|internal|protected\")"); }
			ad->a.visibility_set = 1;
			skip(')');
			break;
		case TOK_ALIGNED1:
		case TOK_ALIGNED2:
			if (tok == '(') { MCC_TRACE("br\n");
				next();
				n = expr_const();
				if (n <= 0 || (n & (n - 1)) != 0)
					{ MCC_TRACE("br\n"); mcc_error("alignment must be a positive power of two"); }
				skip(')');
			} else { MCC_TRACE("br\n");
				n = MCC_MAX_ALIGN;
			}
			ad->a.aligned = exact_log2p1(n);
			if (n != 1 << (ad->a.aligned - 1))
				{ MCC_TRACE("br\n"); mcc_error("alignment of %d is larger than implemented", n); }
			break;
		case TOK_PACKED1:
		case TOK_PACKED2:
			ad->a.packed = 1;
			break;
		case TOK_TRANSPARENT_UNION1:
		case TOK_TRANSPARENT_UNION2:
			ad->a.transp_union = 1;
			break;
		case TOK_WEAK1:
		case TOK_WEAK2:
			ad->a.weak = 1;
			break;
		case TOK_NODEBUG1:
		case TOK_NODEBUG2:
			ad->a.nodebug = 1;
			break;
		case TOK_USED1:
		case TOK_USED2:
		case TOK_UNUSED1:
		case TOK_UNUSED2:
			break;
		case TOK_CONST1:
		case TOK_CONST2:
		case TOK_CONST3:
		case TOK_PURE1:
		case TOK_PURE2:
			break;
		case TOK_NOINLINE:
		case TOK_NOINLINE1:
			ad->f.func_noinl = 1;
			break;
		case TOK_FORMAT1:
		case TOK_FORMAT2:
			goto skip_param;
		case TOK_NORETURN1:
		case TOK_NORETURN2:
			ad->f.func_noreturn = 1;
			break;
		case TOK_CDECL1:
		case TOK_CDECL2:
		case TOK_CDECL3:
			ad->f.func_call = FUNC_CDECL;
			break;
		case TOK_STDCALL1:
		case TOK_STDCALL2:
		case TOK_STDCALL3:
			ad->f.func_call = FUNC_STDCALL;
			break;
#ifdef MCC_TARGET_I386
		case TOK_REGPARM1:
		case TOK_REGPARM2:
			skip('(');
			n = expr_const();
			if (n > 3)
				{ MCC_TRACE("br\n"); n = 3; }
			else if (n < 0)
				{ MCC_TRACE("br\n"); n = 0; }
			if (n > 0)
				{ MCC_TRACE("br\n"); ad->f.func_call = FUNC_FASTCALL1 + n - 1; }
			skip(')');
			break;
		case TOK_FASTCALL1:
		case TOK_FASTCALL2:
		case TOK_FASTCALL3:
			ad->f.func_call = FUNC_FASTCALLW;
			break;
		case TOK_THISCALL1:
		case TOK_THISCALL2:
		case TOK_THISCALL3:
			ad->f.func_call = FUNC_THISCALL;
			break;
#endif
		case TOK_MODE:
			skip('(');
			switch (tok) { MCC_TRACE("br\n");
			case TOK_MODE_DI:
				ad->attr_mode = VT_LLONG + 1;
				break;
			case TOK_MODE_QI:
				ad->attr_mode = VT_BYTE + 1;
				break;
			case TOK_MODE_HI:
				ad->attr_mode = VT_SHORT + 1;
				break;
			case TOK_MODE_SI:
			case TOK_MODE_word:
				ad->attr_mode = VT_INT + 1;
				break;
			case TOK_MODE_SF:
				ad->attr_mode = VT_FLOAT + 1;
				break;
			case TOK_MODE_DF:
				ad->attr_mode = VT_DOUBLE + 1;
				break;
			case TOK_MODE_TI:
				mcc_error("__mode__(TI) requires 128-bit integers, "
									"which mcc does not support");
				break;
			default:
				mcc_warning("__mode__(%s) not supported\n", get_tok_str(tok, NULL));
				break;
			}
			next();
			skip(')');
			break;
		case TOK_DLLEXPORT:
			ad->a.dllexport = 1;
			break;
		case TOK_NODECORATE:
			ad->a.nodecorate = 1;
			break;
		case TOK_DLLIMPORT:
			ad->a.dllimport = 1;
			break;
		default:
			mcc_warning_c(warn_unsupported)("'%s' attribute ignored", get_tok_str(t, NULL));
		skip_param:
			if (tok == '(') { MCC_TRACE("br\n");
				int parenthesis = 0;
				do { MCC_TRACE("br\n");
					if (tok == '(')
						{ MCC_TRACE("br\n"); parenthesis++; }
					else if (tok == ')')
						{ MCC_TRACE("br\n"); parenthesis--; }
					next();
				} while (parenthesis && tok != -1);
			}
			break;
		}
		if (tok != ',')
			{ MCC_TRACE("br\n"); break; }
		next();
	}
	skip(')');
	skip(')');
	goto redo;
}

static Sym *find_field(CType *type, int v, int *cumofs) { MCC_TRACE("enter\n");
	Sym *s = type->ref;
	int v1 = v | SYM_FIELD;
	if (!(v & SYM_FIELD)) { MCC_TRACE("br\n");
		if ((type->t & VT_BTYPE) != VT_STRUCT)
			{ MCC_TRACE("br\n"); expect("struct or union"); }
		if (v < TOK_UIDENT)
			{ MCC_TRACE("br\n"); expect("field name"); }
		if (s->c < 0)
			{ MCC_TRACE("br\n"); mcc_error("dereferencing incomplete type '%s'",
								get_tok_str(s->v & ~SYM_STRUCT, 0)); }
	}
	while ((s = s->next) != NULL) { MCC_TRACE("br\n");
		if (s->v == v1) { MCC_TRACE("br\n");
			*cumofs = s->c;
			return s;
		}
		if ((s->type.t & VT_BTYPE) == VT_STRUCT && s->v >= (SYM_FIRST_ANOM | SYM_FIELD)) { MCC_TRACE("br\n");
			Sym *ret = find_field(&s->type, v1, cumofs);
			if (ret) { MCC_TRACE("br\n");
				*cumofs += s->c;
				return ret;
			}
		}
	}
	if (!(v & SYM_FIELD))
		{ MCC_TRACE("br\n"); mcc_error("field not found: %s", get_tok_str(v, NULL)); }
	return s;
}

static void check_fields(CType *type, int check) { MCC_TRACE("enter\n");
	Sym *s = type->ref;

	while ((s = s->next) != NULL) { MCC_TRACE("br\n");
		int v = s->v & ~SYM_FIELD;
		if (v < SYM_FIRST_ANOM) { MCC_TRACE("br\n");
			TokenSym *ts = table_ident[v - TOK_IDENT];
			if (check && (ts->tok & SYM_FIELD))
				{ MCC_TRACE("br\n"); mcc_error("duplicate member '%s'", get_tok_str(v, NULL)); }
			ts->tok ^= SYM_FIELD;
		} else if ((s->type.t & VT_BTYPE) == VT_STRUCT)
			{ MCC_TRACE("br\n"); check_fields(&s->type, check); }
	}
}

static void struct_layout(CType *type, AttributeDef *ad) { MCC_TRACE("enter\n");
	int size, align, maxalign, offset, c, bit_pos, bit_size;
	int packed, a, bt, prevbt, prev_bit_size;
	int pcc = !mcc_state->ms_bitfields;
	int pragma_pack = *mcc_state->pack_stack_ptr;
	Sym *f;

	maxalign = 1;
	offset = 0;
	c = 0;
	bit_pos = 0;
	prevbt = VT_STRUCT;
	prev_bit_size = 0;

	for (f = type->ref->next; f; f = f->next) { MCC_TRACE("br\n");
		if (f->type.t & VT_BITFIELD)
			{ MCC_TRACE("br\n"); bit_size = BIT_SIZE(f->type.t); }
		else
			{ MCC_TRACE("br\n"); bit_size = -1; }
		size = type_size(&f->type, &align);
		a = f->a.aligned ? 1 << (f->a.aligned - 1) : 0;
		packed = 0;

		if (pcc && bit_size == 0) { MCC_TRACE("br\n");
		} else { MCC_TRACE("br\n");
			if (pcc && (f->a.packed || ad->a.packed))
				{ MCC_TRACE("br\n"); align = packed = 1; }

			if (pragma_pack) { MCC_TRACE("br\n");
				packed = 1;
				if (pragma_pack < align)
					{ MCC_TRACE("br\n"); align = pragma_pack; }
				if (pcc && pragma_pack < a)
					{ MCC_TRACE("br\n"); a = 0; }
			}
		}
		if (a)
			{ MCC_TRACE("br\n"); align = a; }

		if (type->ref->type.t == VT_UNION) { MCC_TRACE("br\n");
			if (pcc && bit_size >= 0)
				{ MCC_TRACE("br\n"); size = (bit_size + 7) >> 3; }
			offset = 0;
			if (size > c)
				{ MCC_TRACE("br\n"); c = size; }
		} else if (bit_size < 0) { MCC_TRACE("br\n");
			if (pcc)
				{ MCC_TRACE("br\n"); c += (bit_pos + 7) >> 3; }
			c = (c + align - 1) & -align;
			offset = c;
			if (size > 0)
				{ MCC_TRACE("br\n"); c += size; }
			bit_pos = 0;
			prevbt = VT_STRUCT;
			prev_bit_size = 0;
		} else { MCC_TRACE("br\n");
			if (pcc) { MCC_TRACE("br\n");
				if (bit_size == 0) { MCC_TRACE("br\n");
				new_field:
					c = (c + ((bit_pos + 7) >> 3) + align - 1) & -align;
					bit_pos = 0;
				} else if (f->a.aligned) { MCC_TRACE("br\n");
					goto new_field;
				} else if (!packed) { MCC_TRACE("br\n");
					int a8 = align * 8;
					int ofs = ((c * 8 + bit_pos) % a8 + bit_size + a8 - 1) / a8;
					if (ofs > size / align)
						{ MCC_TRACE("br\n"); goto new_field; }
				}

				if (size == 8 && bit_size <= 32)
					{ MCC_TRACE("br\n"); f->type.t = (f->type.t & ~VT_BTYPE) | VT_INT, size = 4; }

				while (bit_pos >= align * 8)
					{ MCC_TRACE("br\n"); c += align, bit_pos -= align * 8; }
				offset = c;

				if (f->v & SYM_FIRST_ANOM)
					{ MCC_TRACE("br\n"); align = 1; }
			} else { MCC_TRACE("br\n");
				bt = f->type.t & VT_BTYPE;
				if ((bit_pos + bit_size > size * 8) || (bit_size > 0) == (bt != prevbt)) { MCC_TRACE("br\n");
					c = (c + align - 1) & -align;
					offset = c;
					bit_pos = 0;
					if (bit_size || prev_bit_size)
						{ MCC_TRACE("br\n"); c += size; }
				}
				if (bit_size == 0 && prevbt != bt)
					{ MCC_TRACE("br\n"); align = 1; }
				prevbt = bt;
				prev_bit_size = bit_size;
			}

			f->type.t = (f->type.t & ~(0x3f << VT_STRUCT_SHIFT)) | (bit_pos << VT_STRUCT_SHIFT);
			bit_pos += bit_size;
		}
		if (align > maxalign)
			{ MCC_TRACE("br\n"); maxalign = align; }

		if (g_debug & MCC_DBG_STRUCT) { MCC_TRACE("br\n");
			printf("set field %s offset %-2d size %-2d align %-2d",
						 get_tok_str(f->v & ~SYM_FIELD, NULL), offset, size, align);
			if (f->type.t & VT_BITFIELD) { MCC_TRACE("br\n");
				printf(" pos %-2d bits %-2d",
							 BIT_POS(f->type.t),
							 BIT_SIZE(f->type.t));
			}
			printf("\n");
		}

		f->c = offset;
		f->r = 0;
	}

	if (pcc)
		{ MCC_TRACE("br\n"); c += (bit_pos + 7) >> 3; }

	a = bt = ad->a.aligned ? 1 << (ad->a.aligned - 1) : 1;
	if (a < maxalign)
		{ MCC_TRACE("br\n"); a = maxalign; }
	type->ref->r = a;
	if (pragma_pack && pragma_pack < maxalign && 0 == pcc) { MCC_TRACE("br\n");
		a = pragma_pack;
		if (a < bt)
			{ MCC_TRACE("br\n"); a = bt; }
	}
	c = (c + a - 1) & -a;
	type->ref->c = c;

	if (ad->a.transp_union) { MCC_TRACE("br\n");
		if (!IS_UNION(type->t))
			{ MCC_TRACE("br\n"); mcc_warning("'transparent_union' attribute ignored on non-union"); }
		else
			{ MCC_TRACE("br\n"); type->ref->a.transp_union = 1; }
	}

	if (g_debug & MCC_DBG_STRUCT)
		{ MCC_TRACE("br\n"); printf("struct size %-2d align %-2d\n\n", c, a), fflush(stdout); }

	for (f = type->ref->next; f; f = f->next) { MCC_TRACE("br\n");
		int s, px, cx, c0;
		CType t;

		if (0 == (f->type.t & VT_BITFIELD))
			{ MCC_TRACE("br\n"); continue; }
		f->type.ref = f;
		f->auxtype = -1;
		bit_size = BIT_SIZE(f->type.t);
		if (bit_size == 0)
			{ MCC_TRACE("br\n"); continue; }
		bit_pos = BIT_POS(f->type.t);
		size = type_size(&f->type, &align);

		if (bit_pos + bit_size <= size * 8 && f->c + size <= c
#ifdef MCC_TARGET_ARM
				&& !(f->c & (align - 1))
#endif
		)
			continue;

		c0 = -1, s = align = 1;
		t.t = VT_BYTE;
		for (;;) { MCC_TRACE("br\n");
			px = f->c * 8 + bit_pos;
			cx = (px >> 3) & -align;
			px = px - (cx << 3);
			if (c0 == cx)
				{ MCC_TRACE("br\n"); break; }
			s = (px + bit_size + 7) >> 3;
			if (s > 4) { MCC_TRACE("br\n");
				t.t = VT_LLONG;
			} else if (s > 2) { MCC_TRACE("br\n");
				t.t = VT_INT;
			} else if (s > 1) { MCC_TRACE("br\n");
				t.t = VT_SHORT;
			} else { MCC_TRACE("br\n");
				t.t = VT_BYTE;
			}
			s = type_size(&t, &align);
			c0 = cx;
		}

		if (px + bit_size <= s * 8 && cx + s <= c
#ifdef MCC_TARGET_ARM
				&& !(cx & (align - 1))
#endif
		) { MCC_TRACE("br\n");
			f->c = cx;
			bit_pos = px;
			f->type.t = (f->type.t & ~(0x3f << VT_STRUCT_SHIFT)) | (bit_pos << VT_STRUCT_SHIFT);
			if (s != size)
				{ MCC_TRACE("br\n"); f->auxtype = t.t; }
			if (g_debug & MCC_DBG_STRUCT)
				{ MCC_TRACE("br\n"); printf("FIX field %s offset %-2d size %-2d align %-2d "
							 "pos %-2d bits %-2d\n",
							 get_tok_str(f->v & ~SYM_FIELD, NULL),
							 cx, s, align, px, bit_size); }
		} else { MCC_TRACE("br\n");
			f->auxtype = VT_STRUCT;
			if (g_debug & MCC_DBG_STRUCT)
				{ MCC_TRACE("br\n"); printf("FIX field %s : load byte-wise\n",
							 get_tok_str(f->v & ~SYM_FIELD, NULL)); }
		}
	}
}

static int in_range(long long n, int t) { MCC_TRACE("enter\n");
	unsigned long long m = (1ULL << (btype_size(t & VT_BTYPE) * 8 - 1)) - 1;
	if (t & VT_UNSIGNED)
		{ MCC_TRACE("br\n"); return n <= (m << 1) + 1; }
	return n >= -(long long)m - 1 && n <= (long long)m;
}

static void struct_decl(CType *type, int u) { MCC_TRACE("enter\n");
	int v, c, size, align, flexible;
	int bit_size, bsize, bt, ut;
	Sym *s, *ss, **ps;
	AttributeDef ad, ad1;
	CType type1, btype;

	CST_OPEN(u == VT_ENUM ? CST_Enum : CST_StructOrUnion);
	memset(&ad, 0, sizeof ad);
	next();
	parse_attribute(&ad);

	v = 0;
	if (tok >= TOK_IDENT)
		{ MCC_TRACE("br\n"); v = tok, next(); }

	bt = ut = 0;
	if (u == VT_ENUM) { MCC_TRACE("br\n");
		ut = VT_INT;
		if (tok == ':') { MCC_TRACE("br\n");
			next();
			if (!parse_btype(&btype, &ad1, 0) || !is_integer_btype(btype.t & VT_BTYPE))
				{ MCC_TRACE("br\n"); expect("enum type"); }
			bt = ut = btype.t & (VT_BTYPE | VT_LONG | VT_UNSIGNED | VT_DEFSIGN);
		}
	}

	if (v) { MCC_TRACE("br\n");
		s = struct_find(v);
		if (s && (s->sym_scope == local_scope || (tok != '{' && tok != ';'))) { MCC_TRACE("br\n");
			if (u == s->type.t)
				{ MCC_TRACE("br\n"); goto do_decl; }
			if (u == VT_ENUM && IS_ENUM(s->type.t))
				{ MCC_TRACE("br\n"); goto do_decl; }
			mcc_error("redeclaration of '%s'", get_tok_str(v, NULL));
		}
	} else { MCC_TRACE("br\n");
		if (tok != '{')
			{ MCC_TRACE("br\n"); expect("struct/union/enum name"); }
		v = anon_sym++;
	}
	type1.t = u | ut;
	type1.ref = NULL;
	s = sym_push(v | SYM_STRUCT, &type1, 0, bt ? 0 : -1);
	s->r = 0;
do_decl:
	type->t = s->type.t;
	type->ref = s;

	if (u == VT_ENUM && tok != '{' && s->c == -1)
		{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids forward references to 'enum' types"); }

	if (tok == '{') { MCC_TRACE("br\n");
		next();
		if (s->c != -1 && !(u == VT_ENUM && s->c == 0))
			{ MCC_TRACE("br\n"); mcc_error("struct/union/enum already defined"); }
		s->c = -2;
		ps = &s->next;
		if (u == VT_ENUM) { MCC_TRACE("br\n");
			long long ll = 0, pl = 0, nl = 0;
			CType t;
			t.ref = s;
			s->sym_scope = local_scope;
			t.t = VT_INT | VT_STATIC | VT_ENUM_VAL;
			if (bt)
				{ MCC_TRACE("br\n"); t.t = bt | VT_STATIC | VT_ENUM_VAL; }
			for (;;) { MCC_TRACE("br\n");
				v = tok;
				if (v < TOK_UIDENT)
					{ MCC_TRACE("br\n"); expect("identifier"); }
				next();
				if (tok == '=') { MCC_TRACE("br\n");
					next();
					ice_float_op = ice_nonconst = 0;
					ll = expr_const64();
					if (ice_float_op || ice_nonconst)
						{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids an enumerator value that is "
												 "not an integer constant expression"); }
				}
				if (bt && !in_range(ll, t.t))
					{ MCC_TRACE("br\n"); mcc_error("enumerator '%s' out of range of its type",
										get_tok_str(v, NULL)); }
				if (!bt && ll != (int64_t)(int)ll)
					{ MCC_TRACE("br\n"); mcc_pedantic("ISO C restricts enumerator values "
											 "to the range of 'int'"); }
				ss = sym_push(v, &t, VT_CONST, 0);
				ss->enum_val = ll;
				*ps = ss, ps = &ss->next;
				if (ll < nl)
					{ MCC_TRACE("br\n"); nl = ll; }
				if (ll > pl)
					{ MCC_TRACE("br\n"); pl = ll; }
				if (tok != ',')
					{ MCC_TRACE("br\n"); break; }
				next();
				ll++;
				if (tok == '}') { MCC_TRACE("br\n");
					if (mcc_state->cversion < 199901)
						{ MCC_TRACE("br\n"); mcc_pedantic("trailing comma in enumerator list is a "
												 "C99 feature"); }
					break;
				}
			}
			skip('}');

			if (bt) { MCC_TRACE("br\n");
				t.t = bt;
				s->c = 2;
				goto enum_done;
			}

			t.t = VT_INT;
			if (nl >= 0) { MCC_TRACE("br\n");
				if (pl != (unsigned)pl)
					{ MCC_TRACE("br\n"); t.t = (LONG_SIZE == 8 ? VT_LLONG | VT_LONG : VT_LLONG); }
				t.t |= VT_UNSIGNED;
			} else if (pl != (int)pl || nl != (int)nl)
				{ MCC_TRACE("br\n"); t.t = (LONG_SIZE == 8 ? VT_LLONG | VT_LONG : VT_LLONG); }

			if (mcc_state->short_enums && (t.t & VT_BTYPE) == VT_INT) { MCC_TRACE("br\n");
				if (t.t & VT_UNSIGNED) { MCC_TRACE("br\n");
					if (pl <= 0xff)
						{ MCC_TRACE("br\n"); t.t = VT_BYTE | VT_UNSIGNED; }
					else if (pl <= 0xffff)
						{ MCC_TRACE("br\n"); t.t = VT_SHORT | VT_UNSIGNED; }
				} else { MCC_TRACE("br\n");
					if (nl >= -0x80 && pl <= 0x7f)
						{ MCC_TRACE("br\n"); t.t = VT_BYTE; }
					else if (nl >= -0x8000 && pl <= 0x7fff)
						{ MCC_TRACE("br\n"); t.t = VT_SHORT; }
				}
			}

			for (ss = s->next; ss; ss = ss->next) { MCC_TRACE("br\n");
				ll = ss->enum_val;
				if (ll == (int)ll)
					{ MCC_TRACE("br\n"); continue; }
				if (t.t & VT_UNSIGNED) { MCC_TRACE("br\n");
					ss->type.t |= VT_UNSIGNED;
					if (ll == (unsigned)ll)
						{ MCC_TRACE("br\n"); continue; }
				}
				ss->type.t = (ss->type.t & ~VT_BTYPE) | (LONG_SIZE == 8 ? VT_LLONG | VT_LONG : VT_LLONG);
			}
			s->c = 1;
		enum_done:
			s->type.t = type->t = t.t | VT_ENUM;
		} else { MCC_TRACE("br\n");
			c = 0;
			flexible = 0;
			while (tok != '}') { MCC_TRACE("br\n");
				if (!parse_btype(&btype, &ad1, 0)) { MCC_TRACE("br\n");
					if (tok == TOK_STATIC_ASSERT) { MCC_TRACE("br\n");
						do_Static_assert();
						continue;
					}
					skip(';');
					continue;
				}
				while (1) { MCC_TRACE("br\n");
					if (flexible)
						{ MCC_TRACE("br\n"); mcc_error("flexible array member '%s' not at the end of struct",
											get_tok_str(v, NULL)); }
					bit_size = -1;
					v = 0;
					type1 = btype;
					if (tok != ':') { MCC_TRACE("br\n");
						if (tok != ';')
							{ MCC_TRACE("br\n"); type_decl(&type1, &ad1, &v, TYPE_DIRECT); }
						if (v == 0) { MCC_TRACE("br\n");
							if ((type1.t & VT_BTYPE) != VT_STRUCT) { MCC_TRACE("br\n");
								if (tok == ';')
									{ MCC_TRACE("br\n"); mcc_warning("declaration does not declare anything"); }
								else
									{ MCC_TRACE("br\n"); expect("identifier"); }
							} else { MCC_TRACE("br\n");
								int v = btype.ref->v;
								if ((v & ~SYM_STRUCT) < SYM_FIRST_ANOM) { MCC_TRACE("br\n");
									if (mcc_state->ms_extensions == 0)
										{ MCC_TRACE("br\n"); mcc_warning("declaration does not "
																"declare anything"); }
								} else if (mcc_state->cversion < 201112)
									{ MCC_TRACE("br\n"); mcc_pedantic("anonymous structs/unions are a "
															 "C11 feature"); }
							}
						}
						if (type_size(&type1, &align) < 0) { MCC_TRACE("br\n");
							if ((u == VT_STRUCT) && (type1.t & VT_ARRAY)) { MCC_TRACE("br\n");
								flexible = 1;
								if (mcc_state->cversion < 199901)
									{ MCC_TRACE("br\n"); mcc_pedantic("flexible array members are a "
															 "C99 feature"); }
								if (!c)
									{ MCC_TRACE("br\n"); mcc_pedantic("flexible array member in a "
															 "struct with no named members"); }
							} else
								{ MCC_TRACE("br\n"); mcc_error("field '%s' has incomplete type",
													get_tok_str(v, NULL)); }
						}
						if ((type1.t & VT_BTYPE) == VT_FUNC ||
								(type1.t & VT_BTYPE) == VT_VOID ||
								(type1.t & VT_STORAGE))
							{ MCC_TRACE("br\n"); mcc_error("invalid type for '%s'",
												get_tok_str(v, NULL)); }
						if (type1.t & VT_VLA)
							{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids a member with a "
													 "variably modified type"); }
						if (struct_has_flexible_member(&type1))
							{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids a member that is a "
													 "structure with a flexible array member"); }
					}
					if (tok == ':') { MCC_TRACE("br\n");
						next();
						ice_float_op = ice_nonconst = 0;
						bit_size = expr_const();
						if (ice_float_op || ice_nonconst)
							{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids a bit-field width that is "
													 "not an integer constant expression"); }
						if (bit_size < 0)
							{ MCC_TRACE("br\n"); mcc_error("negative width in bit-field '%s'",
												get_tok_str(v, NULL)); }
						if (v && bit_size == 0)
							{ MCC_TRACE("br\n"); mcc_error("zero width for bit-field '%s'",
												get_tok_str(v, NULL)); }
						parse_attribute(&ad1);
					}
					if ((ad1.storage_class & 8) && bit_size >= 0)
						{ MCC_TRACE("br\n"); mcc_error("'_Alignas' specified for a bit-field"); }
					size = type_size(&type1, &align);
					if (bit_size >= 0) { MCC_TRACE("br\n");
						bt = type1.t & VT_BTYPE;
						if (bt != VT_INT &&
								bt != VT_BYTE &&
								bt != VT_SHORT &&
								bt != VT_BOOL &&
								bt != VT_LLONG)
							{ MCC_TRACE("br\n"); mcc_error("bitfields must have scalar type"); }
						bsize = (bt == VT_BOOL) ? 1 : size * 8;
						if (bit_size > bsize) { MCC_TRACE("br\n");
							mcc_error("width of '%s' exceeds its type",
												get_tok_str(v, NULL));
						} else if (bit_size == bsize && bt != VT_BOOL && !*mcc_state->pack_stack_ptr && !ad.a.packed && !ad1.a.packed) { MCC_TRACE("br\n");
							;
						} else if (bit_size == 64) { MCC_TRACE("br\n");
							;
						} else { MCC_TRACE("br\n");
							type1.t = (type1.t & ~VT_STRUCT_MASK) | VT_BITFIELD | ((unsigned)bit_size << (VT_STRUCT_SHIFT + 6));
						}
					}
					if (v != 0 || (type1.t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
						c = 1;
					}
					if (v == 0 &&
							((type1.t & VT_BTYPE) == VT_STRUCT ||
							 bit_size >= 0)) { MCC_TRACE("br\n");
						v = anon_sym++;
					}
					if (v) { MCC_TRACE("br\n");
						ss = sym_push(v | SYM_FIELD, &type1, 0, 0);
						ss->a = ad1.a;
						*ps = ss;
						ps = &ss->next;
					}
					if (tok == ';' || tok == TOK_EOF)
						{ MCC_TRACE("br\n"); break; }
					skip(',');
				}
				skip(';');
			}
			skip('}');
			if (!c)
				{ MCC_TRACE("br\n"); mcc_pedantic(u == VT_UNION
												 ? "ISO C forbids a union with no named members"
												 : "ISO C forbids a struct with no named members"); }
			parse_attribute(&ad);
			if (ad.cleanup_func) { MCC_TRACE("br\n");
				mcc_warning("attribute '__cleanup__' ignored on type");
			}
			check_fields(type, 1);
			check_fields(type, 0);
			struct_layout(type, &ad);
		}
		if (debug_modes)
			{ MCC_TRACE("br\n"); mcc_debug_fix_forw(mcc_state, type); }
	}
	CST_CLOSE();
}

static void sym_to_attr(AttributeDef *ad, Sym *s) { MCC_TRACE("enter\n");
	merge_symattr(&ad->a, &s->a);
	merge_funcattr(&ad->f, &s->f);
}

static void parse_btype_qualify(CType *type, int qualifiers) { MCC_TRACE("enter\n");
	while (type->t & VT_ARRAY) { MCC_TRACE("br\n");
		type->ref = sym_push(SYM_FIELD, &type->ref->type, 0, type->ref->c);
		type = &type->ref->type;
	}
	type->t |= qualifiers;
}

static void mk_complex_type(CType *type, CType *base) { MCC_TRACE("enter\n");
	CType *cache = mcc_state->gen_complex_type_cache;
	int idx, bt = base->t & VT_BTYPE;
	Sym *s, *f0, *f1;
	AttributeDef ad;

	idx = bt == VT_FLOAT
						? 0
				: bt == VT_DOUBLE
						? ((base->t & VT_LONG) ? 3 : 1)
						: 2;
	if (cache[idx].ref) { MCC_TRACE("br\n");
		*type = cache[idx];
		return;
	}
	if (!mcc_state->gen_complex_re_tok) { MCC_TRACE("br\n");
		mcc_state->gen_complex_re_tok = tok_alloc_const("__real");
		mcc_state->gen_complex_im_tok = tok_alloc_const("__imag");
	}
	s = sym_push2(&global_stack, anon_sym++ | SYM_STRUCT, VT_STRUCT, -1);
	s->r = 0;
	s->a.is_complex = 1;
	f0 = sym_push2(&global_stack, mcc_state->gen_complex_re_tok | SYM_FIELD, base->t, 0);
	f0->type.ref = base->ref;
	f1 = sym_push2(&global_stack, mcc_state->gen_complex_im_tok | SYM_FIELD, base->t, 0);
	f1->type.ref = base->ref;
	s->next = f0, f0->next = f1, f1->next = NULL;
	type->t = VT_STRUCT;
	type->ref = s;
	memset(&ad, 0, sizeof ad);
	struct_layout(type, &ad);
	cache[idx] = *type;
}

static void complex_part(int imag) { MCC_TRACE("enter\n");
	Sym *fre = vtop->type.ref->next;
	CType base = fre->type;
	int ofs = imag ? fre->next->c : fre->c;

#if MCC_CONFIG_OPTIMIZER
	ast_hook_member_begin(0);
#endif
	test_lvalue();
	gaddrof();
	vtop->type = char_pointer_type;
	vpushi(ofs);
	gen_op('+');
	vtop->type = base;
	vtop->r |= VT_LVAL;
#if MCC_CONFIG_OPTIMIZER
	ast_hook_member_end(ofs, &base, 0, 0, 0);
#endif
}

static void cplx_local(CType *cplx, SValue *out) { MCC_TRACE("enter\n");
	int align, size = type_size(cplx, &align);
#if MCC_CONFIG_OPTIMIZER
	loc = ast_alloc_loc(size, align);
#else
	loc = (loc - size) & -align;
#endif
	out->type = *cplx;
	out->r = VT_LOCAL | VT_LVAL;
	out->r2 = VT_CONST;
	out->c.i = loc;
	out->sym = NULL;
}

static void cplx_push_part(SValue *sv, int imag) { MCC_TRACE("enter\n");
	vpushv(sv);
	complex_part(imag);
	gv(MCC_RC_TYPE(vtop->type.t));
}

static void cplx_store_part(SValue *dst, int imag) { MCC_TRACE("enter\n");
	vpushv(dst);
	complex_part(imag);
	vswap();
	vstore();
	vpop();
}

static void gen_imaginary_complex(int t) { MCC_TRACE("enter\n");
	CType cbase, ccplx;
	SValue r;
	cbase.t = t & (VT_BTYPE | VT_LONG);
	cbase.ref = NULL;
	mk_complex_type(&ccplx, &cbase);
	cplx_local(&ccplx, &r);
	cplx_store_part(&r, 1);
	vpushi(0);
	gen_cast(&cbase);
	cplx_store_part(&r, 0);
	vpushv(&r);
}

static void cplx_materialize(CType *cplx, CType *base, SValue *out) { MCC_TRACE("enter\n");
	cplx_local(cplx, out);
	if (is_complex_type(&vtop->type)) { MCC_TRACE("br\n");
		vpushv(out);
		vswap();
		vstore();
		vpop();
	} else { MCC_TRACE("br\n");
		gen_cast(base);
		cplx_store_part(out, 0);
		vpushi(0);
		gen_cast(base);
		cplx_store_part(out, 1);
	}
}

static void gen_complex_call(int op, CType *cplx, CType *base, SValue *a, SValue *b) { MCC_TRACE("enter\n");
	char buf[16];
	int bt = base->t & VT_BTYPE;
	char suf = bt == VT_FLOAT
								 ? 'f'
						 : bt == VT_LDOUBLE
								 ? 'l'
								 : 0;
	int idx = bt == VT_FLOAT ? 0 : bt == VT_DOUBLE ? ((base->t & VT_LONG) ? 3 : 1)
																								 : 2;
	Sym *fsym, *p, *prev;
	CType functype, ptype;
	SValue r;
	int i;

	cplx_local(cplx, &r);

	if (suf)
		{ MCC_TRACE("br\n"); snprintf(buf, sizeof buf, "__mcc_c%s%c", op == '*' ? "mul" : "div", suf); }
	else
		{ MCC_TRACE("br\n"); snprintf(buf, sizeof buf, "__mcc_c%s", op == '*' ? "mul" : "div"); }

	fsym = mcc_state->gen_complex_call_ftype[idx];
	if (!fsym) { MCC_TRACE("br\n");
		ptype = *base;
		mk_pointer(&ptype);
		fsym = sym_push2(&global_stack, SYM_FIELD, VT_VOID, 0);
		fsym->type.ref = NULL;
		fsym->f.func_call = FUNC_CDECL;
		fsym->f.func_type = FUNC_NEW;
		fsym->f.func_args = 5;
		prev = NULL;
		for (i = 0; i < 4; i++) { MCC_TRACE("br\n");
			p = sym_push2(&global_stack, SYM_FIELD, base->t, 0);
			p->type.ref = base->ref;
			p->next = prev;
			prev = p;
		}
		p = sym_push2(&global_stack, SYM_FIELD, ptype.t, 0);
		p->type.ref = ptype.ref;
		p->next = prev;
		fsym->next = p;
		mcc_state->gen_complex_call_ftype[idx] = fsym;
	}
	functype.t = VT_FUNC;
	functype.ref = fsym;

	vpushsym(&functype, external_global_sym(tok_alloc_const(buf), &functype));
	vpushv(&r);
	mk_pointer(&vtop->type);
	gaddrof();
	cplx_push_part(a, 0);
	cplx_push_part(a, 1);
	cplx_push_part(b, 0);
	cplx_push_part(b, 1);
	gfunc_call(5);

	vpushv(&r);
}

static int cplx_extract_const(SValue *sv, SValue *re, SValue *im) { MCC_TRACE("enter\n");
	if (is_complex_type(&sv->type)) { MCC_TRACE("br\n");
		CType sb = sv->type.ref->next->type;
		int sbt = sb.t & VT_BTYPE, bsz, al;
		Section *ssec;
		ElfSym *esym;
		unsigned char *p;
		if (sbt != VT_FLOAT && sbt != VT_DOUBLE)
			{ MCC_TRACE("br\n"); return 0; }
		if ((sv->r & (VT_SYM | VT_CONST | VT_LVAL)) != (VT_SYM | VT_CONST | VT_LVAL) || !sv->sym || sv->sym->v < SYM_FIRST_ANOM)
			{ MCC_TRACE("br\n"); return 0; }
		esym = elfsym(sv->sym);
		if (!esym || esym->st_shndx == SHN_UNDEF)
			{ MCC_TRACE("br\n"); return 0; }
		ssec = mcc_state->sections[esym->st_shndx];
		if (!ssec || !ssec->data || ssec->reloc)
			{ MCC_TRACE("br\n"); return 0; }
		bsz = type_size(&sb, &al);
		p = ssec->data + esym->st_value + (unsigned)sv->c.i;
		memset(re, 0, sizeof *re);
		memset(im, 0, sizeof *im);
		re->type = im->type = sb;
		re->r = im->r = VT_CONST;
		if (sbt == VT_FLOAT) { MCC_TRACE("br\n");
			float fr, fi;
			memcpy(&fr, p, 4);
			memcpy(&fi, p + bsz, 4);
			re->c.f = fr;
			im->c.f = fi;
		} else { MCC_TRACE("br\n");
			double dr, di;
			memcpy(&dr, p, 8);
			memcpy(&di, p + bsz, 8);
			re->c.d = dr;
			im->c.d = di;
		}
		return 1;
	}
	if ((sv->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST && (is_float(sv->type.t) || is_integer_btype(
																																														sv->type.t & VT_BTYPE))) { MCC_TRACE("br\n");
		*re = *sv;
		memset(im, 0, sizeof *im);
		im->type = sv->type;
		im->r = VT_CONST;
		return 1;
	}
	return 0;
}

static void cplx_push_cst(SValue *v, CType *base) { MCC_TRACE("enter\n");
	vpushv(v);
	gen_cast(base);
}

static int float_rank(int t) { MCC_TRACE("enter\n");
	int bt = t & VT_BTYPE;
	if (bt == VT_LDOUBLE)
		{ MCC_TRACE("br\n"); return 3; }
	if (bt == VT_DOUBLE)
		{ MCC_TRACE("br\n"); return (t & VT_LONG) ? 3 : 2; }
	if (bt == VT_FLOAT)
		{ MCC_TRACE("br\n"); return 1; }
	return 0;
}

static void gen_complex_op(int op) { MCC_TRACE("enter\n");
	SValue a, b, r;
	CType cplx, base;

	cplx = is_complex_type(&vtop[-1].type) ? vtop[-1].type : vtop[0].type;
	base = cplx.ref->next->type;

	{
		CType *r0 = NULL, *r1 = NULL, *wb = NULL;
		int k0, k1, kc;
		if (is_complex_type(&vtop[-1].type))
			{ MCC_TRACE("br\n"); r0 = &vtop[-1].type.ref->next->type; }
		else if (is_float(vtop[-1].type.t))
			{ MCC_TRACE("br\n"); r0 = &vtop[-1].type; }
		if (is_complex_type(&vtop[0].type))
			{ MCC_TRACE("br\n"); r1 = &vtop[0].type.ref->next->type; }
		else if (is_float(vtop[0].type.t))
			{ MCC_TRACE("br\n"); r1 = &vtop[0].type; }
		kc = float_rank(base.t);
		k0 = r0 ? float_rank(r0->t) : 0;
		k1 = r1 ? float_rank(r1->t) : 0;
		if (k0 >= k1 && k0 > kc)
			{ MCC_TRACE("br\n"); wb = r0; }
		else if (k1 > k0 && k1 > kc)
			{ MCC_TRACE("br\n"); wb = r1; }
		if (wb) { MCC_TRACE("br\n");
			mk_complex_type(&cplx, wb);
			base = cplx.ref->next->type;
		}
	}

	{
		SValue are, aim, bre, bim;
		int ebt = base.t & VT_BTYPE;
		if ((ebt == VT_FLOAT || ebt == VT_DOUBLE) && CONST_WANTED && (op == '+' || op == '-' || op == '*' || op == '/') && cplx_extract_const(&vtop[-1], &are, &aim) && cplx_extract_const(&vtop[0], &bre, &bim)) { MCC_TRACE("br\n");
			init_params pp = {.sec = rodata_section};
			unsigned long offset;
			int bsz, bal, csz, cal;
			bsz = type_size(&base, &bal);
			csz = type_size(&cplx, &cal);
			if (NODATA_WANTED) { MCC_TRACE("br\n");
				csz = 0;
				cal = 1;
			}
			offset = section_add(pp.sec, csz, cal);
			switch (op) { MCC_TRACE("br\n");
			case '+':
			case '-':
				cplx_push_cst(&are, &base);
				cplx_push_cst(&bre, &base);
				gen_op(op);
				break;
			case '*':
				cplx_push_cst(&are, &base);
				cplx_push_cst(&bre, &base);
				gen_op('*');
				cplx_push_cst(&aim, &base);
				cplx_push_cst(&bim, &base);
				gen_op('*');
				gen_op('-');
				break;
			default:
				cplx_push_cst(&are, &base);
				cplx_push_cst(&bre, &base);
				gen_op('*');
				cplx_push_cst(&aim, &base);
				cplx_push_cst(&bim, &base);
				gen_op('*');
				gen_op('+');
				cplx_push_cst(&bre, &base);
				cplx_push_cst(&bre, &base);
				gen_op('*');
				cplx_push_cst(&bim, &base);
				cplx_push_cst(&bim, &base);
				gen_op('*');
				gen_op('+');
				gen_op('/');
				break;
			}
			init_putv(&pp, &base, offset);
			switch (op) { MCC_TRACE("br\n");
			case '+':
			case '-':
				cplx_push_cst(&aim, &base);
				cplx_push_cst(&bim, &base);
				gen_op(op);
				break;
			case '*':
				cplx_push_cst(&are, &base);
				cplx_push_cst(&bim, &base);
				gen_op('*');
				cplx_push_cst(&aim, &base);
				cplx_push_cst(&bre, &base);
				gen_op('*');
				gen_op('+');
				break;
			default:
				cplx_push_cst(&aim, &base);
				cplx_push_cst(&bre, &base);
				gen_op('*');
				cplx_push_cst(&are, &base);
				cplx_push_cst(&bim, &base);
				gen_op('*');
				gen_op('-');
				cplx_push_cst(&bre, &base);
				cplx_push_cst(&bre, &base);
				gen_op('*');
				cplx_push_cst(&bim, &base);
				cplx_push_cst(&bim, &base);
				gen_op('*');
				gen_op('+');
				gen_op('/');
				break;
			}
			init_putv(&pp, &base, offset + bsz);
			vtop -= 2;
			vpush_ref(&cplx, pp.sec, offset, csz);
			vtop->r |= VT_LVAL;
			return;
		}
	}

	cplx_materialize(&cplx, &base, &b);
	cplx_materialize(&cplx, &base, &a);

	if (op == TOK_EQ || op == TOK_NE) { MCC_TRACE("br\n");
		cplx_push_part(&a, 0);
		cplx_push_part(&b, 0);
		gen_op(TOK_EQ);
		gv(MCC_RC_INT);
		cplx_push_part(&a, 1);
		cplx_push_part(&b, 1);
		gen_op(TOK_EQ);
		gen_op('&');
		if (op == TOK_NE) { MCC_TRACE("br\n");
			vpushi(0);
			gen_op(TOK_EQ);
		}
		return;
	}

	if ((op == '*' || op == '/') &&
			!(mcc_state->cx_limited_range || stdc_cx_limited(mcc_state))) { MCC_TRACE("br\n");
		gen_complex_call(op, &cplx, &base, &a, &b);
		return;
	}

	cplx_local(&cplx, &r);
	if (op == '+' || op == '-') { MCC_TRACE("br\n");
		for (int k = 0; k < 2; k++) { MCC_TRACE("br\n");
			cplx_push_part(&a, k);
			cplx_push_part(&b, k);
			gen_op(op);
			cplx_store_part(&r, k);
		}
	} else if (op == '*') { MCC_TRACE("br\n");
		cplx_push_part(&a, 0);
		cplx_push_part(&b, 0);
		gen_op('*');
		cplx_push_part(&a, 1);
		cplx_push_part(&b, 1);
		gen_op('*');
		gen_op('-');
		cplx_store_part(&r, 0);
		cplx_push_part(&a, 0);
		cplx_push_part(&b, 1);
		gen_op('*');
		cplx_push_part(&a, 1);
		cplx_push_part(&b, 0);
		gen_op('*');
		gen_op('+');
		cplx_store_part(&r, 1);
	} else if (op == '/') { MCC_TRACE("br\n");
		cplx_push_part(&a, 0);
		cplx_push_part(&b, 0);
		gen_op('*');
		cplx_push_part(&a, 1);
		cplx_push_part(&b, 1);
		gen_op('*');
		gen_op('+');
		cplx_push_part(&b, 0);
		cplx_push_part(&b, 0);
		gen_op('*');
		cplx_push_part(&b, 1);
		cplx_push_part(&b, 1);
		gen_op('*');
		gen_op('+');
		gen_op('/');
		cplx_store_part(&r, 0);
		cplx_push_part(&a, 1);
		cplx_push_part(&b, 0);
		gen_op('*');
		cplx_push_part(&a, 0);
		cplx_push_part(&b, 1);
		gen_op('*');
		gen_op('-');
		cplx_push_part(&b, 0);
		cplx_push_part(&b, 0);
		gen_op('*');
		cplx_push_part(&b, 1);
		cplx_push_part(&b, 1);
		gen_op('*');
		gen_op('+');
		gen_op('/');
		cplx_store_part(&r, 1);
	} else { MCC_TRACE("br\n");
		mcc_error("invalid operation on complex operands");
	}
	vpushv(&r);
}

static void gen_complex_cast(CType *dt) { MCC_TRACE("enter\n");
	SValue src, r;

	{
		SValue re, im;
		if (cplx_extract_const(vtop, &re, &im)) { MCC_TRACE("br\n");
			if (is_complex_type(dt)) { MCC_TRACE("br\n");
				CType dbase = dt->ref->next->type;
				init_params pp = {.sec = rodata_section};
				unsigned long offset;
				int bsz, bal, csz, cal;
				bsz = type_size(&dbase, &bal);
				csz = type_size(dt, &cal);
				if (NODATA_WANTED) { MCC_TRACE("br\n");
					csz = 0;
					cal = 1;
				}
#if MCC_CONFIG_OPTIMIZER
				int fc = ast_fconst_reuse();
				if (fc) { MCC_TRACE("br\n");
					vtop--;
					ast_fconst_push_ref(dt, fc);
				} else
#endif
				{ MCC_TRACE("br\n");
					offset = section_add(pp.sec, csz, cal);
					cplx_push_cst(&re, &dbase);
					init_putv(&pp, &dbase, offset);
					cplx_push_cst(&im, &dbase);
					init_putv(&pp, &dbase, offset + bsz);
					vtop--;
					vpush_ref(dt, pp.sec, offset, csz);
					vtop->r |= VT_LVAL;
#if MCC_CONFIG_OPTIMIZER
					ast_fconst_record(vtop->sym->c);
#endif
				}
			} else { MCC_TRACE("br\n");
				vtop--;
				cplx_push_cst(&re, dt);
			}
			return;
		}
	}

	if (is_complex_type(&vtop->type)) { MCC_TRACE("br\n");
		CType sbase = vtop->type.ref->next->type;
		cplx_materialize(&vtop->type, &sbase, &src);
		if (is_complex_type(dt)) { MCC_TRACE("br\n");
			CType dbase = dt->ref->next->type;
			cplx_local(dt, &r);
			cplx_push_part(&src, 0);
			gen_cast(&dbase);
			cplx_store_part(&r, 0);
			cplx_push_part(&src, 1);
			gen_cast(&dbase);
			cplx_store_part(&r, 1);
			vpushv(&r);
		} else { MCC_TRACE("br\n");
			vpushv(&src);
			complex_part(0);
			gen_cast(dt);
		}
	} else { MCC_TRACE("br\n");
		CType dbase = dt->ref->next->type;
		cplx_local(dt, &r);
		gen_cast(&dbase);
		cplx_store_part(&r, 0);
		vpushi(0);
		gen_cast(&dbase);
		cplx_store_part(&r, 1);
		vpushv(&r);
	}
}

static int parse_btype(CType *type, AttributeDef *ad, int ignore_label) { MCC_TRACE("enter\n");
	int t, u, bt, st, type_found, typespec_found, g, n, complex_seen;
	Sym *s;
	CType type1;

	memset(ad, 0, sizeof(AttributeDef));
	type_found = 0;
	typespec_found = 0;
	complex_seen = 0;
	t = VT_INT;
	bt = st = -1;
	type->ref = NULL;

	while (1) { MCC_TRACE("br\n");
		switch (tok) { MCC_TRACE("br\n");
		case TOK_EXTENSION:
			next();
			continue;

		case TOK_CHAR:
			u = VT_BYTE;
		basic_type:
			next();
		basic_type1:
			if (u == VT_SHORT || u == VT_LONG) { MCC_TRACE("br\n");
				if (st != -1 || (bt != -1 && bt != VT_INT))
				{ MCC_TRACE("br\n"); tmbt:
					mcc_error("too many basic types"); }
				st = u;
			} else { MCC_TRACE("br\n");
				if (bt != -1 || (st != -1 && u != VT_INT))
					{ MCC_TRACE("br\n"); goto tmbt; }
				if ((t & VT_DEFSIGN) && (u == VT_VOID || u > VT_LLONG))
					{ MCC_TRACE("br\n"); goto tmbt; }
				bt = u;
			}
			if (u != VT_INT)
				{ MCC_TRACE("br\n"); t = (t & ~(VT_BTYPE | VT_LONG)) | u; }
			typespec_found = 1;
			break;
		case TOK_VOID:
			u = VT_VOID;
			goto basic_type;
		case TOK_SHORT:
			u = VT_SHORT;
			goto basic_type;
		case TOK_INT:
			u = VT_INT;
			goto basic_type;
		case TOK_ALIGNAS: {
			int n;
			AttributeDef ad1;
			next();
			skip('(');
			memset(&ad1, 0, sizeof(AttributeDef));
			if (parse_btype(&type1, &ad1, 0)) { MCC_TRACE("br\n");
				type_decl(&type1, &ad1, &n, TYPE_ABSTRACT);
				if (ad1.a.aligned)
					{ MCC_TRACE("br\n"); n = 1 << (ad1.a.aligned - 1); }
				else
					{ MCC_TRACE("br\n"); type_size(&type1, &n); }
			} else { MCC_TRACE("br\n");
				n = expr_const();
				if (n < 0 || (n & (n - 1)) != 0)
					{ MCC_TRACE("br\n"); mcc_error("alignment must be a positive power of two"); }
			}
			skip(')');
			ad->a.aligned = exact_log2p1(n);
			ad->storage_class |= 8;
		}
			continue;
		case TOK_LONG:
			if ((t & VT_BTYPE) == VT_DOUBLE) { MCC_TRACE("br\n");
				t = (t & ~(VT_BTYPE | VT_LONG)) | VT_LDOUBLE;
			} else if ((t & (VT_BTYPE | VT_LONG)) == VT_LONG) { MCC_TRACE("br\n");
				if (mcc_state->cversion < 199901)
					{ MCC_TRACE("br\n"); mcc_pedantic("ISO C90 does not support 'long long'"); }
				t = (t & ~(VT_BTYPE | VT_LONG)) | VT_LLONG;
			} else { MCC_TRACE("br\n");
				u = VT_LONG;
				goto basic_type;
			}
			next();
			break;
		case TOK_BOOL:
			u = VT_BOOL;
			goto basic_type;
		case TOK_COMPLEX:
			complex_seen = 1;
			next();
			typespec_found = 1;
			break;
		case TOK_IMAGINARY:
			mcc_error("imaginary types are not supported");
			break;
		case TOK_FLOAT:
			u = VT_FLOAT;
			goto basic_type;
		case TOK_DOUBLE:
			if ((t & (VT_BTYPE | VT_LONG)) == VT_LONG) { MCC_TRACE("br\n");
				t = (t & ~(VT_BTYPE | VT_LONG)) | VT_LDOUBLE;
			} else { MCC_TRACE("br\n");
				u = VT_DOUBLE;
				goto basic_type;
			}
			next();
			break;
		case TOK_ENUM:
			struct_decl(&type1, VT_ENUM);
		basic_type2:
			u = type1.t;
			type->ref = type1.ref;
			goto basic_type1;
		case TOK_STRUCT:
			struct_decl(&type1, VT_STRUCT);
			goto basic_type2;
		case TOK_UNION:
			struct_decl(&type1, VT_UNION);
			goto basic_type2;

		case TOK__Atomic:
			if (mcc_state->cversion < 201112)
				{ MCC_TRACE("br\n"); mcc_pedantic("'_Atomic' is a C11 feature"); }
			next();
			type->t = t;
			parse_btype_qualify(type, VT_ATOMIC);
			t = type->t;
			if (tok == '(') { MCC_TRACE("br\n");
				parse_expr_type(&type1);
				if (type1.t & VT_ARRAY)
					{ MCC_TRACE("br\n"); mcc_error("_Atomic cannot be applied to an array type"); }
				if ((type1.t & VT_BTYPE) == VT_FUNC)
					{ MCC_TRACE("br\n"); mcc_error("_Atomic cannot be applied to a function type"); }
				if (type1.t & VT_ATOMIC_BIT)
					{ MCC_TRACE("br\n"); mcc_error("_Atomic cannot be applied to an atomic type"); }
				else if (type1.t & (VT_CONSTANT | VT_VOLATILE))
					{ MCC_TRACE("br\n"); mcc_error("_Atomic cannot be applied to a qualified type"); }
				type1.t &= ~(VT_STORAGE & ~VT_TYPEDEF);
				if (type1.ref)
					{ MCC_TRACE("br\n"); sym_to_attr(ad, type1.ref); }
				goto basic_type2;
			}
			ad->storage_class |= 16;
			break;
		case TOK_CONST1:
		case TOK_CONST2:
		case TOK_CONST3:
			type->t = t;
			parse_btype_qualify(type, VT_CONSTANT);
			t = type->t;
			next();
			break;
		case TOK_VOLATILE1:
		case TOK_VOLATILE2:
		case TOK_VOLATILE3:
			type->t = t;
			parse_btype_qualify(type, VT_VOLATILE);
			t = type->t;
			next();
			break;
		case TOK_SIGNED1:
		case TOK_SIGNED2:
		case TOK_SIGNED3:
			if ((t & (VT_DEFSIGN | VT_UNSIGNED)) == (VT_DEFSIGN | VT_UNSIGNED))
				{ MCC_TRACE("br\n"); mcc_error("signed and unsigned modifier"); }
			t |= VT_DEFSIGN;
			next();
			typespec_found = 1;
			break;
		case TOK_AUTO:
		case TOK_REGISTER:
			if ((t & (VT_EXTERN | VT_STATIC | VT_TYPEDEF)) || (ad->storage_class & 3))
				{ MCC_TRACE("br\n"); mcc_error("multiple storage classes"); }
			ad->storage_class |= (tok == TOK_AUTO) ? 1 : 2;
			next();
			break;
		case TOK_RESTRICT1:
		case TOK_RESTRICT2:
		case TOK_RESTRICT3:
			if (tok == TOK_RESTRICT1 && mcc_state->cversion < 199901)
				{ MCC_TRACE("br\n"); mcc_pedantic("'restrict' is a C99 feature"); }
			ad->storage_class |= 4;
			next();
			break;
		case TOK_UNSIGNED:
			if ((t & (VT_DEFSIGN | VT_UNSIGNED)) == VT_DEFSIGN)
				{ MCC_TRACE("br\n"); mcc_error("signed and unsigned modifier"); }
			t |= VT_DEFSIGN | VT_UNSIGNED;
			next();
			typespec_found = 1;
			break;

		case TOK_EXTERN:
			g = VT_EXTERN;
			goto storage;
		case TOK_STATIC:
			g = VT_STATIC;
			goto storage;
		case TOK_TYPEDEF:
			g = VT_TYPEDEF;
			goto storage;
		storage:
			if ((t & (VT_EXTERN | VT_STATIC | VT_TYPEDEF) & ~g) || (ad->storage_class & 3))
				{ MCC_TRACE("br\n"); mcc_error("multiple storage classes"); }
			t |= g;
			next();
			break;
		case TOK_INLINE1:
		case TOK_INLINE2:
		case TOK_INLINE3:
			if (tok == TOK_INLINE1 && mcc_state->cversion < 199901)
				{ MCC_TRACE("br\n"); mcc_pedantic("'inline' is a C99 feature"); }
			t |= VT_INLINE;
			next();
			break;
		case TOK_NORETURN3:
			next();
			ad->f.func_noreturn = 1;
			ad->storage_class |= 128;
			break;
		case TOK_ATTRIBUTE1:
		case TOK_ATTRIBUTE2:
			parse_attribute(ad);
			if (ad->attr_mode) { MCC_TRACE("br\n");
				u = ad->attr_mode - 1;
				t = (t & ~(VT_BTYPE | VT_LONG)) | u;
			}
			continue;
		case TOK_TYPEOF1:
		case TOK_TYPEOF2:
		case TOK_TYPEOF3:
			next();
			parse_expr_type(&type1);
			type1.t &= ~(VT_STORAGE & ~VT_TYPEDEF);
			if (type1.ref) { MCC_TRACE("br\n");
				sym_to_attr(ad, type1.ref);
				if (type1.t & VT_ARRAY)
					{ MCC_TRACE("br\n"); type1.t |= VT_BT_ARRAY; }
			}
			goto basic_type2;
		case TOK_THREAD_LOCAL:
		case TOK___thread:
			if (tok == TOK_THREAD_LOCAL && mcc_state->cversion < 201112)
				{ MCC_TRACE("br\n"); mcc_pedantic("'_Thread_local' is a C11 feature"); }
			if (t & VT_TLS)
				{ MCC_TRACE("br\n"); mcc_error("multiple thread-local storage specifiers"); }
			t |= VT_TLS;
			next();
			break;
		default:
			if (typespec_found)
				{ MCC_TRACE("br\n"); goto the_end; }
			s = sym_find(tok);
			if (!s || !(s->type.t & VT_TYPEDEF))
				{ MCC_TRACE("br\n"); goto the_end; }

			n = tok, next();
			if (tok == ':' && ignore_label) { MCC_TRACE("br\n");
				unget_tok(n);
				goto the_end;
			}

			t &= ~(VT_BTYPE | VT_LONG);
			u = t & ~VT_QUALIFY, t ^= u;
			type->t = (s->type.t & ~VT_TYPEDEF) | u;
			type->ref = s->type.ref;
			if (t)
				{ MCC_TRACE("br\n"); parse_btype_qualify(type, t); }
			t = type->t;
			if (t & VT_ARRAY)
				{ MCC_TRACE("br\n"); t |= VT_BT_ARRAY; }
			sym_to_attr(ad, s);
			typespec_found = 1;
			st = bt = -2;
			break;
		}
		type_found = 1;
	}
the_end:
	if (type_found && !typespec_found)
		{ MCC_TRACE("br\n"); ad->implicit_int = 1; }
	if (mcc_state->char_is_unsigned) { MCC_TRACE("br\n");
		if ((t & (VT_DEFSIGN | VT_BTYPE)) == VT_BYTE)
			{ MCC_TRACE("br\n"); t |= VT_UNSIGNED; }
	}
	bt = t & (VT_BTYPE | VT_LONG);
	if (bt == VT_LONG)
		{ MCC_TRACE("br\n"); t |= LONG_SIZE == 8 ? VT_LLONG : VT_INT; }
#ifdef MCC_USING_DOUBLE_FOR_LDOUBLE
	if (bt == VT_LDOUBLE)
		{ MCC_TRACE("br\n"); t = (t & ~(VT_BTYPE | VT_LONG)) | (VT_DOUBLE | VT_LONG); }
#endif
	if (complex_seen) { MCC_TRACE("br\n");
		CType base;
		base.t = t & (VT_BTYPE | VT_LONG);
		base.ref = NULL;
		if (!is_float(base.t))
			{ MCC_TRACE("br\n"); mcc_error("_Complex requires a floating-point type"); }
		mk_complex_type(type, &base);
		type->t |= t & (VT_CONSTANT | VT_VOLATILE | VT_ATOMIC_BIT | VT_DEFSIGN | VT_EXTERN | VT_STATIC | VT_TYPEDEF |
										VT_INLINE);
		return type_found;
	}
	type->t = t;
	if (ad->storage_class & 16) { MCC_TRACE("br\n");
		if (t & VT_ARRAY)
			{ MCC_TRACE("br\n"); mcc_error("_Atomic cannot be applied to an array type"); }
		if ((t & VT_BTYPE) == VT_FUNC)
			{ MCC_TRACE("br\n"); mcc_error("_Atomic cannot be applied to a function type"); }
	}
	return type_found;
}

static inline void convert_parameter_type(CType *pt) { MCC_TRACE("enter\n");
	pt->t &= ~VT_QUALIFY;
	pt->t &= ~(VT_ARRAY | VT_VLA);
	if ((pt->t & VT_BTYPE) == VT_FUNC) { MCC_TRACE("br\n");
		mk_pointer(pt);
	}
}

ST_FUNC CString *parse_asm_str(void) { MCC_TRACE("enter\n");
	skip('(');
	return parse_mult_str("string constant");
}

static int asm_label_instr(void) { MCC_TRACE("enter\n");
	int v;
	char *astr;

	next();
	astr = parse_asm_str()->data;
	skip(')');
	if (g_debug & MCC_DBG_ASM)
		{ MCC_TRACE("br\n"); printf("asm_alias: \"%s\"\n", astr); }
	v = tok_alloc_const(astr);
	return v;
}

static int post_type(CType *type, AttributeDef *ad, int storage, int td) { MCC_TRACE("enter\n");
	int n, l, t1, arg_size, align;
	int star_param = 0;
	Sym **plast, *s, *first, **ps, *sr;
	AttributeDef ad1;
	CType pt;
	TokenString *vla_array_tok = NULL;
	int *vla_array_str = NULL;

	if (tok == '(') { MCC_TRACE("br\n");
#if MCC_CONFIG_LSP
		uint32_t cst_pm = CST_MARK();
#endif
		next();
		if (TYPE_DIRECT == (td & (TYPE_DIRECT | TYPE_ABSTRACT)))
			{ MCC_TRACE("br\n"); return 0; }

		ps = local_stack ? &local_stack : &global_stack;
		++local_scope;
		sr = sym_push2(ps, SYM_FIELD, 0, 0);

		if (tok == ')')
			{ MCC_TRACE("br\n"); l = 0; }
		else if (parse_btype(&pt, &ad1, 0))
			{ MCC_TRACE("br\n"); l = FUNC_NEW; }
		else if (td & (TYPE_DIRECT | TYPE_ABSTRACT)) { MCC_TRACE("br\n");
			sym_pop(ps, sr->prev, 0);
			--local_scope;
			merge_attr(ad, &ad1);
			return 0;
		} else
			{ MCC_TRACE("br\n"); l = FUNC_OLD; }

		first = NULL;
		plast = &first;
		arg_size = 0;
		if (l) { MCC_TRACE("br\n");
			for (;;) { MCC_TRACE("br\n");
				if (l != FUNC_OLD) { MCC_TRACE("br\n");
					if ((pt.t & VT_BTYPE) == VT_VOID && tok == ')') { MCC_TRACE("br\n");
						if (pt.t & (VT_CONSTANT | VT_VOLATILE))
							{ MCC_TRACE("br\n"); mcc_error("'void' as only parameter may not be qualified"); }
						break;
					}
					type_decl(&pt, &ad1, &n, TYPE_DIRECT | TYPE_ABSTRACT | TYPE_PARAM);
					if ((pt.t & VT_BTYPE) == VT_VOID)
						{ MCC_TRACE("br\n"); mcc_error("parameter declared as void"); }
					if ((pt.t & (VT_STATIC | VT_EXTERN | VT_TYPEDEF | VT_TLS)) || (ad1.storage_class & 1))
						{ MCC_TRACE("br\n"); mcc_error("storage class specified for parameter"); }
					if (ad1.storage_class & 8)
						{ MCC_TRACE("br\n"); mcc_error("'_Alignas' specified for a function parameter"); }
					if (ad1.storage_class & 32)
						{ MCC_TRACE("br\n"); star_param = 1; }
					if ((ad1.storage_class & 64) && !(pt.t & (VT_ARRAY | VT_VLA)))
						{ MCC_TRACE("br\n"); mcc_error("'static' or type qualifiers used in non-outermost array declarator"); }
					if (n == 0 || (td & TYPE_PARAM))
						{ MCC_TRACE("br\n"); n |= SYM_FIELD; }
				} else { MCC_TRACE("br\n");
					n = tok;
					pt.t = VT_INT | VT_EXTERN;
					pt.ref = NULL;
					next();
				}
				if (n < TOK_UIDENT)
					{ MCC_TRACE("br\n"); expect("identifier"); }
				convert_parameter_type(&pt);
				arg_size += (type_size(&pt, &align) + MCC_PTR_SIZE - 1) / MCC_PTR_SIZE;
				s = sym_push(n, &pt, VT_LOCAL | VT_LVAL, 0);
				s->vla_inner_id = file->line_num;
				s->a.inited = 1;
				if (ad1.storage_class & 2)
					{ MCC_TRACE("br\n"); s->a.is_register = 1; }
				*plast = s;
				plast = &s->next;
				if (tok == ')')
					{ MCC_TRACE("br\n"); break; }
				skip(',');
				if (l == FUNC_NEW && tok == TOK_DOTS) { MCC_TRACE("br\n");
					l = FUNC_ELLIPSIS;
					next();
					break;
				}
				if (l == FUNC_NEW && !parse_btype(&pt, &ad1, 0))
					{ MCC_TRACE("br\n"); mcc_error("invalid type"); }
			}
		} else
			{ MCC_TRACE("br\n"); l = FUNC_OLD; }
		if (l == FUNC_OLD && first == NULL)
			{ MCC_TRACE("br\n"); mcc_warning_c(warn_strict_prototypes)(
					"function declaration isn't a prototype"); }
		skip(')');
#if MCC_CONFIG_LSP
		CST_OPEN_AT(CST_ParamList, cst_pm);
		CST_CLOSE();
#endif
		type->t &= ~VT_CONSTANT;
		if (tok == '[') { MCC_TRACE("br\n");
			next();
			skip(']');
			mk_pointer(type);
		}
		ad->f.func_args = arg_size;
		ad->f.func_type = l;
		ad->f.func_star_param = star_param;
		if ((type->t & VT_BTYPE) == VT_FUNC)
			{ MCC_TRACE("br\n"); mcc_error("function cannot return a function type"); }
		if (type->t & VT_ARRAY)
			{ MCC_TRACE("br\n"); mcc_error("function cannot return an array type"); }
		sr->type = *type, s = sr;
		s->a = ad->a;
		s->f = ad->f;
		s->next = first;
		type->t = VT_FUNC;
		type->ref = s;
		sym_pop(ps, sr, 1);
		--local_scope;
	} else if (tok == '[') { MCC_TRACE("br\n");
		int saved_nocode_wanted = nocode_wanted;
		int saw_static = 0;
		next();
		n = -1;
		t1 = 0;
		if (td & TYPE_PARAM)
			{ MCC_TRACE("br\n"); while (1) { MCC_TRACE("br\n");
				switch (tok) { MCC_TRACE("br\n");
				case TOK_RESTRICT1:
				case TOK_RESTRICT2:
				case TOK_RESTRICT3:
				case TOK_CONST1:
				case TOK_VOLATILE1:
				case TOK_STATIC:
					if (td & TYPE_NEST)
						{ MCC_TRACE("br\n"); mcc_error("'static' or type qualifiers used in non-outermost array declarator"); }
					if (tok == TOK_RESTRICT1 && mcc_state->cversion < 199901)
						{ MCC_TRACE("br\n"); mcc_pedantic("'restrict' is a C99 feature"); }
					if (tok == TOK_STATIC)
						{ MCC_TRACE("br\n"); saw_static = 1; }
					ad->storage_class |= 64;
					next();
					continue;
				case '*':
					next();
					if (tok == ']')
						{ MCC_TRACE("br\n"); ad->storage_class |= 32; }
					continue;
				default:
					break;
				}
				if (tok != ']') { MCC_TRACE("br\n");
					nocode_wanted = 1;
					skip_or_save_block(&vla_array_tok);
					unget_tok(0);
					vla_array_str = vla_array_tok->str;
					begin_macro(vla_array_tok, 2);
					next();
					gexpr();
					end_macro();
					next();
					goto check;
				}
				if (saw_static)
					{ MCC_TRACE("br\n"); mcc_error("'static' may not be used without an array size"); }
				break;
			} }
		else if (tok != ']') { MCC_TRACE("br\n");
			ice_float_op = 0;
			if (!local_stack || (storage & VT_STATIC))
				{ MCC_TRACE("br\n"); vpushi(expr_const()); }
			else { MCC_TRACE("br\n");
				nocode_wanted = 0;
				gexpr();
			}
		check:
			if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
				n = vtop->c.i;
				if (n < 0)
					{ MCC_TRACE("br\n"); mcc_error("invalid array size"); }
				else if (n == 0)
					{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids zero-size array"); }
				if (ice_float_op)
					{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids an array size that is not an "
											 "integer constant expression"); }
			} else { MCC_TRACE("br\n");
				if (!is_integer_btype(vtop->type.t & VT_BTYPE))
					{ MCC_TRACE("br\n"); mcc_error("size of variable length array should be an integer"); }
				n = 0;
				t1 = VT_VLA;
				mcc_warning_c(warn_vla)("ISO C90 forbids variable length array");
				if (mcc_state->cversion < 199901)
					{ MCC_TRACE("br\n"); mcc_pedantic("variable length arrays are a C99 feature"); }
			}
		}
		skip(']');
		post_type(type, ad, storage, (td & ~(TYPE_DIRECT | TYPE_ABSTRACT)) | TYPE_NEST);

		if ((type->t & VT_BTYPE) == VT_FUNC)
			{ MCC_TRACE("br\n"); mcc_error("declaration of an array of functions"); }
		if ((type->t & VT_BTYPE) == VT_VOID || type_size(type, &align) < 0)
			{ MCC_TRACE("br\n"); mcc_error("declaration of an array of incomplete type elements"); }
		if (struct_has_flexible_member(type))
			{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids an array of a structure with a "
									 "flexible array member"); }

		t1 |= type->t & VT_VLA;

		if (t1 & VT_VLA) { MCC_TRACE("br\n");
			if (n < 0) { MCC_TRACE("br\n");
				if (td & TYPE_NEST)
					{ MCC_TRACE("br\n"); mcc_error("need explicit inner array size in VLAs"); }
			} else { MCC_TRACE("br\n");
				loc -= type_size(&int_type, &align);
				loc &= -align;
				n = loc;

				vpush_type_size(type, &align);
				gen_op('*');
				vset(&int_type, VT_LOCAL | VT_LVAL, n);
				vswap();
				vstore();
			}
		}
		if (n != -1)
			{ MCC_TRACE("br\n"); vpop(); }
		nocode_wanted = saved_nocode_wanted;

		s = sym_push(SYM_FIELD, type, 0, n);
		type->t = (t1 ? VT_VLA : VT_ARRAY) | VT_PTR;
		type->ref = s;

		if (vla_array_str) { MCC_TRACE("br\n");
			if ((t1 & VT_VLA) && (td & (TYPE_NEST | TYPE_PARAM)))
				{ MCC_TRACE("br\n"); s->vla_array_str = vla_array_str; }
			else
				{ MCC_TRACE("br\n"); tok_str_free_str(vla_array_str); }
		}
	}
	return 1;
}

struct restrict_ctx {
	Sym *pointee[8];
	int nb;
};

static CType *type_decl_1(CType *type, AttributeDef *ad, int *v, int td,
													struct restrict_ctx *rc) { MCC_TRACE("enter\n");
	CType *post, *ret;
	int qualifiers, restrict_q, storage, arr_nested = 0;

	storage = type->t & VT_STORAGE;
	type->t &= ~VT_STORAGE;
	post = ret = type;

	while (tok == '*') { MCC_TRACE("br\n");
		qualifiers = 0;
		restrict_q = 0;
	redo:
		next();
		switch (tok) { MCC_TRACE("br\n");
		case TOK__Atomic:
			qualifiers |= VT_ATOMIC;
			goto redo;
		case TOK_CONST1:
		case TOK_CONST2:
		case TOK_CONST3:
			qualifiers |= VT_CONSTANT;
			goto redo;
		case TOK_VOLATILE1:
		case TOK_VOLATILE2:
		case TOK_VOLATILE3:
			qualifiers |= VT_VOLATILE;
			goto redo;
		case TOK_RESTRICT1:
		case TOK_RESTRICT2:
		case TOK_RESTRICT3:
			if (tok == TOK_RESTRICT1 && mcc_state->cversion < 199901)
				{ MCC_TRACE("br\n"); mcc_pedantic("'restrict' is a C99 feature"); }
			restrict_q = 1;
			goto redo;
		case TOK_ATTRIBUTE1:
		case TOK_ATTRIBUTE2:
			parse_attribute(ad);
			break;
		}
		mk_pointer(type);
		type->t |= qualifiers;
		if (restrict_q && rc->nb < 8)
			{ MCC_TRACE("br\n"); rc->pointee[rc->nb++] = type->ref; }
		if (ret == type)
			{ MCC_TRACE("br\n"); ret = pointed_type(type); }
	}

	if (tok == '(') { MCC_TRACE("br\n");
		if (!post_type(type, ad, 0, td)) { MCC_TRACE("br\n");
			parse_attribute(ad);
			post = type_decl_1(type, ad, v, td, rc);
			skip(')');
			if (post != ret)
				{ MCC_TRACE("br\n"); arr_nested = 1; }
		} else
			{ MCC_TRACE("br\n"); goto abstract; }
	} else if (tok >= TOK_IDENT && (td & TYPE_DIRECT)) { MCC_TRACE("br\n");
		*v = tok;
#if MCC_CONFIG_LSP
		cst_hook_def(tok, cst_cur_tok_off());
#endif
		next();
	} else { MCC_TRACE("br\n");
	abstract:
		if (!(td & TYPE_ABSTRACT))
			{ MCC_TRACE("br\n"); expect("identifier"); }
		*v = 0;
	}
	post_type(post, ad, post != ret ? 0 : storage,
						(td & ~(TYPE_DIRECT | TYPE_ABSTRACT)) | (arr_nested ? TYPE_NEST : 0));
	parse_attribute(ad);
	type->t |= storage;
	return ret;
}

static CType *type_decl(CType *type, AttributeDef *ad, int *v, int td) { MCC_TRACE("enter\n");
	struct restrict_ctx rc;
	CType *ret;
	int bad = 0;

	CST_OPEN(CST_Declarator);
	rc.nb = 0;
	ret = type_decl_1(type, ad, v, td, &rc);
	while (rc.nb)
		{ MCC_TRACE("br\n"); if ((rc.pointee[--rc.nb]->type.t & VT_BTYPE) == VT_FUNC)
			{ MCC_TRACE("br\n"); bad = 1; } }
	if (bad)
		{ MCC_TRACE("br\n"); mcc_error("pointer to function type may not be "
							"'restrict'-qualified"); }
	CST_CLOSE();
	return ret;
}

ST_FUNC void indir(void) { MCC_TRACE("enter\n");
#if MCC_CONFIG_OPTIMIZER
	ast_hook_indir();
#endif
	if ((vtop->type.t & VT_BTYPE) != VT_PTR) { MCC_TRACE("br\n");
		if ((vtop->type.t & VT_BTYPE) == VT_FUNC)
			{ MCC_TRACE("br\n"); return; }
		expect("pointer");
	}
	if (vtop->r & VT_LVAL)
		{ MCC_TRACE("br\n"); gv(MCC_RC_INT); }
#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64
	if (mcc_state->do_sanitize_undefined)
		{ MCC_TRACE("br\n"); gen_ubsan_nullptr(); }
#endif
#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_ARM64
	if (mcc_state->do_asan_shadow) { MCC_TRACE("br\n");
		int asz, aal;
		asz = type_size(pointed_type(&vtop->type), &aal);
		gen_asan_shadow_check(asz);
	}
#endif
	vtop->type = *pointed_type(&vtop->type);
	if ((vtop->type.t & VT_BTYPE) == VT_VOID && !nocode_wanted)
		{ MCC_TRACE("br\n"); mcc_pedantic("dereferencing a 'void *' pointer"); }
	if (!(vtop->type.t & (VT_ARRAY | VT_VLA)) && (vtop->type.t & VT_BTYPE) != VT_FUNC) { MCC_TRACE("br\n");
		vtop->r |= VT_LVAL;
#if MCC_CONFIG_DIAG_RT >= 2
		if (mcc_state->do_bounds_check)
			{ MCC_TRACE("br\n"); vtop->r |= VT_MUSTBOUND; }
#endif
	}
}

#if defined MCC_TARGET_RISCV64 || defined MCC_TARGET_ARM64 || (defined MCC_TARGET_X86_64 && defined MCC_TARGET_PE)
static void check_va_start_register(void) { MCC_TRACE("enter\n");
	if (vtop->sym && vtop->sym->a.is_register)
		{ MCC_TRACE("br\n"); mcc_warning("undefined behavior when the second parameter of 'va_start' "
								"is declared with 'register' storage"); }
}

static void check_va_start_last_param(void) { MCC_TRACE("enter\n");
	if ((mcc_state->warn_varargs & WARN_ON) && cur_func_last_param && vtop->sym && (vtop->sym->v & ~SYM_FIELD) != cur_func_last_param)
		{ MCC_TRACE("br\n"); mcc_warning_c(warn_varargs)("second argument to 'va_start' is not the "
																"last named parameter"); }
}
#endif

static Sym *transparent_union_member(CType *type) { MCC_TRACE("enter\n");
	Sym *m;
	CType *st = &vtop->type;

	if ((type->t & VT_BTYPE) != VT_STRUCT || type->ref->c < 0 || !type->ref->a.transp_union)
		{ MCC_TRACE("br\n"); return NULL; }
	if ((st->t & VT_BTYPE) == VT_STRUCT && is_compatible_unqualified_types(type, st))
		{ MCC_TRACE("br\n"); return NULL; }
	for (m = type->ref->next; m; m = m->next) { MCC_TRACE("br\n");
		if (is_compatible_unqualified_types(&m->type, st))
			{ MCC_TRACE("br\n"); return m; }
		if ((m->type.t & VT_BTYPE) == VT_PTR && ((st->t & VT_BTYPE) == VT_PTR || is_null_pointer(vtop)))
			{ MCC_TRACE("br\n"); return m; }
	}
	return NULL;
}

static void gfunc_param_typed(Sym *func, Sym *arg) { MCC_TRACE("enter\n");
	int func_type;
	Sym *tu;
	CType type;

	func_type = func->f.func_type;
	if (func_type == FUNC_OLD ||
			(func_type == FUNC_ELLIPSIS && arg == NULL)) { MCC_TRACE("br\n");
		if ((vtop->type.t & VT_BTYPE) == VT_FLOAT) { MCC_TRACE("br\n");
			gen_cast_s(VT_DOUBLE);
		} else if (vtop->type.t & VT_BITFIELD) { MCC_TRACE("br\n");
			type.t = vtop->type.t & (VT_BTYPE | VT_UNSIGNED);
			type.ref = vtop->type.ref;
			gen_cast(&type);
		} else if (vtop->r & VT_MUSTCAST) { MCC_TRACE("br\n");
			force_charshort_cast();
		}
	} else if (arg == NULL) { MCC_TRACE("br\n");
		mcc_error("too many arguments to function");
	} else { MCC_TRACE("br\n");
		type = arg->type;
		type.t &= ~VT_CONSTANT;
		tu = transparent_union_member(&type);
		if (tu)
			{ MCC_TRACE("br\n"); gen_cast(&tu->type); }
		else
			{ MCC_TRACE("br\n"); gen_assign_cast(&type); }
	}
}

static void expr_type(CType *type, void (*expr_fn)(void)) { MCC_TRACE("enter\n");
	nocode_wanted++;
	expr_fn();
	*type = vtop->type;
	vpop();
	nocode_wanted--;
}

static void parse_expr_type(CType *type) { MCC_TRACE("enter\n");
	int n;
	AttributeDef ad;

	skip('(');
	if (parse_btype(type, &ad, 0)) { MCC_TRACE("br\n");
		type_decl(type, &ad, &n, TYPE_ABSTRACT);
	} else { MCC_TRACE("br\n");
		expr_type(type, gexpr);
	}
	skip(')');
}

static void parse_type(CType *type) { MCC_TRACE("enter\n");
	AttributeDef ad;
	int n;

	if (!parse_btype(type, &ad, 0)) { MCC_TRACE("br\n");
		expect("type");
	}
	type_decl(type, &ad, &n, TYPE_ABSTRACT);
}

static void parse_builtin_params(int nc, const char *args) { MCC_TRACE("enter\n");
	char c, sep = '(';
	CType type;
	if (nc)
		{ MCC_TRACE("br\n"); nocode_wanted++; }
	next();
	if (*args == 0)
		{ MCC_TRACE("br\n"); skip(sep); }
	while ((c = *args++)) { MCC_TRACE("br\n");
		skip(sep);
		sep = ',';
		if (c == 't') { MCC_TRACE("br\n");
			parse_type(&type);
			vpush(&type);
			continue;
		}
		expr_eq();
		type.ref = NULL;
		type.t = 0;
		switch (c) { MCC_TRACE("br\n");
		case 'e':
			continue;
		case 'V':
			type.t = VT_CONSTANT;
			FALLTHROUGH;
		case 'v':
			type.t |= VT_VOID;
			mk_pointer(&type);
			break;
		case 'S':
			type.t = VT_CONSTANT;
			FALLTHROUGH;
		case 's':
			type.t |= char_type.t;
			mk_pointer(&type);
			break;
		case 'i':
			type.t = VT_INT;
			break;
		case 'l':
			type.t = VT_SIZE_T;
			break;
		default:
			break;
		}
		gen_assign_cast(&type);
	}
	skip(')');
	if (nc)
		{ MCC_TRACE("br\n"); nocode_wanted--; }
}

static void parse_atomic(int atok) { MCC_TRACE("enter\n");
	int size, align, arg, t, save = 0, use_generic = 0;
	int is_add_sub = atok == TOK___atomic_fetch_add || atok == TOK___atomic_fetch_sub || atok == TOK___atomic_add_fetch || atok == TOK___atomic_sub_fetch;
	CType *atom, *atom_ptr, ct = {0};
	SValue store;
	char buf[40];
	static const char *const templates[] = {

			"alm.?",
			"Asm.v",
			"alsm.v",
			"aplbmm.b",
			"avm.v",
			"avm.v",
			"avm.v",
			"avm.v",
			"avm.v",
			"avm.v",
			"avm.v",
			"avm.v",
			"avm.v",
			"avm.v",
			"avm.v",
			"avm.v"};
	const char *template = templates[(atok - TOK___atomic_store)];

	atom = atom_ptr = NULL;
	size = 0;
	next();
	skip('(');
	for (arg = 0;;) { MCC_TRACE("br\n");
		expr_eq();
		switch (template[arg]) { MCC_TRACE("br\n");
		case 'a':
		case 'A':
			atom_ptr = &vtop->type;
			if ((atom_ptr->t & VT_BTYPE) != VT_PTR)
				{ MCC_TRACE("br\n"); expect("pointer"); }
			atom = pointed_type(atom_ptr);
			size = type_size(atom, &align);
			if (atok <= TOK___atomic_compare_exchange && (size > 8 || ((atom->t & VT_BTYPE) == VT_STRUCT && (atok ==
																																																					 TOK___atomic_load ||
																																																			 atok == TOK___atomic_exchange))))
				{ MCC_TRACE("br\n"); use_generic = 1; }
			else if (size > 8 || (size & (size - 1)))
				{ MCC_TRACE("br\n"); expect("integral or integer-sized pointer target type"); }
			else if (atok > TOK___atomic_compare_exchange) { MCC_TRACE("br\n");
				int abt = atom->t & VT_BTYPE;
				if (abt == VT_PTR) { MCC_TRACE("br\n");
					if (!is_add_sub || !(atom->t & VT_ATOMIC_BIT))
						{ MCC_TRACE("br\n"); expect("integral or integer-sized pointer target type"); }
				} else if (0 == btype_size(abt))
					{ MCC_TRACE("br\n"); expect("integral or integer-sized pointer target type"); }
			}
			break;

		case 'p':
			if (use_generic) { MCC_TRACE("br\n");
				if ((vtop->type.t & VT_BTYPE) != VT_PTR)
					{ MCC_TRACE("br\n"); mcc_error("pointer expected in argument %d", arg + 1); }
				break;
			}
			if ((vtop->type.t & VT_BTYPE) != VT_PTR || type_size(pointed_type(&vtop->type), &align) != size)
				{ MCC_TRACE("br\n"); mcc_error("pointer target type mismatch in argument %d", arg + 1); }
			gen_assign_cast(atom_ptr);
			break;
		case 'v':
			if (is_add_sub && (atom->t & VT_BTYPE) == VT_PTR) { MCC_TRACE("br\n");
				int al;
				vpush_type_size(pointed_type(atom), &al);
				gen_op('*');
			} else
				{ MCC_TRACE("br\n"); gen_assign_cast(atom); }
			break;
		case 'l':
			if (use_generic)
				{ MCC_TRACE("br\n"); break; }
			indir();
			gen_assign_cast(atom);
			break;
		case 's':
			if (use_generic)
				{ MCC_TRACE("br\n"); break; }
			save = 1;
			indir();
			store = *vtop;
			vpop();
			break;
		case 'm':
			gen_assign_cast(&int_type);
			break;
		case 'b':
			if (use_generic) { MCC_TRACE("br\n");
				vpop();
				break;
			}
			ct.t = VT_BOOL;
			gen_assign_cast(&ct);
			break;
		}
		if ('.' == template[++arg])
			{ MCC_TRACE("br\n"); break; }
		skip(',');
	}
	skip(')');

	if (use_generic) { MCC_TRACE("br\n");
		int gn = arg - (atok == TOK___atomic_compare_exchange ? 1 : 0);
		vpushi(size);
		gen_cast_s(VT_SIZE_T);
		vrott(gn + 1);
		vpush_helper_func(atok);
		vrott(gn + 2);
		gfunc_call(gn + 1);
		if (atok == TOK___atomic_compare_exchange) { MCC_TRACE("br\n");
			vpushi(0);
			vtop->type.t = VT_INT;
			PUT_R_RET(vtop, VT_INT);
		} else { MCC_TRACE("br\n");
			ct.t = VT_VOID;
			vpush(&ct);
		}
		return;
	}

	ct.t = VT_VOID;
	switch (template[arg + 1]) { MCC_TRACE("br\n");
	case 'b':
		ct.t = VT_BOOL;
		break;
	case 'v':
		ct = *atom;
		break;
	}

	snprintf(buf, sizeof(buf), "%s_%d", get_tok_str(atok, 0), size);
	vpush_helper_func(tok_alloc_const(buf));
	vrott(arg - save + 1);
	gfunc_call(arg - save);

	vpush(&ct);
	PUT_R_RET(vtop, ct.t);
	t = ct.t & VT_BTYPE;
	if (t == VT_BYTE || t == VT_SHORT || t == VT_BOOL) { MCC_TRACE("br\n");
#ifdef MCC_RET_PROMOTES_INT
		vtop->r |= BFVAL(VT_MUSTCAST, 1);
#else
		vtop->type.t = VT_INT;
#endif
	}
	gen_cast(&ct);
	if (save) { MCC_TRACE("br\n");
		vpush(&ct);
		*vtop = store;
		vswap();
		vstore();
	}
}

static int atomic_rmw_size(SValue *sv, int op) { MCC_TRACE("enter\n");
	int bt, size, align;
	if (!(sv->type.t & VT_ATOMIC_BIT) || !(sv->r & VT_LVAL))
		{ MCC_TRACE("br\n"); return 0; }
	bt = sv->type.t & VT_BTYPE;
	if (bt == VT_PTR) { MCC_TRACE("br\n");
		if (op != '+' && op != '-')
			{ MCC_TRACE("br\n"); return 0; }
		if (type_size(pointed_type(&sv->type), &align) <= 0)
			{ MCC_TRACE("br\n"); return 0; }
	} else { MCC_TRACE("br\n");
		if (op != '+' && op != '-' && op != '&' && op != '|' && op != '^')
			{ MCC_TRACE("br\n"); return 0; }
		if (bt != VT_INT && bt != VT_LLONG && bt != VT_BYTE && bt != VT_SHORT && bt != VT_BOOL)
			{ MCC_TRACE("br\n"); return 0; }
	}
	size = type_size(&sv->type, &align);
	if (size < 1 || size > 8 || (size & (size - 1)))
		{ MCC_TRACE("br\n"); return 0; }
	return size;
}

static void gen_atomic_rmw(int op, int ret_new) { MCC_TRACE("enter\n");
	CType atomtype = vtop[-1].type;
	int size, align, bt;
	char buf[40];
	const char *base;

	atomic_lowering++;
	atomtype.t &= ~VT_QUALIFY;
	size = type_size(&atomtype, &align);
	switch (op) { MCC_TRACE("br\n");
	case '+':
		base = ret_new ? "__atomic_add_fetch" : "__atomic_fetch_add";
		break;
	case '-':
		base = ret_new ? "__atomic_sub_fetch" : "__atomic_fetch_sub";
		break;
	case '&':
		base = ret_new ? "__atomic_and_fetch" : "__atomic_fetch_and";
		break;
	case '|':
		base = ret_new ? "__atomic_or_fetch" : "__atomic_fetch_or";
		break;
	default:
		base = ret_new ? "__atomic_xor_fetch" : "__atomic_fetch_xor";
		break;
	}

	if ((atomtype.t & VT_BTYPE) == VT_PTR) { MCC_TRACE("br\n");
		int al;
		vpush_type_size(pointed_type(&atomtype), &al);
		gen_op('*');
	} else { MCC_TRACE("br\n");
		gen_assign_cast(&atomtype);
	}
	vswap();
	mk_pointer(&vtop->type);
	gaddrof();
	vswap();
	vpushi(0x5);
	snprintf(buf, sizeof buf, "%s_%d", base, size);
	vpush_helper_func(tok_alloc_const(buf));
	vrott(4);
	gfunc_call(3);

	vpush(&atomtype);
	PUT_R_RET(vtop, atomtype.t);
	bt = atomtype.t & VT_BTYPE;
	if (bt == VT_BYTE || bt == VT_SHORT || bt == VT_BOOL) { MCC_TRACE("br\n");
#ifdef MCC_RET_PROMOTES_INT
		vtop->r |= BFVAL(VT_MUSTCAST, 1);
#else
		vtop->type.t = VT_INT;
#endif
	}
	gen_cast(&atomtype);
	atomic_lowering--;
}

static int alloc_local_slot(int size, int align) { MCC_TRACE("enter\n");
	loc = (loc - size) & -align;
	return loc;
}

static int atomic_cas_size(SValue *sv) { MCC_TRACE("enter\n");
	int bt, size, align;
	if (!(sv->type.t & VT_ATOMIC_BIT) || !(sv->r & VT_LVAL))
		{ MCC_TRACE("br\n"); return 0; }
	bt = sv->type.t & VT_BTYPE;
	if (bt != VT_INT && bt != VT_LLONG && bt != VT_BYTE && bt != VT_SHORT && bt != VT_BOOL && bt != VT_FLOAT && bt != VT_DOUBLE)
		{ MCC_TRACE("br\n"); return 0; }
	size = type_size(&sv->type, &align);
	if (size < 1 || size > 8 || (size & (size - 1)))
		{ MCC_TRACE("br\n"); return 0; }
	return size;
}

static void gen_atomic_cas_rmw(int op, int ret_new) { MCC_TRACE("enter\n");
	CType at = vtop[-1].type, pt;
	int size, align, psize, palign;
	int s_pa, s_vo, s_vn, s_vr, loop, c;
	char buf[48];

	atomic_lowering++;
	pt = vtop[-1].type;
	mk_pointer(&pt);
	psize = type_size(&pt, &palign);
	at.t &= ~VT_QUALIFY;
	size = type_size(&at, &align);

	s_pa = alloc_local_slot(psize, palign);
	s_vo = alloc_local_slot(size, align);
	s_vn = alloc_local_slot(size, align);
	s_vr = alloc_local_slot(size, align);

	gen_assign_cast(&at);
	vset(&at, VT_LOCAL | VT_LVAL, s_vr);
	vswap();
	vstore();
	vpop();

	mk_pointer(&vtop->type);
	gaddrof();
	vset(&pt, VT_LOCAL | VT_LVAL, s_pa);
	vswap();
	vstore();
	vpop();

	vset(&pt, VT_LOCAL | VT_LVAL, s_pa);
	indir();
	vset(&at, VT_LOCAL | VT_LVAL, s_vo);
	vswap();
	vstore();
	vpop();

	loop = gind();
	vset(&at, VT_LOCAL | VT_LVAL, s_vo);
	vset(&at, VT_LOCAL | VT_LVAL, s_vr);
	gen_op(op);
	gen_cast(&at);
	vset(&at, VT_LOCAL | VT_LVAL, s_vn);
	vswap();
	vstore();
	vpop();

	{
		CType it;
		it.ref = NULL;
		it.t = (size == 8)
							 ? VT_LLONG
					 : (size == 2)
							 ? VT_SHORT
					 : (size == 1)
							 ? VT_BYTE
							 : VT_INT;
		vset(&pt, VT_LOCAL | VT_LVAL, s_pa);
		vset(&at, VT_LOCAL | VT_LVAL, s_vo);
		mk_pointer(&vtop->type);
		gaddrof();
		vset(&it, VT_LOCAL | VT_LVAL, s_vn);
	}
	vpushi(0);
	vpushi(0x5);
	vpushi(0x5);
	snprintf(buf, sizeof buf, "__atomic_compare_exchange_%d", size);
	vpush_helper_func(tok_alloc_const(buf));
	vrott(7);
	gfunc_call(6);
	vpushi(0);
	vtop->type.t = VT_INT;
	PUT_R_RET(vtop, VT_INT);

	gen_test_zero(TOK_EQ);
	c = gvtst(0, 0);
	gsym_addr(c, loop);

	vset(&at, VT_LOCAL | VT_LVAL, ret_new ? s_vn : s_vo);
	atomic_lowering--;
}

static int atomic_store_needs_libcall(SValue *sv) { MCC_TRACE("enter\n");
	int size, align, bt;
	if (!(sv->type.t & VT_ATOMIC_BIT) || !(sv->r & VT_LVAL))
		{ MCC_TRACE("br\n"); return 0; }
	bt = sv->type.t & VT_BTYPE;
	if (bt == VT_STRUCT || is_float(sv->type.t))
		{ MCC_TRACE("br\n"); return 0; }
	size = type_size(&sv->type, &align);
	if (size <= MCC_PTR_SIZE || size > 8 || (size & (size - 1)))
		{ MCC_TRACE("br\n"); return 0; }
	return size;
}

static void gen_atomic_store_scalar(void) { MCC_TRACE("enter\n");
	CType at = vtop->type;
	int size, align, s_v;
	char buf[40];

	atomic_lowering++;
	at.t &= ~VT_QUALIFY;
	size = type_size(&at, &align);
	s_v = alloc_local_slot(size, align);

	mk_pointer(&vtop->type);
	gaddrof();
	expr_eq();
	gen_assign_cast(&at);
	vset(&at, VT_LOCAL | VT_LVAL, s_v);
	vswap();
	vstore();
	vpop();
	vset(&at, VT_LOCAL | VT_LVAL, s_v);
	vpushi(0x5);
	snprintf(buf, sizeof buf, "__atomic_store_%d", size);
	vpush_helper_func(tok_alloc_const(buf));
	vrott(4);
	gfunc_call(3);
	vset(&at, VT_LOCAL | VT_LVAL, s_v);
	atomic_lowering--;
}

static int atomic_store_needs_generic(SValue *sv) { MCC_TRACE("enter\n");
	int size, align;
	if (!(sv->type.t & VT_ATOMIC_BIT) || !(sv->r & VT_LVAL))
		{ MCC_TRACE("br\n"); return 0; }
	if ((sv->type.t & VT_BTYPE) == VT_STRUCT)
		{ MCC_TRACE("br\n"); return 1; }
	size = type_size(&sv->type, &align);
	return size > 8;
}

static void gen_atomic_store_aggregate(void) { MCC_TRACE("enter\n");
	CType at = vtop->type;
	int size, align, s_v;

	atomic_lowering++;
	at.t &= ~VT_QUALIFY;
	size = type_size(&at, &align);
	s_v = alloc_local_slot(size, align);

	mk_pointer(&vtop->type);
	gaddrof();
	expr_eq();
	gen_assign_cast(&at);
	vset(&at, VT_LOCAL | VT_LVAL, s_v);
	vswap();
	vstore();
	vpop();
	vset(&at, VT_LOCAL | VT_LVAL, s_v);
	mk_pointer(&vtop->type);
	gaddrof();
	vpushi(0x5);
	vpushi(size);
	gen_cast_s(VT_SIZE_T);
	vrott(4);
	vpush_helper_func(TOK___atomic_store);
	vrott(5);
	gfunc_call(4);
	vset(&at, VT_LOCAL | VT_LVAL, s_v);
	atomic_lowering--;
}

static void gen_atomic_load_aggregate(void) { MCC_TRACE("enter\n");
	CType at = vtop->type;
	int size, align, s_v;

	atomic_lowering++;
	at.t &= ~VT_QUALIFY;
	size = type_size(&at, &align);
	s_v = alloc_local_slot(size, align);

	mk_pointer(&vtop->type);
	gaddrof();
	vset(&at, VT_LOCAL | VT_LVAL, s_v);
	mk_pointer(&vtop->type);
	gaddrof();
	vpushi(0x5);
	vpushi(size);
	gen_cast_s(VT_SIZE_T);
	vrott(4);
	vpush_helper_func(TOK___atomic_load);
	vrott(5);
	gfunc_call(4);
	vset(&at, VT_LOCAL | VT_LVAL, s_v);
	atomic_lowering--;
}

static void gen_atomic_load_scalar(void) { MCC_TRACE("enter\n");
	CType at = vtop->type;
	int size, align, bt;
	char buf[40];

	atomic_lowering++;
	at.t &= ~VT_QUALIFY;
	size = type_size(&at, &align);
	mk_pointer(&vtop->type);
	gaddrof();
	vpushi(0x5);
	snprintf(buf, sizeof buf, "__atomic_load_%d", size);
	vpush_helper_func(tok_alloc_const(buf));
	vrott(3);
	gfunc_call(2);
	vpush(&at);
	PUT_R_RET(vtop, at.t);
	bt = at.t & VT_BTYPE;
	if (bt == VT_BYTE || bt == VT_SHORT || bt == VT_BOOL)
		{ MCC_TRACE("br\n"); vtop->type.t = VT_INT; }
	atomic_lowering--;
}

static int format_func_spec(const char *name, int *is_scanf,
														int *fmt_arg, int *first_vararg) { MCC_TRACE("enter\n");
	static const struct
	{
		const char *n;
		char scanf, fmt, va;
	} tbl[] = {
			{"printf", 0, 1, 2},
			{"fprintf", 0, 2, 3},
			{"sprintf", 0, 2, 3},
			{"snprintf", 0, 3, 4},
			{"dprintf", 0, 2, 3},
			{"scanf", 1, 1, 2},
			{"fscanf", 1, 2, 3},
			{"sscanf", 1, 2, 3},
	};
	unsigned i;
	if (!name)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < sizeof tbl / sizeof tbl[0]; i++)
		{ MCC_TRACE("br\n"); if (!strcmp(name, tbl[i].n)) { MCC_TRACE("br\n");
			*is_scanf = tbl[i].scanf;
			*fmt_arg = tbl[i].fmt;
			*first_vararg = tbl[i].va;
			return 1;
		} }
	return 0;
}

static const char *format_str_literal(SValue *sv, int *avail) { MCC_TRACE("enter\n");
	Section *ssec;
	ElfSym *esym;
	unsigned long off;
	if (!(sv->type.t & VT_ARRAY) && (sv->type.t & VT_BTYPE) != VT_PTR)
		{ MCC_TRACE("br\n"); return NULL; }
	if ((sv->r & (VT_SYM | VT_CONST)) != (VT_SYM | VT_CONST) || !sv->sym || sv->sym->v < SYM_FIRST_ANOM)
		{ MCC_TRACE("br\n"); return NULL; }
	esym = elfsym(sv->sym);
	if (!esym || esym->st_shndx == SHN_UNDEF)
		{ MCC_TRACE("br\n"); return NULL; }
	ssec = mcc_state->sections[esym->st_shndx];
	if (!ssec || !ssec->data || ssec->reloc)
		{ MCC_TRACE("br\n"); return NULL; }
	off = esym->st_value + (unsigned)sv->c.i;
	if (off >= ssec->data_offset)
		{ MCC_TRACE("br\n"); return NULL; }
	*avail = (int)(ssec->data_offset - off);
	return (const char *)(ssec->data + off);
}

static int format_arg_class(CType *t) { MCC_TRACE("enter\n");
	int bt = t->t & VT_BTYPE;
	if (t->t & VT_ARRAY)
		{ MCC_TRACE("br\n"); return 2; }
	if (bt == VT_PTR || bt == VT_FUNC)
		{ MCC_TRACE("br\n"); return 2; }
	if (bt == VT_FLOAT || bt == VT_DOUBLE || bt == VT_LDOUBLE)
		{ MCC_TRACE("br\n"); return 1; }
	if (bt == VT_BOOL || bt == VT_BYTE || bt == VT_SHORT || bt == VT_INT || bt == VT_LLONG)
		{ MCC_TRACE("br\n"); return 0; }
	return 3;
}

static void format_check(int is_scanf, const char *fmt, int favail,
												 SValue *args, int nvar) { MCC_TRACE("enter\n");
	int i = 0, used = 0;
	while (i < favail && fmt[i]) { MCC_TRACE("br\n");
		int cls_want;
		char conv;
		if (fmt[i] != '%') { MCC_TRACE("br\n");
			i++;
			continue;
		}
		i++;
		if (i < favail && fmt[i] == '%') { MCC_TRACE("br\n");
			i++;
			continue;
		}
		while (i < favail && (fmt[i] == '-' || fmt[i] == '+' || fmt[i] == ' ' || fmt[i] == '#' || fmt[i] == '0'))
			{ MCC_TRACE("br\n"); i++; }
		if (is_scanf && i < favail && fmt[i] == '*') { MCC_TRACE("br\n");
			i++;
		}
		if (!is_scanf && i < favail && fmt[i] == '*') { MCC_TRACE("br\n");
			if (used < nvar && format_arg_class(&args[used].type) != 0)
				{ MCC_TRACE("br\n"); mcc_warning_c(warn_format)("field width '*' expects an int argument"); }
			used++;
			i++;
		} else
			{ MCC_TRACE("br\n"); while (i < favail && fmt[i] >= '0' && fmt[i] <= '9')
				{ MCC_TRACE("br\n"); i++; } }
		if (i < favail && fmt[i] == '.') { MCC_TRACE("br\n");
			i++;
			if (!is_scanf && i < favail && fmt[i] == '*') { MCC_TRACE("br\n");
				if (used < nvar && format_arg_class(&args[used].type) != 0)
					{ MCC_TRACE("br\n"); mcc_warning_c(warn_format)("precision '*' expects an int argument"); }
				used++;
				i++;
			} else
				{ MCC_TRACE("br\n"); while (i < favail && fmt[i] >= '0' && fmt[i] <= '9')
					{ MCC_TRACE("br\n"); i++; } }
		}
		while (i < favail && (fmt[i] == 'h' || fmt[i] == 'l' || fmt[i] == 'L' || fmt[i] == 'j' || fmt[i] == 'z' || fmt[i] == 't'))
			{ MCC_TRACE("br\n"); i++; }
		if (i >= favail || !fmt[i])
			{ MCC_TRACE("br\n"); break; }
		conv = fmt[i++];
		switch (conv) { MCC_TRACE("br\n");
		case 'd':
		case 'i':
		case 'u':
		case 'o':
		case 'x':
		case 'X':
		case 'c':
			cls_want = 0;
			break;
		case 'f':
		case 'F':
		case 'e':
		case 'E':
		case 'g':
		case 'G':
		case 'a':
		case 'A':
			cls_want = 1;
			break;
		case 's':
		case 'p':
		case 'n':
			cls_want = 2;
			break;
		default:
			continue;
		}
		int sc_base = cls_want;
		if (is_scanf)
			{ MCC_TRACE("br\n"); cls_want = 2; }
		if (used >= nvar) { MCC_TRACE("br\n");
			mcc_warning_c(warn_format)(
					"more conversions than arguments for '%s'",
					is_scanf ? "scanf" : "printf");
			return;
		}
		{
			int got = format_arg_class(&args[used].type);
			if (got <= 2 && got != cls_want) { MCC_TRACE("br\n");
				static const char *cn[] = {
						"an integer", "a floating",
						"a pointer"};
				mcc_warning_c(warn_format)(
						"format '%%%c' expects %s argument", conv, cn[cls_want]);
			} else if (is_scanf && got == 2 && sc_base <= 1 && (args[used].type.t & VT_BTYPE) == VT_PTR) { MCC_TRACE("br\n");
				int pc = format_arg_class(pointed_type(&args[used].type));
				if (pc <= 1 && pc != sc_base) { MCC_TRACE("br\n");
					static const char *cn2[] = {"int", "floating-point"};
					mcc_warning_c(warn_format)(
							"format '%%%c' expects a pointer to %s argument",
							conv, cn2[sc_base]);
				}
			}
		}
		used++;
	}
	if (used < nvar)
		{ MCC_TRACE("br\n"); mcc_warning_c(warn_format)("too many arguments for format string"); }
}

#define sizeof_parsed_type (mcc_state->gen_sizeof_parsed_type)

#define MACRO_EVAL_MAX_ARGS 16

static void macro_eval_call(int v) { MCC_TRACE("enter\n");
	int64_t args[MACRO_EVAL_MAX_ARGS], res;
	int n = 0;

	skip('(');
	if (tok != ')')
		{ MCC_TRACE("br\n"); for (;;) { MCC_TRACE("br\n");
			if (n >= MACRO_EVAL_MAX_ARGS)
				{ MCC_TRACE("br\n"); mcc_error("-fmacro-eval: too many arguments to '%s'",
									get_tok_str(v, NULL)); }
			args[n++] = expr_const64();
			if (tok == ')')
				{ MCC_TRACE("br\n"); break; }
			skip(',');
		} }
	skip(')');
	if (pp_macro_eval(v, args, n, &res))
		{ MCC_TRACE("br\n"); mcc_error("-fmacro-eval: cannot evaluate '%s' as a function",
							get_tok_str(v, NULL)); }
	vpushll(res);
}

static const struct {
	const char *name;
	unsigned char id, flt;
} foldmath_tab[] = {
		{"sin", 0, 0},	 {"sinf", 0, 1},   {"cos", 1, 0},	 {"cosf", 1, 1},
		{"exp", 2, 0},	 {"expf", 2, 1},   {"log", 3, 0},	 {"logf", 3, 1},
		{"log2", 4, 0},  {"log2f", 4, 1},  {"log10", 5, 0}, {"log10f", 5, 1},
		{"tan", 6, 0},	 {"tanf", 6, 1},   {"pow", 7, 0},	 {"powf", 7, 1},
		{"sinh", 8, 0},  {"sinhf", 8, 1},  {"cosh", 9, 0},	 {"coshf", 9, 1},
		{"tanh", 10, 0}, {"tanhf", 10, 1}, {"atan", 11, 0},  {"atanf", 11, 1},
		{"asin", 12, 0}, {"asinf", 12, 1}, {"acos", 13, 0},  {"acosf", 13, 1},
		{"cbrt", 14, 0}, {"cbrtf", 14, 1}, {"atan2", 15, 0}, {"atan2f", 15, 1},
		{"hypot", 16, 0},{"hypotf", 16, 1}, {"exp2", 17, 0},  {"exp2f", 17, 1},
		{"expm1", 18, 0},{"expm1f", 18, 1},{"log1p", 19, 0},  {"log1pf", 19, 1},
		{"asinh", 20, 0},{"asinhf", 20, 1},{"acosh", 21, 0},  {"acoshf", 21, 1},
		{"atanh", 22, 0},{"atanhf", 22, 1}, {"erf", 23, 0},	 {"erff", 23, 1},
		{"erfc", 24, 0}, {"erfcf", 24, 1}, {"lgamma", 25, 0}, {"lgammaf", 25, 1},
		{"tgamma", 26, 0}, {"tgammaf", 26, 1},
};

static double foldm_rint(double x) { MCC_TRACE("enter\n");
	double t = x + 6755399441055744.0;
	return t - 6755399441055744.0;
}

static double foldm_pow2(int e) { MCC_TRACE("enter\n");
	union {
		double d;
		uint64_t u;
	} u;
	u.u = (uint64_t)(0x3ff + e) << 52;
	return u.d;
}

static double foldm_scalbn(double x, int n) { MCC_TRACE("enter\n");
	double y = x;
	if (n > 1023) { MCC_TRACE("br\n");
		y *= foldm_pow2(1023);
		n -= 1023;
		if (n > 1023) { MCC_TRACE("br\n");
			y *= foldm_pow2(1023);
			n -= 1023;
			if (n > 1023)
				{ MCC_TRACE("br\n"); n = 1023; }
		}
	} else if (n < -1022) { MCC_TRACE("br\n");
		y *= foldm_pow2(-969);
		n += 969;
		if (n < -1022) { MCC_TRACE("br\n");
			y *= foldm_pow2(-969);
			n += 969;
			if (n < -1022)
				{ MCC_TRACE("br\n"); n = -1022; }
		}
	}
	return y * foldm_pow2(n);
}

static int32_t foldm_hi(double x) { MCC_TRACE("enter\n");
	uint64_t b;
	memcpy(&b, &x, sizeof b);
	return (int32_t)(uint32_t)(b >> 32);
}

static uint32_t foldm_lo(double x) { MCC_TRACE("enter\n");
	uint64_t b;
	memcpy(&b, &x, sizeof b);
	return (uint32_t)b;
}

static double foldm_sethi(double x, uint32_t hi) { MCC_TRACE("enter\n");
	uint64_t b;
	memcpy(&b, &x, sizeof b);
	b = (b & 0xffffffffull) | ((uint64_t)hi << 32);
	memcpy(&x, &b, sizeof x);
	return x;
}

static double foldm_hilo0(double x) { MCC_TRACE("enter\n");
	uint64_t b;
	memcpy(&b, &x, sizeof b);
	b &= 0xffffffff00000000ull;
	memcpy(&x, &b, sizeof x);
	return x;
}

static double foldm_fabs(double x) { MCC_TRACE("enter\n");
	return x < 0 ? -x : x;
}

static double foldm_ksin(double x) { MCC_TRACE("enter\n");
	double S1 = -1.66666666666666324348e-01, S2 = 8.33333333332248946124e-03,
				 S3 = -1.98412698298579493134e-04, S4 = 2.75573137070700676789e-06,
				 S5 = -2.50507602534068634195e-08, S6 = 1.58969099521155010221e-10;
	double z = x * x;
	double p = S1 + z * (S2 + z * (S3 + z * (S4 + z * (S5 + z * S6))));
	return x + (x * z) * p;
}

static double foldm_kcos(double x) { MCC_TRACE("enter\n");
	double C1 = 4.16666666666666019037e-02, C2 = -1.38888888888741095749e-03,
				 C3 = 2.48015872894767294178e-05, C4 = -2.75573143513906633035e-07,
				 C5 = 2.08757232129817482790e-09, C6 = -1.13596475577881948265e-11;
	double z = x * x;
	double p = C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6))));
	return 1.0 - 0.5 * z + (z * z) * p;
}

static int foldm_rem_pio2(double x, double *y0, double *y1) { MCC_TRACE("enter\n");
	double invpio2 = 6.36619772367581382433e-01;
	double pio2_1 = 1.57079632673412561417e+00,
				 pio2_1t = 6.07710050650619224932e-11;
	double pio2_2 = 6.07710050630396597660e-11,
				 pio2_2t = 2.02226624879595063154e-21;
	double pio2_3 = 2.02226624871116645580e-21,
				 pio2_3t = 8.47842766036889956997e-32;
	double fn = foldm_rint(x * invpio2);
	double r = x - fn * pio2_1, w = fn * pio2_1t, y = r - w;
	double t = r;
	w = fn * pio2_2;
	r = t - w;
	w = fn * pio2_2t - ((t - r) - w);
	y = r - w;
	t = r;
	w = fn * pio2_3;
	r = t - w;
	w = fn * pio2_3t - ((t - r) - w);
	y = r - w;
	*y0 = y;
	*y1 = (r - y) - w;
	return (int)fn;
}

static double foldm_sincos(int wantcos, double x) { MCC_TRACE("enter\n");
	double y0, y1;
	int n = foldm_rem_pio2(x, &y0, &y1);
	int q = (n + (wantcos ? 1 : 0)) & 3;
	double s = foldm_ksin(y0), c = foldm_kcos(y0);
	switch (q) { MCC_TRACE("br\n");
	case 0:
		return s;
	case 1:
		return c;
	case 2:
		return -s;
	default:
		return -c;
	}
}

static double foldm_ktan(double x, double y, int iy) { MCC_TRACE("enter\n");
	double one = 1.0;
	double pio4 = 7.85398163397448278999e-01,
				 pio4lo = 3.06161699786838301793e-17;
	double T0 = 3.33333333333334091986e-01, T1 = 1.33333333333201242699e-01,
				 T2 = 5.39682539762260521377e-02, T3 = 2.18694882948595424599e-02,
				 T4 = 8.86323982359930005737e-03, T5 = 3.59207910759131235356e-03,
				 T6 = 1.45620945432529025516e-03, T7 = 5.88041240820264096874e-04,
				 T8 = 2.46463134818469906812e-04, T9 = 7.81794442939557092300e-05,
				 T10 = 7.14072491382608190305e-05, T11 = -1.85586374855275456654e-05,
				 T12 = 2.59073051863633712884e-05;
	double z, r, v, w, s;
	int32_t hx = foldm_hi(x), ix = hx & 0x7fffffff;
	if (ix < 0x3e300000) { MCC_TRACE("br\n");
		if ((int)x == 0) { MCC_TRACE("br\n");
			if (((uint32_t)ix | foldm_lo(x) | (uint32_t)(iy + 1)) == 0)
				{ MCC_TRACE("br\n"); return one / foldm_fabs(x); }
			return (iy == 1) ? x : -one / x;
		}
	}
	if (ix >= 0x3FE59428) { MCC_TRACE("br\n");
		if (hx < 0) { MCC_TRACE("br\n");
			x = -x;
			y = -y;
		}
		z = pio4 - x;
		w = pio4lo - y;
		x = z + w;
		y = 0.0;
	}
	z = x * x;
	w = z * z;
	r = T1 + w * (T3 + w * (T5 + w * (T7 + w * (T9 + w * T11))));
	v = z * (T2 + w * (T4 + w * (T6 + w * (T8 + w * (T10 + w * T12)))));
	s = z * x;
	r = y + z * (s * (r + v) + y);
	r += T0 * s;
	w = x + r;
	if (ix >= 0x3FE59428) { MCC_TRACE("br\n");
		v = (double)iy;
		return (double)(1 - ((hx >> 30) & 2)) *
					 (v - 2.0 * (x - (w * w / (w + v) - r)));
	}
	if (iy == 1)
		{ MCC_TRACE("br\n"); return w; }
	else { MCC_TRACE("br\n");
		double a, t;
		z = w;
		z = foldm_hilo0(z);
		v = r - (z - x);
		t = a = -1.0 / w;
		t = foldm_hilo0(t);
		s = 1.0 + t * z;
		return t + a * (s + t * v);
	}
}

static double foldm_tan(double x) { MCC_TRACE("enter\n");
	double y0, y1;
	int n = foldm_rem_pio2(x, &y0, &y1);
	return foldm_ktan(y0, y1, 1 - ((n & 1) << 1));
}

static double foldm_exp(double x) { MCC_TRACE("enter\n");
	double invln2 = 1.44269504088896338700e+00;
	double ln2hi = 6.93147180369123816490e-01,
				 ln2lo = 1.90821492927058770002e-10;
	double P1 = 1.66666666666666019037e-01, P2 = -2.77777777770155933842e-03,
				 P3 = 6.61375632143793436117e-05, P4 = -1.65339022054652515390e-06,
				 P5 = 4.13813679705723846039e-08;
	double k = foldm_rint(x * invln2);
	int ki = (int)k;
	double hi = x - k * ln2hi;
	double lo = k * ln2lo;
	double r = hi - lo;
	double t = r * r;
	double c = r - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
	double y = 1.0 - ((lo - (r * c) / (2.0 - c)) - hi);
	return foldm_scalbn(y, ki);
}

static double foldm_log(double x) { MCC_TRACE("enter\n");
	double ln2_hi = 6.93147180369123816490e-01,
				 ln2_lo = 1.90821492927058770002e-10;
	double two54 = 1.80143985094819840000e+16;
	double Lg1 = 6.666666666666735130e-01, Lg2 = 3.999999999940941908e-01,
				 Lg3 = 2.857142874366239149e-01, Lg4 = 2.222219843214978396e-01,
				 Lg5 = 1.818357216161805012e-01, Lg6 = 1.531383769920937332e-01,
				 Lg7 = 1.479819860511658591e-01;
	double hfsq, f, s, z, R, w, t1, t2, dk;
	int32_t k, hx, i, j;
	uint32_t lx;
	hx = foldm_hi(x);
	lx = foldm_lo(x);
	k = 0;
	if (hx < 0x00100000) { MCC_TRACE("br\n");
		if (((hx & 0x7fffffff) | lx) == 0)
			{ MCC_TRACE("br\n"); return -two54 / 0.0; }
		if (hx < 0)
			{ MCC_TRACE("br\n"); return (x - x) / 0.0; }
		k -= 54;
		x *= two54;
		hx = foldm_hi(x);
	}
	if (hx >= 0x7ff00000)
		{ MCC_TRACE("br\n"); return x + x; }
	k += (hx >> 20) - 1023;
	hx &= 0x000fffff;
	i = (hx + 0x95f64) & 0x100000;
	x = foldm_sethi(x, (uint32_t)(hx | (i ^ 0x3ff00000)));
	k += (i >> 20);
	f = x - 1.0;
	if ((0x000fffff & (2 + hx)) < 3) { MCC_TRACE("br\n");
		if (f == 0.0) { MCC_TRACE("br\n");
			if (k == 0)
				{ MCC_TRACE("br\n"); return 0.0; }
			dk = (double)k;
			return dk * ln2_hi + dk * ln2_lo;
		}
		R = f * f * (0.5 - 0.33333333333333333 * f);
		if (k == 0)
			{ MCC_TRACE("br\n"); return f - R; }
		dk = (double)k;
		return dk * ln2_hi - ((R - dk * ln2_lo) - f);
	}
	s = f / (2.0 + f);
	dk = (double)k;
	z = s * s;
	i = hx - 0x6147a;
	w = z * z;
	j = 0x6b851 - hx;
	t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
	t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
	i |= j;
	R = t2 + t1;
	if (i > 0) { MCC_TRACE("br\n");
		hfsq = 0.5 * f * f;
		if (k == 0)
			{ MCC_TRACE("br\n"); return f - (hfsq - s * (hfsq + R)); }
		return dk * ln2_hi - ((hfsq - (s * (hfsq + R) + dk * ln2_lo)) - f);
	}
	if (k == 0)
		{ MCC_TRACE("br\n"); return f - s * (f - R); }
	return dk * ln2_hi - ((s * (f - R) - dk * ln2_lo) - f);
}

static double foldm_sinh_mag(double ax) { MCC_TRACE("enter\n");
	if (ax < 0.5) { MCC_TRACE("br\n");
		double z = ax * ax;
		double p = 1.0 +
			z * (1.66666666666666657415e-01 +
			z * (8.33333333333333321769e-03 +
			z * (1.98412698412698412526e-04 +
			z * (2.75573192239858925110e-06 +
			z * (2.50521083854417187751e-08 +
			z * 1.60590438368216145994e-10)))));
		return ax * p;
	}
	double ex = foldm_exp(ax);
	return 0.5 * (ex - 1.0 / ex);
}

static double foldm_cosh_mag(double ax) { MCC_TRACE("enter\n");
	double ex = foldm_exp(ax);
	return 0.5 * (ex + 1.0 / ex);
}

static double foldm_powi(double x, int n) { MCC_TRACE("enter\n");
	int neg = n < 0, i, m = neg ? -n : n;
	double r = 1.0;
	for (i = 0; i < m; i++)
		{ MCC_TRACE("br\n"); r *= x; }
	return neg ? 1.0 / r : r;
}

static double foldm_frombits(uint32_t hi, uint32_t lo) { MCC_TRACE("enter\n");
	uint64_t b = ((uint64_t)hi << 32) | lo;
	double x;
	memcpy(&x, &b, sizeof x);
	return x;
}

static double foldm_sqrt(double x) { MCC_TRACE("enter\n");
	int32_t hi;
	double y;
	if (x == 0.0)
		{ MCC_TRACE("br\n"); return x; }
	if (x < 0.0)
		{ MCC_TRACE("br\n"); return 0.0; }
	hi = foldm_hi(x);
	y = foldm_sethi(x, (uint32_t)(((hi - 0x3ff00000) >> 1) + 0x3ff00000));
	y = 0.5 * (y + x / y);
	y = 0.5 * (y + x / y);
	y = 0.5 * (y + x / y);
	y = 0.5 * (y + x / y);
	y = 0.5 * (y + x / y);
	return y;
}

static double foldm_atan(double x) { MCC_TRACE("enter\n");
	double atanhi[] = {4.63647609000806093515e-01, 7.85398163397448278999e-01,
										 9.82793723247329054082e-01, 1.57079632679489655800e+00};
	double atanlo[] = {2.26987774529616870924e-17, 3.06161699786838301793e-17,
										 1.39033110312309984516e-17, 6.12323399573676603587e-17};
	double aT[] = {3.33333333333329318027e-01,	-1.99999999998764832476e-01,
								 1.42857142725034663711e-01,	-1.11111104054623557880e-01,
								 9.09088713343650656196e-02,	-7.69187620504482999495e-02,
								 6.66107313738753120669e-02,	-5.83357013379057348645e-02,
								 4.97687799461593236017e-02,	-3.65315727442169155270e-02,
								 1.62858201153657823623e-02};
	double w, s1, s2, z;
	int32_t ix, hx, id;
	hx = foldm_hi(x);
	ix = hx & 0x7fffffff;
	if (ix >= 0x44100000) { MCC_TRACE("br\n");
		if (hx > 0)
			{ MCC_TRACE("br\n"); return atanhi[3] + atanlo[3]; }
		return -atanhi[3] - atanlo[3];
	}
	if (ix < 0x3fdc0000) { MCC_TRACE("br\n");
		if (ix < 0x3e400000)
			{ MCC_TRACE("br\n"); return x; }
		id = -1;
	} else { MCC_TRACE("br\n");
		x = foldm_fabs(x);
		if (ix < 0x3ff30000) { MCC_TRACE("br\n");
			if (ix < 0x3fe60000) { MCC_TRACE("br\n");
				id = 0;
				x = (2.0 * x - 1.0) / (2.0 + x);
			} else { MCC_TRACE("br\n");
				id = 1;
				x = (x - 1.0) / (x + 1.0);
			}
		} else { MCC_TRACE("br\n");
			if (ix < 0x40038000) { MCC_TRACE("br\n");
				id = 2;
				x = (x - 1.5) / (1.0 + 1.5 * x);
			} else { MCC_TRACE("br\n");
				id = 3;
				x = -1.0 / x;
			}
		}
	}
	z = x * x;
	w = z * z;
	s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
	s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
	if (id < 0)
		{ MCC_TRACE("br\n"); return x - x * (s1 + s2); }
	z = atanhi[id] - ((x * (s1 + s2) - atanlo[id]) - x);
	return (hx < 0) ? -z : z;
}

static double foldm_asin_r(double u) { MCC_TRACE("enter\n");
	double pS0 = 1.66666666666666657415e-01, pS1 = -3.25565818622400915405e-01,
				 pS2 = 2.01212532134862925881e-01, pS3 = -4.00555345006794114027e-02,
				 pS4 = 7.91534994289814532176e-04, pS5 = 3.47933107596021167570e-05;
	double qS1 = -2.40339491173441421878e+00, qS2 = 2.02094576023350569471e+00,
				 qS3 = -6.88283971605453293030e-01, qS4 = 7.70381505559019352791e-02;
	double p = u * (pS0 + u * (pS1 + u * (pS2 + u * (pS3 + u * (pS4 + u * pS5)))));
	double q = 1.0 + u * (qS1 + u * (qS2 + u * (qS3 + u * qS4)));
	return p / q;
}

static double foldm_asin(double x) { MCC_TRACE("enter\n");
	double pio2_hi = 1.57079632679489655800e+00,
				 pio2_lo = 6.12323399573676603587e-17,
				 pio4_hi = 7.85398163397448278999e-01;
	double t, w, p, q, c, r, s;
	int32_t hx = foldm_hi(x), ix = hx & 0x7fffffff;
	if (ix >= 0x3ff00000) { MCC_TRACE("br\n");
		uint32_t lx = foldm_lo(x);
		if (((ix - 0x3ff00000) | (int32_t)lx) == 0)
			{ MCC_TRACE("br\n"); return x * pio2_hi + x * pio2_lo; }
		return 0.0;
	} else if (ix < 0x3fe00000) { MCC_TRACE("br\n");
		if (ix < 0x3e500000)
			{ MCC_TRACE("br\n"); return x; }
		t = x * x;
		w = foldm_asin_r(t);
		return x + x * w;
	}
	w = 1.0 - foldm_fabs(x);
	t = w * 0.5;
	s = foldm_sqrt(t);
	if (ix >= 0x3FEF3333) { MCC_TRACE("br\n");
		w = foldm_asin_r(t);
		t = pio2_hi - (2.0 * (s + s * w) - pio2_lo);
	} else { MCC_TRACE("br\n");
		w = s;
		w = foldm_hilo0(w);
		c = (t - w * w) / (s + w);
		r = foldm_asin_r(t);
		p = 2.0 * s * r - (pio2_lo - 2.0 * c);
		q = pio4_hi - 2.0 * w;
		t = pio4_hi - (p - q);
	}
	return (hx > 0) ? t : -t;
}

static double foldm_acos(double x) { MCC_TRACE("enter\n");
	double pi = 3.14159265358979311600e+00,
				 pio2_hi = 1.57079632679489655800e+00,
				 pio2_lo = 6.12323399573676603587e-17;
	double z, r, w, s, c, df;
	int32_t hx = foldm_hi(x), ix = hx & 0x7fffffff;
	if (ix >= 0x3ff00000) { MCC_TRACE("br\n");
		uint32_t lx = foldm_lo(x);
		if (((ix - 0x3ff00000) | (int32_t)lx) == 0) { MCC_TRACE("br\n");
			if (hx > 0)
				{ MCC_TRACE("br\n"); return 0.0; }
			return pi + 2.0 * pio2_lo;
		}
		return 0.0;
	}
	if (ix < 0x3fe00000) { MCC_TRACE("br\n");
		if (ix <= 0x3c600000)
			{ MCC_TRACE("br\n"); return pio2_hi + pio2_lo; }
		z = x * x;
		r = foldm_asin_r(z);
		return pio2_hi - (x - (pio2_lo - x * r));
	} else if (hx < 0) { MCC_TRACE("br\n");
		z = (1.0 + x) * 0.5;
		s = foldm_sqrt(z);
		r = foldm_asin_r(z);
		w = r * s - pio2_lo;
		return pi - 2.0 * (s + w);
	} else { MCC_TRACE("br\n");
		z = (1.0 - x) * 0.5;
		s = foldm_sqrt(z);
		df = s;
		df = foldm_hilo0(df);
		c = (z - df * df) / (s + df);
		r = foldm_asin_r(z);
		w = r * s + c;
		return 2.0 * (df + w);
	}
}

static double foldm_cbrt(double x) { MCC_TRACE("enter\n");
	uint32_t B1 = 715094163u, B2 = 696219795u;
	double C = 5.42857142857142815906e-01, D = -7.05306122448979611050e-01,
				 E = 1.41428571428571436819e+00, F = 1.60714285714285720630e+00,
				 G = 3.57142857142857150787e-01;
	int32_t hx = foldm_hi(x);
	uint32_t sign = (uint32_t)hx & 0x80000000u;
	uint32_t high, low;
	double r, s, t, w;
	hx ^= (int32_t)sign;
	low = foldm_lo(x);
	if ((hx | (int32_t)low) == 0)
		{ MCC_TRACE("br\n"); return x; }
	x = foldm_sethi(x, (uint32_t)hx);
	if (hx < 0x00100000) { MCC_TRACE("br\n");
		t = foldm_frombits(0x43500000, 0);
		t *= x;
		high = (uint32_t)foldm_hi(t);
		t = foldm_sethi(t, high / 3 + B2);
	} else { MCC_TRACE("br\n");
		t = foldm_frombits((uint32_t)hx / 3 + B1, 0);
	}
	r = t * t / x;
	s = C + r * t;
	t *= G + F / (s + E + D / s);
	high = (uint32_t)foldm_hi(t);
	t = foldm_frombits(high + 1, 0);
	s = t * t;
	r = x / s;
	w = t + t;
	r = (r - t) / (w + r);
	t = t + t * r;
	t = foldm_sethi(t, (uint32_t)foldm_hi(t) | sign);
	return t;
}

static double foldm_hypot(double x, double y) { MCC_TRACE("enter\n");
	double a, b, t1, t2, y1, y2, w;
	int32_t j, k, ha, hb;
	ha = foldm_hi(x) & 0x7fffffff;
	hb = foldm_hi(y) & 0x7fffffff;
	if (hb > ha) { MCC_TRACE("br\n");
		a = y;
		b = x;
		j = ha;
		ha = hb;
		hb = j;
	} else { MCC_TRACE("br\n");
		a = x;
		b = y;
	}
	a = foldm_fabs(a);
	b = foldm_fabs(b);
	if ((ha - hb) > 0x3c00000)
		{ MCC_TRACE("br\n"); return a + b; }
	k = 0;
	if (ha > 0x5f300000) { MCC_TRACE("br\n");
		if (ha >= 0x7ff00000) { MCC_TRACE("br\n");
			uint32_t low;
			w = a + b;
			low = foldm_lo(a);
			if (((ha & 0xfffff) | (int32_t)low) == 0)
				{ MCC_TRACE("br\n"); w = a; }
			low = foldm_lo(b);
			if (((hb ^ 0x7ff00000) | (int32_t)low) == 0)
				{ MCC_TRACE("br\n"); w = b; }
			return w;
		}
		ha -= 0x25800000;
		hb -= 0x25800000;
		k += 600;
		a = foldm_sethi(a, (uint32_t)ha);
		b = foldm_sethi(b, (uint32_t)hb);
	}
	if (hb < 0x20b00000) { MCC_TRACE("br\n");
		if (hb <= 0x000fffff) { MCC_TRACE("br\n");
			uint32_t low = foldm_lo(b);
			if ((hb | (int32_t)low) == 0)
				{ MCC_TRACE("br\n"); return a; }
			t1 = foldm_frombits(0x7fd00000, 0);
			b *= t1;
			a *= t1;
			k -= 1022;
		} else { MCC_TRACE("br\n");
			ha += 0x25800000;
			hb += 0x25800000;
			k -= 600;
			a = foldm_sethi(a, (uint32_t)ha);
			b = foldm_sethi(b, (uint32_t)hb);
		}
	}
	w = a - b;
	if (w > b) { MCC_TRACE("br\n");
		t1 = foldm_frombits((uint32_t)ha, 0);
		t2 = a - t1;
		w = foldm_sqrt(t1 * t1 - (b * (-b) - t2 * (a + t1)));
	} else { MCC_TRACE("br\n");
		a = a + a;
		y1 = foldm_frombits((uint32_t)hb, 0);
		y2 = b - y1;
		t1 = foldm_frombits((uint32_t)(ha + 0x00100000), 0);
		t2 = a - t1;
		w = foldm_sqrt(t1 * y1 - (w * (-w) - (t1 * y2 + t2 * b)));
	}
	if (k != 0) { MCC_TRACE("br\n");
		uint32_t high;
		t1 = 1.0;
		high = (uint32_t)foldm_hi(t1);
		t1 = foldm_sethi(t1, high + ((uint32_t)k << 20));
		return t1 * w;
	}
	return w;
}

static double foldm_exp2(double x) { MCC_TRACE("enter\n");
	double ln2 = 0.69314718055994530942;
	double n = foldm_rint(x);
	int k = (int)n;
	double r = x - n;
	double y = foldm_exp(r * ln2);
	return foldm_scalbn(y, k);
}

static double foldm_expm1(double x) { MCC_TRACE("enter\n");
	double o_threshold = 7.09782712893383973096e+02;
	double ln2_hi = 6.93147180369123816490e-01,
				 ln2_lo = 1.90821492927058770002e-10;
	double invln2 = 1.44269504088896338700e+00;
	double Q1 = -3.33333333333331316428e-02, Q2 = 1.58730158725481460165e-03,
				 Q3 = -7.93650757867487942473e-05, Q4 = 4.00821782732936239552e-06,
				 Q5 = -2.01099218183624371326e-07;
	double y, hi, lo, c, t, e, hxs, hfx, r1, twopk;
	int k, sign;
	uint32_t hx;
	c = 0.0;
	k = 0;
	hx = (uint32_t)foldm_hi(x) & 0x7fffffff;
	sign = (foldm_hi(x) < 0);
	if (hx >= 0x4043687A) { MCC_TRACE("br\n");
		if (sign)
			{ MCC_TRACE("br\n"); return -1.0; }
		if (x > o_threshold)
			{ MCC_TRACE("br\n"); return x * foldm_pow2(1023); }
	}
	if (hx > 0x3fd62e42) { MCC_TRACE("br\n");
		if (hx < 0x3FF0A2B2) { MCC_TRACE("br\n");
			if (!sign) { MCC_TRACE("br\n");
				hi = x - ln2_hi;
				lo = ln2_lo;
				k = 1;
			} else { MCC_TRACE("br\n");
				hi = x + ln2_hi;
				lo = -ln2_lo;
				k = -1;
			}
		} else { MCC_TRACE("br\n");
			k = (int)(invln2 * x + (sign ? -0.5 : 0.5));
			t = k;
			hi = x - t * ln2_hi;
			lo = t * ln2_lo;
		}
		x = hi - lo;
		c = (hi - x) - lo;
	} else if (hx < 0x3c900000) { MCC_TRACE("br\n");
		return x;
	}
	hfx = 0.5 * x;
	hxs = x * hfx;
	r1 = 1.0 + hxs * (Q1 + hxs * (Q2 + hxs * (Q3 + hxs * (Q4 + hxs * Q5))));
	t = 3.0 - r1 * hfx;
	e = hxs * ((r1 - t) / (6.0 - x * t));
	if (k == 0)
		{ MCC_TRACE("br\n"); return x - (x * e - hxs); }
	e = x * (e - c) - c;
	e -= hxs;
	if (k == -1)
		{ MCC_TRACE("br\n"); return 0.5 * (x - e) - 0.5; }
	if (k == 1) { MCC_TRACE("br\n");
		if (x < -0.25)
			{ MCC_TRACE("br\n"); return -2.0 * (e - (x + 0.5)); }
		return 1.0 + 2.0 * (x - e);
	}
	twopk = foldm_frombits((uint32_t)((0x3ff + k) << 20), 0);
	if (k < 0 || k > 56) { MCC_TRACE("br\n");
		y = x - e + 1.0;
		if (k == 1024)
			{ MCC_TRACE("br\n"); y = y * 2.0 * foldm_pow2(1023); }
		else
			{ MCC_TRACE("br\n"); y = y * twopk; }
		return y - 1.0;
	}
	{
		double twonk = foldm_frombits((uint32_t)((0x3ff - k) << 20), 0);
		if (k < 20)
			{ MCC_TRACE("br\n"); y = (x - e + (1.0 - twonk)) * twopk; }
		else
			{ MCC_TRACE("br\n"); y = (x - e - twonk + 1.0) * twopk; }
	}
	return y;
}

static double foldm_log1p(double x) { MCC_TRACE("enter\n");
	double ln2_hi = 6.93147180369123816490e-01,
				 ln2_lo = 1.90821492927058770002e-10;
	double Lg1 = 6.666666666666735130e-01, Lg2 = 3.999999999940941908e-01,
				 Lg3 = 2.857142874366239149e-01, Lg4 = 2.222219843214978396e-01,
				 Lg5 = 1.818357216161805012e-01, Lg6 = 1.531383769920937332e-01,
				 Lg7 = 1.479819860511658591e-01;
	double hfsq, f, c, s, z, R, w, t1, t2, dk;
	uint32_t hx, hu;
	int k;
	f = 0.0;
	c = 0.0;
	hx = (uint32_t)foldm_hi(x);
	k = 1;
	if (hx < 0x3fda827a || (hx >> 31)) { MCC_TRACE("br\n");
		if (hx >= 0xbff00000) { MCC_TRACE("br\n");
			if (x == -1.0)
				{ MCC_TRACE("br\n"); return x / 0.0; }
			return (x - x) / 0.0;
		}
		if ((hx << 1) < (0x3ca00000u << 1))
			{ MCC_TRACE("br\n"); return x; }
		if (hx <= 0xbfd2bec4) { MCC_TRACE("br\n");
			k = 0;
			c = 0.0;
			f = x;
		}
	} else if (hx >= 0x7ff00000)
		{ MCC_TRACE("br\n"); return x; }
	if (k) { MCC_TRACE("br\n");
		double u = 1.0 + x;
		hu = (uint32_t)foldm_hi(u);
		hu += 0x3ff00000 - 0x3fe6a09e;
		k = (int)(hu >> 20) - 0x3ff;
		if (k < 54) { MCC_TRACE("br\n");
			c = k >= 2 ? 1.0 - (u - x) : x - (u - 1.0);
			c /= u;
		} else
			{ MCC_TRACE("br\n"); c = 0.0; }
		hu = (hu & 0x000fffff) + 0x3fe6a09e;
		u = foldm_sethi(u, hu);
		f = u - 1.0;
	}
	hfsq = 0.5 * f * f;
	s = f / (2.0 + f);
	z = s * s;
	w = z * z;
	t1 = w * (Lg2 + w * (Lg4 + w * Lg6));
	t2 = z * (Lg1 + w * (Lg3 + w * (Lg5 + w * Lg7)));
	R = t2 + t1;
	dk = (double)k;
	return s * (hfsq + R) + (dk * ln2_lo + c) - hfsq + f + dk * ln2_hi;
}

static double foldm_asinh(double x) { MCC_TRACE("enter\n");
	double ln2 = 0.693147180559945309417232121458176568;
	int32_t hx = foldm_hi(x);
	int s = (hx < 0);
	unsigned e = ((uint32_t)hx >> 20) & 0x7ff;
	double ax = foldm_fabs(x), w;
	if (e >= 0x3ff + 26) { MCC_TRACE("br\n");
		w = foldm_log(ax) + ln2;
	} else if (e >= 0x3ff + 1) { MCC_TRACE("br\n");
		w = foldm_log(2.0 * ax + 1.0 / (foldm_sqrt(ax * ax + 1.0) + ax));
	} else if (e >= 0x3ff - 26) { MCC_TRACE("br\n");
		w = foldm_log1p(ax + ax * ax / (foldm_sqrt(ax * ax + 1.0) + 1.0));
	} else { MCC_TRACE("br\n");
		w = ax;
	}
	return s ? -w : w;
}

static double foldm_acosh(double x) { MCC_TRACE("enter\n");
	double ln2 = 0.693147180559945309417232121458176568;
	int32_t hx = foldm_hi(x);
	unsigned e = ((uint32_t)hx >> 20) & 0x7ff;
	if (e < 0x3ff + 1)
		{ MCC_TRACE("br\n"); return foldm_log1p(x - 1.0 +
											 foldm_sqrt((x - 1.0) * (x - 1.0) + 2.0 * (x - 1.0))); }
	if (e < 0x3ff + 26)
		{ MCC_TRACE("br\n"); return foldm_log(2.0 * x - 1.0 / (x + foldm_sqrt(x * x - 1.0))); }
	return foldm_log(x) + ln2;
}

static double foldm_atanh(double x) { MCC_TRACE("enter\n");
	int32_t hx = foldm_hi(x);
	int s = (hx < 0);
	unsigned e = ((uint32_t)hx >> 20) & 0x7ff;
	double y = foldm_fabs(x);
	if (e < 0x3ff - 1) { MCC_TRACE("br\n");
		if (e >= 0x3ff - 32)
			{ MCC_TRACE("br\n"); y = 0.5 * foldm_log1p(2.0 * y + 2.0 * y * y / (1.0 - y)); }
	} else { MCC_TRACE("br\n");
		y = 0.5 * foldm_log1p(2.0 * (y / (1.0 - y)));
	}
	return s ? -y : y;
}

static double foldm_erfc1(double x) { MCC_TRACE("enter\n");
	double erx = 8.45062911510467529297e-01;
	double pa0 = -2.36211856075265944077e-03, pa1 = 4.14856118683748331666e-01,
				 pa2 = -3.72207876035701323847e-01, pa3 = 3.18346619901161753674e-01,
				 pa4 = -1.10894694282396677476e-01, pa5 = 3.54783043256182359371e-02,
				 pa6 = -2.16637559486879084300e-03;
	double qa1 = 1.06420880400844228286e-01, qa2 = 5.40397917702171048937e-01,
				 qa3 = 7.18286544141962662868e-02, qa4 = 1.26171219808761642112e-01,
				 qa5 = 1.36370839120290507362e-02, qa6 = 1.19844998467991074170e-02;
	double s, P, Q;
	s = foldm_fabs(x) - 1.0;
	P = pa0 + s * (pa1 + s * (pa2 + s * (pa3 + s * (pa4 + s * (pa5 + s * pa6)))));
	Q = 1.0 + s * (qa1 + s * (qa2 + s * (qa3 + s * (qa4 + s * (qa5 + s * qa6)))));
	return 1.0 - erx - P / Q;
}

static double foldm_erfc2(uint32_t ix, double x) { MCC_TRACE("enter\n");
	double ra0 = -9.86494403484714822705e-03, ra1 = -6.93858572707181764372e-01,
				 ra2 = -1.05586262253232909814e+01, ra3 = -6.23753324503260060396e+01,
				 ra4 = -1.62396669462573470355e+02, ra5 = -1.84605092906711035994e+02,
				 ra6 = -8.12874355063065934246e+01, ra7 = -9.81432934416914548592e+00;
	double sa1 = 1.96512716674392571292e+01, sa2 = 1.37657754143519042600e+02,
				 sa3 = 4.34565877475229228821e+02, sa4 = 6.45387271733267880336e+02,
				 sa5 = 4.29008140027567833386e+02, sa6 = 1.08635005541779435134e+02,
				 sa7 = 6.57024977031928170135e+00, sa8 = -6.04244152148580987438e-02;
	double rb0 = -9.86494292470009928597e-03, rb1 = -7.99283237680523006574e-01,
				 rb2 = -1.77579549177547519889e+01, rb3 = -1.60636384855821916062e+02,
				 rb4 = -6.37566443368389627722e+02, rb5 = -1.02509513161107724954e+03,
				 rb6 = -4.83519191608651397019e+02;
	double sb1 = 3.03380607434824582924e+01, sb2 = 3.25792512996573918826e+02,
				 sb3 = 1.53672958608443695994e+03, sb4 = 3.19985821950859553908e+03,
				 sb5 = 2.55305040643316442583e+03, sb6 = 4.74528541206955367215e+02,
				 sb7 = -2.24409524465858183362e+01;
	double s, R, S, z;
	if (ix < 0x3ff40000)
		{ MCC_TRACE("br\n"); return foldm_erfc1(x); }
	x = foldm_fabs(x);
	s = 1.0 / (x * x);
	if (ix < 0x4006db6d) { MCC_TRACE("br\n");
		R = ra0 +
				s * (ra1 +
						 s * (ra2 +
									s * (ra3 + s * (ra4 + s * (ra5 + s * (ra6 + s * ra7))))));
		S = 1.0 +
				s * (sa1 +
						 s * (sa2 +
									s * (sa3 +
											 s * (sa4 +
														s * (sa5 + s * (sa6 + s * (sa7 + s * sa8)))))));
	} else { MCC_TRACE("br\n");
		R = rb0 +
				s * (rb1 + s * (rb2 + s * (rb3 + s * (rb4 + s * (rb5 + s * rb6)))));
		S = 1.0 +
				s * (sb1 +
						 s * (sb2 +
									s * (sb3 + s * (sb4 + s * (sb5 + s * (sb6 + s * sb7))))));
	}
	z = foldm_hilo0(x);
	return foldm_exp(-z * z - 0.5625) * foldm_exp((z - x) * (z + x) + R / S) / x;
}

static double foldm_erf(double x) { MCC_TRACE("enter\n");
	double efx8 = 1.02703333676410069053e+00;
	double pp0 = 1.28379167095512558561e-01, pp1 = -3.25042107247001499370e-01,
				 pp2 = -2.84817495755985104766e-02, pp3 = -5.77027029648944159157e-03,
				 pp4 = -2.37630166566501626084e-05;
	double qq1 = 3.97917223959155352819e-01, qq2 = 6.50222499887672944485e-02,
				 qq3 = 5.08130628187576562776e-03, qq4 = 1.32494738004321644526e-04,
				 qq5 = -3.96022827877536812320e-06;
	double r, s, z, y;
	uint32_t ix;
	int sign;
	ix = (uint32_t)foldm_hi(x);
	sign = (int)(ix >> 31);
	ix &= 0x7fffffff;
	if (ix >= 0x7ff00000)
		{ MCC_TRACE("br\n"); return 1.0 - 2.0 * sign + 1.0 / x; }
	if (ix < 0x3feb0000) { MCC_TRACE("br\n");
		if (ix < 0x3e300000)
			{ MCC_TRACE("br\n"); return 0.125 * (8.0 * x + efx8 * x); }
		z = x * x;
		r = pp0 + z * (pp1 + z * (pp2 + z * (pp3 + z * pp4)));
		s = 1.0 + z * (qq1 + z * (qq2 + z * (qq3 + z * (qq4 + z * qq5))));
		y = r / s;
		return x + x * y;
	}
	if (ix < 0x40180000)
		{ MCC_TRACE("br\n"); y = 1.0 - foldm_erfc2(ix, x); }
	else
		{ MCC_TRACE("br\n"); y = 1.0 - foldm_pow2(-1022); }
	return sign ? -y : y;
}

static double foldm_erfc(double x) { MCC_TRACE("enter\n");
	double pp0 = 1.28379167095512558561e-01, pp1 = -3.25042107247001499370e-01,
				 pp2 = -2.84817495755985104766e-02, pp3 = -5.77027029648944159157e-03,
				 pp4 = -2.37630166566501626084e-05;
	double qq1 = 3.97917223959155352819e-01, qq2 = 6.50222499887672944485e-02,
				 qq3 = 5.08130628187576562776e-03, qq4 = 1.32494738004321644526e-04,
				 qq5 = -3.96022827877536812320e-06;
	double r, s, z, y;
	uint32_t ix;
	int sign;
	ix = (uint32_t)foldm_hi(x);
	sign = (int)(ix >> 31);
	ix &= 0x7fffffff;
	if (ix >= 0x7ff00000)
		{ MCC_TRACE("br\n"); return 2.0 * sign + 1.0 / x; }
	if (ix < 0x3feb0000) { MCC_TRACE("br\n");
		if (ix < 0x3c700000)
			{ MCC_TRACE("br\n"); return 1.0 - x; }
		z = x * x;
		r = pp0 + z * (pp1 + z * (pp2 + z * (pp3 + z * pp4)));
		s = 1.0 + z * (qq1 + z * (qq2 + z * (qq3 + z * (qq4 + z * qq5))));
		y = r / s;
		if (sign || ix < 0x3fd00000)
			{ MCC_TRACE("br\n"); return 1.0 - (x + x * y); }
		return 0.5 - (x - 0.5 + x * y);
	}
	if (ix < 0x403c0000)
		{ MCC_TRACE("br\n"); return sign ? 2.0 - foldm_erfc2(ix, x) : foldm_erfc2(ix, x); }
	return sign ? 2.0 - foldm_pow2(-1022)
							: foldm_pow2(-1022) * foldm_pow2(-1022);
}

static double foldm_floor(double x) { MCC_TRACE("enter\n");
	double toint = 4503599627370496.0;
	int e = (foldm_hi(x) >> 20) & 0x7ff;
	double y;
	if (e >= 0x3ff + 52 || x == 0.0)
		{ MCC_TRACE("br\n"); return x; }
	if (foldm_hi(x) < 0)
		{ MCC_TRACE("br\n"); y = x - toint + toint - x; }
	else
		{ MCC_TRACE("br\n"); y = x + toint - toint - x; }
	if (e <= 0x3ff - 1)
		{ MCC_TRACE("br\n"); return foldm_hi(x) < 0 ? -1.0 : 0.0; }
	if (y > 0)
		{ MCC_TRACE("br\n"); return x + y - 1.0; }
	return x + y;
}

static double foldm_powg(double x, double y) { MCC_TRACE("enter\n");
	double bp[] = {1.0, 1.5};
	double dp_h[] = {0.0, 5.84962487220764160156e-01};
	double dp_l[] = {0.0, 1.35003920212974897128e-08};
	double one = 1.0, two = 2.0, two53 = 9007199254740992.0;
	double L1 = 5.99999999999994648725e-01, L2 = 4.28571428578550184252e-01,
				 L3 = 3.33333329818377432918e-01, L4 = 2.72728123808534006489e-01,
				 L5 = 2.30660745775561366331e-01, L6 = 2.06975017800338417784e-01;
	double P1 = 1.66666666666666019037e-01, P2 = -2.77777777770155933842e-03,
				 P3 = 6.61375632143793436117e-05, P4 = -1.65339022054652515390e-06,
				 P5 = 4.13813679705723846039e-08;
	double lg2 = 6.93147180559945286227e-01, lg2_h = 6.93147182464599609375e-01,
				 lg2_l = -1.90465429995776804525e-09;
	double cp = 9.61796693925975554329e-01, cp_h = 9.61796700954437255859e-01,
				 cp_l = -7.02846165095275826516e-09;
	double z, ax, z_h, z_l, p_h, p_l, y1, t1, t2, r, s, t, u, v, w, ss, s2, s_h,
			s_l, t_h, t_l;
	int32_t i, j, k, n, hx, ix;
	hx = foldm_hi(x);
	ix = hx & 0x7fffffff;
	ax = x;
	n = 0;
	if (ix < 0x00100000) { MCC_TRACE("br\n");
		ax *= two53;
		n -= 53;
		ix = foldm_hi(ax);
	}
	n += (ix >> 20) - 0x3ff;
	j = ix & 0x000fffff;
	ix = j | 0x3ff00000;
	if (j <= 0x3988E)
		{ MCC_TRACE("br\n"); k = 0; }
	else if (j < 0xBB67A)
		{ MCC_TRACE("br\n"); k = 1; }
	else { MCC_TRACE("br\n");
		k = 0;
		n += 1;
		ix -= 0x00100000;
	}
	ax = foldm_sethi(ax, (uint32_t)ix);
	u = ax - bp[k];
	v = one / (ax + bp[k]);
	ss = u * v;
	s_h = foldm_hilo0(ss);
	t_h = foldm_frombits(
			(uint32_t)((((ix >> 1) & 0xfffff000) | 0x20000000) + 0x00080000 +
								 (k << 18)),
			0);
	t_l = ax - (t_h - bp[k]);
	s_l = v * ((u - s_h * t_h) - s_h * t_l);
	s2 = ss * ss;
	r = s2 * s2 * (L1 + s2 * (L2 + s2 * (L3 + s2 * (L4 + s2 * (L5 + s2 * L6)))));
	r += s_l * (s_h + ss);
	s2 = s_h * s_h;
	t_h = foldm_hilo0(3.0 + s2 + r);
	t_l = r - ((t_h - 3.0) - s2);
	u = s_h * t_h;
	v = s_l * t_h + t_l * ss;
	p_h = foldm_hilo0(u + v);
	p_l = v - (p_h - u);
	z_h = cp_h * p_h;
	z_l = cp_l * p_h + p_l * cp + dp_l[k];
	t = (double)n;
	t1 = foldm_hilo0(((z_h + z_l) + dp_h[k]) + t);
	t2 = z_l - (((t1 - t) - dp_h[k]) - z_h);
	s = 1.0;
	y1 = foldm_hilo0(y);
	p_l = (y - y1) * t1 + y * t2;
	p_h = y1 * t1;
	z = p_l + p_h;
	j = foldm_hi(z);
	if (j >= 0x40900000)
		{ MCC_TRACE("br\n"); return foldm_pow2(1023) * foldm_pow2(1023); }
	if ((uint32_t)(j & 0x7fffffff) >= 0x4090cc00)
		{ MCC_TRACE("br\n"); return foldm_pow2(-1022) * foldm_pow2(-1022) * 0.0; }
	i = j & 0x7fffffff;
	k = (i >> 20) - 0x3ff;
	n = 0;
	if (i > 0x3fe00000) { MCC_TRACE("br\n");
		n = j + (0x00100000 >> (k + 1));
		k = ((n & 0x7fffffff) >> 20) - 0x3ff;
		t = foldm_sethi(0.0, (uint32_t)(n & ~(0x000fffff >> k)));
		n = ((n & 0x000fffff) | 0x00100000) >> (20 - k);
		if (j < 0)
			{ MCC_TRACE("br\n"); n = -n; }
		p_h -= t;
	}
	t = foldm_hilo0(p_l + p_h);
	u = t * lg2_h;
	v = (p_l - (t - p_h)) * lg2 + t * lg2_l;
	z = u + v;
	w = v - (z - u);
	t = z * z;
	t1 = z - t * (P1 + t * (P2 + t * (P3 + t * (P4 + t * P5))));
	r = (z * t1) / (t1 - two) - (w + z * w);
	z = one - (r - z);
	j = foldm_hi(z);
	j += (n << 20);
	if ((j >> 20) <= 0)
		{ MCC_TRACE("br\n"); z = foldm_scalbn(z, n); }
	else
		{ MCC_TRACE("br\n"); z = foldm_sethi(z, (uint32_t)foldm_hi(z) + (uint32_t)(n << 20)); }
	return s * z;
}

static double foldm_lgamma(double x) { MCC_TRACE("enter\n");
	double a0 = 7.72156649015328655494e-02, a1 = 3.22467033424113591611e-01,
				 a2 = 6.73523010531292681824e-02, a3 = 2.05808084325167332806e-02,
				 a4 = 7.38555086081402883957e-03, a5 = 2.89051383673415629091e-03,
				 a6 = 1.19270763183362067845e-03, a7 = 5.10069792153511336608e-04,
				 a8 = 2.20862790713908385557e-04, a9 = 1.08011567247583939954e-04,
				 a10 = 2.52144565451257326939e-05, a11 = 4.48640949618915160150e-05;
	double tc = 1.46163214496836224576e+00, tf = -1.21486290535849611461e-01,
				 tt = -3.63867699703950536541e-18;
	double t0 = 4.83836122723810047042e-01, t1c = -1.47587722994593911752e-01,
				 t2c = 6.46249402391333854778e-02, t3c = -3.27885410759859649565e-02,
				 t4c = 1.79706750811820387126e-02, t5c = -1.03142241298341437450e-02,
				 t6c = 6.10053870246291332635e-03, t7c = -3.68452016781138256760e-03,
				 t8c = 2.25964780900612472250e-03, t9c = -1.40346469989232843813e-03,
				 t10c = 8.81081882437654011382e-04, t11c = -5.38595305356740546715e-04,
				 t12c = 3.15632070903625950361e-04, t13c = -3.12754168375120860518e-04,
				 t14c = 3.35529192635519073543e-04;
	double u0 = -7.72156649015328655494e-02, u1 = 6.32827064025093366517e-01,
				 u2 = 1.45492250137234768737e+00, u3 = 9.77717527963372745603e-01,
				 u4 = 2.28963728064692451092e-01, u5 = 1.33810918536787660377e-02;
	double v1 = 2.45597793713041134822e+00, v2 = 2.12848976379893395361e+00,
				 v3 = 7.69285150456672783825e-01, v4 = 1.04222645593369134254e-01,
				 v5 = 3.21709242282423911810e-03;
	double s0 = -7.72156649015328655494e-02, s1 = 2.14982415960608852501e-01,
				 s2c = 3.25778796408930981787e-01, s3 = 1.46350472652464452805e-01,
				 s4 = 2.66422703033638609560e-02, s5 = 1.84028451407337715652e-03,
				 s6 = 3.19475326584100867617e-05;
	double r1 = 1.39200533467621045958e+00, r2 = 7.21935547567138069525e-01,
				 r3 = 1.71933865632803078993e-01, r4 = 1.86459191715652901344e-02,
				 r5 = 7.77942496381893596434e-04, r6 = 7.32668430744625636189e-06;
	double w0 = 4.18938533204672725052e-01, w1 = 8.33333333333329678849e-02,
				 w2 = -2.77777777728775536470e-03, w3 = 7.93650558643019558500e-04,
				 w4 = -5.95187557450339963135e-04, w5 = 8.36339918996282139126e-04,
				 w6 = -1.63092934096575273989e-03;
	double t, y, z, p, p1, p2, p3, q, r, w;
	uint32_t ix;
	int i;
	ix = (uint32_t)foldm_hi(x) & 0x7fffffff;
	if (ix >= 0x7ff00000)
		{ MCC_TRACE("br\n"); return x * x; }
	if (ix < ((0x3ffu - 70) << 20))
		{ MCC_TRACE("br\n"); return -foldm_log(x); }
	if ((ix == 0x3ff00000 || ix == 0x40000000) && foldm_lo(x) == 0)
		{ MCC_TRACE("br\n"); r = 0; }
	else if (ix < 0x40000000) { MCC_TRACE("br\n");
		if (ix <= 0x3feccccc) { MCC_TRACE("br\n");
			r = -foldm_log(x);
			if (ix >= 0x3FE76944) { MCC_TRACE("br\n");
				y = 1.0 - x;
				i = 0;
			} else if (ix >= 0x3FCDA661) { MCC_TRACE("br\n");
				y = x - (tc - 1.0);
				i = 1;
			} else { MCC_TRACE("br\n");
				y = x;
				i = 2;
			}
		} else { MCC_TRACE("br\n");
			r = 0.0;
			if (ix >= 0x3FFBB4C3) { MCC_TRACE("br\n");
				y = 2.0 - x;
				i = 0;
			} else if (ix >= 0x3FF3B4C4) { MCC_TRACE("br\n");
				y = x - tc;
				i = 1;
			} else { MCC_TRACE("br\n");
				y = x - 1.0;
				i = 2;
			}
		}
		switch (i) { MCC_TRACE("br\n");
		case 0:
			z = y * y;
			p1 = a0 + z * (a2 + z * (a4 + z * (a6 + z * (a8 + z * a10))));
			p2 = z * (a1 + z * (a3 + z * (a5 + z * (a7 + z * (a9 + z * a11)))));
			p = y * p1 + p2;
			r += (p - 0.5 * y);
			break;
		case 1:
			z = y * y;
			w = z * y;
			p1 = t0 + w * (t3c + w * (t6c + w * (t9c + w * t12c)));
			p2 = t1c + w * (t4c + w * (t7c + w * (t10c + w * t13c)));
			p3 = t2c + w * (t5c + w * (t8c + w * (t11c + w * t14c)));
			p = z * p1 - (tt - w * (p2 + y * p3));
			r += tf + p;
			break;
		case 2:
			p1 = y * (u0 + y * (u1 + y * (u2 + y * (u3 + y * (u4 + y * u5)))));
			p2 = 1.0 + y * (v1 + y * (v2 + y * (v3 + y * (v4 + y * v5))));
			r += -0.5 * y + p1 / p2;
		}
	} else if (ix < 0x40200000) { MCC_TRACE("br\n");
		i = (int)x;
		y = x - (double)i;
		p = y * (s0 + y * (s1 + y * (s2c + y * (s3 + y * (s4 + y * (s5 + y * s6))))));
		q = 1.0 + y * (r1 + y * (r2 + y * (r3 + y * (r4 + y * (r5 + y * r6)))));
		r = 0.5 * y + p / q;
		z = 1.0;
		switch (i) { MCC_TRACE("br\n");
		case 7:
			z *= y + 6.0;
		case 6:
			z *= y + 5.0;
		case 5:
			z *= y + 4.0;
		case 4:
			z *= y + 3.0;
		case 3:
			z *= y + 2.0;
			r += foldm_log(z);
			break;
		}
	} else if (ix < 0x43900000) { MCC_TRACE("br\n");
		t = foldm_log(x);
		z = 1.0 / x;
		y = z * z;
		w = w0 + z * (w1 + y * (w2 + y * (w3 + y * (w4 + y * (w5 + y * w6)))));
		r = (x - 0.5) * (t - 1.0) + w;
	} else
		{ MCC_TRACE("br\n"); r = x * (foldm_log(x) - 1.0); }
	return r;
}

static double foldm_gS(double x) { MCC_TRACE("enter\n");
	double Snum[] = {23531376880.410759688572007674451636754734846804940,
									 42919803642.649098768957899047001988850926355848959,
									 35711959237.355668049440185451547166705960488635843,
									 17921034426.037209699919755754458931112671403265390,
									 6039542586.3520280050642916443072979210699388420708,
									 1439720407.3117216736632230727949123939715485786772,
									 248874557.86205415651146038641322942321632125127801,
									 31426415.585400194380614231628318205362874684987640,
									 2876370.6289353724412254090516208496135991145378768,
									 186056.26539522349504029498971604569928220784236328,
									 8071.6720023658162106380029022722506138218516325024,
									 210.82427775157934587250973392071336271166969580291,
									 2.5066282746310002701649081771338373386264310793408};
	double Sden[] = {0,		 39916800, 120543840, 150917976, 105258076, 45995730,
									 13339535, 2637558,  357423,		32670,		1925,			66,
									 1};
	double num = 0, den = 0;
	int i;
	if (x < 8)
		{ MCC_TRACE("br\n"); for (i = 12; i >= 0; i--) { MCC_TRACE("br\n");
			num = num * x + Snum[i];
			den = den * x + Sden[i];
		} }
	else
		{ MCC_TRACE("br\n"); for (i = 0; i <= 12; i++) { MCC_TRACE("br\n");
			num = num / x + Snum[i];
			den = den / x + Sden[i];
		} }
	return num / den;
}

static double foldm_tgamma(double x) { MCC_TRACE("enter\n");
	double gmhalf = 5.524680040776729583740234375;
	double gfact[] = {1,				 1,					2,					6,
										24,				 120,				720,				5040.0,
										40320.0,	 362880.0,	3628800.0,	39916800.0,
										479001600.0, 6227020800.0,	87178291200.0,	1307674368000.0,
										20922789888000.0,	355687428096000.0,	6402373705728000.0,
										121645100408832000.0,	2432902008176640000.0,
										51090942171709440000.0, 1124000727777607680000.0};
	double absx, y, dy, z, r;
	uint32_t ix = (uint32_t)foldm_hi(x) & 0x7fffffff;
	if (ix >= 0x7ff00000)
		{ MCC_TRACE("br\n"); return x; }
	if (ix < ((0x3ffu - 54) << 20))
		{ MCC_TRACE("br\n"); return 1.0 / x; }
	if (x == foldm_floor(x)) { MCC_TRACE("br\n");
		if (x <= 23.0)
			{ MCC_TRACE("br\n"); return gfact[(int)x - 1]; }
	}
	if (ix >= 0x40670000)
		{ MCC_TRACE("br\n"); return x * foldm_pow2(1023); }
	absx = x;
	y = absx + gmhalf;
	if (absx > gmhalf) { MCC_TRACE("br\n");
		dy = y - absx;
		dy -= gmhalf;
	} else { MCC_TRACE("br\n");
		dy = y - gmhalf;
		dy -= absx;
	}
	z = absx - 0.5;
	r = foldm_gS(absx) * foldm_exp(-y);
	r += dy * (gmhalf + 0.5) * r / y;
	z = foldm_powg(y, 0.5 * z);
	y = r * z * z;
	return y;
}

static int foldmath_eval(int id, int flt, uint64_t inbits, uint64_t *out) { MCC_TRACE("enter\n");
	double x, res;
	uint64_t ib;
	if (flt) { MCC_TRACE("br\n");
		float xf;
		uint32_t b = (uint32_t)inbits;
		memcpy(&xf, &b, sizeof xf);
		x = (double)xf;
	} else { MCC_TRACE("br\n");
		memcpy(&x, &inbits, sizeof x);
	}
	if (x != x)
		{ MCC_TRACE("br\n"); return 0; }
	memcpy(&ib, &x, sizeof ib);
	switch (id) { MCC_TRACE("br\n");
	case 2: {
		int neg = (int)(ib >> 63);
		uint64_t infb = 0x7ff0000000000000ull;
		if ((ib & 0x7fffffffffffffffull) == 0x7ff0000000000000ull) { MCC_TRACE("br\n");
			if (neg)
				{ MCC_TRACE("br\n"); res = 0.0; }
			else
				{ MCC_TRACE("br\n"); memcpy(&res, &infb, sizeof res); }
		} else if (x > 709.782712893384) { MCC_TRACE("br\n");
			memcpy(&res, &infb, sizeof res);
		} else if (x < -745.2) { MCC_TRACE("br\n");
			res = 0.0;
		} else { MCC_TRACE("br\n");
			res = foldm_exp(x);
		}
		break;
	}
	case 0:
	case 1:
	case 6: {
		double ax;
		if ((ib & 0x7fffffffffffffffull) >= 0x7ff0000000000000ull)
			{ MCC_TRACE("br\n"); return 0; }
		ax = x < 0 ? -x : x;
		if (ax > 1048576.0)
			{ MCC_TRACE("br\n"); return 0; }
		res = id == 6 ? foldm_tan(x) : foldm_sincos(id, x);
		break;
	}
	case 3:
	case 4:
	case 5: {
		double l;
		if (x <= 0.0)
			{ MCC_TRACE("br\n"); return 0; }
		l = foldm_log(x);
		if (id == 3)
			{ MCC_TRACE("br\n"); res = l; }
		else if (id == 4)
			{ MCC_TRACE("br\n"); res = l / 0.69314718055994530942; }
		else
			{ MCC_TRACE("br\n"); res = l / 2.30258509299404568402; }
		break;
	}
	case 8:
	case 9:
	case 10: {
		double ax, s;
		if ((ib & 0x7fffffffffffffffull) >= 0x7ff0000000000000ull)
			{ MCC_TRACE("br\n"); return 0; }
		ax = x < 0 ? -x : x;
		s = x < 0 ? -1.0 : 1.0;
		if (id == 10) { MCC_TRACE("br\n");
			res = ax > 20.0 ? s * 1.0
											: s * (foldm_sinh_mag(ax) / foldm_cosh_mag(ax));
		} else { MCC_TRACE("br\n");
			if (ax > 709.782712893384)
				{ MCC_TRACE("br\n"); return 0; }
			res = id == 9 ? foldm_cosh_mag(ax) : s * foldm_sinh_mag(ax);
		}
		break;
	}
	case 11: {
		if ((ib & 0x7fffffffffffffffull) >= 0x7ff0000000000000ull)
			{ MCC_TRACE("br\n"); return 0; }
		res = foldm_atan(x);
		break;
	}
	case 12:
	case 13: {
		double ax = x < 0 ? -x : x;
		if (ax > 1.0)
			{ MCC_TRACE("br\n"); return 0; }
		res = id == 12 ? foldm_asin(x) : foldm_acos(x);
		break;
	}
	case 14: {
		if ((ib & 0x7fffffffffffffffull) >= 0x7ff0000000000000ull)
			{ MCC_TRACE("br\n"); return 0; }
		res = foldm_cbrt(x);
		break;
	}
	case 17: {
		uint64_t infb = 0x7ff0000000000000ull;
		if ((ib & 0x7fffffffffffffffull) >= 0x7ff0000000000000ull)
			{ MCC_TRACE("br\n"); return 0; }
		if (x >= 1024.0) { MCC_TRACE("br\n");
			memcpy(&res, &infb, sizeof res);
			break;
		}
		res = foldm_exp2(x);
		break;
	}
	case 18: {
		if ((ib & 0x7fffffffffffffffull) >= 0x7ff0000000000000ull)
			{ MCC_TRACE("br\n"); return 0; }
		res = foldm_expm1(x);
		break;
	}
	case 19: {
		if ((ib & 0x7fffffffffffffffull) >= 0x7ff0000000000000ull)
			{ MCC_TRACE("br\n"); return 0; }
		if (x <= -1.0)
			{ MCC_TRACE("br\n"); return 0; }
		res = foldm_log1p(x);
		break;
	}
	case 20: {
		if ((ib & 0x7fffffffffffffffull) >= 0x7ff0000000000000ull)
			{ MCC_TRACE("br\n"); return 0; }
		res = foldm_asinh(x);
		break;
	}
	case 21: {
		if ((ib & 0x7fffffffffffffffull) >= 0x7ff0000000000000ull)
			{ MCC_TRACE("br\n"); return 0; }
		if (x < 1.0)
			{ MCC_TRACE("br\n"); return 0; }
		res = foldm_acosh(x);
		break;
	}
	case 22: {
		double ax = x < 0 ? -x : x;
		if ((ib & 0x7fffffffffffffffull) >= 0x7ff0000000000000ull)
			{ MCC_TRACE("br\n"); return 0; }
		if (ax >= 1.0)
			{ MCC_TRACE("br\n"); return 0; }
		res = foldm_atanh(x);
		break;
	}
	case 23: {
		res = foldm_erf(x);
		break;
	}
	case 24: {
		res = foldm_erfc(x);
		break;
	}
	case 25: {
		if (x <= 0.0)
			{ MCC_TRACE("br\n"); return 0; }
		res = foldm_lgamma(x);
		break;
	}
	case 26: {
		if (x <= 0.0)
			{ MCC_TRACE("br\n"); return 0; }
		res = foldm_tgamma(x);
		break;
	}
	default:
		return 0;
	}
	if (flt) { MCC_TRACE("br\n");
		float rf = (float)res;
		uint32_t rb;
		memcpy(&rb, &rf, sizeof rf);
		*out = rb;
	} else { MCC_TRACE("br\n");
		memcpy(out, &res, sizeof res);
	}
	return 1;
}

static int foldm_pow_eval(double x, double y, double *out) { MCC_TRACE("enter\n");
	uint64_t xb;
	double ry;
	if (x != x || y != y)
		{ MCC_TRACE("br\n"); return 0; }
	if (y == 0.0) { MCC_TRACE("br\n");
		*out = 1.0;
		return 1;
	}
	if (x == 1.0) { MCC_TRACE("br\n");
		*out = 1.0;
		return 1;
	}
	memcpy(&xb, &x, sizeof xb);
	if ((xb & 0x7fffffffffffffffull) >= 0x7ff0000000000000ull)
		{ MCC_TRACE("br\n"); return 0; }
	ry = foldm_rint(y);
	if (ry == y && x > 0.0 && ry >= -64.0 && ry <= 64.0) { MCC_TRACE("br\n");
		*out = foldm_powi(x, (int)ry);
		return 1;
	}
	return 0;
}

static int foldm_atan2_eval(double y, double x, double *out) { MCC_TRACE("enter\n");
	double pi = 3.1415926535897931160e+00, pi_o_2 = 1.5707963267948965580e+00,
				 pi_lo = 1.2246467991473531772e-16;
	int32_t hx, hy, ix, iy, k, m;
	uint32_t lx, ly;
	double z;
	if (x != x || y != y)
		{ MCC_TRACE("br\n"); return 0; }
	hx = foldm_hi(x);
	lx = foldm_lo(x);
	ix = hx & 0x7fffffff;
	hy = foldm_hi(y);
	ly = foldm_lo(y);
	iy = hy & 0x7fffffff;
	if (ix >= 0x7ff00000 || iy >= 0x7ff00000)
		{ MCC_TRACE("br\n"); return 0; }
	if (((hx - 0x3ff00000) | (int32_t)lx) == 0) { MCC_TRACE("br\n");
		*out = foldm_atan(y);
		return 1;
	}
	m = ((hy >> 31) & 1) | ((hx >> 30) & 2);
	if ((iy | (int32_t)ly) == 0) { MCC_TRACE("br\n");
		switch (m) { MCC_TRACE("br\n");
		case 0:
		case 1:
			*out = y;
			return 1;
		case 2:
			*out = pi;
			return 1;
		default:
			*out = -pi;
			return 1;
		}
	}
	if ((ix | (int32_t)lx) == 0) { MCC_TRACE("br\n");
		*out = (hy < 0) ? -pi_o_2 : pi_o_2;
		return 1;
	}
	k = (iy - ix) >> 20;
	if (k > 60) { MCC_TRACE("br\n");
		z = pi_o_2 + 0.5 * pi_lo;
		m &= 1;
	} else if (hx < 0 && k < -60) { MCC_TRACE("br\n");
		z = 0.0;
	} else { MCC_TRACE("br\n");
		z = foldm_atan(foldm_fabs(y / x));
	}
	switch (m) { MCC_TRACE("br\n");
	case 0:
		*out = z;
		return 1;
	case 1:
		*out = -z;
		return 1;
	case 2:
		*out = pi - (z - pi_lo);
		return 1;
	default:
		*out = (z - pi_lo) - pi;
		return 1;
	}
}

static int foldm_hypot_eval(double x, double y, double *out) { MCC_TRACE("enter\n");
	int32_t hx = foldm_hi(x), hy = foldm_hi(y);
	if (x != x || y != y)
		{ MCC_TRACE("br\n"); return 0; }
	if ((hx & 0x7fffffff) >= 0x7ff00000 || (hy & 0x7fffffff) >= 0x7ff00000)
		{ MCC_TRACE("br\n"); return 0; }
	*out = foldm_hypot(x, y);
	return 1;
}

static int foldm_isconst(SValue *v, int bt) { MCC_TRACE("enter\n");
	return (v->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST &&
				 (v->type.t & VT_BTYPE) == bt;
}

static double foldm_svd(SValue *v, int flt) { MCC_TRACE("enter\n");
	if (flt) { MCC_TRACE("br\n");
		float f;
		uint32_t b;
		memcpy(&b, &v->c.f, sizeof b);
		memcpy(&f, &b, sizeof f);
		return (double)f;
	} else { MCC_TRACE("br\n");
		double d;
		memcpy(&d, &v->c.d, sizeof d);
		return d;
	}
}

static void foldm_push(int bt, int flt, double res) { MCC_TRACE("enter\n");
	CValue cv;
	CType rtype;
	rtype.t = bt;
	rtype.ref = NULL;
	if (flt) { MCC_TRACE("br\n");
		float rf = (float)res;
		uint32_t b;
		memcpy(&b, &rf, sizeof rf);
		memcpy(&cv.f, &b, sizeof cv.f);
	} else { MCC_TRACE("br\n");
		memcpy(&cv.d, &res, sizeof cv.d);
	}
	vsetc(&rtype, VT_CONST, &cv);
}

static int foldmath_try(Sym *ftype, int nb_args) { MCC_TRACE("enter\n");
	SValue *fsv, *arg;
	Sym *cs;
	ElfSym *es;
	const char *nm;
	int nfn, bi, flt, bt, id;
	uint64_t inbits, res;

	if (nb_args < 1 || nb_args > 2)
		{ MCC_TRACE("br\n"); return 0; }
	fsv = vtop - nb_args;
	if (!(fsv->r & VT_SYM))
		{ MCC_TRACE("br\n"); return 0; }
	cs = fsv->sym;
	if (!cs || (cs->type.t & VT_BTYPE) != VT_FUNC || (cs->type.t & VT_STATIC))
		{ MCC_TRACE("br\n"); return 0; }
	es = elfsym(cs);
	if (es && es->st_shndx != SHN_UNDEF)
		{ MCC_TRACE("br\n"); return 0; }
	nm = get_tok_str(cs->v, NULL);
	nfn = (int)(sizeof foldmath_tab / sizeof *foldmath_tab);
	for (bi = 0; bi < nfn; bi++)
		{ MCC_TRACE("br\n"); if (!strcmp(nm, foldmath_tab[bi].name))
			{ MCC_TRACE("br\n"); break; } }
	if (bi == nfn)
		{ MCC_TRACE("br\n"); return 0; }
	id = foldmath_tab[bi].id;
	flt = foldmath_tab[bi].flt;
	bt = flt ? VT_FLOAT : VT_DOUBLE;
	if ((ftype->type.t & VT_BTYPE) != bt)
		{ MCC_TRACE("br\n"); return 0; }
	if (id == 7 || id == 15 || id == 16) { MCC_TRACE("br\n");
		double xb, yb, pres;
		int ok;
		if (nb_args != 2)
			{ MCC_TRACE("br\n"); return 0; }
		if (!foldm_isconst(vtop - 1, bt) || !foldm_isconst(vtop, bt))
			{ MCC_TRACE("br\n"); return 0; }
		xb = foldm_svd(vtop - 1, flt);
		yb = foldm_svd(vtop, flt);
		if (id == 7)
			{ MCC_TRACE("br\n"); ok = foldm_pow_eval(xb, yb, &pres); }
		else if (id == 15)
			{ MCC_TRACE("br\n"); ok = foldm_atan2_eval(xb, yb, &pres); }
		else
			{ MCC_TRACE("br\n"); ok = foldm_hypot_eval(xb, yb, &pres); }
		if (!ok)
			{ MCC_TRACE("br\n"); return 0; }
		vpop();
		vpop();
		vpop();
		foldm_push(bt, flt, pres);
		return 1;
	}
	if (nb_args != 1)
		{ MCC_TRACE("br\n"); return 0; }
	arg = vtop;
	if (!foldm_isconst(arg, bt))
		{ MCC_TRACE("br\n"); return 0; }
	if (flt) { MCC_TRACE("br\n");
		uint32_t b;
		memcpy(&b, &arg->c.f, sizeof b);
		inbits = b;
	} else { MCC_TRACE("br\n");
		memcpy(&inbits, &arg->c.d, sizeof inbits);
	}
	if (!foldmath_eval(id, flt, inbits, &res))
		{ MCC_TRACE("br\n"); return 0; }
	vpop();
	vpop();
	{
		double rd;
		if (flt) { MCC_TRACE("br\n");
			float rf;
			uint32_t b = (uint32_t)res;
			memcpy(&rf, &b, sizeof rf);
			rd = (double)rf;
		} else { MCC_TRACE("br\n");
			memcpy(&rd, &res, sizeof rd);
		}
		foldm_push(bt, flt, rd);
	}
	return 1;
}

/*
 * -ffold-math for the time-series forecasting formulas (mccforecast.h). A call to
 * one of the mcc_fc_<model> builtins with all-constant double arguments folds at
 * compile time to that model's one-step-ahead prediction over the argument vector
 * (mcc_fc_forecast uses the full ensemble). Same implementation as the -O4+ search
 * predictor — one module, two consumers.
 */
static const struct {
	const char *name;
	int model; /* index into ast_fc_models, or -1 for the ensemble */
} foldfc_tab[] = {
		{"mcc_fc_forecast", -1}, {"mcc_fc_rw", 0},     {"mcc_fc_ses", 1},
		{"mcc_fc_ar1", 2},       {"mcc_fc_lin", 3},    {"mcc_fc_pspline", 4},
		{"mcc_fc_gam", 5},       {"mcc_fc_bsts", 6},   {"mcc_fc_bridge", 7},
		{"mcc_fc_gp", 8},        {"mcc_fc_gbm", 9},    {"mcc_fc_holt", 10},
		{"mcc_fc_theilsen", 11}, {"mcc_fc_movmed", 12},
};

static int foldfc_try(Sym *ftype, int nb_args) { MCC_TRACE("enter\n");
	SValue *fsv;
	Sym *cs;
	ElfSym *es;
	const char *nm;
	double y[AST_FC_MAXN], pred;
	int i, nfc, model = -2;

	if (nb_args < 1 || nb_args > AST_FC_MAXN)
		{ MCC_TRACE("br\n"); return 0; }
	fsv = vtop - nb_args;
	if (!(fsv->r & VT_SYM))
		{ MCC_TRACE("br\n"); return 0; }
	cs = fsv->sym;
	if (!cs || (cs->type.t & VT_BTYPE) != VT_FUNC || (cs->type.t & VT_STATIC))
		{ MCC_TRACE("br\n"); return 0; }
	es = elfsym(cs);
	if (es && es->st_shndx != SHN_UNDEF)
		{ MCC_TRACE("br\n"); return 0; }
	if ((ftype->type.t & VT_BTYPE) != VT_DOUBLE)
		{ MCC_TRACE("br\n"); return 0; }
	nm = get_tok_str(cs->v, NULL);
	nfc = (int)(sizeof foldfc_tab / sizeof *foldfc_tab);
	for (i = 0; i < nfc; i++)
		{ MCC_TRACE("br\n"); if (!strcmp(nm, foldfc_tab[i].name)) { MCC_TRACE("br\n");
			model = foldfc_tab[i].model;
			break;
		} }
	if (model == -2)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < nb_args; i++) { MCC_TRACE("br\n");
		SValue *a = vtop - nb_args + 1 + i;
		if (!foldm_isconst(a, VT_DOUBLE))
			{ MCC_TRACE("br\n"); return 0; }
		y[i] = foldm_svd(a, 0);
	}
	pred = model < 0 ? ast_fc_forecast(y, nb_args) : ast_fc_call(model, y, nb_args);
	for (i = 0; i < nb_args; i++)
		{ MCC_TRACE("br\n"); vpop(); }
	vpop();
	foldm_push(VT_DOUBLE, 0, pred);
	return 1;
}

ST_FUNC void unary(void) { MCC_TRACE("enter\n");
	int n, t, align, size, r;
	CType type;
#if MCC_CONFIG_LSP
	uint32_t cst_um = CST_MARK();
	uint16_t cst_nk = 0;
#define CST_PRIMARY()                 \
	do {                                \
		CST_OPEN_AT(CST_Primary, cst_um); \
		CST_CLOSE();                      \
	} while (0)
#else
#define CST_PRIMARY() ((void)0)
#endif
	Sym *s;
	AttributeDef ad;

	if (debug_modes)
		{ MCC_TRACE("br\n"); mcc_debug_line(mcc_state), mcc_tcov_check_line(mcc_state, 1); }

	type.ref = NULL;
tok_next:
#if MCC_CONFIG_LSP
	switch (tok) { MCC_TRACE("br\n");
	case '*':
	case '&':
	case '!':
	case '~':
	case '+':
	case '-':
	case TOK_INC:
	case TOK_DEC:
	case TOK_REALPART1:
	case TOK_REALPART2:
	case TOK_IMAGPART1:
	case TOK_IMAGPART2:
	case TOK_SIZEOF:
	case TOK_ALIGNOF1:
	case TOK_ALIGNOF2:
	case TOK_ALIGNOF3:
		cst_nk = CST_Unary;
		break;
	default:
		break;
	}
#endif
	switch (tok) { MCC_TRACE("br\n");
	case TOK_EXTENSION:
		next();
		goto tok_next;
	case TOK_U16CHAR:
		t = VT_SHORT | VT_UNSIGNED;
		goto push_tokc;
	case TOK_U32CHAR:
		t = VT_INT | VT_UNSIGNED;
		goto push_tokc;
	case TOK_LCHAR:
#ifdef MCC_TARGET_PE
		t = VT_SHORT | VT_UNSIGNED;
		goto push_tokc;
#endif
	case TOK_CINT:
	case TOK_CCHAR:
		t = VT_INT;
	push_tokc:
		type.t = t;
		vsetc(&type, VT_CONST, &tokc);
		if (tok_imaginary) { MCC_TRACE("br\n");
#if MCC_CONFIG_OPTIMIZER
			ast_hook_imag_begin();
#endif
			gen_imaginary_complex(t);
#if MCC_CONFIG_OPTIMIZER
			ast_hook_imag_end(t);
#endif
		}
		next();
		CST_PRIMARY();
		break;
	case TOK_CUINT:
		t = VT_INT | VT_UNSIGNED;
		goto push_tokc;
	case TOK_CLLONG:
		t = VT_LLONG;
		goto push_tokc;
	case TOK_CULLONG:
		t = VT_LLONG | VT_UNSIGNED;
		goto push_tokc;
	case TOK_CFLOAT:
		t = VT_FLOAT;
		goto push_tokc;
	case TOK_CDOUBLE:
		t = VT_DOUBLE;
		goto push_tokc;
	case TOK_CLDOUBLE:
#ifdef MCC_USING_DOUBLE_FOR_LDOUBLE
		t = VT_DOUBLE | VT_LONG;
		tokc.d = tokc.ld;
#else
		t = VT_LDOUBLE;
#endif
		goto push_tokc;
	case TOK_CLONG:
		t = (LONG_SIZE == 8 ? VT_LLONG : VT_INT) | VT_LONG;
		goto push_tokc;
	case TOK_CULONG:
		t = (LONG_SIZE == 8 ? VT_LLONG : VT_INT) | VT_LONG | VT_UNSIGNED;
		goto push_tokc;
	case TOK___FUNCTION__:
		if (!gnu_ext)
			{ MCC_TRACE("br\n"); goto tok_identifier; }
		FALLTHROUGH;
	case TOK___FUNC__:
		if (!funcname[0])
			{ MCC_TRACE("br\n"); mcc_warning("'__func__' is not defined outside of function scope"); }
		tok = TOK_STR;
		cstr_reset(&tokcstr);
		cstr_cat(&tokcstr, funcname, 0);
		tokc.str.size = tokcstr.size;
		tokc.str.data = tokcstr.data;
		goto case_TOK_STR;
	case TOK_U16STR:
		t = VT_SHORT | VT_UNSIGNED;
		goto str_init;
	case TOK_U32STR:
		t = VT_INT | VT_UNSIGNED;
		goto str_init;
	case TOK_LSTR:
#ifdef MCC_TARGET_PE
		t = VT_SHORT | VT_UNSIGNED;
#else
		t = VT_INT;
#endif
		goto str_init;
	case TOK_U8STR:
		t = char_type.t;
		goto str_init;
	case TOK_STR:
	case_TOK_STR:
		t = char_type.t;
	str_init:
		if (mcc_state->warn_write_strings & WARN_ON)
			{ MCC_TRACE("br\n"); t |= VT_CONSTANT; }
		type.t = t;
		mk_pointer(&type);
		type.t |= VT_ARRAY;
		memset(&ad, 0, sizeof(AttributeDef));
		ad.section = rodata_section;
		decl_initializer_alloc(&type, &ad, VT_CONST, 2, 0, 0);
		CST_PRIMARY();
		break;
	case TOK_SOTYPE:
	case '(':
		t = tok;
		next();
#if MCC_CONFIG_LSP
		uint32_t cst_tm = CST_MARK();
#endif
		if (parse_btype(&type, &ad, 0)) { MCC_TRACE("br\n");
			type_decl(&type, &ad, &n, TYPE_ABSTRACT);
#if MCC_CONFIG_LSP
			CST_OPEN_AT(CST_TypeName, cst_tm);
			CST_CLOSE();
#endif
			skip(')');
			if (tok == '{') { MCC_TRACE("br\n");
				if (mcc_state->cversion < 199901)
					{ MCC_TRACE("br\n"); mcc_pedantic("compound literals are a C99 feature"); }
				if (global_expr) { MCC_TRACE("br\n");
					if (local_scope)
						{ MCC_TRACE("br\n"); mcc_error("initializer element is not constant"); }
					r = VT_CONST;
				} else
					{ MCC_TRACE("br\n"); r = VT_LOCAL; }
				if (!(type.t & VT_ARRAY))
					{ MCC_TRACE("br\n"); r |= VT_LVAL; }
				memset(&ad, 0, sizeof(AttributeDef));
				{
					int aci_prev = assign_ctx_is_init;
					assign_ctx_is_init = 1;
					decl_initializer_alloc(&type, &ad, r, 1, 0, 0);
					assign_ctx_is_init = aci_prev;
				}
			} else if (t == TOK_SOTYPE) { MCC_TRACE("br\n");
				sizeof_parsed_type = 1;
				vpush(&type);
				return;
			} else { MCC_TRACE("br\n");
				unary();
				if (type.t & (VT_ARRAY | VT_VLA))
					{ MCC_TRACE("br\n"); mcc_error("conversion to non-scalar type requested"); }
				if ((type.t & VT_BTYPE) == VT_STRUCT && type.ref->type.t != VT_UNION && !is_complex_type(&type)) { MCC_TRACE("br\n");
					if (!is_compatible_unqualified_types(&type, &vtop->type))
						{ MCC_TRACE("br\n"); mcc_error("conversion to non-scalar type requested"); }
					else
						{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids casting nonscalar to the same type"); }
				}
				gen_cast(&type);
				if ((type.t & VT_BTYPE) == VT_VOID)
					{ MCC_TRACE("br\n"); expr_has_effect = 1; }
				if ((vtop->r & VT_LVAL) && !nocode_wanted && !asm_lvalue_cast) { MCC_TRACE("br\n");
					int bt = vtop->type.t & VT_BTYPE;
					if (bt != VT_STRUCT && bt != VT_VOID && !is_complex_type(&vtop->type))
						{ MCC_TRACE("br\n"); gv(MCC_RC_TYPE(vtop->type.t)); }
				}
				CST_OPEN_AT(CST_Cast, cst_um);
				CST_CLOSE();
			}
		} else if (tok == '{') { MCC_TRACE("br\n");
			int saved_nocode_wanted = nocode_wanted;
			if (CONST_WANTED && !NOEVAL_WANTED)
				{ MCC_TRACE("br\n"); expect("constant"); }
			if (0 == local_scope)
				{ MCC_TRACE("br\n"); mcc_error("statement expression outside of function"); }
			save_regs(0);
			vpushi(0), vtop->type.t = VT_VOID;
			block(STMT_EXPR);
			if (saved_nocode_wanted)
				{ MCC_TRACE("br\n"); nocode_wanted = saved_nocode_wanted; }
			skip(')');
		} else { MCC_TRACE("br\n");
			gexpr();
			skip(')');
			CST_OPEN_AT(CST_Paren, cst_um);
			CST_CLOSE();
		}
		break;
	case '*':
		next();
		unary();
		indir();
		break;
	case '&':
		next();
		unary();
		if (vtop->type.t & VT_BITFIELD)
			{ MCC_TRACE("br\n"); mcc_error("cannot take address of bit-field"); }
		if (vtop->sym && vtop->sym->a.is_register)
			{ MCC_TRACE("br\n"); mcc_error("address of register variable '%s' requested",
								get_tok_str(vtop->sym->v, NULL)); }
		if ((vtop->type.t & VT_BTYPE) != VT_FUNC &&
				!(vtop->type.t & (VT_ARRAY | VT_VLA)))
			{ MCC_TRACE("br\n"); test_lvalue(); }
		if (vtop->r & VT_NONLVAL)
			{ MCC_TRACE("br\n"); mcc_error("cannot take the address of a function-call result"); }
		if (vtop->sym)
			{ MCC_TRACE("br\n"); vtop->sym->a.addrtaken = 1; }
		mk_pointer(&vtop->type);
		gaddrof();
		break;
	case '!':
		next();
		unary();
		gen_test_zero(TOK_EQ);
		break;
	case '~':
		next();
		unary();
		vpushi(-1);
		gen_op('^');
		break;
	case TOK_REALPART1:
	case TOK_REALPART2:
	case TOK_IMAGPART1:
	case TOK_IMAGPART2: {
		int imag = (tok == TOK_IMAGPART1 || tok == TOK_IMAGPART2);
		next();
		unary();
		if (is_complex_type(&vtop->type)) { MCC_TRACE("br\n");
			complex_part(imag);
		} else if (imag) { MCC_TRACE("br\n");
			CType bt = vtop->type;
			vpop();
			vpushi(0);
			gen_cast(&bt);
		}
	} break;
	case '+':
		next();
		unary();
		if ((vtop->type.t & VT_BTYPE) == VT_PTR)
			{ MCC_TRACE("br\n"); mcc_error("pointer not accepted for unary plus"); }
		if (!is_float(vtop->type.t)) { MCC_TRACE("br\n");
			vpushi(0);
			gen_op('+');
		}
		break;
	case TOK_SIZEOF:
	case TOK_ALIGNOF1:
	case TOK_ALIGNOF2:
	case TOK_ALIGNOF3:
		t = tok;
		next();
		sizeof_parsed_type = 0;
		if (tok == '(')
			{ MCC_TRACE("br\n"); tok = TOK_SOTYPE; }
		expr_type(&type, unary);
		if (type.t & VT_BITFIELD)
			{ MCC_TRACE("br\n"); mcc_error("'%s' cannot be applied to a bit-field",
								t == TOK_SIZEOF ? "sizeof" : "_Alignof"); }
		if (t == TOK_ALIGNOF3 && !sizeof_parsed_type)
			{ MCC_TRACE("br\n"); mcc_pedantic("ISO C does not allow '_Alignof' applied to an expression"); }
		if ((type.t & VT_BTYPE) == VT_FUNC)
			{ MCC_TRACE("br\n"); mcc_pedantic(t == TOK_SIZEOF
											 ? "'sizeof' applied to a function type"
											 : "'_Alignof' applied to a function type"); }
		if ((type.t & VT_BTYPE) == VT_VOID) { MCC_TRACE("br\n");
			if (t == TOK_SIZEOF)
				{ MCC_TRACE("br\n"); mcc_pedantic("'sizeof' applied to a void type"); }
			else if (t == TOK_ALIGNOF3)
				{ MCC_TRACE("br\n"); mcc_error("'_Alignof' applied to a void type"); }
		}
		if (t == TOK_SIZEOF) { MCC_TRACE("br\n");
			vpush_type_size(&type, &align);
			gen_cast_s(VT_SIZE_T);
		} else { MCC_TRACE("br\n");
			if (type_size(&type, &align) < 0)
				{ MCC_TRACE("br\n"); mcc_error("'_Alignof' applied to an incomplete type"); }
			s = NULL;
			if (vtop[1].r & VT_SYM)
				{ MCC_TRACE("br\n"); s = vtop[1].sym; }
			if (s && s->a.aligned)
				{ MCC_TRACE("br\n"); align = 1 << (s->a.aligned - 1); }
			vpushs(align);
		}
		break;

	case TOK_builtin_expect:
		next();
		skip('(');
		expr_eq();
		skip(',');
		expr_const64();
		skip(')');
		break;
	case TOK_builtin_types_compatible_p:
		parse_builtin_params(0, "tt");
		vtop[-1].type.t &= ~VT_QUALIFY;
		vtop[0].type.t &= ~VT_QUALIFY;
		n = is_compatible_types(&vtop[-1].type, &vtop[0].type);
		vtop -= 2;
		vpushi(n);
		break;
	case TOK_builtin_choose_expr: {
		int64_t c;
		next();
		skip('(');
		c = expr_const64();
		skip(',');
		if (!c) { MCC_TRACE("br\n");
			nocode_wanted++;
		}
		expr_eq();
		if (!c) { MCC_TRACE("br\n");
			vpop();
			nocode_wanted--;
		}
		skip(',');
		if (c) { MCC_TRACE("br\n");
			nocode_wanted++;
		}
		expr_eq();
		if (c) { MCC_TRACE("br\n");
			vpop();
			nocode_wanted--;
		}
		skip(')');
	} break;
	case TOK_builtin_complex: {
		CType cbase, ccplx;
		SValue r;
		next();
#if MCC_CONFIG_OPTIMIZER
		ast_hook_builtin_complex_begin();
#endif
		skip('(');
		expr_eq();
		skip(',');
		expr_eq();
		skip(')');
		cbase = vtop[-1].type;
		cbase.t &= (VT_BTYPE | VT_LONG);
		if (!is_float(cbase.t))
			{ MCC_TRACE("br\n"); cbase.t = VT_DOUBLE; }
		cbase.ref = NULL;
		mk_complex_type(&ccplx, &cbase);
		if ((vtop[-1].r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST && (vtop[0].r & (VT_VALMASK | VT_LVAL |
																																										VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
			init_params p = {.sec = rodata_section};
			unsigned long offset;
			int bsz, bal, csz, cal;
			bsz = type_size(&cbase, &bal);
			csz = type_size(&ccplx, &cal);
			if (NODATA_WANTED) { MCC_TRACE("br\n");
				csz = 0;
				cal = 1;
			}
			offset = section_add(p.sec, csz, cal);
			init_putv(&p, &cbase, offset + bsz);
			init_putv(&p, &cbase, offset);
			vpush_ref(&ccplx, p.sec, offset, csz);
			vtop->r |= VT_LVAL;
		} else { MCC_TRACE("br\n");
			cplx_local(&ccplx, &r);
			gen_cast(&cbase);
			cplx_store_part(&r, 1);
			gen_cast(&cbase);
			cplx_store_part(&r, 0);
			vpushv(&r);
		}
#if MCC_CONFIG_OPTIMIZER
		ast_hook_builtin_complex_end();
#endif
	} break;
	case TOK_builtin_constant_p:
		parse_builtin_params(1, "e");
		n = 1;
		if ((vtop->r & (VT_VALMASK | VT_LVAL)) != VT_CONST || ((vtop->r & VT_SYM) && vtop->sym->a.addrtaken))
			{ MCC_TRACE("br\n"); n = 0; }
		vtop--;
		vpushi(n);
		break;
	case TOK_builtin_unreachable:
		parse_builtin_params(0, "");
		type.t = VT_VOID;
		vpush(&type);
		CODE_OFF();
		break;
	case TOK_builtin_nan:
	case TOK_builtin_nanf:
	case TOK_builtin_nanl:
	case TOK_builtin_inf:
	case TOK_builtin_inff:
	case TOK_builtin_infl:
	case TOK_builtin_huge_val:
	case TOK_builtin_huge_valf:
	case TOK_builtin_huge_vall: {
		int btok = tok, fbt;
		unsigned long long bits;
		CValue cv;
		int is_nan = (btok == TOK_builtin_nan || btok == TOK_builtin_nanf || btok == TOK_builtin_nanl);
		int is_float = (btok == TOK_builtin_nanf || btok == TOK_builtin_inff || btok == TOK_builtin_huge_valf);
		int is_ld = (btok == TOK_builtin_nanl || btok == TOK_builtin_infl || btok == TOK_builtin_huge_vall);

		if (is_nan)
			{ MCC_TRACE("br\n"); parse_builtin_params(1, "e"); }
		else
			{ MCC_TRACE("br\n"); parse_builtin_params(0, ""); }
		if (is_nan)
			{ MCC_TRACE("br\n"); vtop--; }
		if (is_float) { MCC_TRACE("br\n");
			union {
				unsigned u;
				float f;
			} x;
			x.u = is_nan ? 0x7fc00000U : 0x7f800000U;
			cv.f = x.f;
			fbt = VT_FLOAT;
		} else { MCC_TRACE("br\n");
			union {
				unsigned long long u;
				double d;
			} x;
			x.u = is_nan ? 0x7ff8000000000000ULL : 0x7ff0000000000000ULL;
			cv.d = x.d;
			if (is_ld) { MCC_TRACE("br\n");
				cv.ld = (long double)x.d;
				fbt = VT_LDOUBLE;
			} else
				{ MCC_TRACE("br\n"); fbt = VT_DOUBLE; }
		}
		type.t = fbt;
		type.ref = NULL;
		vsetc(&type, VT_CONST, &cv);
		(void)bits;
	} break;
	case TOK_builtin_signbit:
	case TOK_builtin_signbitf:
	case TOK_builtin_signbitl:

	{
		int btok = tok, bt;
		parse_builtin_params(0, "e");
		bt = vtop->type.t & VT_BTYPE;
		if (bt != VT_FLOAT && bt != VT_DOUBLE && bt != VT_LDOUBLE)
			{ MCC_TRACE("br\n"); mcc_error("non-floating-point argument in call to function '%s'",
								get_tok_str(btok, NULL)); }
		if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
			int sb;
			if (bt == VT_FLOAT) { MCC_TRACE("br\n");
				uint32_t u;
				memcpy(&u, &vtop->c.f, 4);
				sb = u >> 31;
			} else if (bt == VT_DOUBLE) { MCC_TRACE("br\n");
				uint64_t u;
				memcpy(&u, &vtop->c.d, 8);
				sb = (int)(u >> 63);
			} else { MCC_TRACE("br\n");
				unsigned char lb[16];
				memset(lb, 0, sizeof lb);
				write_ldouble(lb, &vtop->c.ld);
#if defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64

				sb = lb[MCC_LDOUBLE_SIZE > 8 ? 9 : 7] >> 7;
#else
				sb = lb[MCC_LDOUBLE_SIZE - 1] >> 7;
#endif
			}
			vpop();
			vpushi(sb);
		} else { MCC_TRACE("br\n");
			vpush_helper_func(tok_alloc_const(
					bt == VT_FLOAT
							? "__mcc_signbitf"
					: bt == VT_LDOUBLE
							? "__mcc_signbitl"
							: "__mcc_signbit"));
			vrott(2);
			gfunc_call(1);
			vpushi(0);
			vtop->type.t = VT_INT;
			PUT_R_RET(vtop, VT_INT);
		}
	} break;
	case TOK_builtin_ffs:
	case TOK_builtin_ffsl:
	case TOK_builtin_ffsll:
	case TOK_builtin_clz:
	case TOK_builtin_clzl:
	case TOK_builtin_clzll:
	case TOK_builtin_ctz:
	case TOK_builtin_ctzl:
	case TOK_builtin_ctzll:
	case TOK_builtin_clrsb:
	case TOK_builtin_clrsbl:
	case TOK_builtin_clrsbll:
	case TOK_builtin_popcount:
	case TOK_builtin_popcountl:
	case TOK_builtin_popcountll:
	case TOK_builtin_parity:
	case TOK_builtin_parityl:
	case TOK_builtin_parityll: {
		int bop, bw, op_unsigned;
		int btok = tok;
		CType at;
		switch (btok) { MCC_TRACE("br\n");
		case TOK_builtin_ffs:
		case TOK_builtin_ffsl:
		case TOK_builtin_ffsll:
			bop = BB_FFS;
			op_unsigned = 0;
			break;
		case TOK_builtin_clz:
		case TOK_builtin_clzl:
		case TOK_builtin_clzll:
			bop = BB_CLZ;
			op_unsigned = 1;
			break;
		case TOK_builtin_ctz:
		case TOK_builtin_ctzl:
		case TOK_builtin_ctzll:
			bop = BB_CTZ;
			op_unsigned = 1;
			break;
		case TOK_builtin_clrsb:
		case TOK_builtin_clrsbl:
		case TOK_builtin_clrsbll:
			bop = BB_CLRSB;
			op_unsigned = 0;
			break;
		case TOK_builtin_popcount:
		case TOK_builtin_popcountl:
		case TOK_builtin_popcountll:
			bop = BB_POPCOUNT;
			op_unsigned = 1;
			break;
		default:
			bop = BB_PARITY;
			op_unsigned = 1;
			break;
		}
		switch (btok) { MCC_TRACE("br\n");
		case TOK_builtin_ffsll:
		case TOK_builtin_clzll:
		case TOK_builtin_ctzll:
		case TOK_builtin_clrsbll:
		case TOK_builtin_popcountll:
		case TOK_builtin_parityll:
			bw = 64;
			at.t = VT_LLONG;
			break;
		case TOK_builtin_ffsl:
		case TOK_builtin_clzl:
		case TOK_builtin_ctzl:
		case TOK_builtin_clrsbl:
		case TOK_builtin_popcountl:
		case TOK_builtin_parityl:
			bw = LONG_SIZE * 8;
			at.t = (LONG_SIZE == 8 ? VT_LLONG | VT_LONG : VT_INT);
			break;
		default:
			bw = 32;
			at.t = VT_INT;
			break;
		}
		at.ref = NULL;
		if (op_unsigned)
			{ MCC_TRACE("br\n"); at.t |= VT_UNSIGNED; }
		parse_builtin_params(0, "e");
		gen_assign_cast(&at);
		if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
			int64_t cv = vtop->c.i;
			vpop();
			vpushi(fold_bit_builtin(bop, bw, cv));
		} else { MCC_TRACE("br\n");
			vpush_helper_func(btok);
			vrott(2);
			gfunc_call(1);
			vpushi(0);
			vtop->type.t = VT_INT;
			PUT_R_RET(vtop, VT_INT);
		}
	} break;
	case TOK_builtin_bswap16:
	case TOK_builtin_bswap32:
	case TOK_builtin_bswap64: {
		int btok = tok;
		CType at;
		int rt = (btok == TOK_builtin_bswap16)
								 ? (VT_SHORT | VT_UNSIGNED)
						 : (btok == TOK_builtin_bswap32)
								 ? (VT_INT | VT_UNSIGNED)
								 : (VT_LLONG | VT_UNSIGNED);
		at.ref = NULL;
		at.t = rt;
		parse_builtin_params(0, "e");
		gen_assign_cast(&at);
		if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
			uint64_t u = (uint64_t)vtop->c.i, r2 = 0;
			int i, nb = (btok == TOK_builtin_bswap16)
											? 2
									: (btok == TOK_builtin_bswap32)
											? 4
											: 8;
			for (i = 0; i < nb; i++)
				{ MCC_TRACE("br\n"); r2 |= ((u >> (i * 8)) & 0xff) << ((nb - 1 - i) * 8); }
			vtop->c.i = r2;
			vtop->r = VT_CONST;
		} else { MCC_TRACE("br\n");
			vpush_helper_func(btok);
			vrott(2);
			gfunc_call(1);
			vpush(&at);
			PUT_R_RET(vtop, at.t);
		}
	} break;
	case TOK_builtin_frame_address:
	case TOK_builtin_return_address: {
		int tok1 = tok;
		int level;
		next();
		skip('(');
		level = expr_const();
		if (level < 0)
			{ MCC_TRACE("br\n"); mcc_error("%s only takes positive integers", get_tok_str(tok1, 0)); }
		skip(')');
		type.t = VT_VOID;
		mk_pointer(&type);
		vset(&type, VT_LOCAL, 0);
		while (level--) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_RISCV64
			vpushi(2 * MCC_PTR_SIZE);
			gen_op('-');
#endif
			mk_pointer(&vtop->type);
			indir();
		}
		if (tok1 == TOK_builtin_return_address) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_ARM
			vpushi(2 * MCC_PTR_SIZE);
			gen_op('+');
#elif defined MCC_TARGET_RISCV64
			vpushi(MCC_PTR_SIZE);
			gen_op('-');
#else
			vpushi(MCC_PTR_SIZE);
			gen_op('+');
#endif
			mk_pointer(&vtop->type);
			indir();
		}
	} break;
#ifdef MCC_TARGET_RISCV64
	case TOK_builtin_va_start:
		parse_builtin_params(0, "ee");
		check_va_start_register();
		check_va_start_last_param();
		r = vtop->r & VT_VALMASK;
		if (r == VT_LLOCAL)
			{ MCC_TRACE("br\n"); r = VT_LOCAL; }
		if (r != VT_LOCAL)
			{ MCC_TRACE("br\n"); mcc_error("__builtin_va_start expects a local variable"); }
		gen_va_start();
		vstore();
		break;
#endif
#ifdef MCC_TARGET_X86_64
#ifdef MCC_TARGET_PE
	case TOK_builtin_va_start:
		parse_builtin_params(0, "ee");
		check_va_start_register();
		check_va_start_last_param();
		r = vtop->r & VT_VALMASK;
		if (r == VT_LLOCAL)
			{ MCC_TRACE("br\n"); r = VT_LOCAL; }
		if (r != VT_LOCAL)
			{ MCC_TRACE("br\n"); mcc_error("__builtin_va_start expects a local variable"); }
		vtop->r = r;
		vtop->type = char_pointer_type;
		vtop->c.i += 8;
		vstore();
		break;
#else
	case TOK_builtin_va_arg_types:
		parse_builtin_params(0, "t");
		vpushi(classify_x86_64_va_arg(&vtop->type));
		vswap();
		vpop();
		break;
#endif
#endif

#ifdef MCC_TARGET_ARM64
	case TOK_builtin_va_start: {
		parse_builtin_params(0, "ee");
		check_va_start_register();
		check_va_start_last_param();
		gen_va_start();
		vpushi(0);
		vtop->type.t = VT_VOID;
		break;
	}
	case TOK_builtin_va_arg: {
		parse_builtin_params(0, "et");
		type = vtop->type;
		vpop();
		gen_va_arg(&type);
		vtop->type = type;
#if MCC_CONFIG_OPTIMIZER
		if (ast_active)
			{ MCC_TRACE("br\n"); ast_bail = 1; }
#endif
		break;
	}
#endif
#ifdef MCC_TARGET_ARM64
	case TOK___arm64_clear_cache: {
		parse_builtin_params(0, "ee");
		gen_clear_cache();
		vpushi(0);
		vtop->type.t = VT_VOID;
		break;
	}
#endif
#ifdef MCC_TARGET_RISCV64
	case TOK___riscv64_clear_cache: {
		parse_builtin_params(0, "ee");
		vpop();
		vpop();
		gen_clear_cache();
		vpushi(0);
		vtop->type.t = VT_VOID;
		break;
	}
#endif

	case TOK___atomic_store:
	case TOK___atomic_load:
	case TOK___atomic_exchange:
	case TOK___atomic_compare_exchange:
	case TOK___atomic_fetch_add:
	case TOK___atomic_fetch_sub:
	case TOK___atomic_fetch_or:
	case TOK___atomic_fetch_xor:
	case TOK___atomic_fetch_and:
	case TOK___atomic_fetch_nand:
	case TOK___atomic_add_fetch:
	case TOK___atomic_sub_fetch:
	case TOK___atomic_or_fetch:
	case TOK___atomic_xor_fetch:
	case TOK___atomic_and_fetch:
	case TOK___atomic_nand_fetch:
		parse_atomic(tok);
		break;

	case TOK_INC:
	case TOK_DEC:
		t = tok;
		next();
		unary();
		inc(0, t);
		expr_has_effect = 1;
		break;
	case '-':
		next();
		unary();
		if ((vtop->type.t & VT_BTYPE) == VT_PTR)
			{ MCC_TRACE("br\n"); mcc_error("pointer not accepted for unary minus"); }
		if (is_float(vtop->type.t)) { MCC_TRACE("br\n");
			gen_opif(TOK_NEG);
		} else { MCC_TRACE("br\n");
			vpushi(0);
			vswap();
			gen_op('-');
		}
		break;
	case TOK_LAND:
		if (!gnu_ext)
			{ MCC_TRACE("br\n"); goto tok_identifier; }
		next();
		if (tok < TOK_UIDENT)
			{ MCC_TRACE("br\n"); expect("label identifier"); }
		s = label_find(tok);
		if (!s) { MCC_TRACE("br\n");
			s = label_push(&global_label_stack, tok, LABEL_FORWARD);
		} else { MCC_TRACE("br\n");
			if (s->r == LABEL_DECLARED)
				{ MCC_TRACE("br\n"); s->r = LABEL_FORWARD; }
		}
		if ((s->type.t & VT_BTYPE) != VT_PTR) { MCC_TRACE("br\n");
			s->type.t = VT_VOID;
			mk_pointer(&s->type);
			s->type.t |= VT_STATIC;
		}
		vpushsym(&s->type, s);
		next();
		break;

	case TOK_GENERIC: {
		CType controlling_type;
		int has_default = 0;
		int has_match = 0;
		int learn = 0;
		TokenString *str = NULL;
		CType *assoc_types = NULL;
		int nb_assoc = 0;
		int saved_nocode_wanted = nocode_wanted;
		nocode_wanted &= ~CONST_WANTED_MASK;

		next();
		skip('(');
		expr_type(&controlling_type, expr_eq);
		convert_parameter_type(&controlling_type);

		nocode_wanted = saved_nocode_wanted;

		for (;;) { MCC_TRACE("br\n");
			learn = 0;
			skip(',');
			if (tok == TOK_DEFAULT) { MCC_TRACE("br\n");
				if (has_default)
					{ MCC_TRACE("br\n"); mcc_error("too many 'default'"); }
				has_default = 1;
				if (!has_match)
					{ MCC_TRACE("br\n"); learn = 1; }
				next();
			} else { MCC_TRACE("br\n");
				int v, i;
				parse_btype(&type, &ad, 0);
				type_decl(&type, &ad, &v, TYPE_ABSTRACT);
				if (type.t & VT_VLA)
					{ MCC_TRACE("br\n"); mcc_error("'_Generic' association has a variably modified type"); }
				else if ((type.t & VT_BTYPE) == VT_FUNC)
					{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids a '_Generic' association with a "
											 "function type"); }
				else { MCC_TRACE("br\n");
					int gsz, galign;
					gsz = type_size(&type, &galign);
					if (gsz < 0 || (type.t & VT_BTYPE) == VT_VOID)
						{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids a '_Generic' association "
												 "with an incomplete type"); }
				}
				for (i = 0; i < nb_assoc; i++)
					{ MCC_TRACE("br\n"); if (compare_types(&assoc_types[i], &type, 0))
						{ MCC_TRACE("br\n"); mcc_error("_Generic specifies two compatible types"); } }
				assoc_types = mcc_realloc(assoc_types,
																	(nb_assoc + 1) * sizeof(CType));
				assoc_types[nb_assoc++] = type;
				if (compare_types(&controlling_type, &type, 0)) { MCC_TRACE("br\n");
					if (has_match) { MCC_TRACE("br\n");
						mcc_error("type match twice");
					}
					has_match = 1;
					learn = 1;
				}
			}
			skip(':');
			if (learn) { MCC_TRACE("br\n");
				if (str)
					{ MCC_TRACE("br\n"); tok_str_free(str); }
				skip_or_save_block(&str);
			} else { MCC_TRACE("br\n");
				skip_or_save_block(NULL);
			}
			if (tok == ')')
				{ MCC_TRACE("br\n"); break; }
		}
		if (!str) { MCC_TRACE("br\n");
			char buf[60];
			type_to_str(buf, sizeof buf, &controlling_type, NULL);
			mcc_error("type '%s' does not match any association", buf);
		}
		mcc_free(assoc_types);
		begin_macro(str, 1);
		next();
		expr_eq();
		if (tok != TOK_EOF)
			{ MCC_TRACE("br\n"); expect(","); }
		end_macro();
		next();
		break;
	}
	case TOK___NAN__:
		n = 0x7fc00000;
	special_math_val:
		vpushi(n);
		vtop->type.t = VT_FLOAT;
		next();
		CST_PRIMARY();
		break;
	case TOK___SNAN__:
		n = 0x7f800001;
		goto special_math_val;
	case TOK___INF__:
		n = 0x7f800000;
		goto special_math_val;

	default:
	tok_identifier:
		if (tok < TOK_UIDENT)
			{ MCC_TRACE("br\n"); mcc_error("expression expected before '%s'", get_tok_str(tok, &tokc)); }
		t = tok;
#if MCC_CONFIG_LSP
		cst_hook_use(t, cst_cur_tok_off());
#endif
		next();
		s = sym_find(t);
		if (!s || IS_ASM_SYM(s)) { MCC_TRACE("br\n");
			const char *name = get_tok_str(t, NULL);
			if (tok != '(')
				{ MCC_TRACE("br\n"); mcc_error("'%s' undeclared", name); }
			if (mcc_state->macro_eval && pp_macro_is_func(t)) { MCC_TRACE("br\n");
				macro_eval_call(t);
				break;
			}
			mcc_warning_c(warn_implicit_function_declaration)(
					"implicit declaration of function '%s'", name);
			s = external_global_sym(t, &func_old_type);
		}

		s->a.used = 1;

		if (cur_func_inline_extern &&
				(s->type.t & (VT_BTYPE | VT_STATIC | VT_INLINE)) == (VT_FUNC | VT_STATIC)) { MCC_TRACE("br\n");
			char pbuf[256];
			snprintf(pbuf, sizeof pbuf,
							 "'%s' has internal linkage but is referenced in an "
							 "inline function with external linkage",
							 get_tok_str(s->v, NULL));
			mcc_pedantic(pbuf);
		}

		r = s->r;
		if ((r & VT_VALMASK) < VT_CONST)
			{ MCC_TRACE("br\n"); r = (r & ~VT_VALMASK) | VT_LOCAL; }

		vset(&s->type, r, s->c);
		vtop->sym = s;

		if (r & VT_SYM) { MCC_TRACE("br\n");
			vtop->c.i = 0;
#ifdef MCC_TARGET_PE
			if (s->a.dllimport) { MCC_TRACE("br\n");
				mk_pointer(&vtop->type);
				vtop->r |= VT_LVAL;
				indir();
			}
#endif
		} else if (r == VT_CONST && IS_ENUM_VAL(s->type.t)) { MCC_TRACE("br\n");
			vtop->c.i = s->enum_val;
		}
		CST_PRIMARY();
		break;
	}

	while (1) { MCC_TRACE("br\n");
		if (tok == TOK_INC || tok == TOK_DEC) { MCC_TRACE("br\n");
			inc(1, tok);
			expr_has_effect = 1;
			next();
		} else if (tok == '.' || tok == TOK_ARROW) { MCC_TRACE("br\n");
			int qualifiers, cumofs, base_nonlval;
#if MCC_CONFIG_OPTIMIZER
			ast_hook_member_begin(tok == TOK_ARROW);
#endif
			if (tok == TOK_ARROW)
				{ MCC_TRACE("br\n"); indir(); }
			qualifiers = vtop->type.t & VT_QUALIFY;
			base_nonlval = vtop->r & VT_NONLVAL;
			test_lvalue();
			next();
			s = find_field(&vtop->type, tok, &cumofs);
			gaddrof();
			vtop->type = char_pointer_type;
			vpushi(cumofs);
			gen_op('+');
			vtop->type = s->type;
			if (qualifiers)
				{ MCC_TRACE("br\n"); parse_btype_qualify(&vtop->type, qualifiers); }
			if (!(vtop->type.t & VT_ARRAY)) { MCC_TRACE("br\n");
				vtop->r |= VT_LVAL | base_nonlval;
#if MCC_CONFIG_DIAG_RT >= 2
				if (mcc_state->do_bounds_check)
					{ MCC_TRACE("br\n"); vtop->r |= VT_MUSTBOUND; }
#endif
			}
#if MCC_CONFIG_OPTIMIZER
			{
				int _mbc = 0;
#if MCC_CONFIG_DIAG_RT >= 2
				_mbc = mcc_state->do_bounds_check;
#endif
				ast_hook_member_end(cumofs, &s->type, base_nonlval, qualifiers, _mbc);
			}
#endif
			next();
#if MCC_CONFIG_LSP
			CST_OPEN_AT(CST_Member, cst_um);
			CST_CLOSE();
#endif
		} else if (tok == '[') { MCC_TRACE("br\n");
			next();
			gexpr();
			gen_op('+');
			indir();
			skip(']');
#if MCC_CONFIG_LSP
			CST_OPEN_AT(CST_Index, cst_um);
			CST_CLOSE();
#endif
		} else if (tok == '(') { MCC_TRACE("br\n");
			SValue ret;
			Sym *sa;
			int nb_args, ret_nregs, ret_align, regsize, variadic;
			TokenString *p, *p2;
			const char *fmt_fname = NULL;

			if ((mcc_state->warn_format & WARN_ON) && (vtop->r & VT_SYM) && vtop->sym)
				{ MCC_TRACE("br\n"); fmt_fname = get_tok_str(vtop->sym->v, NULL); }

			if ((vtop->type.t & VT_BTYPE) != VT_FUNC) { MCC_TRACE("br\n");
				if ((vtop->type.t & (VT_BTYPE | VT_ARRAY)) == VT_PTR) { MCC_TRACE("br\n");
					vtop->type = *pointed_type(&vtop->type);
					if ((vtop->type.t & VT_BTYPE) != VT_FUNC)
						{ MCC_TRACE("br\n"); goto error_func; }
				} else { MCC_TRACE("br\n");
				error_func:
					expect("function pointer");
				}
			} else { MCC_TRACE("br\n");
				vtop->r &= ~VT_LVAL;
			}
			s = vtop->type.ref;
			next();
			sa = s->next;
			nb_args = regsize = 0;
			ret.r2 = VT_CONST;
			if ((s->type.t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
				variadic = (s->f.func_type == FUNC_ELLIPSIS);
				ret_nregs = gfunc_sret(&s->type, variadic, &ret.type,
															 &ret_align, &regsize);
				if (ret_nregs <= 0) { MCC_TRACE("br\n");
					size = type_size(&s->type, &align);
#ifdef MCC_TARGET_ARM64
					if (size < 16)
						{ MCC_TRACE("br\n"); while (size & (size - 1))
							{ MCC_TRACE("br\n"); size = (size | (size - 1)) + 1; } }
#endif
#if MCC_CONFIG_OPTIMIZER
					loc = ast_alloc_loc(size, align);
#else
					loc = (loc - size) & -align;
#endif
					ret.type = s->type;
					ret.r = VT_LOCAL | VT_LVAL;
					vseti(VT_LOCAL, loc);
#if MCC_CONFIG_DIAG_RT >= 2
					if (mcc_state->do_bounds_check)
						{ MCC_TRACE("br\n"); --loc; }
#endif
					ret.c = vtop->c;
					if (ret_nregs < 0)
						{ MCC_TRACE("br\n"); vtop--; }
					else
						{ MCC_TRACE("br\n"); nb_args++; }
				}
			} else { MCC_TRACE("br\n");
				ret_nregs = 1;
				ret.type = s->type;
			}

			if (ret_nregs > 0) { MCC_TRACE("br\n");
				ret.c.i = 0;
				PUT_R_RET(&ret, ret.type.t);
			}

			p = NULL;
			if (tok != ')') { MCC_TRACE("br\n");
				r = mcc_state->reverse_funcargs;
				for (;;) { MCC_TRACE("br\n");
					if (r) { MCC_TRACE("br\n");
						skip_or_save_block(&p2);
						p2->prev = p, p = p2;
					} else { MCC_TRACE("br\n");
						expr_eq();
						gfunc_param_typed(s, sa);
						seqp_flush();
					}
					nb_args++;
					if (sa)
						{ MCC_TRACE("br\n"); sa = sa->next; }
					if (tok == ')')
						{ MCC_TRACE("br\n"); break; }
					skip(',');
				}
			}
			if (sa)
				{ MCC_TRACE("br\n"); mcc_error("too few arguments to function"); }

			if (p) { MCC_TRACE("br\n");
				for (n = 0; p; p = p2, ++n) { MCC_TRACE("br\n");
					p2 = p, sa = s;
					do { MCC_TRACE("br\n");
						sa = sa->next, p2 = p2->prev;
					} while (p2 && sa);
					p2 = p->prev;
					begin_macro(p, 1), next();
					expr_eq();
					gfunc_param_typed(s, sa);
					end_macro();
					seqp_flush();
				}
				vrev(n);
			}

			if (fmt_fname && !mcc_state->reverse_funcargs && (s->type.t & VT_BTYPE) != VT_STRUCT) { MCC_TRACE("br\n");
				int is_scanf, fa, fv;
				if (format_func_spec(fmt_fname, &is_scanf, &fa, &fv) && nb_args >= fa) { MCC_TRACE("br\n");
					int favail;
					const char *fstr =
							format_str_literal(vtop - (nb_args - fa), &favail);
					if (fstr) { MCC_TRACE("br\n");
						int nvar = nb_args - fv + 1;
						if (nvar < 0)
							{ MCC_TRACE("br\n"); nvar = 0; }
						format_check(is_scanf, fstr, favail,
												 vtop - (nb_args - fv), nvar);
					}
				}
			}

			next();
			vcheck_cmp();
			if (mcc_state->fold_math &&
					(foldmath_try(s, nb_args) || foldfc_try(s, nb_args))) { MCC_TRACE("br\n");
				expr_has_effect = 1;
				goto call_folded;
			}
#if MCC_CONFIG_OPTIMIZER
			ast_hook_call_begin(nb_args, (s->type.t & VT_BTYPE) == VT_STRUCT,
													ret_nregs, s->f.func_type == FUNC_ELLIPSIS);
#endif
			gfunc_call(nb_args);
			expr_has_effect = 1;

			if (ret_nregs < 0) { MCC_TRACE("br\n");
				vsetc(&ret.type, ret.r, &ret.c);
#if defined(MCC_TARGET_RISCV64) || (defined(MCC_TARGET_X86_64) && !defined(MCC_TARGET_PE))
				arch_transfer_ret_regs(1);
#endif
			} else { MCC_TRACE("br\n");
				n = ret_nregs;
				while (n > 1) { MCC_TRACE("br\n");
					int rc = reg_classes[ret.r] & ~(MCC_RC_INT | MCC_RC_FLOAT);
					rc <<= --n;
					for (r = 0; r < MCC_NB_REGS; ++r)
						{ MCC_TRACE("br\n"); if (reg_classes[r] & rc)
							{ MCC_TRACE("br\n"); break; } }
					vsetc(&ret.type, r, &ret.c);
				}
				vsetc(&ret.type, ret.r, &ret.c);
				vtop->r2 = ret.r2;

				if (((s->type.t & VT_BTYPE) == VT_STRUCT) && ret_nregs) { MCC_TRACE("br\n");
					int addr, offset;

					size = type_size(&s->type, &align);
					size = (size + regsize - 1) & -regsize;
					if (ret_align > align)
						{ MCC_TRACE("br\n"); align = ret_align; }
#if MCC_CONFIG_OPTIMIZER
					loc = ast_alloc_loc(size, align);
#else
					loc = (loc - size) & -align;
#endif
					addr = loc;
					offset = 0;
					for (;;) { MCC_TRACE("br\n");
						vset(&ret.type, VT_LOCAL | VT_LVAL, addr + offset);
						vswap();
						vstore();
						vtop--;
						if (--ret_nregs == 0)
							{ MCC_TRACE("br\n"); break; }
						offset += regsize;
					}
					vset(&s->type, VT_LOCAL | VT_LVAL, addr);
				}

				t = s->type.t & VT_BTYPE;
				if (t == VT_BYTE || t == VT_SHORT || t == VT_BOOL) { MCC_TRACE("br\n");
#ifdef MCC_RET_PROMOTES_INT
					vtop->r |= BFVAL(VT_MUSTCAST, 1);
#else
					vtop->type.t = VT_INT;
#endif
				}
			}
			if ((s->type.t & VT_BTYPE) == VT_STRUCT)
				{ MCC_TRACE("br\n"); vtop->r |= VT_NONLVAL; }
			if (s->f.func_noreturn) { MCC_TRACE("br\n");
				if (debug_modes)
					{ MCC_TRACE("br\n"); mcc_tcov_block_end(mcc_state, -1); }
				CODE_OFF();
			}
#if MCC_CONFIG_OPTIMIZER
			ast_hook_call_end();
#endif
		call_folded:;
#if MCC_CONFIG_LSP
			CST_OPEN_AT(CST_Call, cst_um);
			CST_CLOSE();
#endif
		} else { MCC_TRACE("br\n");
			break;
		}
	}
#if MCC_CONFIG_LSP
	if (cst_nk) { MCC_TRACE("br\n");
		CST_OPEN_AT(cst_nk, cst_um);
		CST_CLOSE();
	}
#endif
}
#undef CST_PRIMARY

#define expr_landor_next(op) unary(), expr_infix(precedence(op) + 1)
#define expr_lor() unary(), expr_infix(1)

static int precedence(int tok) { MCC_TRACE("enter\n");
	switch (tok) { MCC_TRACE("br\n");
	case TOK_LOR:
		return 1;
	case TOK_LAND:
		return 2;
	case '|':
		return 3;
	case '^':
		return 4;
	case '&':
		return 5;
	case TOK_EQ:
	case TOK_NE:
		return 6;
	relat:
	case TOK_ULT:
	case TOK_UGE:
		return 7;
	case TOK_SHL:
	case TOK_SAR:
		return 8;
	case '+':
	case '-':
		return 9;
	case '*':
	case '/':
	case '%':
		return 10;
	default:
		if (tok >= TOK_ULE && tok <= TOK_GT)
			{ MCC_TRACE("br\n"); goto relat; }
		return 0;
	}
}

#define prec (mcc_state->gen_prec)

static void init_prec(void) { MCC_TRACE("enter\n");
	for (int i = 0; i < 256; i++)
		{ MCC_TRACE("br\n"); prec[i] = precedence(i); }
}

#define precedence(i) ((unsigned)i < 256 ? prec[i] : 0)

static void expr_landor(int op);

static void expr_infix(int p) { MCC_TRACE("enter\n");
	int t = tok, p2;
	while ((p2 = precedence(t)) >= p) { MCC_TRACE("br\n");
		if (t == TOK_LOR || t == TOK_LAND) { MCC_TRACE("br\n");
			expr_landor(t);
		} else { MCC_TRACE("br\n");
			next();
			unary();
			if (precedence(tok) > p2)
				{ MCC_TRACE("br\n"); expr_infix(p2 + 1); }
			gen_op(t);
		}
		t = tok;
	}
}

static int condition_3way(void) { MCC_TRACE("enter\n");
	int c = -1;
	if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST &&
			(!(vtop->r & VT_SYM) || !vtop->sym->a.weak)) { MCC_TRACE("br\n");
		vdup();
		gen_cast_s(VT_BOOL);
		c = vtop->c.i;
		vpop();
	}
	return c;
}

static void expr_landor(int op) { MCC_TRACE("enter\n");
	int t = 0, cc = 1, f = 0, i = op == TOK_LAND, c;
#if MCC_CONFIG_OPTIMIZER
	int first = 1;
#endif
	for (;;) { MCC_TRACE("br\n");
		c = f ? i : condition_3way();
		if (c < 0)
			{ MCC_TRACE("br\n"); save_regs(1), cc = 0; }
		else if (c != i)
			{ MCC_TRACE("br\n"); nocode_wanted++, f = 1; }
#if MCC_CONFIG_OPTIMIZER
		ast_hook_landor_operand(op, c, first);
		first = 0;
#endif
		if (tok != op)
			{ MCC_TRACE("br\n"); break; }
		if (c < 0)
			{ MCC_TRACE("br\n"); t = gvtst(i, t); }
		else
			{ MCC_TRACE("br\n"); vpop(); }
		next();
		seqp_flush();
#if MCC_CONFIG_OPTIMIZER
		ast_hook_landor_next();
#endif
		expr_landor_next(op);
	}
	if (cc || f) { MCC_TRACE("br\n");
		vpop();
		vpushi(i ^ f);
		gsym(t);
		nocode_wanted -= f;
	} else { MCC_TRACE("br\n");
		gvtst_set(i, t);
	}
#if MCC_CONFIG_OPTIMIZER
	ast_hook_landor_end(cc || f);
#endif
}

static int is_cond_bool(SValue *sv) { MCC_TRACE("enter\n");
	if ((sv->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST && (sv->type.t & VT_BTYPE) == VT_INT)
		{ MCC_TRACE("br\n"); return (unsigned)sv->c.i < 2; }
	if (sv->r == VT_CMP)
		{ MCC_TRACE("br\n"); return 1; }
	return 0;
}

static void expr_cond(void) { MCC_TRACE("enter\n");
	int tt, u, r1, r2, rc, t1, t2, islv, c, g;
	SValue sv;
	CType type;

#if MCC_CONFIG_LSP
	uint32_t cst_m = CST_MARK();
	unary();
	int cst_has_binop = precedence(tok) >= 1;
	expr_infix(1);
	if (cst_has_binop) { MCC_TRACE("br\n");
		CST_OPEN_AT(CST_Binary, cst_m);
		CST_CLOSE();
	}
#else
	expr_lor();
#endif
	if (tok == '?') { MCC_TRACE("br\n");
		next();
		c = condition_3way();
		seqp_flush();
		g = (tok == ':' && gnu_ext);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_ternary_begin(c, g);
#endif
		tt = 0;
		if (!g) { MCC_TRACE("br\n");
			if (c < 0) { MCC_TRACE("br\n");
				save_regs(1);
				tt = gvtst(1, 0);
			} else { MCC_TRACE("br\n");
				vpop();
			}
		} else if (c < 0) { MCC_TRACE("br\n");
			save_regs(1);
			gv_dup();
			tt = gvtst(0, 0);
		}

		if (c == 0)
			{ MCC_TRACE("br\n"); nocode_wanted++; }
#if MCC_CONFIG_OPTIMIZER
		ast_hook_ternary_branch(0);
#endif
		if (!g)
			{ MCC_TRACE("br\n"); gexpr(); }
#if MCC_CONFIG_OPTIMIZER
		ast_hook_ternary_branch_done(0);
#endif

		if ((vtop->type.t & VT_BTYPE) == VT_FUNC)
			{ MCC_TRACE("br\n"); mk_pointer(&vtop->type); }
		sv = *vtop;
		vtop--;

		if (g) { MCC_TRACE("br\n");
			u = tt;
		} else if (c < 0) { MCC_TRACE("br\n");
			u = gjmp(0);
			gsym(tt);
		} else
			{ MCC_TRACE("br\n"); u = 0; }

		if (c == 0)
			{ MCC_TRACE("br\n"); nocode_wanted--; }
		if (c == 1)
			{ MCC_TRACE("br\n"); nocode_wanted++; }
		skip(':');
#if MCC_CONFIG_OPTIMIZER
		ast_hook_ternary_branch(1);
#endif
		expr_cond();
#if MCC_CONFIG_OPTIMIZER
		ast_hook_ternary_branch_done(1);
#endif
#if MCC_CONFIG_LSP
		CST_OPEN_AT(CST_Cond, cst_m);
		CST_CLOSE();
#endif

		if ((vtop->type.t & VT_BTYPE) == VT_FUNC)
			{ MCC_TRACE("br\n"); mk_pointer(&vtop->type); }

		if (!combine_types(&type, &sv, vtop, '?'))
			{ MCC_TRACE("br\n"); type_incompatibility_error(&sv.type, &vtop->type,
																 "type mismatch in conditional expression (have '%s' and '%s')"); }

		if (((sv.type.t & VT_BTYPE) == VT_VOID) != ((vtop->type.t & VT_BTYPE) == VT_VOID))
			{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids conditional expression with "
									 "only one void operand"); }

		if (CONST_WANTED && c >= 0) { MCC_TRACE("br\n");
			SValue *dead = (c == 1) ? vtop : &sv;
			if ((dead->r & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
				{ MCC_TRACE("br\n"); ice_nonconst = 1; }
		}

		if (c < 0 && is_cond_bool(vtop) && is_cond_bool(&sv)) { MCC_TRACE("br\n");
			t1 = gvtst(0, 0);
			t2 = gjmp(0);
			gsym(u);
			vpushv(&sv);
			gvtst_set(0, t1);
			gvtst_set(1, t2);
			gen_cast(&type);
			return;
		}

		islv = VT_STRUCT == (type.t & VT_BTYPE);

		if (c != 1) { MCC_TRACE("br\n");
			gen_cast(&type);
			if (islv) { MCC_TRACE("br\n");
				mk_pointer(&vtop->type);
				gaddrof();
			}
		}

		rc = MCC_RC_TYPE(type.t);
		if (USING_TWO_WORDS(type.t))
			{ MCC_TRACE("br\n"); rc = MCC_RC_RET(type.t); }

		tt = r2 = 0;
		if (c < 0) { MCC_TRACE("br\n");
			if (type.t != VT_VOID)
				{ MCC_TRACE("br\n"); r2 = gv(rc); }
			tt = gjmp(0);
		}
		gsym(u);
		if (c == 1)
			{ MCC_TRACE("br\n"); nocode_wanted--; }

		if (c != 0) { MCC_TRACE("br\n");
			*vtop = sv;
			gen_cast(&type);
			if (islv) { MCC_TRACE("br\n");
				mk_pointer(&vtop->type);
				gaddrof();
			}
		}

		if (c < 0) { MCC_TRACE("br\n");
			if (type.t != VT_VOID) { MCC_TRACE("br\n");
				r1 = gv(rc);
				move_reg(r2, r1, islv ? VT_PTR : type.t);
				vtop->r = r2;
			}
			gsym(tt);
		}

		if (islv)
			{ MCC_TRACE("br\n"); indir(); }
#if MCC_CONFIG_OPTIMIZER
		ast_hook_ternary_end();
#endif
	}
}

static void expr_eq(void) { MCC_TRACE("enter\n");
	int t;
	int was_assign = 0;
#if MCC_CONFIG_LSP
	uint32_t cst_m = CST_MARK();
#endif

	expr_cond();
	if ((t = tok) == '=' || TOK_ASSIGN(t)) { MCC_TRACE("br\n");
		was_assign = 1;
		test_lvalue();
		if (vtop->r & VT_NONLVAL)
			{ MCC_TRACE("br\n"); mcc_error("expression is not assignable (function-call result)"); }
		if (t != '=' && (vtop->type.t & VT_ATOMIC_BIT)) { MCC_TRACE("br\n");
			int op = TOK_ASSIGN_OP(t);
			if (atomic_rmw_size(vtop, op)) { MCC_TRACE("br\n");
				next();
				expr_eq();
				gen_atomic_rmw(op, 1);
				return;
			}
			if (atomic_cas_size(vtop)) { MCC_TRACE("br\n");
				next();
				expr_eq();
				gen_atomic_cas_rmw(op, 1);
				return;
			}
			mcc_error("this compound assignment to an '_Atomic' object is not "
								"supported (float, or larger than a machine word)");
		}
		if (t == '=' && atomic_store_needs_libcall(vtop)) { MCC_TRACE("br\n");
			next();
			gen_atomic_store_scalar();
			return;
		}
		if (t == '=' && atomic_store_needs_generic(vtop)) { MCC_TRACE("br\n");
			next();
			gen_atomic_store_aggregate();
			return;
		}
		next();
		if (t == '=') { MCC_TRACE("br\n");
			expr_eq();
		} else { MCC_TRACE("br\n");
			vdup();
			expr_eq();
			gen_op(TOK_ASSIGN_OP(t));
		}
		vstore();
	}
#if MCC_CONFIG_LSP
	if (was_assign) { MCC_TRACE("br\n");
		CST_OPEN_AT(CST_Binary, cst_m);
		CST_CLOSE();
	}
#endif
	expr_was_assign = was_assign;
	if (was_assign)
		{ MCC_TRACE("br\n"); expr_has_effect = 1; }
}

ST_FUNC void gexpr(void) { MCC_TRACE("enter\n");
#if MCC_CONFIG_LSP
	uint32_t cst_m = CST_MARK();
#endif
	expr_eq();
	if (tok == ',') { MCC_TRACE("br\n");
		if (CONST_WANTED && !NOEVAL_WANTED)
			{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids a comma operator "
									 "in a constant expression"); }
		do { MCC_TRACE("br\n");
			vpop();
			next();
			seqp_flush();
			expr_eq();
		} while (tok == ',');
#if MCC_CONFIG_LSP
		CST_OPEN_AT(CST_Comma, cst_m);
		CST_CLOSE();
#endif

		convert_parameter_type(&vtop->type);

		if ((vtop->r & VT_LVAL) && !nocode_wanted) { MCC_TRACE("br\n");
			int bt = vtop->type.t & VT_BTYPE;
			if (bt != VT_STRUCT && bt != VT_VOID && bt != VT_FUNC && !(vtop->type.t & (VT_ARRAY | VT_VLA)) && !is_complex_type(&vtop->type))
				{ MCC_TRACE("br\n"); gv(MCC_RC_TYPE(vtop->type.t)); }
		}

		if ((vtop->r & VT_VALMASK) == VT_CONST && nocode_wanted && !CONST_WANTED)
			{ MCC_TRACE("br\n"); if (vtop->type.t != VT_VOID && (vtop->type.t & VT_BTYPE) != VT_STRUCT)
				{ MCC_TRACE("br\n"); gv(MCC_RC_TYPE(vtop->type.t)); } }
	}
}

static void expr_const1(void) { MCC_TRACE("enter\n");
	nocode_wanted += CONST_WANTED_BIT;
	expr_cond();
	nocode_wanted -= CONST_WANTED_BIT;
}

static inline int64_t expr_const64(void) { MCC_TRACE("enter\n");
	int64_t c;
	expr_const1();
	if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM | VT_NONCONST)) != VT_CONST)
		{ MCC_TRACE("br\n"); expect("constant expression"); }
	if (is_float(vtop->type.t))
		{ MCC_TRACE("br\n"); mcc_error("integer constant expression must have integer type"); }
	c = vtop->c.i;
	vpop();
	return c;
}

ST_FUNC int expr_const(void) { MCC_TRACE("enter\n");
	int c;
	int64_t wc = expr_const64();
	c = wc;
	if (c != wc && (unsigned)c != wc)
		{ MCC_TRACE("br\n"); mcc_error("constant exceeds 32 bit"); }
	return c;
}

#ifndef MCC_TARGET_ARM64
static void gfunc_return(CType *func_type) { MCC_TRACE("enter\n");
	if ((func_type->t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
		CType type, ret_type;
		int ret_align, ret_nregs, regsize;
		ret_nregs = gfunc_sret(func_type, func_var, &ret_type,
													 &ret_align, &regsize);
		if (ret_nregs < 0) { MCC_TRACE("br\n");
#if defined(MCC_TARGET_RISCV64) || (defined(MCC_TARGET_X86_64) && !defined(MCC_TARGET_PE))
			arch_transfer_ret_regs(0);
#endif
		} else if (0 == ret_nregs) { MCC_TRACE("br\n");
			type = *func_type;
			mk_pointer(&type);
			vset(&type, VT_LOCAL | VT_LVAL, func_vc);
			indir();
			vswap();
			vstore();
		} else { MCC_TRACE("br\n");
			int size, addr, align, rc, n;
			size = type_size(func_type, &align);
			if (ret_nregs * regsize > size ||
					((align & (ret_align - 1)) && ((vtop->r & VT_VALMASK) < VT_CONST || (vtop->c.i & (ret_align - 1))))) { MCC_TRACE("br\n");
				if (ret_nregs * regsize > size)
					{ MCC_TRACE("br\n"); size = ret_nregs * regsize; }
				if (ret_align > align)
					{ MCC_TRACE("br\n"); align = ret_align; }
				addr = ast_alloc_loc(size, align);
				type = *func_type;
				vset(&type, VT_LOCAL | VT_LVAL, addr);
				vswap();
				vstore();
				vpop();
				vset(&ret_type, VT_LOCAL | VT_LVAL, addr);
			}
			vtop->type = ret_type;
			rc = MCC_RC_RET(ret_type.t);
			for (n = ret_nregs; --n > 0;) { MCC_TRACE("br\n");
				vdup();
				gv(rc);
				vswap();
				incr_offset(regsize);
				rc <<= 1;
			}
			gv(rc);
			vtop -= ret_nregs - 1;
		}
	} else { MCC_TRACE("br\n");
		gv(MCC_RC_RET(func_type->t));
	}
	vtop--;
}
#endif

static void check_func_return(void) { MCC_TRACE("enter\n");
	if ((func_vt.t & VT_BTYPE) == VT_VOID)
		{ MCC_TRACE("br\n"); return; }
	if ((!strcmp(funcname, "main") || func_old) && (func_vt.t & VT_BTYPE) == VT_INT) { MCC_TRACE("br\n");
#if MCC_CONFIG_OPTIMIZER
		ast_hook_implicit_return();
#endif
		vpushi(0);
		gfunc_return(&func_vt);
	} else { MCC_TRACE("br\n");
		mcc_warning("function might return no value: '%s'", funcname);
	}
}

static int case_cmp(uint64_t a, uint64_t b) { MCC_TRACE("enter\n");
	if (cur_switch->sv.type.t & VT_UNSIGNED)
		{ MCC_TRACE("br\n"); return a < b ? -1 : a > b; }
	else
		{ MCC_TRACE("br\n"); return (int64_t)a<(int64_t)b ? -1 : (int64_t)a>(int64_t) b; }
}

static int case_cmp_qs(const void *pa, const void *pb) { MCC_TRACE("enter\n");
	return case_cmp((*(struct case_t **)pa)->v1, (*(struct case_t **)pb)->v1);
}

static void case_sort(struct switch_t *sw) { MCC_TRACE("enter\n");
	struct case_t **p;
	if (sw->n < 2)
		{ MCC_TRACE("br\n"); return; }
	qsort(sw->p, sw->n, sizeof *sw->p, case_cmp_qs);
	p = sw->p;
	while (p < sw->p + sw->n - 1) { MCC_TRACE("br\n");
		if (case_cmp(p[0]->v2, p[1]->v1) >= 0) { MCC_TRACE("br\n");
			int l1 = p[0]->line, l2 = p[1]->line;
			mcc_error("%i:duplicate case value", l1 > l2 ? l1 : l2);
		} else if (p[0]->v2 + 1 == p[1]->v1 && p[0]->ind == p[1]->ind) { MCC_TRACE("br\n");
			p[1]->v1 = p[0]->v1;
			mcc_free(p[0]);
			memmove(p, p + 1, (--sw->n - (p - sw->p)) * sizeof *p);
		} else
			{ MCC_TRACE("br\n"); ++p; }
	}
}

static int gcase(struct case_t **base, int len, int dsym) { MCC_TRACE("enter\n");
	struct case_t *p;
	int t, l2, e;

	t = vtop->type.t & VT_BTYPE;
	if (t != VT_LLONG)
		{ MCC_TRACE("br\n"); t = VT_INT; }
	while (len) { MCC_TRACE("br\n");
		l2 = len > 8 ? len / 2 : 0;
		p = base[l2];
		vdup(), vpush64(t, p->v2);
		if (l2 == 0 && p->v1 == p->v2) { MCC_TRACE("br\n");
			gen_op(TOK_EQ);
			gsym_addr(gvtst(0, 0), p->ind);
		} else { MCC_TRACE("br\n");
			gen_op(TOK_GT);
			if (len == 1)
				{ MCC_TRACE("br\n"); dsym = gvtst(0, dsym), e = 0; }
			else
				{ MCC_TRACE("br\n"); e = gvtst(0, 0); }
			vdup(), vpush64(t, p->v1);
			gen_op(TOK_GE);
			gsym_addr(gvtst(0, 0), p->ind);
			dsym = gcase(base, l2, dsym);
			gsym(e);
		}
		++l2, base += l2, len -= l2;
	}
	return gjmp(dsym);
}

static void end_switch(void) { MCC_TRACE("enter\n");
	struct switch_t *sw = cur_switch;
	dynarray_reset(&sw->p, &sw->n);
	cur_switch = sw->prev;
	mcc_free(sw);
}

static void save_lvalues(void) { MCC_TRACE("enter\n");
	SValue *sv = vtop;
	while (sv >= vstack) { MCC_TRACE("br\n");
		if (sv->sym && (sv->r & VT_LVAL)) { MCC_TRACE("br\n");
			int align, size = type_size(&sv->type, &align);
			int r2, l = get_temp_local_var(size, align, &r2);
			vset(&sv->type, VT_LOCAL | VT_LVAL, l), vtop->r2 = r2;
			vpushv(sv), *sv = vtop[-1], vstore(), --vtop;
		}
		--sv;
	}
}

static void try_call_scope_cleanup(Sym *stop) { MCC_TRACE("enter\n");
	Sym *cls = cur_scope->cl.s;
	for (; cls != stop; cls = cls->next) { MCC_TRACE("br\n");
		Sym *fs = cls->cleanup_func;
		Sym *vs = cls->cleanup_sym;
		save_lvalues();
		vpushsym(&fs->type, fs);
		vset(&vs->type, vs->r, vs->c);
		vtop->sym = vs;
		mk_pointer(&vtop->type);
		gaddrof();
		gfunc_call(1);
	}
}

static void try_call_cleanup_goto(Sym *cleanupstate) { MCC_TRACE("enter\n");
	Sym *oc, *cc;
	int ocd, ccd;

	if (!cur_scope->cl.s)
		{ MCC_TRACE("br\n"); return; }

	ocd = cleanupstate ? cleanupstate->v & ~SYM_FIELD : 0;
	for (ccd = cur_scope->cl.n, oc = cleanupstate; ocd > ccd; --ocd, oc = oc->next)
		;
	for (cc = cur_scope->cl.s; ccd > ocd; --ccd, cc = cc->next)
		;
	for (; cc != oc; cc = cc->next, oc = oc->next, --ccd)
		;

	try_call_scope_cleanup(cc);
}

static void block_cleanup(struct scope *o) { MCC_TRACE("enter\n");
	int jmp = 0;
	Sym *g, **pg;
	for (pg = &pending_gotos; (g = *pg) && g->c > o->cl.n;) { MCC_TRACE("br\n");
		if (g->cleanup_label->r & LABEL_FORWARD) { MCC_TRACE("br\n");
			Sym *pcl = g->next;
			if (!jmp)
				{ MCC_TRACE("br\n"); jmp = gjmp(0); }
			gsym(pcl->jnext);
			try_call_scope_cleanup(o->cl.s);
			pcl->jnext = gjmp(0);
			if (!o->cl.n)
				{ MCC_TRACE("br\n"); goto remove_pending; }
			g->c = o->cl.n;
			pg = &g->prev;
		} else { MCC_TRACE("br\n");
		remove_pending:
			*pg = g->prev;
			sym_free(g);
		}
	}
	gsym(jmp);
	try_call_scope_cleanup(o->cl.s);
}

static void vla_restore(int loc) { MCC_TRACE("enter\n");
	if (loc)
		{ MCC_TRACE("br\n"); gen_vla_sp_restore(loc); }
#if MCC_CONFIG_OPTIMIZER
	ast_hook_vla_restore(loc);
#endif
}

static int vla_scope_open(int id) { MCC_TRACE("enter\n");
	int i;
	if (id == 0)
		{ MCC_TRACE("br\n"); return 1; }
	if (vla_track_ovf)
		{ MCC_TRACE("br\n"); return 1; }
	for (i = 0; i < nb_vla_open; i++)
		{ MCC_TRACE("br\n"); if (vla_open_birth[i] == id)
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int vla_inner_scope(void) { MCC_TRACE("enter\n");
	return nb_vla_open ? vla_open_birth[nb_vla_open - 1] : 0;
}

static void vla_leave(struct scope *o) { MCC_TRACE("enter\n");
	struct scope *c = cur_scope, *v = NULL;
	for (; c != o && c; c = c->prev)
		{ MCC_TRACE("br\n"); if (c->vla.num)
			{ MCC_TRACE("br\n"); v = c; } }
	if (v)
		{ MCC_TRACE("br\n"); vla_restore(v->vla.locorig); }
}

static void new_scope(struct scope *o) { MCC_TRACE("enter\n");
	*o = *cur_scope;
	o->prev = cur_scope;
	cur_scope = o;
	cur_scope->vla.num = 0;
	cur_scope->vla_diag = 0;

	o->stdc_fp_contract = mcc_state->stdc_fp_contract;
	o->stdc_fenv_access = mcc_state->stdc_fenv_access;
	o->stdc_cx_limited = mcc_state->stdc_cx_limited;

	o->lstk = local_stack;
	o->llstk = local_label_stack;
	++local_scope;
}

static void prev_scope(struct scope *o, int is_expr) { MCC_TRACE("enter\n");
	if (o->vla_diag) { MCC_TRACE("br\n");
		nb_vla_open -= o->vla_diag;
		if (nb_vla_open < 0)
			{ MCC_TRACE("br\n"); nb_vla_open = 0; }
	}

	vla_leave(o->prev);

	if (o->cl.s != o->prev->cl.s)
		{ MCC_TRACE("br\n"); block_cleanup(o->prev); }

	if (debug_modes)
		{ MCC_TRACE("br\n"); mcc_debug_end_scope(o->lstk, !is_expr); }
	else if (mcc_state->do_asan_shadow && !is_expr)
		{ MCC_TRACE("br\n"); add_asan_locals(local_stack, o->lstk); }

	label_pop(&local_label_stack, o->llstk, is_expr);

	if ((mcc_state->warn_unused_variable & WARN_ON) && !is_expr) { MCC_TRACE("br\n");
		Sym *sm;
		for (sm = local_stack; sm && sm != o->lstk; sm = sm->prev) { MCC_TRACE("br\n");
			if ((sm->r & VT_VALMASK) == VT_LOCAL && !sm->a.used && sm->v >= TOK_IDENT && sm->v < SYM_FIRST_ANOM && !(sm->type.t & VT_TYPEDEF))
				{ MCC_TRACE("br\n"); mcc_warning_c(warn_unused_variable)(
						"%i:unused variable '%s'",
						sm->vla_inner_id, get_tok_str(sm->v, NULL)); }
		}
	}

	sym_pop(&local_stack, o->lstk, is_expr);

	mcc_state->stdc_fp_contract = o->stdc_fp_contract;
	mcc_state->stdc_fenv_access = o->stdc_fenv_access;
	mcc_state->stdc_cx_limited = o->stdc_cx_limited;

	cur_scope = o->prev;
	--local_scope;
}

static void leave_scope(struct scope *o) { MCC_TRACE("enter\n");
	if (!o)
		{ MCC_TRACE("br\n"); return; }
	try_call_scope_cleanup(o->cl.s);
	vla_leave(o);
}

static void new_scope_s(struct scope *o) { MCC_TRACE("enter\n");
	o->lstk = local_stack;
	++local_scope;
}

static void prev_scope_s(struct scope *o) { MCC_TRACE("enter\n");
	sym_pop(&local_stack, o->lstk, 0);
	--local_scope;
}

static void lblock(int *bsym, int *csym) { MCC_TRACE("enter\n");
	struct scope *lo = loop_scope, *co = cur_scope;
	int *b = co->bsym, *c = co->csym;
	if (csym) { MCC_TRACE("br\n");
		co->csym = csym;
		loop_scope = co;
	}
	co->bsym = bsym;
	block(0);
	co->bsym = b;
	if (csym) { MCC_TRACE("br\n");
		co->csym = c;
		loop_scope = lo;
	}
}

static void gexpr_decl(void) { MCC_TRACE("enter\n");
	int v = decl(VT_JMP);
	if (v > 1 && tok != ';') { MCC_TRACE("br\n");
		Sym *s = sym_find(v);
		vset(&s->type, s->r, (s->r & VT_SYM) ? 0 : s->c);
		vtop->sym = s;
	} else { MCC_TRACE("br\n");
		if (v)
			{ MCC_TRACE("br\n"); skip(';'); }
		gexpr();
	}
}

static int tok_starts_declspec(void) { MCC_TRACE("enter\n");
	switch (tok) { MCC_TRACE("br\n");
	case TOK_CHAR:
	case TOK_VOID:
	case TOK_SHORT:
	case TOK_INT:
	case TOK_LONG:
	case TOK_BOOL:
	case TOK_COMPLEX:
	case TOK_IMAGINARY:
	case TOK_FLOAT:
	case TOK_DOUBLE:
	case TOK_ENUM:
	case TOK_STRUCT:
	case TOK_UNION:
	case TOK__Atomic:
	case TOK_UNSIGNED:
	case TOK_CONST1:
	case TOK_CONST2:
	case TOK_CONST3:
	case TOK_VOLATILE1:
	case TOK_VOLATILE2:
	case TOK_VOLATILE3:
	case TOK_SIGNED1:
	case TOK_SIGNED2:
	case TOK_SIGNED3:
	case TOK_RESTRICT1:
	case TOK_RESTRICT2:
	case TOK_RESTRICT3:
	case TOK_AUTO:
	case TOK_REGISTER:
	case TOK_EXTERN:
	case TOK_STATIC:
	case TOK_TYPEDEF:
	case TOK_INLINE1:
	case TOK_INLINE2:
	case TOK_INLINE3:
	case TOK_NORETURN3:
	case TOK_THREAD_LOCAL:
	case TOK_ALIGNAS:
	case TOK_STATIC_ASSERT:
	case TOK_EXTENSION:
		return 1;
	default:
		if (tok >= TOK_IDENT) { MCC_TRACE("br\n");
			Sym *s = sym_find(tok);
			return s && (s->type.t & VT_TYPEDEF);
		}
		return 0;
	}
}

#if MCC_CONFIG_LSP
static uint16_t cst_stmt_kind(int t) { MCC_TRACE("enter\n");
	switch (t) { MCC_TRACE("br\n");
	case '{':
		return CST_CompoundStmt;
	case TOK_IF:
		return CST_If;
	case TOK_WHILE:
		return CST_While;
	case TOK_FOR:
		return CST_For;
	case TOK_DO:
		return CST_Do;
	case TOK_SWITCH:
		return CST_Switch;
	case TOK_RETURN:
		return CST_Return;
	case TOK_GOTO:
		return CST_Goto;
	default:
		return CST_ExprStmt;
	}
}
#endif

static void block(int flags) { MCC_TRACE("enter\n");
	int a, b, c, d, e, t;
	struct scope o;
	Sym *s;
	unsigned char stdc_save_fp, stdc_save_fenv, stdc_save_cx;

#if MCC_CONFIG_LSP
	CST_OPEN(cst_stmt_kind(tok));
	uint32_t cst_lm = 0;
#endif
again:
#if MCC_CONFIG_LSP
	cst_lm = CST_MARK();
#endif
	t = tok;
#if MCC_CONFIG_OPTIMIZER
	ast_hook_stmt(t);
#endif
	if (TOK_HAS_VALUE(t))
		{ MCC_TRACE("br\n"); goto expr; }
	stdc_save_fp = mcc_state->stdc_fp_contract;
	stdc_save_fenv = mcc_state->stdc_fenv_access;
	stdc_save_cx = mcc_state->stdc_cx_limited;
	next();

	seqp_reset();

	if (debug_modes)
		{ MCC_TRACE("br\n"); mcc_tcov_check_line(mcc_state, 0), mcc_tcov_block_begin(mcc_state); }

	if (t == TOK_IF) { MCC_TRACE("br\n");
		new_scope_s(&o);
		skip('(');
		gexpr_decl();
		if (expr_was_assign)
			{ MCC_TRACE("br\n"); mcc_warning_c(warn_parentheses)("suggest parentheses around "
																			"assignment used as a truth value"); }
		seqp_check();
#if MCC_CONFIG_OPTIMIZER
		ast_hook_if_begin();
#endif
		a = gvtst(1, 0);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_if_gvtst_done();
#endif
		skip(')');
		block(0);
		if (tok == TOK_ELSE) { MCC_TRACE("br\n");
			d = gjmp(0);
			gsym(a);
			next();
#if MCC_CONFIG_OPTIMIZER
			ast_hook_if_else();
#endif
			block(0);
			gsym(d);
		} else { MCC_TRACE("br\n");
			gsym(a);
		}
#if MCC_CONFIG_OPTIMIZER
		ast_hook_if_end();
#endif
		prev_scope_s(&o);
	} else if (t == TOK_WHILE) { MCC_TRACE("br\n");
		new_scope_s(&o);
		d = gind();
		skip('(');
		gexpr();
		if (expr_was_assign)
			{ MCC_TRACE("br\n"); mcc_warning_c(warn_parentheses)("suggest parentheses around "
																			"assignment used as a truth value"); }
		seqp_check();
#if MCC_CONFIG_OPTIMIZER
		ast_hook_while_begin();
#endif
		a = gvtst(1, 0);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_if_gvtst_done();
#endif
		skip(')');
		b = 0;
		lblock(&a, &b);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_while_end();
#endif
		gjmp_addr(d);
		gsym_addr(b, d);
		gsym(a);
		prev_scope_s(&o);
	} else if (t == '{') { MCC_TRACE("br\n");
		if (debug_modes)
			{ MCC_TRACE("br\n"); mcc_debug_stabn(mcc_state, N_LBRAC, ind - func_ind); }
		new_scope(&o);
		o.stdc_fp_contract = stdc_save_fp;
		o.stdc_fenv_access = stdc_save_fenv;
		o.stdc_cx_limited = stdc_save_cx;

		while (tok == TOK_LABEL) { MCC_TRACE("br\n");
			do { MCC_TRACE("br\n");
				next();
				if (tok < TOK_UIDENT)
					{ MCC_TRACE("br\n"); expect("label identifier"); }
				label_push(&local_label_stack, tok, LABEL_DECLARED);
				next();
			} while (tok == ',');
			skip(';');
		}

		{
			int seen_stmt = 0;
			while (tok != '}') { MCC_TRACE("br\n");
				if (seen_stmt && mcc_state->cversion < 199901 &&
						tok_starts_declspec())
					{ MCC_TRACE("br\n"); mcc_pedantic("mixed declarations and code are a "
											 "C99 feature"); }
				decl(VT_LOCAL);
				if (tok != '}') { MCC_TRACE("br\n");
					block(flags | STMT_COMPOUND);
					seen_stmt = 1;
				}
			}
		}

		prev_scope(&o, flags & STMT_EXPR);
		if (debug_modes)
			{ MCC_TRACE("br\n"); mcc_debug_stabn(mcc_state, N_RBRAC, ind - func_ind); }
		if (local_scope)
			{ MCC_TRACE("br\n"); next(); }
		else if (!nocode_wanted)
			{ MCC_TRACE("br\n"); check_func_return(); }
	} else if (t == TOK_RETURN) { MCC_TRACE("br\n");
		if (cur_func_noreturn)
			{ MCC_TRACE("br\n"); mcc_warning("function declared 'noreturn' has a 'return' statement"); }
		b = (func_vt.t & VT_BTYPE) != VT_VOID;
		if (tok != ';') { MCC_TRACE("br\n");
			gexpr();
#if MCC_CONFIG_OPTIMIZER
			ast_hook_ret_expr_done();
#endif
			seqp_check();
			if (b) { MCC_TRACE("br\n");
				gen_assign_cast(&func_vt);
			} else { MCC_TRACE("br\n");
				if (vtop->type.t != VT_VOID)
					{ MCC_TRACE("br\n"); mcc_warning_c(warn_return_type)("void function returns a value"); }
				vtop--;
			}
		} else if (b && func_old && (func_vt.t & VT_BTYPE) == VT_INT) { MCC_TRACE("br\n");
			vpushi(0);
		} else if (b) { MCC_TRACE("br\n");
			mcc_warning_c(warn_return_type)("'return' with no value");
			b = 0;
		}
#if MCC_CONFIG_OPTIMIZER
		ast_hook_return(b);
#endif
		leave_scope(root_scope);
		if (b)
			{ MCC_TRACE("br\n"); gfunc_return(&func_vt); }
		skip(';');
		{
			int ret_jumps = (tok != '}' || local_scope != 1);
			if (ret_jumps)
				{ MCC_TRACE("br\n"); rsym = gjmp(rsym); }
#if MCC_CONFIG_OPTIMIZER
			ast_hook_return_jmp(ret_jumps);
#endif
		}
		if (debug_modes)
			{ MCC_TRACE("br\n"); mcc_tcov_block_end(mcc_state, -1); }
		CODE_OFF();
	} else if (t == TOK_BREAK) { MCC_TRACE("br\n");
		if (!cur_scope->bsym)
			{ MCC_TRACE("br\n"); mcc_error("cannot break"); }
		if (cur_switch && cur_scope->bsym == cur_switch->bsym)
			{ MCC_TRACE("br\n"); leave_scope(cur_switch->scope); }
		else
			{ MCC_TRACE("br\n"); leave_scope(loop_scope); }
		*cur_scope->bsym = gjmp(*cur_scope->bsym);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_break_continue(0);
#endif
		skip(';');
	} else if (t == TOK_CONTINUE) { MCC_TRACE("br\n");
		if (!cur_scope->csym)
			{ MCC_TRACE("br\n"); mcc_error("cannot continue"); }
		leave_scope(loop_scope);
		*cur_scope->csym = gjmp(*cur_scope->csym);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_break_continue(1);
#endif
		skip(';');
	} else if (t == TOK_FOR) { MCC_TRACE("br\n");
		new_scope(&o);
		o.stdc_fp_contract = stdc_save_fp;
		o.stdc_fenv_access = stdc_save_fenv;
		o.stdc_cx_limited = stdc_save_cx;

		skip('(');
		if (tok != ';') { MCC_TRACE("br\n");
			in_for_init = 1;
			if (!decl(VT_JMP)) { MCC_TRACE("br\n");
				gexpr();
				vpop();
			} else if (mcc_state->cversion < 199901) { MCC_TRACE("br\n");
				mcc_pedantic("'for' loop initial declarations are a C99 feature");
			}
			in_for_init = 0;
		}
		seqp_flush();
		skip(';');
		a = b = 0;
		c = d = gind();
#if MCC_CONFIG_OPTIMIZER
		ast_hook_for_begin(tok != ';');
#endif
		if (tok != ';') { MCC_TRACE("br\n");
			gexpr();
			if (expr_was_assign)
				{ MCC_TRACE("br\n"); mcc_warning_c(warn_parentheses)("suggest parentheses around "
																				"assignment used as a truth value"); }
#if MCC_CONFIG_OPTIMIZER
			ast_hook_for_cond();
#endif
			a = gvtst(1, 0);
#if MCC_CONFIG_OPTIMIZER
			ast_hook_if_gvtst_done();
#endif
		}
		seqp_flush();
		skip(';');
		if (tok != ')') { MCC_TRACE("br\n");
			e = gjmp(0);
			d = gind();
#if MCC_CONFIG_OPTIMIZER
			ast_hook_for_incr_begin();
#endif
			gexpr();
			seqp_check();
			vpop();
#if MCC_CONFIG_OPTIMIZER
			ast_hook_for_incr_end();
#endif
			gjmp_addr(c);
			gsym(e);
		}
#if MCC_CONFIG_OPTIMIZER
		else
			{ MCC_TRACE("br\n"); ast_hook_for_no_incr(); }
#endif
		skip(')');
#if MCC_CONFIG_OPTIMIZER
		ast_hook_for_body_begin();
#endif
		lblock(&a, &b);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_for_end();
#endif
		gjmp_addr(d);
		gsym_addr(b, d);
		gsym(a);
		prev_scope(&o, 0);
	} else if (t == TOK_DO) { MCC_TRACE("br\n");
		new_scope_s(&o);
		a = b = 0;
		d = gind();
#if MCC_CONFIG_OPTIMIZER
		ast_hook_do_begin();
#endif
		lblock(&a, &b);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_do_body_end();
#endif
		gsym(b);
		skip(TOK_WHILE);
		skip('(');
		gexpr();
		if (expr_was_assign)
			{ MCC_TRACE("br\n"); mcc_warning_c(warn_parentheses)("suggest parentheses around "
																			"assignment used as a truth value"); }
		seqp_check();
#if MCC_CONFIG_OPTIMIZER
		ast_hook_do_cond();
#endif
		c = gvtst(0, 0);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_if_gvtst_done();
#endif
		skip(')');
		skip(';');
		gsym_addr(c, d);
		gsym(a);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_do_end();
#endif
		prev_scope_s(&o);
	} else if (t == TOK_SWITCH) { MCC_TRACE("br\n");
		struct switch_t *sw;

		sw = mcc_mallocz(sizeof *sw);
		sw->bsym = &a;
		sw->scope = cur_scope;
		sw->prev = cur_switch;
		sw->nocode_wanted = nocode_wanted;
		sw->vla_gpp = vla_seq;
		cur_switch = sw;

		new_scope_s(&o);
		skip('(');
		gexpr_decl();
		seqp_check();
		if (!is_integer_btype(vtop->type.t & VT_BTYPE))
			{ MCC_TRACE("br\n"); mcc_error("switch value not an integer"); }
		skip(')');
#if MCC_CONFIG_OPTIMIZER
		ast_hook_switch_begin();
#endif
		sw->sv = *vtop--;
		a = 0;
		b = gjmp(0);
		lblock(&a, NULL);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_switch_body_end();
#endif
		a = gjmp(a);
		gsym(b);
		prev_scope_s(&o);
		if ((mcc_state->warn_switch & WARN_ON) && IS_ENUM(sw->sv.type.t) && !sw->def_sym) { MCC_TRACE("br\n");
			Sym *ev;
			for (ev = sw->sv.type.ref->next; ev; ev = ev->next) { MCC_TRACE("br\n");
				int64_t val = ev->enum_val;
				int i, covered = 0;
				for (i = 0; i < sw->n; i++)
					{ MCC_TRACE("br\n"); if (val >= sw->p[i]->v1 && val <= sw->p[i]->v2) { MCC_TRACE("br\n");
						covered = 1;
						break;
					} }
				if (!covered)
					{ MCC_TRACE("br\n"); mcc_warning_c(warn_switch)(
							"enumeration value '%s' not handled in switch",
							get_tok_str(ev->v & ~SYM_FIELD, NULL)); }
			}
		}
		if (sw->nocode_wanted)
			{ MCC_TRACE("br\n"); goto skip_switch; }
		case_sort(sw);
		sw->bsym = NULL;
		vpushv(&sw->sv);
		gv(MCC_RC_INT);
		d = gcase(sw->p, sw->n, 0);
		vpop();
		if (sw->def_sym)
			{ MCC_TRACE("br\n"); gsym_addr(d, sw->def_sym); }
		else
			{ MCC_TRACE("br\n"); gsym(d); }
	skip_switch:
		gsym(a);
		end_switch();
#if MCC_CONFIG_OPTIMIZER
		ast_hook_switch_end();
#endif
	} else if (t == TOK_CASE) { MCC_TRACE("br\n");
		struct case_t *cr;
		if (!cur_switch)
			{ MCC_TRACE("br\n"); expect("switch"); }
		cr = mcc_malloc(sizeof(struct case_t));
		dynarray_add(&cur_switch->p, &cur_switch->n, cr);
		t = cur_switch->sv.type.t;
		ice_float_op = ice_nonconst = 0;
		cr->v1 = cr->v2 = value64(expr_const64(), t);
		if (ice_float_op || ice_nonconst)
			{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids a case label that is not an integer "
									 "constant expression"); }
		if (tok == TOK_DOTS && gnu_ext) { MCC_TRACE("br\n");
			next();
			cr->v2 = value64(expr_const64(), t);
			if (case_cmp(cr->v2, cr->v1) < 0)
				{ MCC_TRACE("br\n"); mcc_warning("empty case range"); }
		}
		if (!cur_switch->nocode_wanted)
			{ MCC_TRACE("br\n"); cr->ind = gind(); }
		cr->line = file->line_num;
#if MCC_CONFIG_OPTIMIZER
		ast_hook_case(cr->v1, cr->v2, t);
#endif
		skip(':');
		if (cur_switch->vla_gpp < vla_inner_scope())
			{ MCC_TRACE("br\n"); mcc_error("switch jumps into the scope of a variably modified declaration"); }
		goto block_after_label;
	} else if (t == TOK_DEFAULT) { MCC_TRACE("br\n");
		if (!cur_switch)
			{ MCC_TRACE("br\n"); expect("switch"); }
		if (cur_switch->def_sym)
			{ MCC_TRACE("br\n"); mcc_error("too many 'default'"); }
		cur_switch->def_sym = cur_switch->nocode_wanted ? -1 : gind();
#if MCC_CONFIG_OPTIMIZER
		ast_hook_default();
#endif
		skip(':');
		if (cur_switch->vla_gpp < vla_inner_scope())
			{ MCC_TRACE("br\n"); mcc_error("switch jumps into the scope of a variably modified declaration"); }
		goto block_after_label;
	} else if (t == TOK_GOTO) { MCC_TRACE("br\n");
		vla_restore(cur_scope->vla.locorig);
		if (tok == '*' && gnu_ext) { MCC_TRACE("br\n");
			next();
			gexpr();
			if ((vtop->type.t & VT_BTYPE) != VT_PTR)
				{ MCC_TRACE("br\n"); expect("pointer"); }
			ggoto();
		} else if (tok >= TOK_UIDENT) { MCC_TRACE("br\n");
			s = label_find(tok);
			if (!s)
				{ MCC_TRACE("br\n"); s = label_push(&global_label_stack, tok, LABEL_FORWARD); }
			else if (s->r == LABEL_DECLARED)
				{ MCC_TRACE("br\n"); s->r = LABEL_FORWARD; }

			if (s->r & LABEL_FORWARD) { MCC_TRACE("br\n");
				if (vla_seq < s->vla_min_goto_gpp)
					{ MCC_TRACE("br\n"); s->vla_min_goto_gpp = vla_seq; }
				if (cur_scope->cl.s && !nocode_wanted) { MCC_TRACE("br\n");
					sym_push2(&pending_gotos, SYM_FIELD, 0, cur_scope->cl.n);
					pending_gotos->cleanup_label = s;
					s = sym_push2(&s->next, SYM_FIELD, 0, 0);
					pending_gotos->next = s;
				}
				s->jnext = gjmp(s->jnext);
			} else { MCC_TRACE("br\n");
				if (!vla_scope_open(s->vla_inner_id))
					{ MCC_TRACE("br\n"); mcc_error("goto jumps into the scope of a variably modified declaration"); }
				try_call_cleanup_goto(s->cleanupstate);
				gjmp_addr(s->jind);
			}
#if MCC_CONFIG_OPTIMIZER
			ast_hook_goto(tok);
#endif
			next();
		} else { MCC_TRACE("br\n");
			expect("label identifier");
		}
		skip(';');
	} else if (t == TOK_ASM1 || t == TOK_ASM2 || t == TOK_ASM3) { MCC_TRACE("br\n");
#if MCC_CONFIG_OPTIMIZER
		ast_func_has_asm = 1;
#endif
#if MCC_CONFIG_ASM
		asm_instr();
#else
		mcc_error("inline assembler not supported (built without MCC_CONFIG_ASM)");
#endif
	} else { MCC_TRACE("br\n");
		if (tok == ':' && t >= TOK_UIDENT) { MCC_TRACE("br\n");
			next();
#if MCC_CONFIG_LSP
			CST_OPEN_AT(CST_Label, cst_lm);
			CST_CLOSE();
#endif
			s = label_find(t);
			if (s) { MCC_TRACE("br\n");
				if (s->r == LABEL_DEFINED)
					{ MCC_TRACE("br\n"); mcc_error("duplicate label '%s'", get_tok_str(s->v, NULL)); }
				s->r = LABEL_DEFINED;
				if (s->next) { MCC_TRACE("br\n");
					Sym *pcl;
					for (pcl = s->next; pcl; pcl = pcl->prev)
						{ MCC_TRACE("br\n"); gsym(pcl->jnext); }
					sym_pop(&s->next, NULL, 0);
				} else
					{ MCC_TRACE("br\n"); gsym(s->jnext); }
			} else { MCC_TRACE("br\n");
				s = label_push(&global_label_stack, t, LABEL_DEFINED);
			}
			s->jind = gind();
			s->cleanupstate = cur_scope->cl.s;
			s->vla_inner_id = vla_inner_scope();
			if (s->vla_min_goto_gpp < s->vla_inner_id)
				{ MCC_TRACE("br\n"); mcc_error("goto jumps into the scope of a variably modified declaration"); }
#if MCC_CONFIG_OPTIMIZER
			ast_hook_label(t);
#endif

		block_after_label:
			parse_attribute(NULL);

			if (tok != '}' && mcc_state->cversion < 202311 && tok_starts_declspec())
				{ MCC_TRACE("br\n"); mcc_pedantic("a label can only be part of a statement and a "
										 "declaration is not a statement"); }

			if (debug_modes)
				{ MCC_TRACE("br\n"); mcc_tcov_reset_ind(mcc_state); }
			vla_restore(cur_scope->vla.loc);

			if (tok != '}') { MCC_TRACE("br\n");
				if (0 == (flags & STMT_COMPOUND))
					{ MCC_TRACE("br\n"); goto again; }
			} else { MCC_TRACE("br\n");
				if (mcc_state->cversion < 202311 && (mcc_state->warn_pedantic || mcc_state->pedantic_errors))
					{ MCC_TRACE("br\n"); mcc_pedantic("label at end of compound statement is a "
											 "C23 feature"); }
				else
					{ MCC_TRACE("br\n"); mcc_warning_c(warn_all)("deprecated use of label at end of "
																	"compound statement"); }
			}
		} else { MCC_TRACE("br\n");
			if (t != ';') { MCC_TRACE("br\n");
				unget_tok(t);
			expr:
				seqp_reset();
				if (flags & STMT_EXPR) { MCC_TRACE("br\n");
					vpop();
					gexpr();
				} else { MCC_TRACE("br\n");
					expr_has_effect = 0;
					gexpr();
					if ((mcc_state->warn_unused_value & WARN_ON) && !expr_has_effect && !(vtop->type.t & VT_VOLATILE))
						{ MCC_TRACE("br\n"); mcc_warning_c(warn_unused_value)(
								"value computed is not used"); }
					vpop();
				}
				seqp_check();
				skip(';');
			}
		}
	}

	if (debug_modes)
		{ MCC_TRACE("br\n"); mcc_tcov_check_line(mcc_state, 0), mcc_tcov_block_end(mcc_state, 0); }
#if MCC_CONFIG_LSP
	CST_CLOSE();
#endif
}

static void skip_or_save_block(TokenString **str) { MCC_TRACE("enter\n");
	int braces = tok == '{';
	int level = 0;
	if (str)
		{ MCC_TRACE("br\n"); *str = tok_str_alloc(); }

	while (1) { MCC_TRACE("br\n");
		int t = tok;
		if (level == 0 && (t == ',' || t == ';' || t == '}' || t == ')' || t == ']'))
			{ MCC_TRACE("br\n"); break; }
		if (t == TOK_EOF) { MCC_TRACE("br\n");
			if (str || level > 0)
				{ MCC_TRACE("br\n"); mcc_error("unexpected end of file"); }
			else
				{ MCC_TRACE("br\n"); break; }
		}
		if (str)
			{ MCC_TRACE("br\n"); tok_str_add_tok(*str); }
		next();
		if (t == '{' || t == '(' || t == '[') { MCC_TRACE("br\n");
			level++;
		} else if (t == '}' || t == ')' || t == ']') { MCC_TRACE("br\n");
			level--;
			if (level == 0 && braces && t == '}')
				{ MCC_TRACE("br\n"); break; }
		}
	}
	if (str)
		{ MCC_TRACE("br\n"); tok_str_add(*str, TOK_EOF); }
}

#define EXPR_CONST 1
#define EXPR_ANY 2

static void parse_init_elem(int expr_type) { MCC_TRACE("enter\n");
	int saved_global_expr;
	switch (expr_type) { MCC_TRACE("br\n");
	case EXPR_CONST:
		saved_global_expr = global_expr;
		global_expr = 1;
		expr_const1();
		global_expr = saved_global_expr;
		if (((vtop->r & (VT_VALMASK | VT_LVAL)) != VT_CONST && ((vtop->r & (VT_SYM | VT_LVAL)) != (VT_SYM | VT_LVAL) ||
																														vtop->sym->v < SYM_FIRST_ANOM))
#ifdef MCC_TARGET_PE
				|| ((vtop->r & VT_SYM) && vtop->sym->a.dllimport)
#endif
		)
			mcc_error("initializer element is not constant");
		break;
	case EXPR_ANY:
		expr_eq();
		break;
	}
}

#ifndef NDEBUG
static void init_assert(init_params *p, int offset) { MCC_TRACE("enter\n");
	if (p->sec
					? !NODATA_WANTED && offset > p->sec->data_offset
					: !nocode_wanted && offset > p->local_offset)
		{ MCC_TRACE("br\n"); mcc_internal_error("initializer overflow"); }
}
#else
#define init_assert(sec, offset)
#endif

static void init_llocal_push_addr(init_params *p, unsigned long c) { MCC_TRACE("enter\n");
	vset(&char_pointer_type, VT_LOCAL | VT_LVAL, p->llocal);
	if (c) { MCC_TRACE("br\n");
		vpushi((int)c);
		gen_op('+');
	}
}

static void init_putz(init_params *p, unsigned long c, int size) { MCC_TRACE("enter\n");
	init_assert(p, c + size);
	if (p->sec) { MCC_TRACE("br\n");
	} else if (p->llocal) { MCC_TRACE("br\n");
		vpush_helper_func(TOK_memset);
		init_llocal_push_addr(p, c);
		vpushi(0);
		vpushs(size);
#if defined MCC_TARGET_ARM && defined MCC_ARM_EABI
		vswap();
#endif
#if MCC_CONFIG_OPTIMIZER
		ast_hook_call_begin(3, 0, 1, 0);
#endif
		gfunc_call(3);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_call_effect_end();
#endif
	} else { MCC_TRACE("br\n");
		vpush_helper_func(TOK_memset);
		vseti(VT_LOCAL, c);
		vpushi(0);
		vpushs(size);
#if defined MCC_TARGET_ARM && defined MCC_ARM_EABI
		vswap();
#endif
#if MCC_CONFIG_OPTIMIZER
		ast_hook_call_begin(3, 0, 1, 0);
#endif
		gfunc_call(3);
#if MCC_CONFIG_OPTIMIZER
		ast_hook_call_effect_end();
#endif
	}
}

#define DIF_FIRST 1
#define DIF_SIZE_ONLY 2
#define DIF_HAVE_ELEM 4
#define DIF_CLEAR 8

#if defined MCC_TARGET_X86_64 && !defined MCC_TARGET_PE
#define STACK_OVERALIGN_MAX 16
#elif defined MCC_TARGET_RISCV64
#define STACK_OVERALIGN_MAX 16
#elif defined MCC_TARGET_ARM64
#define STACK_OVERALIGN_MAX 16
#elif defined MCC_TARGET_I386
#define STACK_OVERALIGN_MAX 8
#elif defined MCC_TARGET_ARM
#define STACK_OVERALIGN_MAX 8
#endif

static void decl_design_delrels(Section *sec, int c, int size) { MCC_TRACE("enter\n");
	ElfW_Rel *rel, *rel2, *rel_end;
	if (!sec || !sec->reloc)
		{ MCC_TRACE("br\n"); return; }
	rel = rel2 = (ElfW_Rel *)sec->reloc->data;
	rel_end = (ElfW_Rel *)(sec->reloc->data + sec->reloc->data_offset);
	while (rel < rel_end) { MCC_TRACE("br\n");
		if (rel->r_offset >= c && rel->r_offset < c + size) { MCC_TRACE("br\n");
			sec->reloc->data_offset -= sizeof *rel;
		} else { MCC_TRACE("br\n");
			if (rel2 != rel)
				{ MCC_TRACE("br\n"); memcpy(rel2, rel, sizeof *rel); }
			++rel2;
		}
		++rel;
	}
}

static void decl_design_flex(init_params *p, Sym *ref, int index) { MCC_TRACE("enter\n");
	if (ref == p->flex_array_ref) { MCC_TRACE("br\n");
		if (p->flex_is_member && index >= 0 && !p->flex_warned) { MCC_TRACE("br\n");
			p->flex_warned = 1;
			mcc_pedantic("initialization of a flexible array member is not "
									 "allowed in ISO C");
		}
		if (index >= ref->c)
			{ MCC_TRACE("br\n"); ref->c = index + 1; }
	} else if (ref->c < 0 && index >= 0)
		{ MCC_TRACE("br\n"); mcc_error("flexible array has zero size in this context"); }
}

static int decl_designator(init_params *p, CType *type, unsigned long c,
													 Sym **cur_field, int flags, int al) { MCC_TRACE("enter\n");
	Sym *s, *f;
	int index, index_last, align, l, nb_elems, elem_size, routed = 0;
	unsigned long corig = c;

	elem_size = 0;
	nb_elems = 1;

	if (flags & DIF_HAVE_ELEM)
		{ MCC_TRACE("br\n"); goto no_designator; }

	if (gnu_ext && tok >= TOK_UIDENT) { MCC_TRACE("br\n");
		l = tok, next();
		if (tok == ':')
			{ MCC_TRACE("br\n"); goto struct_field; }
		unget_tok(l);
	}

	if ((tok == '[' || tok == '.') && mcc_state->cversion < 199901)
		{ MCC_TRACE("br\n"); mcc_pedantic("designated initializers are a C99 feature"); }

	while (nb_elems == 1 && (tok == '[' || tok == '.')) { MCC_TRACE("br\n");
		if (tok == '[') { MCC_TRACE("br\n");
			if (!(type->t & VT_ARRAY))
				{ MCC_TRACE("br\n"); expect("array type"); }
			next();
			index = index_last = expr_const();
			if (tok == TOK_DOTS && gnu_ext) { MCC_TRACE("br\n");
				next();
				index_last = expr_const();
			}
			skip(']');
			s = type->ref;
			decl_design_flex(p, s, index_last);
			if (index < 0 || index_last >= s->c || index_last < index)
				{ MCC_TRACE("br\n"); mcc_error("index exceeds array bounds or range is empty"); }
			if (cur_field)
				{ MCC_TRACE("br\n"); (*cur_field)->c = index_last; }
			type = pointed_type(type);
			elem_size = type_size(type, &align);
			c += index * elem_size;
			nb_elems = index_last - index + 1;
		} else { MCC_TRACE("br\n");
			int cumofs;
			next();
			l = tok;
		struct_field:
			next();
			f = find_field(type, l, &cumofs);
			if (cur_field)
				{ MCC_TRACE("br\n"); *cur_field = f; }
			type = &f->type;
			c += cumofs;
		}
		if (((type->t & VT_ARRAY) || (type->t & VT_BTYPE) == VT_STRUCT) && (tok == '[' || tok == '.')) { MCC_TRACE("br\n");
			cur_field = NULL;
			routed = 1;
			break;
		}
		cur_field = NULL;
	}
	if (!cur_field) { MCC_TRACE("br\n");
		if (tok == '=') { MCC_TRACE("br\n");
			next();
		} else if (!gnu_ext) { MCC_TRACE("br\n");
			expect("=");
		}
	} else { MCC_TRACE("br\n");
	no_designator:
		if (type->t & VT_ARRAY) { MCC_TRACE("br\n");
			index = (*cur_field)->c;
			s = type->ref;
			decl_design_flex(p, s, index);
			if (index >= s->c)
				{ MCC_TRACE("br\n"); mcc_error("too many initializers"); }
			type = pointed_type(type);
			elem_size = type_size(type, &align);
			c += index * elem_size;
		} else { MCC_TRACE("br\n");
			f = *cur_field;
			while (f && (f->v & SYM_FIRST_ANOM) &&
						 is_integer_btype(f->type.t & VT_BTYPE))
				{ MCC_TRACE("br\n"); *cur_field = f = f->next; }
			if (!f)
				{ MCC_TRACE("br\n"); mcc_error("too many initializers"); }
			type = &f->type;
			c += f->c;
		}
	}

	if (!elem_size)
		{ MCC_TRACE("br\n"); elem_size = type_size(type, &align); }

	if (!routed && !(flags & DIF_SIZE_ONLY) && c - corig < al) { MCC_TRACE("br\n");
		decl_design_delrels(p->sec, c, elem_size * nb_elems);
		flags &= ~DIF_CLEAR;
	}

	decl_initializer(p, type, c, flags & ~DIF_FIRST);

	if (!(flags & DIF_SIZE_ONLY) && nb_elems > 1) { MCC_TRACE("br\n");
		Sym aref = {0};
		CType t1;
		if (p->sec || (type->t & VT_ARRAY)) { MCC_TRACE("br\n");
			aref.c = elem_size;
			t1.t = VT_STRUCT, t1.ref = &aref;
			type = &t1;
		}
		if (p->sec)
			{ MCC_TRACE("br\n"); vpush_ref(type, p->sec, c, elem_size); }
		else
			{ MCC_TRACE("br\n"); vset(type, VT_LOCAL | VT_LVAL, c); }
		for (int i = 1; i < nb_elems; i++) { MCC_TRACE("br\n");
			vdup();
			init_putv(p, type, c + elem_size * i);
		}
		vpop();
	}

	c += nb_elems * elem_size;
	if (c - corig > al)
		{ MCC_TRACE("br\n"); al = c - corig; }
	return al;
}

static void write_ldouble(unsigned char *d, void *s) { MCC_TRACE("enter\n");
#ifdef MCC_CROSS_TEST
	if (MCC_LDOUBLE_SIZE >= 10) { MCC_TRACE("br\n");
		double b = *(long double *)s;
		s = &b;
#else
	if (sizeof(long double) == 8 && MCC_LDOUBLE_SIZE >= 10) { MCC_TRACE("br\n");
#endif
		uint64_t m = *(uint64_t *)s;
		int e = m >> 48;
		int f = e >> 4 & 0x7FF;
		m <<= 11;
		if (0 == f) { MCC_TRACE("br\n");
			if (0 == m)
				{ MCC_TRACE("br\n"); goto set; }
			for (f = 1; !(m & 1ULL << 63); --f)
				{ MCC_TRACE("br\n"); m <<= 1; }
		}
		if (f == 0x7ff)
			{ MCC_TRACE("br\n"); f = 0x43FF; }
		e = (e & 0x8000) | (f + 0x3C00);
		m |= 1ULL << 63;
	set:
#if (defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64)
		write64le(d, m);
		write16le(d + 8, e);
#elif MCC_LDOUBLE_SIZE == 16
		write64le(d + 6, m << 1);
		write16le(d + 14, e);
#endif
		;
	} else { MCC_TRACE("br\n");
#if MCC_LDOUBLE_SIZE == 8
		double b = *(long double *)s;
		memcpy(d, &b, 8);
#elif (__i386__ || __x86_64__) && (defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64)
		memcpy(d, s, 10);
#elif (__i386__ || __x86_64__) && (defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64)
	uint64_t m = *(uint64_t *)s;
	int e = *(uint16_t *)((char *)s + 8);
	write64le(d + 6, m << 1);
	write16le(d + 14, e);
#elif (__aarch64__ || __riscv) && (defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64)
	uint64_t m = read64le((unsigned char *)s + 6);
	int e = read16le((unsigned char *)s + 14);

	if ((e & 0x7fff) && (m & 1) && 0 == ++m)
		{ MCC_TRACE("br\n"); ++e; }
	write64le(d, m >> 1 | ((e & 0x7fff) ? 1ULL << 63 : 0));
	write16le(d + 8, e);
#else
	if (sizeof(long double) == MCC_LDOUBLE_SIZE)
		{ MCC_TRACE("br\n"); memcpy(d, s, MCC_LDOUBLE_SIZE); }
#endif
	}
}

static void init_putv(init_params *p, CType *type, unsigned long c) {
	int bt;
	void *ptr;
	CType dtype;
	int size, align;
	Section *sec = p->sec;
	uint64_t val;

	dtype = *type;
	dtype.t &= ~VT_CONSTANT;

	size = type_size(type, &align);
	if (type->t & VT_BITFIELD)
		{ MCC_TRACE("br\n"); size = (BIT_POS(type->t) + BIT_SIZE(type->t) + 7) / 8; }
	init_assert(p, c + size);

	if (sec) { MCC_TRACE("br\n");
		if (is_complex_type(type) && !is_complex_type(&vtop->type)) { MCC_TRACE("br\n");
			CType base = type->ref->next->type;
			int bsz, bal;
			bsz = type_size(&base, &bal);
			init_putv(p, &base, c);
			vpushi(0);
			init_putv(p, &base, c + bsz);
			return;
		}
		gen_assign_cast(&dtype);
		bt = type->t & VT_BTYPE;

		if ((vtop->r & VT_SYM) && bt != VT_PTR && (bt != (MCC_PTR_SIZE == 8 ? VT_LLONG : VT_INT) || (type->t & VT_BITFIELD)) && !((vtop->r & VT_CONST) && vtop->sym->v >= SYM_FIRST_ANOM))
			{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load time"); }

		if (NODATA_WANTED) { MCC_TRACE("br\n");
			vtop--;
			return;
		}

		ptr = sec->data + c;
		val = vtop->c.i;

		if ((vtop->r & (VT_SYM | VT_CONST)) == (VT_SYM | VT_CONST) && vtop->sym->v >= SYM_FIRST_ANOM && ((vtop->r & VT_LVAL) || bt == VT_STRUCT)) { MCC_TRACE("br\n");
			Section *ssec;
			ElfSym *esym;
			ElfW_Rel *rel;
			esym = elfsym(vtop->sym);
			ssec = mcc_state->sections[esym->st_shndx];
			memmove(ptr, ssec->data + esym->st_value + (int)vtop->c.i, size);
			if (ssec->reloc) { MCC_TRACE("br\n");
				unsigned long relofs = ssec->reloc->data_offset;
				while (relofs >= sizeof(*rel)) { MCC_TRACE("br\n");
					relofs -= sizeof(*rel);
					rel = (ElfW_Rel *)(ssec->reloc->data + relofs);
					if (rel->r_offset >= esym->st_value + size)
						{ MCC_TRACE("br\n"); continue; }
					if (rel->r_offset < esym->st_value)
						{ MCC_TRACE("br\n"); break; }
					put_elf_reloca(symtab_section, sec,
												 c + rel->r_offset - esym->st_value,
												 ELFW(R_TYPE)(rel->r_info),
												 ELFW(R_SYM)(rel->r_info),
#if MCC_PTR_SIZE == 8
												 rel->r_addend
#else
												 0
#endif
					);
				}
			}
		} else { MCC_TRACE("br\n");
			if (type->t & VT_BITFIELD) { MCC_TRACE("br\n");
				int bit_pos, bit_size, bits, n;
				unsigned char *p, v, m;
				bit_pos = BIT_POS(vtop->type.t);
				bit_size = BIT_SIZE(vtop->type.t);
				p = (unsigned char *)ptr + (bit_pos >> 3);
				bit_pos &= 7, bits = 0;
				while (bit_size) { MCC_TRACE("br\n");
					n = 8 - bit_pos;
					if (n > bit_size)
						{ MCC_TRACE("br\n"); n = bit_size; }
					v = val >> bits << bit_pos;
					m = ((1 << n) - 1) << bit_pos;
					*p = (*p & ~m) | (v & m);
					bits += n, bit_size -= n, bit_pos = 0, ++p;
				}
			} else
				switch (bt) { MCC_TRACE("br\n");
				case VT_BOOL:
					*(char *)ptr = val != 0;
					break;
				case VT_BYTE:
					*(char *)ptr = val;
					break;
				case VT_SHORT:
					write16le(ptr, val);
					break;
				case VT_FLOAT:
					write32le(ptr, val);
					break;
				case VT_DOUBLE:
					write64le(ptr, val);
					break;
				case VT_LDOUBLE:
					write_ldouble(ptr, &vtop->c.ld);
					break;

#if MCC_PTR_SIZE == 8
				case VT_LLONG:
				case VT_PTR:
					if (vtop->r & VT_SYM)
						{ MCC_TRACE("br\n"); greloca(sec, vtop->sym, c, R_DATA_PTR, val); }
					else
						{ MCC_TRACE("br\n"); write64le(ptr, val); }
					break;
				case VT_INT:
					write32le(ptr, val);
					break;
#else
				case VT_LLONG:
					write64le(ptr, val);
					break;
				case VT_PTR:
				case VT_INT:
					if (vtop->r & VT_SYM)
						{ MCC_TRACE("br\n"); greloc(sec, vtop->sym, c, R_DATA_PTR); }
					write32le(ptr, val);
					break;
#endif
				default:
					break;
				}
		}
		vtop--;
	} else if (p->llocal) { MCC_TRACE("br\n");
		int rr;
		init_llocal_push_addr(p, c);
		rr = gv(MCC_RC_INT);
		vtop->type = dtype;
		vtop->r = rr | VT_LVAL;
		vtop->c.i = 0;
		vswap();
		vstore();
		vpop();
	} else { MCC_TRACE("br\n");
		vset(&dtype, VT_LOCAL | VT_LVAL, c);
		vswap();
		vstore();
		vpop();
	}
}

static int merge_str_kind(int merge_kind, int tk) {
	if (tk != TOK_STR) { MCC_TRACE("br\n");
		if (merge_kind && merge_kind != tk)
			{ MCC_TRACE("br\n"); mcc_error("unsupported concatenation of string literals "
								"with different encoding prefixes"); }
		merge_kind = tk;
	}
	return merge_kind;
}

static void decl_initializer(init_params *p, CType *type, unsigned long c, int flags) {
	int len, n, no_oblock;
	int size1, align1;
	Sym *s, *f;
	Sym indexsym;
	CType *t1;

	if (debug_modes && !(flags & DIF_SIZE_ONLY) && !p->sec)
		{ MCC_TRACE("br\n"); mcc_debug_line(mcc_state), mcc_tcov_check_line(mcc_state, 1); }

	if (!(flags & DIF_HAVE_ELEM) && tok != '{' &&
			tok != TOK_LSTR && tok != TOK_STR && tok != TOK_U32STR && tok != TOK_U16STR &&
			tok != TOK_U8STR &&
			!(((type->t & VT_ARRAY) || (type->t & VT_BTYPE) == VT_STRUCT) && (tok == '[' || tok == '.')) &&
			(!(flags & DIF_SIZE_ONLY) || (type->t & VT_BTYPE) == VT_STRUCT)) { MCC_TRACE("br\n");
		int ncw_prev = nocode_wanted;
		int aci_prev = assign_ctx_is_init;
		if ((flags & DIF_SIZE_ONLY) && !p->sec)
			{ MCC_TRACE("br\n"); ++nocode_wanted; }
		assign_ctx_is_init = 0;
		parse_init_elem(!p->sec ? EXPR_ANY : EXPR_CONST);
		assign_ctx_is_init = aci_prev;
		nocode_wanted = ncw_prev;
		flags |= DIF_HAVE_ELEM;
	}

	if (type->t & VT_ARRAY) { MCC_TRACE("br\n");
		no_oblock = 1;
		if (((flags & DIF_FIRST) && tok != TOK_LSTR && tok != TOK_STR && tok != TOK_U32STR && tok != TOK_U16STR && tok != TOK_U8STR) ||
				tok == '{') { MCC_TRACE("br\n");
			skip('{');
			no_oblock = 0;
		}

		s = type->ref;
		n = s->c;
		t1 = pointed_type(type);
		size1 = type_size(t1, &align1);

		if ((tok == TOK_LSTR &&
#ifdef MCC_TARGET_PE
				 (t1->t & VT_BTYPE) == VT_SHORT && (t1->t & VT_UNSIGNED)
#else
				 (t1->t & VT_BTYPE) == VT_INT
#endif
						 ) ||
				(tok == TOK_U32STR && (t1->t & VT_BTYPE) == VT_INT) || (tok == TOK_U16STR && (t1->t & VT_BTYPE) == VT_SHORT && (t1->t & VT_UNSIGNED)) || ((tok == TOK_STR || tok == TOK_U8STR) && (t1->t & VT_BTYPE) == VT_BYTE)) { MCC_TRACE("br\n");
			len = 0;
			cstr_reset(&initstr);
			{
				int merge_kind = (tok == TOK_STR) ? 0 : tok;
				while (tok == TOK_STR || tok == TOK_LSTR || tok == TOK_U16STR || tok == TOK_U32STR || tok == TOK_U8STR) { MCC_TRACE("br\n");
					int toksz = tok == TOK_STR || tok == TOK_U8STR
													? 1
											: tok == TOK_U16STR
													? 2
											: tok == TOK_U32STR
													? 4
													: sizeof(nwchar_t);
					int nch = tokc.str.size / toksz, i;
					merge_kind = merge_str_kind(merge_kind, tok);
					if (toksz > size1)
						{ MCC_TRACE("br\n"); mcc_error("a wide string literal cannot follow a narrower "
											"string literal in a concatenation"); }
					if (initstr.size)
						{ MCC_TRACE("br\n"); initstr.size -= size1; }
					len += nch - 1;
					if (toksz == size1) { MCC_TRACE("br\n");
						cstr_cat(&initstr, tokc.str.data, tokc.str.size);
					} else { MCC_TRACE("br\n");
						for (i = 0; i < nch; i++) { MCC_TRACE("br\n");
							unsigned int ch =
									toksz == 1
											? ((unsigned char *)tokc.str.data)[i]
									: toksz == 2
											? ((unsigned short *)tokc.str.data)[i]
											: ((unsigned int *)tokc.str.data)[i];
							if (size1 == 1) { MCC_TRACE("br\n");
								unsigned char v = ch;
								cstr_cat(&initstr, (char *)&v, 1);
							} else if (size1 == 2) { MCC_TRACE("br\n");
								unsigned short v = ch;
								cstr_cat(&initstr, (char *)&v, 2);
							} else { MCC_TRACE("br\n");
								unsigned int v = ch;
								cstr_cat(&initstr, (char *)&v, 4);
							}
						}
					}
					next();
				}
			}
			if (tok != ')' && tok != '}' && tok != ',' && tok != ';' && tok != TOK_EOF) { MCC_TRACE("br\n");
				unget_tok(size1 == 1
											? TOK_STR
									: size1 == 2
											? TOK_U16STR
									: size1 == 4
											? TOK_U32STR
											: TOK_LSTR);
				tokc.str.size = initstr.size;
				tokc.str.data = initstr.data;
				goto do_init_array;
			}

			decl_design_flex(p, s, len);
			if (!(flags & DIF_SIZE_ONLY)) { MCC_TRACE("br\n");
				int nb = n, ch;
				if (len < nb)
					{ MCC_TRACE("br\n"); nb = len; }
				if (len > nb)
					{ MCC_TRACE("br\n"); mcc_warning("initializer-string for array is too long"); }
				if (p->sec && size1 == 1) { MCC_TRACE("br\n");
					init_assert(p, c + nb);
					if (!NODATA_WANTED)
						{ MCC_TRACE("br\n"); memcpy(p->sec->data + c, initstr.data, nb); }
				} else { MCC_TRACE("br\n");
					for (int i = 0; i < n; i++) { MCC_TRACE("br\n");
						if (i >= nb) { MCC_TRACE("br\n");
							if (flags & DIF_CLEAR)
								{ MCC_TRACE("br\n"); break; }
							if (n - i >= 4) { MCC_TRACE("br\n");
								init_putz(p, c + i * size1, (n - i) * size1);
								break;
							}
							ch = 0;
						} else if (size1 == 1)
							{ MCC_TRACE("br\n"); ch = ((unsigned char *)initstr.data)[i]; }
						else if (size1 == 2)
							{ MCC_TRACE("br\n"); ch = ((unsigned short *)initstr.data)[i]; }
						else
							{ MCC_TRACE("br\n"); ch = ((unsigned int *)initstr.data)[i]; }
						vpushi(ch);
						init_putv(p, t1, c + i * size1);
					}
				}
			}
			if (tok == ',' && !no_oblock)
				{ MCC_TRACE("br\n"); next(); }
		} else if (no_oblock && !(t1->t & VT_ARRAY) && (tok == TOK_STR || tok == TOK_LSTR || tok == TOK_U16STR || tok == TOK_U32STR || tok == TOK_U8STR)) { MCC_TRACE("br\n");
			char buf[64];
			type_to_str(buf, sizeof(buf), t1, NULL);
			mcc_error("cannot initialize array of '%s' from a string literal "
								"of a different character type",
								buf);
		} else { MCC_TRACE("br\n");
		do_init_array:
			indexsym.c = 0;
			f = &indexsym;

		do_init_list:
			if (tok == '}' && !(flags & (DIF_HAVE_ELEM | DIF_SIZE_ONLY)) &&
					mcc_state->cversion < 202311)
				{ MCC_TRACE("br\n"); mcc_pedantic("empty initializer braces are a C23 feature"); }
			if (!(flags & (DIF_CLEAR | DIF_SIZE_ONLY))) { MCC_TRACE("br\n");
				init_putz(p, c, n * size1);
				flags |= DIF_CLEAR;
			}

			len = 0;
			decl_design_flex(p, s, len - 1);
			{
				int sublist_comma = 0;
				while (tok != '}' || (flags & DIF_HAVE_ELEM)) { MCC_TRACE("br\n");
					if (no_oblock && sublist_comma && !(flags & DIF_HAVE_ELEM)) { MCC_TRACE("br\n");
						int climb = 0;
						if (tok == '[' && !(type->t & VT_ARRAY))
							{ MCC_TRACE("br\n"); climb = 1; }
						else if (tok == '.') { MCC_TRACE("br\n");
							if (type->t & VT_ARRAY) { MCC_TRACE("br\n");
								climb = 1;
							} else { MCC_TRACE("br\n");
								int cumofs;
								next();
								if (tok >= TOK_UIDENT && !find_field(type, tok | SYM_FIELD, &cumofs))
									{ MCC_TRACE("br\n"); climb = 1; }
								unget_tok('.');
							}
						}
						if (climb) { MCC_TRACE("br\n");
							unget_tok(',');
							break;
						}
					}
					len = decl_designator(p, type, c, &f, flags, len);
					flags &= ~DIF_HAVE_ELEM;
					if (type->t & VT_ARRAY) { MCC_TRACE("br\n");
						++indexsym.c;
						if (no_oblock && len >= n * size1)
							{ MCC_TRACE("br\n"); break; }
					} else { MCC_TRACE("br\n");
						if (s->type.t == VT_UNION)
							{ MCC_TRACE("br\n"); f = NULL; }
						else
							{ MCC_TRACE("br\n"); f = f->next; }
						if (no_oblock && f == NULL)
							{ MCC_TRACE("br\n"); break; }
					}

					if (tok == '}')
						{ MCC_TRACE("br\n"); break; }
					skip(',');
					sublist_comma = 1;
					seqp_flush();
				}
			}
		}
		if (!no_oblock)
			{ MCC_TRACE("br\n"); skip('}'); }
	} else if ((flags & DIF_HAVE_ELEM) && (is_compatible_unqualified_types(type, &vtop->type) || (is_complex_type(type) &&
																																																(is_complex_type(&vtop->type) || is_integer_btype(vtop->type.t & VT_BTYPE) || is_float(vtop->type.t))))) { MCC_TRACE("br\n");
		goto one_elem;
	} else if ((type->t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
		no_oblock = 1;
		if ((flags & DIF_FIRST) || tok == '{') { MCC_TRACE("br\n");
			skip('{');
			no_oblock = 0;
		}
		s = type->ref;
		f = s->next;
		n = s->c;
		size1 = 1;
		goto do_init_list;
	} else if (tok == '{') { MCC_TRACE("br\n");
		if (flags & DIF_HAVE_ELEM)
			{ MCC_TRACE("br\n"); skip(';'); }
		next();
		if (tok == '{') { MCC_TRACE("br\n");
			if (mcc_state->warn_pedantic || mcc_state->pedantic_errors)
				{ MCC_TRACE("br\n"); mcc_pedantic("too many braces around scalar initializer"); }
			else
				{ MCC_TRACE("br\n"); mcc_warning_c(warn_all)(
						"too many braces around scalar initializer"); }
		}
		decl_initializer(p, type, c, flags & ~DIF_HAVE_ELEM);
		skip('}');
	} else
	one_elem:
		if ((flags & DIF_SIZE_ONLY)) { MCC_TRACE("br\n");
			if (flags & DIF_HAVE_ELEM)
				{ MCC_TRACE("br\n"); vpop(); }
			else
				{ MCC_TRACE("br\n"); skip_or_save_block(NULL); }
		} else { MCC_TRACE("br\n");
			if (!(flags & DIF_HAVE_ELEM)) { MCC_TRACE("br\n");
				int aci_prev = assign_ctx_is_init;
				if (tok != TOK_STR && tok != TOK_LSTR && tok != TOK_U32STR && tok != TOK_U16STR && tok != TOK_U8STR)
					{ MCC_TRACE("br\n"); expect("string constant"); }
				assign_ctx_is_init = 0;
				parse_init_elem(!p->sec ? EXPR_ANY : EXPR_CONST);
				assign_ctx_is_init = aci_prev;
			}
			if (!p->sec && (flags & DIF_CLEAR) && (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST && vtop->c.i == 0 && btype_size(type->t & VT_BTYPE))
				{ MCC_TRACE("br\n"); vpop(); }
			else
				{ MCC_TRACE("br\n"); init_putv(p, type, c); }
		}
}

static TokenString *gather_string_run(CType *type, int has_init) {
#define MCC_STRSZ(k) ((k) == TOK_STR || (k) == TOK_U8STR ? 1 : (k) == TOK_U16STR ? 2 \
																													 : (k) == TOK_U32STR	 ? 4 \
																																								 : (int)sizeof(nwchar_t))
	TokenString *istr = tok_str_alloc();
	CString *parts = NULL;
	int *pkinds = NULL, nparts = 0, firstsz = 0, wsz = 0, wkind = TOK_STR;
	while (tok == TOK_STR || tok == TOK_LSTR || tok == TOK_U32STR || tok == TOK_U16STR || tok == TOK_U8STR) { MCC_TRACE("br\n");
		int sz = MCC_STRSZ(tok);
		tok_str_add_tok(istr);
		parts = mcc_realloc(parts, (nparts + 1) * sizeof(CString));
		pkinds = mcc_realloc(pkinds, (nparts + 1) * sizeof(int));
		cstr_new(&parts[nparts]);
		cstr_cat(&parts[nparts], tokc.str.data, tokc.str.size);
		pkinds[nparts] = tok;
		if (!firstsz)
			{ MCC_TRACE("br\n"); firstsz = sz; }
		if (sz > wsz) { MCC_TRACE("br\n");
			wsz = sz;
			wkind = tok;
		}
		nparts++;
		next();
	}
	tok_str_add(istr, TOK_EOF);
	if (firstsz && wsz > firstsz) { MCC_TRACE("br\n");
		int wet = wkind == TOK_U16STR
									? (VT_SHORT | VT_UNSIGNED)
							: wkind == TOK_U32STR
									? (VT_INT | VT_UNSIGNED)
#ifdef MCC_TARGET_PE
									: (VT_SHORT | VT_UNSIGNED);
#else
									: VT_INT;
#endif
		CString cc;
		int merge_kind = 0, j, i, save_tok = tok;
		CValue save_tokc = tokc;
		cstr_new(&cc);
		for (j = 0; j < nparts; j++) { MCC_TRACE("br\n");
			int tk = pkinds[j], toksz = MCC_STRSZ(tk);
			int nch = parts[j].size / toksz;
			merge_kind = merge_str_kind(merge_kind, tk);
			for (i = 0; i < nch - 1; i++) { MCC_TRACE("br\n");
				unsigned int ch =
						toksz == 1
								? ((unsigned char *)parts[j].data)[i]
						: toksz == 2
								? ((unsigned short *)parts[j].data)[i]
								: ((unsigned int *)parts[j].data)[i];
				if (wsz == 2) { MCC_TRACE("br\n");
					unsigned short v = ch;
					cstr_cat(&cc, (char *)&v, 2);
				} else { MCC_TRACE("br\n");
					unsigned int v = ch;
					cstr_cat(&cc, (char *)&v, 4);
				}
			}
		}
		{
			unsigned int z = 0;
			cstr_cat(&cc, (char *)&z, wsz);
		}
		tok_str_free(istr);
		istr = tok_str_alloc();
		tok = wkind;
		tokc.str.size = cc.size;
		tokc.str.data = cc.data;
		tok_str_add_tok(istr);
		tok = save_tok;
		tokc = save_tokc;
		tok_str_add(istr, TOK_EOF);
		cstr_free(&cc);
		if (has_init == 2 && (type->t & VT_ARRAY))
			{ MCC_TRACE("br\n"); type->ref->type.t = wet | (type->ref->type.t & VT_CONSTANT); }
	}
	for (int j = 0; j < nparts; j++)
		{ MCC_TRACE("br\n"); cstr_free(&parts[j]); }
	mcc_free(parts);
	mcc_free(pkinds);
	return istr;
#undef MCC_STRSZ
}

static void decl_initializer_alloc(CType *type, AttributeDef *ad, int r,
																	 int has_init, int v, int scope) {
	int size, align, addr;
	TokenString *init_str = NULL;

	Section *sec;
	Sym *flexible_array;
	Sym *sym = NULL;
	int saved_nocode_wanted = nocode_wanted;
#if MCC_CONFIG_DIAG_RT >= 2
	int bcheck = mcc_state->do_bounds_check && !NODATA_WANTED;
#endif
	int asan_g = mcc_state->do_asan_shadow && !NODATA_WANTED;
	init_params p = {0};

	if (scope == VT_CONST) { MCC_TRACE("br\n");
		sym = sym_find(v);
		if (sym) { MCC_TRACE("br\n");
			patch_storage(sym, ad, type);
			if (!has_init && sym->c && elfsym(sym)->st_shndx != SHN_UNDEF)
				{ MCC_TRACE("br\n"); return; }
			type = &sym->type;
		}
	}

	if (v && (r & VT_VALMASK) == VT_CONST)
		{ MCC_TRACE("br\n"); nocode_wanted |= DATA_ONLY_WANTED; }

	flexible_array = NULL;
	size = type_size(type, &align);

	if (size < 0) { MCC_TRACE("br\n");
		if (!(type->t & VT_ARRAY))
			{ MCC_TRACE("br\n"); mcc_error("initialization of incomplete type"); }
		if (IS_BT_ARRAY(type->t))
			{ MCC_TRACE("br\n"); type->ref = sym_push(SYM_FIELD, &type->ref->type, 0, type->ref->c); }
		p.flex_array_ref = type->ref;
	} else if (has_init && (type->t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
		Sym *field = type->ref->next;
		if (field) { MCC_TRACE("br\n");
			while (field->next)
				{ MCC_TRACE("br\n"); field = field->next; }
			if (field->type.t & VT_ARRAY && field->type.ref->c < 0) { MCC_TRACE("br\n");
				flexible_array = field;
				p.flex_array_ref = field->type.ref;
				p.flex_is_member = 1;
				size = -1;
			}
		}
	}

	if (size < 0) { MCC_TRACE("br\n");
		if (!has_init)
			{ MCC_TRACE("br\n"); goto err_size; }
		if (has_init == 2 || tok == TOK_STR || tok == TOK_LSTR || tok == TOK_U16STR || tok == TOK_U32STR || tok == TOK_U8STR) { MCC_TRACE("br\n");
			init_str = gather_string_run(type, has_init);
		} else
			{ MCC_TRACE("br\n"); skip_or_save_block(&init_str); }
		unget_tok(0);

		begin_macro(init_str, 1);
		next();
		decl_initializer(&p, type, 0, DIF_FIRST | DIF_SIZE_ONLY);
		macro_ptr = init_str->str;
		next();

		size = type_size(type, &align);
		if (size < 0)
		{ MCC_TRACE("br\n"); err_size:
			mcc_error("unknown type size"); }

		if (flexible_array && flexible_array->type.ref->c > 0)
			{ MCC_TRACE("br\n"); size += flexible_array->type.ref->c * pointed_size(&flexible_array->type); }
	}

	if (ad->a.aligned) { MCC_TRACE("br\n");
		int speca = 1 << (ad->a.aligned - 1);
		if (speca > align)
			{ MCC_TRACE("br\n"); align = speca; }
	} else if (ad->a.packed) { MCC_TRACE("br\n");
		align = 1;
	}

	if (!v && NODATA_WANTED)
		{ MCC_TRACE("br\n"); size = 0, align = 1; }

	if ((r & VT_VALMASK) == VT_LOCAL) { MCC_TRACE("br\n");
		int overalign_indirect = 0;
		sec = NULL;
#ifdef STACK_OVERALIGN_MAX
		overalign_indirect = v && align > STACK_OVERALIGN_MAX &&
												 !(type->t & VT_VLA) && size > 0 && !NODATA_WANTED &&
												 !asan_g
#if MCC_CONFIG_DIAG_RT >= 2
												 && !bcheck
#endif
				;
#endif
		if (overalign_indirect) { MCC_TRACE("br\n");
			int ptr_slot;
			int vla_new_save = 0;
#if MCC_CONFIG_OPTIMIZER
			ast_hook_vla_alloc_begin();
#endif
			if (cur_scope->vla.num == 0) { MCC_TRACE("br\n");
				if (cur_scope->prev && cur_scope->prev->vla.num) { MCC_TRACE("br\n");
					cur_scope->vla.locorig = cur_scope->prev->vla.loc;
				} else { MCC_TRACE("br\n");
					gen_vla_sp_save(loc -= MCC_PTR_SIZE);
					cur_scope->vla.locorig = loc;
					vla_new_save = 1;
				}
			}
			ptr_slot = (loc -= MCC_PTR_SIZE);
			vpushs(size);
			gen_vla_alloc(type, align);
			gen_vla_sp_save(ptr_slot);
			cur_scope->vla.loc = ptr_slot;
			cur_scope->vla.num++;
#if MCC_CONFIG_OPTIMIZER
			ast_hook_vla_alloc_end(type, ptr_slot, vla_new_save, cur_scope->vla.locorig);
#endif
			addr = 0;
			p.llocal = ptr_slot;
			p.local_offset = size;
			sym = sym_push(v, type, (r & ~VT_VALMASK) | VT_LLOCAL, ptr_slot);
			if (ad->cleanup_func) { MCC_TRACE("br\n");
				Sym *cls = sym_push2(&all_cleanups,
														 SYM_FIELD | ++cur_scope->cl.n, 0, 0);
				cls->cleanup_sym = sym;
				cls->cleanup_func = ad->cleanup_func;
				cls->next = cur_scope->cl.s;
				cur_scope->cl.s = cls;
			}
			sym->a = ad->a;
			sym->vla_inner_id = file->line_num;
			if (has_init)
				{ MCC_TRACE("br\n"); sym->a.inited = 1; }
		} else { MCC_TRACE("br\n");
#if MCC_CONFIG_DIAG_RT >= 2
			if (bcheck && v) { MCC_TRACE("br\n");
				loc -= align;
			}
#endif
			if (asan_g && v) { MCC_TRACE("br\n");
				loc -= MCC_ASAN_REDZONE;
				if (align < 8)
					{ MCC_TRACE("br\n"); align = 8; }
			}
			loc = (loc - size) & -align;
			addr = loc;
			p.local_offset = addr + size;
#if MCC_CONFIG_DIAG_RT >= 2
			if (bcheck && v) { MCC_TRACE("br\n");
				loc -= align;
			}
#endif
			if (v) { MCC_TRACE("br\n");
#if MCC_CONFIG_ASM
				if (ad->asm_label) { MCC_TRACE("br\n");
					int reg = asm_parse_regvar(ad->asm_label);
					if (reg >= 0)
						{ MCC_TRACE("br\n"); r = (r & ~VT_VALMASK) | reg; }
				}
#endif
				sym = sym_push(v, type, r, addr);
				if (ad->cleanup_func) { MCC_TRACE("br\n");
					Sym *cls = sym_push2(&all_cleanups,
															 SYM_FIELD | ++cur_scope->cl.n, 0, 0);
					cls->cleanup_sym = sym;
					cls->cleanup_func = ad->cleanup_func;
					cls->next = cur_scope->cl.s;
					cur_scope->cl.s = cls;
				}

				sym->a = ad->a;
				sym->vla_inner_id = file->line_num;
				if (has_init)
					{ MCC_TRACE("br\n"); sym->a.inited = 1; }
			} else { MCC_TRACE("br\n");
				vset(type, r, addr);
			}
		}
	} else { MCC_TRACE("br\n");
		sec = ad->section;
		if (!sec) { MCC_TRACE("br\n");
			CType *tp = type;
			while ((tp->t & (VT_BTYPE | VT_ARRAY)) == (VT_PTR | VT_ARRAY))
				{ MCC_TRACE("br\n"); tp = &tp->ref->type; }
			if (type->t & VT_TLS) { MCC_TRACE("br\n");
				if (has_init)
					{ MCC_TRACE("br\n"); sec = tdata_section; }
				else
					{ MCC_TRACE("br\n"); sec = tbss_section; }
			} else if (tp->t & VT_CONSTANT) { MCC_TRACE("br\n");
				sec = rodata_section;
			} else if (has_init) { MCC_TRACE("br\n");
				sec = data_section;
			} else if (mcc_state->nocommon)
				{ MCC_TRACE("br\n"); sec = bss_section; }
			else if (asan_g && v && size && !(type->t & VT_TLS))
				{ MCC_TRACE("br\n"); sec = bss_section; }
		}

		if (asan_g && v && size && sec && !(type->t & VT_TLS) && align < 8)
			{ MCC_TRACE("br\n"); align = 8; }

		if (sec) { MCC_TRACE("br\n");
			addr = section_add(sec, size, align);
#if MCC_CONFIG_DIAG_RT >= 2
			if (bcheck)
				{ MCC_TRACE("br\n"); section_add(sec, 1, 1); }
#endif
			if (asan_g && v && size && !(type->t & VT_TLS))
				{ MCC_TRACE("br\n"); section_add(sec, ((size + 7) & ~7) - size + MCC_ASAN_REDZONE, 1); }
		} else { MCC_TRACE("br\n");
			addr = align;
			sec = common_section;
		}

		if (v) { MCC_TRACE("br\n");
			if (!sym) { MCC_TRACE("br\n");
				sym = sym_push(v, type, r | VT_SYM, 0);
				patch_storage(sym, ad, NULL);
			}
			put_extern_sym(sym, sec, addr, size);
		} else { MCC_TRACE("br\n");
			vpush_ref(type, sec, addr, size);
			sym = vtop->sym;
			vtop->r |= r;
		}

#if MCC_CONFIG_DIAG_RT >= 2
		if (bcheck) { MCC_TRACE("br\n");
			addr_t *bounds_ptr;

			greloca(bounds_section, sym, bounds_section->data_offset, R_DATA_PTR, 0);
			bounds_ptr = section_ptr_add(bounds_section, 2 * sizeof(addr_t));
			bounds_ptr[0] = 0;
			bounds_ptr[1] = size;
		}
#endif
		if (asan_g && v && size && sym && !(type->t & VT_TLS)) { MCC_TRACE("br\n");
			Section *gsec = find_section(mcc_state, "__asan_globals");
			addr_t *gptr;
			gsec->sh_flags |= SHF_WRITE;
			greloca(gsec, sym, gsec->data_offset, R_DATA_PTR, 0);
			gptr = section_ptr_add(gsec, 2 * sizeof(addr_t));
			gptr[0] = 0;
			gptr[1] = size;
		}
	}

	if (!(type->t & VT_VLA) && type_is_vm(type)) { MCC_TRACE("br\n");
		if (nb_vla_open < VLA_TRACK_MAX)
			{ MCC_TRACE("br\n"); vla_open_birth[nb_vla_open++] = ++vla_seq; }
		else
			{ MCC_TRACE("br\n"); vla_track_ovf = 1; }
		cur_scope->vla_diag++;
	}

	if (type->t & VT_VLA) { MCC_TRACE("br\n");
		int a;

		if (has_init)
			{ MCC_TRACE("br\n"); mcc_error("variable length array cannot be initialized"); }

		if (nb_vla_open < VLA_TRACK_MAX)
			{ MCC_TRACE("br\n"); vla_open_birth[nb_vla_open++] = ++vla_seq; }
		else
			{ MCC_TRACE("br\n"); vla_track_ovf = 1; }
		cur_scope->vla_diag++;

		if (NODATA_WANTED)
			{ MCC_TRACE("br\n"); goto no_alloc; }

		int vla_new_save = 0;
#if MCC_CONFIG_OPTIMIZER
		ast_hook_vla_alloc_begin();
#endif
		if (cur_scope->vla.num == 0) { MCC_TRACE("br\n");
			if (cur_scope->prev && cur_scope->prev->vla.num) { MCC_TRACE("br\n");
				cur_scope->vla.locorig = cur_scope->prev->vla.loc;
			} else { MCC_TRACE("br\n");
				gen_vla_sp_save(loc -= MCC_PTR_SIZE);
				cur_scope->vla.locorig = loc;
				vla_new_save = 1;
			}
		}

		vpush_type_size(type, &a);
		gen_vla_alloc(type, a);
#if defined MCC_TARGET_PE && defined MCC_TARGET_X86_64
		gen_vla_result(addr), addr = (loc -= MCC_PTR_SIZE);
#endif
		gen_vla_sp_save(addr);
		cur_scope->vla.loc = addr;
		cur_scope->vla.num++;
#if MCC_CONFIG_OPTIMIZER
		ast_hook_vla_alloc_end(type, addr, vla_new_save, cur_scope->vla.locorig);
#endif
	} else if (has_init) { MCC_TRACE("br\n");
		p.sec = sec;
		if (!init_str && size >= 0 && (type->t & VT_ARRAY) && (tok == TOK_STR || tok == TOK_LSTR || tok == TOK_U16STR || tok == TOK_U32STR || tok == TOK_U8STR)) { MCC_TRACE("br\n");
			int el_al, el_sz = type_size(pointed_type(type), &el_al);
			int firstsz = tok == TOK_STR || tok == TOK_U8STR
												? 1
										: tok == TOK_U16STR
												? 2
										: tok == TOK_U32STR
												? 4
												: (int)sizeof(nwchar_t);
			if (el_sz > firstsz) { MCC_TRACE("br\n");
				init_str = gather_string_run(type, has_init);
				unget_tok(0);
				begin_macro(init_str, 1);
				next();
			}
		}
		seqp_reset();
#if MCC_CONFIG_OPTIMIZER
		unsigned long zbss_rel0 = data_section->reloc ? data_section->reloc->data_offset : 0;
#endif
		decl_initializer(&p, type, addr, DIF_FIRST);
		seqp_check();
		if (flexible_array)
			{ MCC_TRACE("br\n"); flexible_array->type.ref->c = -1; }
#if MCC_CONFIG_OPTIMIZER
		/* M5 const-data visibility: record this initialized static/global object AFTER
		 * its bytes are written, so the side-car can also estimate datacomp
		 * compressibility (M6 candidate ID). Read-only; changes no emitted bytes. */
		if (sec && size > 0)
			{ MCC_TRACE("br\n"); ast_hook_data(sec, addr, size, sec == rodata_section); }
		/* M6z: an all-zero writable static is semantically identical to an uninitialized one
		 * (C11 6.7.9), so relocate it from .data (zero disk bytes) to .bss (NOBITS). Guarded
		 * to a provably-safe subset — named object, last allocation in data_section, its
		 * initializer emitted no relocation (excludes pointer inits whose zero bytes carry a
		 * reloc), all bytes zero. Relocations are symbol-keyed, so re-binding the symbol fixes
		 * every reference. Opt-in via MCC_ZERO_BSS. */
		if (ast_zero_bss_env && v && sym && size > 0 && sec == data_section &&
#if MCC_CONFIG_DIAG_RT >= 2
				!bcheck &&
#endif
				!asan_g && !flexible_array && !(type->t & VT_TLS) &&
				(unsigned long)(addr + size) == data_section->data_offset &&
				(data_section->reloc ? data_section->reloc->data_offset : 0) == zbss_rel0 &&
				ast_data_all_zero(data_section, addr, size)) { MCC_TRACE("br\n");
			int new_addr;
			data_section->data_offset = addr;
			new_addr = (int)section_add(bss_section, size, align);
			put_extern_sym(sym, bss_section, new_addr, size);
			MCC_TRACE("zero-bss move v=%d size=%d data@%d -> bss@%d\n", v, size, addr, new_addr);
		}
		/* -fmerge-constants: an anonymous rodata string literal (v==0, put in rodata via
		 * str_init) whose exact bytes already appeared this TU can share the prior copy.
		 * C11 6.4.5p7 leaves identical literals' distinctness unspecified, so this is sound.
		 * Same symbol-rebind mechanism as M6z: re-home the literal's anon symbol at the shared
		 * offset (references are symbol-keyed) and roll back the just-written duplicate bytes.
		 * Guarded to the last allocation so the truncation is safe. Opt-in via MCC_MERGE_STRINGS. */
		if (ast_merge_strings_env && v == 0 && sym && size > 0 && sec == rodata_section &&
				(unsigned long)(addr + size) == rodata_section->data_offset) { MCC_TRACE("br\n");
			long shared = ast_strpool_find_or_add(sec, addr, size, align);
			if (shared >= 0 && shared != addr) { MCC_TRACE("br\n");
				/* Zero the reclaimed slot: the duplicate's bytes are still in the buffer, and
				 * the next allocation's initializer may rely on pre-zeroed tail padding (string
				 * literals memcpy only their content, expecting the trailing NUL already zero). */
				memset(rodata_section->data + addr, 0, (size_t)size);
				rodata_section->data_offset = addr;
				put_extern_sym(sym, rodata_section, shared, size);
				MCC_TRACE("string merge size=%d rodata@%d -> shared@%ld\n", size, addr, shared);
			}
		}
#endif
	}

no_alloc:
	if (init_str) { MCC_TRACE("br\n");
		end_macro();
		next();
	}

	nocode_wanted = saved_nocode_wanted;
}

static void func_vla_arg_code(Sym *arg) {
	int align;
	TokenString *vla_array_tok = NULL;

	if (arg->type.ref)
		{ MCC_TRACE("br\n"); func_vla_arg_code(arg->type.ref); }

	if ((arg->type.t & VT_VLA) && arg->type.ref->vla_array_str) { MCC_TRACE("br\n");
		loc -= type_size(&int_type, &align);
		loc &= -align;
		arg->type.ref->c = loc;

		unget_tok(0);
		vla_array_tok = tok_str_alloc();
		vla_array_tok->str = arg->type.ref->vla_array_str;
		begin_macro(vla_array_tok, 1);
		next();
		gexpr();
		end_macro();
		next();
		vpush_type_size(&arg->type.ref->type, &align);
		gen_op('*');
		vset(&int_type, VT_LOCAL | VT_LVAL, arg->type.ref->c);
		vswap();
		vstore();
		vpop();
	}
}

static void vla_arg_eval_discard(int *vla_str) {
	TokenString *vla_array_tok = tok_str_alloc();

	unget_tok(0);
	vla_array_tok->str = vla_str;
	begin_macro(vla_array_tok, 1);
	next();
	gexpr();
	end_macro();
	next();
	vpop();
}

static void func_vla_arg(Sym *sym) {
	Sym *arg;

	for (arg = sym->type.ref->next; arg; arg = arg->next) { MCC_TRACE("br\n");
		if ((arg->type.t & VT_BTYPE) != VT_PTR || !arg->type.ref)
			{ MCC_TRACE("br\n"); continue; }
		if (arg->type.ref->type.t & VT_VLA)
			{ MCC_TRACE("br\n"); func_vla_arg_code(arg->type.ref); }
		if (arg->type.ref->vla_array_str)
			{ MCC_TRACE("br\n"); vla_arg_eval_discard(arg->type.ref->vla_array_str); }
	}
}

ST_FUNC Sym *gfunc_set_param(Sym *s, int c, int byref) {
	s = sym_find(s->v);
	if (!s)
		{ MCC_TRACE("br\n"); return NULL; }
	s->c = c;
	if (byref)
		{ MCC_TRACE("br\n"); s->r = VT_LLOCAL | VT_LVAL; }
	return s;
}

static void sym_push_params(Sym *ref) {
	Sym *s = ref;
	while (s->next)
		{ MCC_TRACE("br\n"); s = s->next; }
	while (s != ref) { MCC_TRACE("br\n");
		if ((s->v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
			{ MCC_TRACE("br\n"); sym_copy(s, &local_stack); }
		s = s->prev;
	}
}

static void gen_function(Sym *sym) {
	MCC_TRACE("%s\n", get_tok_str(sym->v, NULL));
	struct scope f = {0};

	total_funcs++;
	cur_scope = root_scope = &f;
	nocode_wanted = 0;

	ind = cur_text_section->data_offset;
	if (sym->a.aligned) { MCC_TRACE("br\n");
		size_t newoff = section_add(cur_text_section, 0,
																1 << (sym->a.aligned - 1));
		gen_fill_nops(newoff - ind);
	}

	funcname = get_tok_str(sym->v, NULL);
	func_ind = ind;
	func_vt = sym->type.ref->type;
	func_var = sym->type.ref->f.func_type == FUNC_ELLIPSIS;
	cur_func_last_param = 0;
	if (func_var) { MCC_TRACE("br\n");
		Sym *pa;
		for (pa = sym->type.ref->next; pa; pa = pa->next)
			{ MCC_TRACE("br\n"); cur_func_last_param = pa->v & ~SYM_FIELD; }
	}
	func_old = sym->type.ref->f.func_type == FUNC_OLD;
	cur_func_noreturn = sym->type.ref->f.func_noreturn;
	cur_func_inline_extern =
			(sym->type.t & VT_INLINE) && !(sym->type.t & VT_STATIC);
	vla_seq = 0;
	nb_vla_open = 0;
	vla_track_ovf = 0;

	put_extern_sym(sym, cur_text_section, ind, 0);

	if (sym->type.ref->f.func_ctor)
		{ MCC_TRACE("br\n"); add_array(mcc_state, ".init_array", sym->c); }
	if (sym->type.ref->f.func_dtor)
		{ MCC_TRACE("br\n"); add_array(mcc_state, ".fini_array", sym->c); }

	mcc_debug_funcstart(mcc_state, sym);

	sym_push2(&local_stack, SYM_FIELD, 0, 0);
	local_scope = 1;
	sym_push_params(sym->type.ref);

	sym->vla_inner_id = file->line_num;

	local_scope = 0;
	rsym = 0;
	nb_temp_local_vars = 0;

	gfunc_prolog(sym);
	mcc_debug_prolog_epilog(mcc_state, 0);
	func_vla_arg(sym);
#if MCC_CONFIG_OPTIMIZER
	ast_func_begin(sym);
	block(0);
	ast_func_end(sym);
#else
	block(0);
#endif
	if ((mcc_state->warn_unused_parameter & WARN_ON) && sym->type.ref->f.func_type != FUNC_OLD) { MCC_TRACE("br\n");
		Sym *pc;
		for (pc = local_stack; pc; pc = pc->prev) { MCC_TRACE("br\n");
			if ((pc->r & VT_VALMASK) == VT_LOCAL && !pc->a.used && pc->v >= TOK_IDENT && pc->v < SYM_FIRST_ANOM && !(pc->type.t & VT_TYPEDEF))
				{ MCC_TRACE("br\n"); mcc_warning_c(warn_unused_parameter)(
						"%i:unused parameter '%s'",
						pc->vla_inner_id, get_tok_str(pc->v, NULL)); }
		}
	}
	cur_func_inline_extern = 0;
	gsym(rsym);
#if MCC_CONFIG_OPTIMIZER
	ast_func_epilog();
#endif
	nocode_wanted = 0;
	mcc_debug_end_scope(NULL, !func_var);
	mcc_debug_prolog_epilog(mcc_state, 1);
	gfunc_epilog();

	mcc_debug_funcend(mcc_state, ind - func_ind);

	elfsym(sym)->st_size = ind - func_ind;
	cur_text_section->data_offset = ind;

	sym_pop(&local_stack, NULL, 0);
	label_pop(&global_label_stack, NULL, 0);
	sym_pop(&all_cleanups, NULL, 0);
	local_scope = 0;

	cur_text_section = NULL;
	funcname = "";
	func_vt.t = VT_VOID;
	func_var = 0;
	ind = 0;
	func_ind = -1;
	nocode_wanted = DATA_ONLY_WANTED;
	check_vstack();

	next();
}

static void gen_inline_functions(MCCState *s) {
	Sym *sym;
	int inline_generated;
	struct InlineFunc *fn;

	mcc_open_bf(s, ":inline:", 0);
	do { MCC_TRACE("br\n");
		inline_generated = 0;
		for (int i = 0; i < s->nb_inline_fns; ++i) { MCC_TRACE("br\n");
			fn = s->inline_fns[i];
			sym = fn->sym;
			if (!sym)
				{ MCC_TRACE("br\n"); continue; }
			int emit = !(sym->type.t & VT_INLINE) ||
								 ((sym->type.t & VT_STATIC) && sym->c);
			int diag_only = !emit && sym->c &&
											(sym->type.t & VT_INLINE) && !(sym->type.t & VT_STATIC);
			if (emit || diag_only) { MCC_TRACE("br\n");
				fn->sym = NULL;
				mccpp_putfile(fn->filename);
				begin_macro(fn->func_str, 1);
				next();
				cur_text_section = text_section;
				if (diag_only) { MCC_TRACE("br\n");
					Section *ts = cur_text_section;
					int save_off = ts->data_offset;
					int save_rel = ts->reloc ? ts->reloc->data_offset : 0;
					ElfSym saved = *elfsym(sym);
					gen_function(sym);
					ts->data_offset = save_off;
					if (ts->reloc)
						{ MCC_TRACE("br\n"); ts->reloc->data_offset = save_rel; }
					*elfsym(sym) = saved;
				} else { MCC_TRACE("br\n");
					gen_function(sym);
				}
				end_macro();

				inline_generated = 1;
			}
		}
	} while (inline_generated);
	mcc_close();
}

static void resolve_alias_fixups(MCCState *s) {
	for (int i = 0; i < s->nb_alias_fixups; i++) { MCC_TRACE("br\n");
		AliasFixup *af = s->alias_fixups[i];
		ElfSym *esym = elfsym(sym_find(af->target_v));
		if (!esym || esym->st_shndx == SHN_UNDEF)
			{ MCC_TRACE("br\n"); mcc_error("undefined alias target '%s'",
								get_tok_str(af->target_v, NULL)); }
		put_extern_sym2(sym_find(af->alias_v), esym->st_shndx,
										esym->st_value, esym->st_size, 1);
	}
	dynarray_reset(&s->alias_fixups, &s->nb_alias_fixups);
}

static void free_inline_functions(MCCState *s) {
	for (int i = 0; i < s->nb_inline_fns; ++i) { MCC_TRACE("br\n");
		struct InlineFunc *fn = s->inline_fns[i];
		if (fn->sym)
			{ MCC_TRACE("br\n"); tok_str_free(fn->func_str); }
	}
	dynarray_reset(&s->inline_fns, &s->nb_inline_fns);
}

static void do_Static_assert(void) {
	int c;
	const char *msg;

	if (mcc_state->cversion < 201112)
		{ MCC_TRACE("br\n"); mcc_pedantic("ISO C does not support '_Static_assert' before C11"); }
	next();
	skip('(');
	c = expr_const();
	msg = "_Static_assert fail";
	if (tok == ',') { MCC_TRACE("br\n");
		next();
		msg = parse_mult_str("string constant")->data;
	}
	skip(')');
	if (c == 0)
		{ MCC_TRACE("br\n"); mcc_error("%s", msg); }
	skip(';');
}

#ifdef MCC_TARGET_PE
static void pe_check_linkage(CType *type, AttributeDef *ad) {
	if (!ad->a.dllimport && !ad->a.dllexport)
		{ MCC_TRACE("br\n"); return; }
	if (type->t & VT_STATIC)
		{ MCC_TRACE("br\n"); mcc_error("cannot have dll linkage with static"); }
	if (type->t & VT_TYPEDEF) { MCC_TRACE("br\n");
		const char *m = ad->a.dllimport ? "im" : "ex";
		mcc_warning("'dll%sport' attribute ignored for typedef", m);
		ad->a.dllimport = 0;
		ad->a.dllexport = 0;
	} else if (ad->a.dllimport) { MCC_TRACE("br\n");
		if ((type->t & VT_BTYPE) == VT_FUNC)
			{ MCC_TRACE("br\n"); ad->a.dllimport = 0; }
		else
			{ MCC_TRACE("br\n"); type->t |= VT_EXTERN; }
	}
}
#endif

static int decl(int l) {
	int v, has_init, r, oldint;
	CType type, btype;
	Sym *sym, *sa;
	AttributeDef ad, adbase;
	ElfSym *esym;

	while (1) { MCC_TRACE("br\n");
#if MCC_CONFIG_LSP
		uint32_t cst_dm = CST_MARK();
#endif

		oldint = 0;
		if (parse_btype(&btype, &adbase, l == VT_LOCAL)) { MCC_TRACE("br\n");
			if (adbase.implicit_int)
				{ MCC_TRACE("br\n"); mcc_warning_c(warn_implicit_int)("type defaults to 'int' in declaration"); }
		} else { MCC_TRACE("br\n");
			if (l == VT_JMP)
				{ MCC_TRACE("br\n"); return 0; }
			if (tok == ';' && l != VT_CMP) { MCC_TRACE("br\n");
				if (l == VT_CONST)
					{ MCC_TRACE("br\n"); mcc_pedantic("ISO C does not allow an empty declaration"); }
				next();
				continue;
			}
			if (tok == TOK_STATIC_ASSERT) { MCC_TRACE("br\n");
				do_Static_assert();
				continue;
			}
			if (l != VT_CONST)
				{ MCC_TRACE("br\n"); break; }
			if (tok == TOK_ASM1 || tok == TOK_ASM2 || tok == TOK_ASM3) { MCC_TRACE("br\n");
#if MCC_CONFIG_ASM
				asm_global_instr();
				continue;
#else
				mcc_error("assembler not supported (built without MCC_CONFIG_ASM)");
#endif
			}
			if (tok >= TOK_UIDENT) { MCC_TRACE("br\n");
				btype.t = VT_INT;
				oldint = 1;
			} else { MCC_TRACE("br\n");
				if (tok != TOK_EOF)
					{ MCC_TRACE("br\n"); expect("declaration"); }
				break;
			}
		}

		if (tok == ';') { MCC_TRACE("br\n");
			if ((btype.t & VT_BTYPE) == VT_STRUCT && (btype.ref->v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
				{ MCC_TRACE("br\n"); ; }
			else if (IS_ENUM(btype.t))
				{ MCC_TRACE("br\n"); ; }
			else
				{ MCC_TRACE("br\n"); mcc_warning("useless type defines no instances"); }
			if (l == VT_JMP)
				{ MCC_TRACE("br\n"); return 1; }
			next();
			continue;
		}

		while (1) { MCC_TRACE("br\n");
			type = btype;
			ad = adbase;
			type_decl(&type, &ad, &v, l == VT_CMP ? TYPE_DIRECT | TYPE_PARAM : TYPE_DIRECT);
			if ((type.t & VT_BTYPE) == VT_FUNC) { MCC_TRACE("br\n");
				if (oldint)
					{ MCC_TRACE("br\n"); mcc_warning_c(warn_implicit_int)("return type defaults to 'int'"); }
				if ((type.t & VT_STATIC) && (l != VT_CONST))
					{ MCC_TRACE("br\n"); mcc_error("function without file scope cannot be static"); }
				sym = type.ref;
				if (sym->f.func_type == FUNC_OLD && l == VT_CONST) { MCC_TRACE("br\n");
					func_vt = type;
					++local_scope;
					decl(VT_CMP);
					--local_scope;
				}
				if ((type.t & (VT_EXTERN | VT_INLINE)) == (VT_EXTERN | VT_INLINE)) { MCC_TRACE("br\n");
					if (mcc_state->gnu89_inline || sym->f.func_alwinl)
						{ MCC_TRACE("br\n"); type.t = (type.t & ~VT_EXTERN) | VT_STATIC; }
					else
						{ MCC_TRACE("br\n"); type.t &= ~VT_INLINE; }
				} else if (mcc_state->gnu89_inline &&
									 (type.t & (VT_INLINE | VT_STATIC | VT_EXTERN)) == VT_INLINE) { MCC_TRACE("br\n");
					type.t &= ~VT_INLINE;
				}
			} else if (oldint) { MCC_TRACE("br\n");
				mcc_warning("type defaults to int");
			}

			if (in_for_init && (type.t & (VT_STATIC | VT_EXTERN | VT_TYPEDEF)))
				{ MCC_TRACE("br\n"); mcc_pedantic("ISO C forbids a 'static', 'extern' or 'typedef' "
										 "declaration in a 'for' loop initializer"); }

			if (l == VT_CONST && (ad.storage_class & 1))
				{ MCC_TRACE("br\n"); mcc_error("file-scope declaration of '%s' specifies 'auto'",
									get_tok_str(v, NULL)); }
			if (l == VT_CONST && (ad.storage_class & 2))
				{ MCC_TRACE("br\n"); mcc_error("file-scope declaration of '%s' specifies 'register'",
									get_tok_str(v, NULL)); }
			if ((ad.storage_class & 4) && !(type.t & VT_TYPEDEF) && ((type.t & VT_BTYPE) != VT_PTR || (type.t & VT_ARRAY)))
				{ MCC_TRACE("br\n"); mcc_error("'restrict' requires a pointer type"); }
			if ((ad.storage_class & 8) && ad.a.aligned) { MCC_TRACE("br\n");
				int nat;
				if (type.t & VT_TYPEDEF)
					{ MCC_TRACE("br\n"); mcc_error("'_Alignas' specified for a typedef"); }
				else if ((type.t & VT_BTYPE) == VT_FUNC)
					{ MCC_TRACE("br\n"); mcc_error("'_Alignas' specified for a function"); }
				else if (ad.storage_class & 2)
					{ MCC_TRACE("br\n"); mcc_error("'_Alignas' specified for a 'register' object"); }
				else if ((type_size(&type, &nat), nat > 0) && (1 << (ad.a.aligned - 1)) < nat)
					{ MCC_TRACE("br\n"); mcc_error("requested alignment is less than the minimum "
										"alignment of the type"); }
			}
			if ((type.t & VT_BTYPE) != VT_FUNC && (type.t & VT_INLINE))
				{ MCC_TRACE("br\n"); mcc_error("'inline' used outside of a function declaration"); }
			if ((type.t & VT_VLA) && (type.t & VT_EXTERN) && (type.t & VT_BTYPE) != VT_FUNC)
				{ MCC_TRACE("br\n"); mcc_error("object with variably modified type must have no linkage"); }
			if ((ad.storage_class & 128) && (type.t & VT_BTYPE) != VT_FUNC)
				{ MCC_TRACE("br\n"); mcc_pedantic("'_Noreturn' used outside of a function declaration"); }
			if (type.t & VT_TLS) { MCC_TRACE("br\n");
				if (type.t & VT_TYPEDEF)
					{ MCC_TRACE("br\n"); mcc_error("'_Thread_local' used with 'typedef'"); }
				else if ((type.t & VT_BTYPE) == VT_FUNC)
					{ MCC_TRACE("br\n"); mcc_error("'_Thread_local' applied to a function"); }
				else if (l != VT_CONST && !(type.t & (VT_STATIC | VT_EXTERN)))
					{ MCC_TRACE("br\n"); mcc_error("'_Thread_local' at block scope requires "
										"'static' or 'extern'"); }
			}

			if (l != VT_CONST && (type.t & VT_BTYPE) == VT_FUNC && (ad.storage_class & 3))
				{ MCC_TRACE("br\n"); mcc_error("invalid storage class for function '%s'",
									get_tok_str(v, NULL)); }

			if (gnu_ext && (tok == TOK_ASM1 || tok == TOK_ASM2 || tok == TOK_ASM3)) { MCC_TRACE("br\n");
				ad.asm_label = asm_label_instr();
				parse_attribute(&ad);
			}

#ifdef MCC_TARGET_PE
			pe_check_linkage(&type, &ad);
#endif
			if (tok == '{') { MCC_TRACE("br\n");
				if (l != VT_CONST)
					{ MCC_TRACE("br\n"); mcc_error("cannot use local functions"); }
				if (type.t & VT_TYPEDEF)
					{ MCC_TRACE("br\n"); mcc_error("function definition declared 'typedef'"); }
				if ((type.t & VT_BTYPE) != VT_FUNC)
					{ MCC_TRACE("br\n"); expect("function definition"); }
				if (type.ref->f.func_star_param)
					{ MCC_TRACE("br\n"); mcc_error("'[*]' not allowed in a function definition"); }

				if (ad.f.func_type == 0)
					{ MCC_TRACE("br\n"); mcc_error("function definition declared with a typedef'd function type"); }
				merge_funcattr(&type.ref->f, &ad.f);
				type.t &= ~VT_EXTERN;
				if ((type.t & VT_INLINE) && !mcc_state->freestanding &&
						v >= TOK_IDENT && !strcmp(get_tok_str(v, NULL), "main"))
					{ MCC_TRACE("br\n"); mcc_warning("'main' is not allowed to be declared inline"); }
				sym = external_sym(v, &type, 0, &ad);

				{
					CType *rt = &sym->type.ref->type;
					if ((rt->t & VT_BTYPE) != VT_VOID && ((rt->t & VT_BTYPE) == VT_STRUCT || IS_ENUM(rt->t)) && rt->ref->c < 0)
						{ MCC_TRACE("br\n"); mcc_error("return type is an incomplete type"); }
				}

				for (sa = sym->type.ref; (sa = sa->next) != NULL;) { MCC_TRACE("br\n");
					if (!(sa->v & ~SYM_FIELD))
						{ MCC_TRACE("br\n"); expect("identifier"); }
					if (((sa->type.t & VT_BTYPE) == VT_STRUCT || IS_ENUM(sa->type.t)) && sa->type.ref->c < 0)
						{ MCC_TRACE("br\n"); mcc_error("parameter '%s' has incomplete type",
											get_tok_str(sa->v & ~SYM_FIELD, NULL)); }
					if (sym->type.ref->f.func_type == FUNC_OLD) { MCC_TRACE("br\n");
						if (sa->type.t & VT_EXTERN) { MCC_TRACE("br\n");
							if (mcc_state->cversion >= 199901)
								{ MCC_TRACE("br\n"); mcc_error_noabort("type of '%s' defaults to 'int' (implicit int removed in C99)",
																	get_tok_str(sa->v & ~SYM_FIELD, NULL)); }
							else
								{ MCC_TRACE("br\n"); mcc_warning_c(warn_implicit_int)("type of '%s' defaults to 'int'",
																								 get_tok_str(sa->v & ~SYM_FIELD, NULL)); }
						}
						if (sa->type.t == VT_FLOAT)
							{ MCC_TRACE("br\n"); sa->type.t = VT_DOUBLE; }
					}
				}

				if (sym->type.t & VT_INLINE) { MCC_TRACE("br\n");
					struct InlineFunc *fn;
					fn = mcc_malloc(sizeof *fn + strlen(file->filename));
					strcpy(fn->filename, file->filename);
					fn->sym = sym;
					dynarray_add(&mcc_state->inline_fns,
											 &mcc_state->nb_inline_fns, fn);
					skip_or_save_block(&fn->func_str);
				} else { MCC_TRACE("br\n");
					cur_text_section = ad.section;
					if (!cur_text_section)
						{ MCC_TRACE("br\n"); cur_text_section = text_section; }
					else if (cur_text_section->sh_num > bss_section->sh_num)
						{ MCC_TRACE("br\n"); cur_text_section->sh_flags = text_section->sh_flags; }
					gen_function(sym);
				}
#if MCC_CONFIG_LSP
				CST_OPEN_AT(CST_FunctionDef, cst_dm);
				CST_CLOSE();
#endif
				break;
			} else { MCC_TRACE("br\n");
				has_init = 0;
				if ((type.t & VT_BTYPE) == VT_FUNC && type.ref->f.func_type == FUNC_OLD && type.ref->next != NULL)
					{ MCC_TRACE("br\n"); mcc_error("parameter names (without types) in function declaration"); }
				if (l == VT_CMP) { MCC_TRACE("br\n");
					for (sym = func_vt.ref->next; sym; sym = sym->next)
						{ MCC_TRACE("br\n"); if ((sym->v & ~SYM_FIELD) == v)
							{ MCC_TRACE("br\n"); goto found; } }
					mcc_error("declaration for parameter '%s' but no such parameter",
										get_tok_str(v, NULL));
				found:
					if (type.t & VT_STORAGE)
						{ MCC_TRACE("br\n"); mcc_error("storage class specified for '%s'",
											get_tok_str(v, NULL)); }
					if (!(sym->type.t & VT_EXTERN))
						{ MCC_TRACE("br\n"); mcc_error("redefinition of parameter '%s'",
											get_tok_str(v, NULL)); }
					convert_parameter_type(&type);
					sym->type = type;
				} else if (type.t & VT_TYPEDEF) { MCC_TRACE("br\n");
					sym = sym_find(v);
					if (sym && sym->sym_scope == local_scope) { MCC_TRACE("br\n");
						if (!is_compatible_types(&sym->type, &type) || !(sym->type.t & VT_TYPEDEF))
							{ MCC_TRACE("br\n"); mcc_error("incompatible redefinition of '%s'",
												get_tok_str(v, NULL)); }
						else if ((sym->type.t & VT_VLA) || (type.t & VT_VLA))
							{ MCC_TRACE("br\n"); mcc_error("redefinition of variably modified typedef '%s'",
												get_tok_str(v, NULL)); }
						else if (mcc_state->cversion < 201112)
							{ MCC_TRACE("br\n"); mcc_pedantic("redefinition of typedef is a C11 feature"); }
						sym->type = type;
					} else { MCC_TRACE("br\n");
						sym = sym_push(v, &type, 0, 0);
					}
					sym->a = ad.a;
					if ((type.t & VT_BTYPE) == VT_FUNC)
						{ MCC_TRACE("br\n"); merge_funcattr(&sym->type.ref->f, &ad.f); }
					if (debug_modes)
						{ MCC_TRACE("br\n"); mcc_debug_typedef(mcc_state, sym); }
				} else if ((type.t & VT_BTYPE) == VT_VOID && !(type.t & VT_EXTERN)) { MCC_TRACE("br\n");
					mcc_error("declaration of void object");
				} else { MCC_TRACE("br\n");
					{
						Sym *prev = sym_find(v);
						if (prev && (prev->type.t & VT_TYPEDEF) && prev->sym_scope == local_scope)
							{ MCC_TRACE("br\n"); mcc_error("'%s' redeclared as different kind of symbol",
												get_tok_str(v, NULL)); }
						else if ((mcc_state->warn_shadow & WARN_ON) && local_scope > 0 && prev && prev->sym_scope != local_scope && v >= TOK_IDENT && v < SYM_FIRST_ANOM && (type.t & VT_BTYPE) != VT_FUNC && !(type.t & (VT_TYPEDEF | VT_EXTERN)) && (prev->type.t & VT_BTYPE) != VT_FUNC && !(prev->type.t & VT_TYPEDEF) && !IS_ENUM_VAL(prev->type.t)) { MCC_TRACE("br\n");
							if ((prev->r & VT_VALMASK) == VT_LOCAL)
								{ MCC_TRACE("br\n"); mcc_warning_c(warn_shadow)(
										"declaration of '%s' shadows a previous local",
										get_tok_str(v, NULL)); }
							else
								{ MCC_TRACE("br\n"); mcc_warning_c(warn_shadow)(
										"declaration of '%s' shadows a global declaration",
										get_tok_str(v, NULL)); }
						}
					}
					r = 0;
					if ((type.t & VT_BTYPE) == VT_FUNC) { MCC_TRACE("br\n");
						merge_funcattr(&type.ref->f, &ad.f);
					} else if (!(type.t & VT_ARRAY)) { MCC_TRACE("br\n");
						r |= VT_LVAL;
					}

					if (tok == '=')
						{ MCC_TRACE("br\n"); has_init = 1; }

					if (((type.t & VT_EXTERN) && (!has_init || l != VT_CONST)) || (type.t & VT_BTYPE) == VT_FUNC || ((type.t & VT_ARRAY) && !has_init && l == VT_CONST && type.ref->c < 0)) { MCC_TRACE("br\n");
						int was_tentative_flex =
								!(type.t & VT_EXTERN) && (type.t & VT_ARRAY) && !has_init && l == VT_CONST && type.ref->c < 0;
						type.t |= VT_EXTERN;
						sym = external_sym(v, &type, r, &ad);
						if (was_tentative_flex && sym)
							{ MCC_TRACE("br\n"); sym->a.tentative_array = 1; }
					} else { MCC_TRACE("br\n");
						if (cur_func_inline_extern && l != VT_CONST && (type.t & VT_STATIC) && !(type.t & VT_CONSTANT) && v)
							{ MCC_TRACE("br\n"); mcc_warning("'%s' is static but declared in inline "
													"function '%s' which is not static",
													get_tok_str(v, NULL), funcname); }
						if (l == VT_CONST || (type.t & VT_STATIC))
							{ MCC_TRACE("br\n"); r |= VT_CONST; }
						else
							{ MCC_TRACE("br\n"); r |= VT_LOCAL; }
						type.t &= ~VT_EXTERN;
						if (has_init)
							{ MCC_TRACE("br\n"); next(); }
						else if (l == VT_CONST) { MCC_TRACE("br\n");
							if (!(type.t & VT_STATIC)) { MCC_TRACE("br\n");
								Sym *pe = sym_find(v);
								if (pe && (pe->type.t & VT_STATIC))
									{ MCC_TRACE("br\n"); mcc_error("non-static declaration of '%s' "
														"follows static declaration",
														get_tok_str(v, NULL)); }
							}
							type.t |= VT_EXTERN;
						}
#if MCC_CONFIG_LSP
						uint32_t cst_im = has_init ? CST_MARK() : 0;
#endif
						{
							int aci_prev = assign_ctx_is_init;
							assign_ctx_is_init = has_init ? 1 : aci_prev;
							decl_initializer_alloc(&type, &ad, r, has_init, v, l);
							assign_ctx_is_init = aci_prev;
						}
#if MCC_CONFIG_LSP
						if (has_init) { MCC_TRACE("br\n");
							CST_OPEN_AT(CST_Initializer, cst_im);
							CST_CLOSE();
						}
#endif
					}

					if ((ad.storage_class & 2) && v) { MCC_TRACE("br\n");
						Sym *rs = sym_find(v);
						if (rs)
							{ MCC_TRACE("br\n"); rs->a.is_register = 1; }
					}

					if (ad.alias_target && l == VT_CONST) { MCC_TRACE("br\n");
						esym = elfsym(sym_find(ad.alias_target));
						if (esym && esym->st_shndx != SHN_UNDEF) { MCC_TRACE("br\n");
							put_extern_sym2(sym_find(v), esym->st_shndx,
															esym->st_value, esym->st_size, 1);
						} else { MCC_TRACE("br\n");
							AliasFixup *af = mcc_malloc(sizeof *af);
							af->alias_v = v;
							af->target_v = ad.alias_target;
							dynarray_add(&mcc_state->alias_fixups,
													 &mcc_state->nb_alias_fixups, af);
						}
					}
				}
				if (tok != ',') { MCC_TRACE("br\n");
					if (l == VT_JMP)
						{ MCC_TRACE("br\n"); return has_init ? v : 1; }
					skip(';');
#if MCC_CONFIG_LSP
					CST_OPEN_AT(CST_Declaration, cst_dm);
					CST_CLOSE();
#endif
					break;
				}
				next();
			}
		}
	}
	return 0;
}

#undef gjmp_addr
#undef gjmp

#if MCC_CONFIG_OPTIMIZER && defined(MCC_AMALGAMATED) && !MCC_AMALGAMATED
#include "mccast.c"
#endif
