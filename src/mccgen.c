#define USING_GLOBALS
#include "mcc.h"


ST_DATA int rsym, anon_sym, ind, loc;

ST_DATA Sym *global_stack;
ST_DATA Sym *local_stack;
ST_DATA Sym *define_stack;
ST_DATA Sym *global_label_stack;
ST_DATA Sym *local_label_stack;

static Sym *sym_free_first;
static void **sym_pools;
static int nb_sym_pools;

static Sym *all_cleanups, *pending_gotos;
static int local_scope;
ST_DATA char debug_modes;

ST_DATA SValue *vtop;
static SValue _vstack[1 + VSTACK_SIZE];
#define vstack (_vstack + 1)

ST_DATA int nocode_wanted;
#define NODATA_WANTED (nocode_wanted > 0)

ST_DATA int asm_lvalue_cast;
#define DATA_ONLY_WANTED 0x80000000

#define CODE_OFF_BIT 0x20000000
#define CODE_OFF() if(!nocode_wanted)(nocode_wanted |= CODE_OFF_BIT)
#define CODE_ON() (nocode_wanted &= ~CODE_OFF_BIT)

#define NOEVAL_MASK 0x0000FFFF
#define NOEVAL_WANTED (nocode_wanted & NOEVAL_MASK)

#define CONST_WANTED_BIT  0x00010000
#define CONST_WANTED_MASK 0x0FFF0000
#define CONST_WANTED  (nocode_wanted & CONST_WANTED_MASK)

ST_DATA int global_expr;
ST_DATA CType func_vt;
ST_DATA int func_var;
ST_DATA int func_vc;
ST_DATA int func_ind;
static int func_old;
static int cur_func_noreturn;
static int cur_func_last_param;
static int expr_was_assign;
static int expr_has_effect;
static int cur_func_inline_extern;
static int ice_float_op;
static int ice_nonconst;
ST_DATA const char *funcname;
ST_DATA CType int_type, func_old_type, char_type, char_pointer_type;
static CString initstr;

#if PTR_SIZE == 4
#define VT_SIZE_T (VT_INT | VT_UNSIGNED)
#define VT_PTRDIFF_T VT_INT
#elif LONG_SIZE == 4
#define VT_SIZE_T (VT_LLONG | VT_UNSIGNED)
#define VT_PTRDIFF_T VT_LLONG
#else
#define VT_SIZE_T (VT_LONG | VT_LLONG | VT_UNSIGNED)
#define VT_PTRDIFF_T (VT_LONG | VT_LLONG)
#endif

static struct switch_t {
    struct case_t {
        int64_t v1, v2;
        int ind, line;
    } **p; int n;
    int def_sym;
    int nocode_wanted;
    int *bsym;
    struct scope *scope;
    struct switch_t *prev;
    SValue sv;
    int vla_gpp;
} *cur_switch;

static int atomic_lowering;

static int in_for_init;

#define VLA_TRACK_MAX 512
static int vla_seq;
static int vla_open_birth[VLA_TRACK_MAX];
static int nb_vla_open;
static int vla_track_ovf;

#define MAX_TEMP_LOCAL_VARIABLE_NUMBER 8
static struct temp_local_variable {
	int location;
	short size;
	short align;
} arr_temp_local_vars[MAX_TEMP_LOCAL_VARIABLE_NUMBER];
static int nb_temp_local_vars;

static struct scope {
    struct scope *prev;
    struct { int loc, locorig, num; } vla;
    int vla_diag;
    struct { Sym *s; int n; } cl;
    int *bsym, *csym;
    Sym *lstk, *llstk;
    unsigned char stdc_fp_contract, stdc_fenv_access, stdc_cx_limited;
} *cur_scope, *loop_scope, *root_scope;

typedef struct {
    Section *sec;
    int local_offset;
    Sym *flex_array_ref;
    char flex_is_member;
    char flex_warned;
} init_params;

#if 1
#define precedence_parser
static void init_prec(void);
#endif

static void block(int flags);
#define STMT_EXPR 1
#define STMT_COMPOUND 2


enum { SEQP_READ, SEQP_WRITE };
#define SEQP_MAX 64
static struct { Sym *obj; unsigned long long off; unsigned char kind; }
    seqp_ev[SEQP_MAX];
static int nb_seqp;
static int seqp_overflow;

static void seqp_reset(void)
{
    nb_seqp = 0;
    seqp_overflow = 0;
}

static int seqp_key(SValue *sv, Sym **obj, unsigned long long *off)
{
    int r = sv->r;
    if (!(r & VT_LVAL) || !sv->sym || (sv->type.t & VT_BITFIELD))
        return 0;
    if ((r & VT_VALMASK) != VT_LOCAL && (r & VT_VALMASK) != VT_CONST)
        return 0;
    *obj = sv->sym;
    *off = (unsigned long long)sv->c.i;
    return 1;
}

static void seqp_record_sv(SValue *sv, int kind)
{
    Sym *obj;
    unsigned long long off;
    if (nocode_wanted)
        return;
    if (!seqp_key(sv, &obj, &off))
        return;
    if ((mcc_state->warn_uninitialized & WARN_ON)
        && (obj->r & VT_VALMASK) == VT_LOCAL
        && obj->v >= TOK_IDENT && obj->v < SYM_FIRST_ANOM) {
        if (kind == SEQP_WRITE)
            obj->a.inited = 1;
        else if (!obj->a.inited && !obj->a.addrtaken) {
            mcc_warning_c(warn_uninitialized)(
                "'%s' is used uninitialized", get_tok_str(obj->v, NULL));
            obj->a.inited = 1;
        }
    }
    if (!mcc_state->warn_sequence_point)
        return;
    if (nb_seqp >= SEQP_MAX) {
        seqp_overflow = 1;
        return;
    }
    seqp_ev[nb_seqp].obj = obj;
    seqp_ev[nb_seqp].off = off;
    seqp_ev[nb_seqp].kind = kind;
    nb_seqp++;
}

static void seqp_check(void)
{
    int i, j;
    if (seqp_overflow || !mcc_state->warn_sequence_point)
        return;
    for (i = 0; i < nb_seqp; i++) {
        Sym *o = seqp_ev[i].obj;
        unsigned long long ooff = seqp_ev[i].off;
        int writes = 0;
        if (!o || seqp_ev[i].kind != SEQP_WRITE)
            continue;
        for (j = i; j < nb_seqp; j++)
            if (seqp_ev[j].obj == o && seqp_ev[j].off == ooff
                && seqp_ev[j].kind == SEQP_WRITE)
                writes++;
        for (j = i + 1; j < nb_seqp; j++)
            if (seqp_ev[j].obj == o && seqp_ev[j].off == ooff)
                seqp_ev[j].obj = NULL;
        if (writes >= 2)
            mcc_warning_c(warn_sequence_point)(
                "operation on '%s' may be undefined", get_tok_str(o->v, NULL));
    }
}

static void seqp_flush(void)
{
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
static void free_inline_functions(MCCState *s);
static void finalize_tentative_arrays(void);
static void skip_or_save_block(TokenString **str);
static void gv_dup(void);
static int get_temp_local_var(int size,int align,int *r2);
static void cast_error(CType *st, CType *dt);
static void write_ldouble(unsigned char *d, void *s);
static void end_switch(void);
static void do_Static_assert(void);

static void mcc_pedantic(const char *msg)
{
    if (!mcc_state->warn_pedantic)
        return;
    if (mcc_state->error_set_jmp_enabled) {
        BufferedFile *wf;
        for (wf = file; wf && wf->filename[0] == ':'; wf = wf->prev)
            ;
        if (wf && wf->system_header)
            return;
    }
    if (mcc_state->pedantic_errors)
        mcc_error("%s", msg);
    else
        mcc_warning_c(warn_pedantic)("%s", msg);
}

static int struct_has_flexible_member(CType *type)
{
    Sym *f, *last = NULL;
    int align;
    if ((type->t & VT_BTYPE) != VT_STRUCT)
        return 0;
    for (f = type->ref->next; f; f = f->next)
        last = f;
    return last && (last->type.t & VT_ARRAY)
        && type_size(&last->type, &align) < 0;
}

/* Append one byte to the current text section, growing it as needed. Identical
   for every backend (operates only on shared globals), so it lives here rather
   than being copied into each per-arch backend file. */
ST_FUNC void g(int c)
{
    int ind1;
    if (nocode_wanted)
        return;
    ind1 = ind + 1;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    cur_text_section->data[ind] = c;
    ind = ind1;
}

/* Little-endian 16-bit emit — identical for every backend. (gen_le32 stays
   per-arch: fixed-width ISAs write the 4 bytes directly rather than via g().) */
ST_FUNC void gen_le16(int c)
{
    g(c);
    g(c >> 8);
}

/* Thread a new target onto a jump chain. The generic read32le/write32le walk is
   shared by every fixed-width backend; arm keeps its own (variable jump encoding
   via decbranch), so exclude it here. */
#ifndef MCC_TARGET_ARM
ST_FUNC int gjmp_append(int n, int t)
{
    void *p;
    if (n) {
        uint32_t n1 = n, n2;
        while ((n2 = read32le(p = cur_text_section->data + n1)))
            n1 = n2;
        write32le(p, t);
        t = n;
    }
    return t;
}
#endif

/* Emit opcode c followed by a 32-bit operand, returning the operand's offset.
   x86 family only (uses the variable-length o()); shared by i386 and x86_64. */
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
ST_FUNC int oad(int c, int s)
{
    int t;
    if (nocode_wanted)
        return s;
    o(c);
    t = ind;
    gen_le32(s);
    return t;
}
#endif

#ifdef CONFIG_MCC_BCHECK
/* Shared prologue for every backend's gen_bounds_epilog: reserve the trailing
   zero bounds slot and resolve the bounds-data symbol for this function's
   offset (a per-arch static, so it is passed in). Returns 0 when no epilog is
   needed — the caller returns too; otherwise fills *psym_data / *poffset_modified
   and returns 1. The arch tail then emits the actual local-new/local-delete calls. */
ST_FUNC int gen_bounds_epilog_head(addr_t func_bound_offset,
                                   Sym **psym_data, int *poffset_modified)
{
    addr_t *bounds_ptr;
    int offset_modified = func_bound_offset != lbounds_section->data_offset;

    *poffset_modified = offset_modified;
    if (!offset_modified && !func_bound_add_epilog)
        return 0;

    bounds_ptr = section_ptr_add(lbounds_section, sizeof(addr_t));
    *bounds_ptr = 0;

    *psym_data = get_sym_ref(&char_pointer_type, lbounds_section,
                             func_bound_offset, PTR_SIZE);
    return 1;
}
#endif

ST_FUNC void gsym(int t)
{
  if (t) {
    gsym_addr(t, ind);
    CODE_ON();
  }
}

static int gind()
{
  int t = ind;
  CODE_ON();
  if (debug_modes)
    mcc_tcov_block_begin(mcc_state);
  return t;
}

static void gjmp_addr_acs(int t)
{
  gjmp_addr(t);
  CODE_OFF();
}

static int gjmp_acs(int t)
{
  t = gjmp(t);
  CODE_OFF();
  return t;
}

#define gjmp_addr gjmp_addr_acs
#define gjmp gjmp_acs

ST_INLN int is_float(int t)
{
    int bt = t & VT_BTYPE;
    return bt == VT_LDOUBLE
        || bt == VT_DOUBLE
        || bt == VT_FLOAT
        || bt == VT_QFLOAT;
}

static inline int is_complex_type(CType *type)
{
    return (type->t & VT_BTYPE) == VT_STRUCT && type->ref->a.is_complex;
}

static inline int is_integer_btype(int bt)
{
    return bt == VT_BYTE
        || bt == VT_BOOL
        || bt == VT_SHORT
        || bt == VT_INT
        || bt == VT_LLONG;
}

static int type_is_vm(CType *type)
{
    CType *t = type;
    while ((t->t & VT_BTYPE) == VT_PTR) {
        if (t->t & VT_VLA)
            return 1;
        t = &t->ref->type;
    }
    return 0;
}

static int btype_size(int bt)
{
    return bt == VT_BYTE || bt == VT_BOOL ? 1 :
        bt == VT_SHORT ? 2 :
        bt == VT_INT ? 4 :
        bt == VT_LLONG ? 8 :
        bt == VT_PTR ? PTR_SIZE : 0;
}

static int R_RET(int t)
{
    if (!is_float(t))
        return REG_IRET;
#ifdef MCC_TARGET_X86_64
    if ((t & VT_BTYPE) == VT_LDOUBLE)
        return TREG_ST0;
#elif defined MCC_TARGET_RISCV64
    if ((t & VT_BTYPE) == VT_LDOUBLE)
        return REG_IRET;
#endif
    return REG_FRET;
}

static int R2_RET(int t)
{
    t &= VT_BTYPE;
    (void)t;
#if PTR_SIZE == 4
    if (t == VT_LLONG)
        return REG_IRE2;
#elif defined MCC_TARGET_X86_64
    if (t == VT_QLONG)
        return REG_IRE2;
    if (t == VT_QFLOAT)
        return REG_FRE2;
#elif defined MCC_TARGET_RISCV64
    if (t == VT_LDOUBLE)
        return REG_IRE2;
#endif
    return VT_CONST;
}

#define USING_TWO_WORDS(t) (R2_RET(t) != VT_CONST)

static void PUT_R_RET(SValue *sv, int t)
{
    sv->r = R_RET(t), sv->r2 = R2_RET(t);
}

enum { BB_FFS, BB_CLZ, BB_CTZ, BB_CLRSB, BB_POPCOUNT, BB_PARITY };
static int fold_bit_builtin(int op, int W, int64_t s)
{
    uint64_t u = (uint64_t)s;
    int i, n = 0;
    if (W < 64)
        u &= ((uint64_t)1 << W) - 1;
    switch (op) {
    case BB_CLZ:
        for (i = W - 1; i >= 0 && !((u >> i) & 1); i--) n++;
        return n;
    case BB_CTZ:
        if (u == 0) return W;
        for (i = 0; i < W && !((u >> i) & 1); i++) n++;
        return n;
    case BB_FFS:
        if (u == 0) return 0;
        for (i = 0; i < W && !((u >> i) & 1); i++) ;
        return i + 1;
    case BB_POPCOUNT:
        for (i = 0; i < W; i++) n += (u >> i) & 1;
        return n;
    case BB_PARITY:
        for (i = 0; i < W; i++) n += (u >> i) & 1;
        return n & 1;
    case BB_CLRSB: {
        int sign = (u >> (W - 1)) & 1;
        for (i = W - 2; i >= 0 && (int)((u >> i) & 1) == sign; i--) n++;
        return n;
    }
    }
    return 0;
}

static int RC_RET(int t)
{
    return reg_classes[R_RET(t)] & ~(RC_FLOAT | RC_INT);
}

static int RC_TYPE(int t)
{
    if (!is_float(t))
        return RC_INT;
#ifdef MCC_TARGET_X86_64
    if ((t & VT_BTYPE) == VT_LDOUBLE)
        return RC_ST0;
    if ((t & VT_BTYPE) == VT_QFLOAT)
        return RC_FRET;
#elif defined MCC_TARGET_RISCV64
    if ((t & VT_BTYPE) == VT_LDOUBLE)
        return RC_INT;
#endif
    return RC_FLOAT;
}

static int RC2_TYPE(int t, int rc)
{
    if (!USING_TWO_WORDS(t))
        return 0;
#ifdef RC_IRE2
    if (rc == RC_IRET)
        return RC_IRE2;
#endif
#ifdef RC_FRE2
    if (rc == RC_FRET)
        return RC_FRE2;
#endif
    if (rc & RC_FLOAT)
        return RC_FLOAT;
    return RC_INT;
}

ST_FUNC int ieee_finite(double d)
{
    int p[4];
    memcpy(p, &d, sizeof(double));
    return ((unsigned)((p[1] | 0x800fffff) + 1)) >> 31;
}

ST_FUNC void test_lvalue(void)
{
    if (!(vtop->r & VT_LVAL))
        expect("lvalue");
}

ST_FUNC void check_vstack(void)
{
    if (vtop != vstack - 1)
        mcc_error("internal compiler error: vstack leak (%d)",
                  (int)(vtop - vstack + 1));
}

#if 0
void pv (const char *lbl, int a, int b)
{
    int i;
    for (i = a; i < a + b; ++i) {
        SValue *p = &vtop[-i];
        printf("%s vtop[-%d] : type.t:%04x  r:%04x  r2:%04x  c.i:%d\n",
            lbl, i, p->type.t, p->r, p->r2, (int)p->c.i);
    }
}

static inline void psyms(const char *msg, Sym *s, Sym *last)
{
    printf("%-8s scope         v        c        r   type.t\n", msg);
    while (s && s != last) {
        printf("      %8x  %08x %08x %08x %08x %s\n",
            s->sym_scope, s->v, s->c, s->r, s->type.t, get_tok_str(s->v, 0));
        s = s->prev;
    }
}

static void type_to_str(char *buf, int buf_size, CType *type, const char *varstr);

static void ptype(const char *msg, CType *type, int v)
{
    char buf[500];
    type_to_str(buf, sizeof(buf), type,
        (v & ~SYM_FIELD) ? get_tok_str(v, NULL) : NULL);
    printf("%s : %s;\n", msg, buf);
}
#endif

ST_FUNC void mccgen_init(MCCState *s1)
{
    vtop = vstack - 1;
    memset(vtop, 0, sizeof *vtop);

    int_type.t = VT_INT;

    char_type.t = VT_BYTE;
    if (s1->char_is_unsigned)
        char_type.t |= VT_UNSIGNED;
    char_pointer_type = char_type;
    mk_pointer(&char_pointer_type);

    func_old_type.t = VT_FUNC;
    func_old_type.ref = sym_push(SYM_FIELD, &int_type, 0, 0);
    func_old_type.ref->f.func_call = FUNC_CDECL;
    func_old_type.ref->f.func_type = FUNC_OLD;
#ifdef precedence_parser
    init_prec();
#endif
    cstr_new(&initstr);
}

ST_FUNC int mccgen_compile(MCCState *s1)
{
    funcname = "";
    func_ind = -1;
    anon_sym = SYM_FIRST_ANOM;
    nocode_wanted = DATA_ONLY_WANTED;
    debug_modes = (s1->do_debug ? 1 : 0) | s1->test_coverage << 1;
    global_expr = 0;

    mcc_debug_start(s1);
    mcc_tcov_start (s1);
#ifdef MCC_TARGET_ARM
    arm_init(s1);
#endif
#ifdef INC_DEBUG
    printf("%s: **** new file\n", file->filename);
#endif
    parse_flags = PARSE_FLAG_PREPROCESS | PARSE_FLAG_TOK_NUM | PARSE_FLAG_TOK_STR;
    next();
    decl(VT_CONST);
    finalize_tentative_arrays();
    gen_inline_functions(s1);
    if (mcc_state->warn_unused_function & WARN_ON) {
        Sym *fs;
        for (fs = global_stack; fs; fs = fs->prev) {
            ElfSym *es;
            if ((fs->type.t & VT_BTYPE) != VT_FUNC
                || !(fs->type.t & VT_STATIC) || (fs->type.t & VT_INLINE)
                || fs->a.used
                || fs->v < TOK_IDENT || fs->v >= SYM_FIRST_ANOM)
                continue;
            es = elfsym(fs);
            if (!es || es->st_shndx == SHN_UNDEF)
                continue;
            mcc_warning_c(warn_unused_function)(
                "%i:'%s' defined but not used",
                fs->vla_inner_id, get_tok_str(fs->v, NULL));
        }
    }
    if (mcc_state->warn_all & WARN_ON) {
        Sym *fs;
        for (fs = global_stack; fs; fs = fs->prev) {
            ElfSym *es;
            if ((fs->type.t & VT_BTYPE) != VT_FUNC
                || !(fs->type.t & VT_STATIC) || (fs->type.t & VT_INLINE)
                || !fs->a.used
                || fs->v < TOK_IDENT || fs->v >= SYM_FIRST_ANOM)
                continue;
            es = elfsym(fs);
            if (es && es->st_shndx != SHN_UNDEF)
                continue;
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

static void finalize_tentative_arrays(void)
{
    Sym *sym;
    for (sym = global_stack; sym; sym = sym->prev) {
        ElfSym *esym;
        int size, align;
        if (!sym->a.tentative_array)
            continue;
        if (!(sym->type.t & VT_ARRAY) || sym->type.ref->c >= 0)
            continue;
        esym = elfsym(sym);
        if (esym && esym->st_shndx != SHN_UNDEF)
            continue;
        mcc_warning_c(warn_all)("array '%s' assumed to have one element",
                                get_tok_str(sym->v, NULL));
        sym->type.ref->c = 1;
        sym->type.t &= ~VT_EXTERN;
        size = type_size(&sym->type, &align);
        put_extern_sym(sym, common_section, align, size);
    }
}

ST_FUNC void mccgen_finish(MCCState *s1)
{
    mcc_debug_end(s1);
    free_inline_functions(s1);
    sym_pop(&global_stack, NULL, 0);
    sym_pop(&local_stack, NULL, 0);
    free_defines(NULL);
    dynarray_reset(&sym_pools, &nb_sym_pools);
    cstr_free(&initstr);
    dynarray_reset(&stk_data, &nb_stk_data);
    while (cur_switch)
        end_switch();
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

ST_FUNC ElfSym *elfsym(Sym *s)
{
  if (!s || !s->c)
    return NULL;
  return &((ElfSym *)symtab_section->data)[s->c];
}

ST_FUNC void update_storage(Sym *sym)
{
    ElfSym *esym;
    int sym_bind, old_sym_bind;

    esym = elfsym(sym);
    if (!esym)
        return;

    if (sym->a.visibility_set)
        esym->st_other = (esym->st_other & ~ELFW(ST_VISIBILITY)(-1))
            | sym->a.visibility;
    else if (mcc_state->visibility && esym->st_shndx != SHN_UNDEF)
        esym->st_other = (esym->st_other & ~ELFW(ST_VISIBILITY)(-1))
            | mcc_state->visibility;

    if (sym->type.t & (VT_STATIC | VT_INLINE))
        sym_bind = STB_LOCAL;
    else if (sym->a.weak)
        sym_bind = STB_WEAK;
    else
        sym_bind = STB_GLOBAL;
    old_sym_bind = ELFW(ST_BIND)(esym->st_info);
    if (sym_bind != old_sym_bind) {
        esym->st_info = ELFW(ST_INFO)(sym_bind, ELFW(ST_TYPE)(esym->st_info));
    }

#ifdef MCC_TARGET_PE
    if (sym->a.dllimport)
        esym->st_other |= ST_PE_IMPORT;
    if (sym->a.dllexport)
        esym->st_other |= ST_PE_EXPORT;
#endif

#if 0
    printf("storage %s: bind=%c vis=%d exp=%d imp=%d\n",
        get_tok_str(sym->v, NULL),
        sym_bind == STB_WEAK ? 'w' : sym_bind == STB_LOCAL ? 'l' : 'g',
        sym->a.visibility,
        sym->a.dllexport,
        sym->a.dllimport
        );
#endif
}


ST_FUNC void put_extern_sym2(Sym *sym, int sh_num,
                            addr_t value, unsigned long size,
                            int can_add_underscore)
{
    int sym_type, sym_bind, info, other, t;
    ElfSym *esym;
    const char *name;
    char buf1[256];

    if (!sym->c) {
        name = get_tok_str(sym->v, NULL);
        t = sym->type.t;
        if ((t & VT_BTYPE) == VT_FUNC) {
            sym_type = STT_FUNC;
        } else if ((t & VT_BTYPE) == VT_VOID) {
            sym_type = STT_NOTYPE;
            if (IS_ASM_FUNC(t))
                sym_type = STT_FUNC;
        } else if (t & VT_TLS) {
            sym_type = STT_TLS;
        } else {
            sym_type = STT_OBJECT;
        }
        if (t & (VT_STATIC | VT_INLINE))
            sym_bind = STB_LOCAL;
        else
            sym_bind = STB_GLOBAL;
        other = 0;

#ifdef MCC_TARGET_PE
        if (sym_type == STT_FUNC && sym->type.ref) {
            Sym *ref = sym->type.ref;
            if (ref->a.nodecorate) {
                can_add_underscore = 0;
            }
            if (ref->f.func_call == FUNC_STDCALL && can_add_underscore) {
                snprintf(buf1, sizeof(buf1), "_%s@%d", name, ref->f.func_args * PTR_SIZE);
                name = buf1;
                other |= ST_PE_STDCALL;
                can_add_underscore = 0;
            }
        }
#endif

        if (sym->asm_label) {
            name = get_tok_str(sym->asm_label, NULL);
            can_add_underscore = 0;
        }

        if (mcc_state->leading_underscore && can_add_underscore) {
            buf1[0] = '_';
            pstrcpy(buf1 + 1, sizeof(buf1) - 1, name);
            name = buf1;
        }

        info = ELFW(ST_INFO)(sym_bind, sym_type);
        sym->c = put_elf_sym(symtab_section, value, size, info, other, sh_num, name);

        if (debug_modes)
            mcc_debug_extern_sym(mcc_state, sym, sh_num, sym_bind, sym_type);

    } else {
        esym = elfsym(sym);
        esym->st_value = value;
        esym->st_size = size;
        esym->st_shndx = sh_num;
    }
    update_storage(sym);
}

ST_FUNC void put_extern_sym(Sym *sym, Section *s, addr_t value, unsigned long size)
{
    if (nocode_wanted && (NODATA_WANTED || (s && s == cur_text_section)))
        return;
    put_extern_sym2(sym, s ? s->sh_num : SHN_UNDEF, value, size, 1);
}

ST_FUNC void greloca(Section *s, Sym *sym, unsigned long offset, int type,
                     addr_t addend)
{
    int c = 0;

    if (nocode_wanted && s == cur_text_section)
        return;

    if (sym) {
        if (0 == sym->c) {
            put_extern_sym(sym, NULL, 0, 0);
            if (sym->sym_scope
                && (sym->type.t & (VT_STATIC|VT_EXTERN)) == (VT_STATIC|VT_EXTERN)) {
                Sym *s = sym;
                while (s->prev_tok)
                    s = s->prev_tok;
                s->c = sym->c;
            }
        }
        c = sym->c;
    }

    put_elf_reloca(symtab_section, s, offset, type, c, addend);
}

#if PTR_SIZE == 4
ST_FUNC void greloc(Section *s, Sym *sym, unsigned long offset, int type)
{
    greloca(s, sym, offset, type, 0);
}
#endif

static Sym *__sym_malloc(void)
{
    Sym *sym_pool, *sym, *last_sym;

    sym_pool = mcc_malloc(SYM_POOL_NB * sizeof(Sym));
    dynarray_add(&sym_pools, &nb_sym_pools, sym_pool);

    last_sym = sym_free_first;
    sym = sym_pool;
    for(int i = 0; i < SYM_POOL_NB; i++) {
        sym->next = last_sym;
        last_sym = sym;
        sym++;
    }
    sym_free_first = last_sym;
    return last_sym;
}

static inline Sym *sym_malloc(void)
{
    Sym *sym;
#ifndef SYM_DEBUG
    sym = sym_free_first;
    if (!sym)
        sym = __sym_malloc();
    sym_free_first = sym->next;
    return sym;
#else
    sym = mcc_malloc(sizeof(Sym));
    return sym;
#endif
}

ST_INLN void sym_free(Sym *sym)
{
#ifndef SYM_DEBUG
    sym->next = sym_free_first;
    sym_free_first = sym;
#else
    mcc_free(sym);
#endif
}

ST_FUNC Sym *sym_push2(Sym **ps, int v, int t, int c)
{
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

ST_FUNC Sym *sym_find2(Sym *s, int v)
{
    while (s) {
        if (s->v == v)
            return s;
        s = s->prev;
    }
    return NULL;
}

ST_INLN Sym *struct_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_struct;
}

ST_INLN Sym *sym_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_identifier;
}

static inline void sym_link(Sym *s, int yes)
{
    TokenSym *ts = table_ident[(s->v & ~SYM_STRUCT) - TOK_IDENT];
    Sym **ps;
    if (s->v & SYM_STRUCT)
        ps = &ts->sym_struct;
    else
        ps = &ts->sym_identifier;
    if (yes) {
        s->prev_tok = *ps, *ps = s;
        s->sym_scope = local_scope;
    } else {
        *ps = s->prev_tok;
    }
}

static inline int sym_scope_ex(Sym *s)
{
    return IS_ENUM_VAL (s->type.t)
        ? s->type.ref->sym_scope
        : s->sym_scope;
}

ST_FUNC Sym *sym_push(int v, CType *type, int r, int c)
{
    Sym *s, **ps;
    if (local_stack)
        ps = &local_stack;
    else
        ps = &global_stack;
    s = sym_push2(ps, v, type->t, c);
    s->type.ref = type->ref;
    s->r = r;
    if ((v & ~SYM_STRUCT) < SYM_FIRST_ANOM) {
        sym_link(s, 1);
        if (s->prev_tok && sym_scope_ex(s->prev_tok) == local_scope)
            mcc_error("redeclaration of '%s'", get_tok_str(s->v, NULL));
    }
    return s;
}

ST_FUNC Sym *global_identifier_push(int v, int t, int c)
{
    Sym *s, **ps;
    s = sym_push2(&global_stack, v, t, c);
    s->r = VT_CONST | VT_SYM;
    if (v < SYM_FIRST_ANOM) {
        ps = &table_ident[v - TOK_IDENT]->sym_identifier;
        while (*ps != NULL && (*ps)->sym_scope)
            ps = &(*ps)->prev_tok;
        s->prev_tok = *ps;
        *ps = s;
    }
    return s;
}

ST_FUNC void sym_pop(Sym **ptop, Sym *b, int keep)
{
    Sym *s, *ss;
    int v;

    s = *ptop;
    while(s != b) {
        ss = s->prev;
        v = s->v;
        if ((v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
            sym_link(s, 0);
	if (!keep)
	    sym_free(s);
        s = ss;
    }
    if (!keep)
	*ptop = b;
}

ST_FUNC Sym *label_find(int v)
{
    v -= TOK_IDENT;
    if ((unsigned)v >= (unsigned)(tok_ident - TOK_IDENT))
        return NULL;
    return table_ident[v]->sym_label;
}

ST_FUNC Sym *label_push(Sym **ptop, int v, int flags)
{
    Sym *s, **ps;
    s = sym_push2(ptop, v, VT_STATIC, 0);
    s->r = flags;
    s->vla_inner_id = 0;
    s->vla_min_goto_gpp = INT_MAX;
    ps = &table_ident[v - TOK_IDENT]->sym_label;
    if (ptop == &global_label_stack) {
        while (*ps != NULL)
            ps = &(*ps)->prev_tok;
    }
    s->prev_tok = *ps;
    *ps = s;
    return s;
}

ST_FUNC void label_pop(Sym **ptop, Sym *slast, int keep)
{
    Sym *s, *s1;
    for(s = *ptop; s != slast; s = s1) {
        s1 = s->prev;
        if (s->r == LABEL_DECLARED) {
            mcc_warning_c(warn_all)("label '%s' declared but not used", get_tok_str(s->v, NULL));
        } else if (s->r == LABEL_FORWARD) {
                mcc_error("label '%s' used but not defined",
                      get_tok_str(s->v, NULL));
        } else {
            if (s->c) {
                put_extern_sym(s, cur_text_section, s->jnext, 1);
            }
        }
        if (s->r != LABEL_GONE)
            table_ident[s->v - TOK_IDENT]->sym_label = s->prev_tok;
        if (!keep)
            sym_free(s);
        else
            s->r = LABEL_GONE;
    }
    if (!keep)
        *ptop = slast;
}

static void vcheck_cmp(void)
{



    if (vtop->r == VT_CMP && 0 == (nocode_wanted & ~CODE_OFF_BIT))
        gv(RC_INT);
}

static void vsetc(CType *type, int r, CValue *vc)
{
    if (vtop >= vstack + (VSTACK_SIZE - 1))
        mcc_error("memory full (vstack)");
    vcheck_cmp();
    vtop++;
    vtop->type = *type;
    vtop->r = r;
    vtop->r2 = VT_CONST;
    vtop->c = *vc;
    vtop->sym = NULL;
}

ST_FUNC void vswap(void)
{
    SValue tmp;

    vcheck_cmp();
    tmp = vtop[0];
    vtop[0] = vtop[-1];
    vtop[-1] = tmp;
}

ST_FUNC void vpop(void)
{
    int v;
    v = vtop->r & VT_VALMASK;
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
    if (v == TREG_ST0) {
        o(0xd8dd);
    } else
#endif
    if (v == VT_CMP) {
        gsym(vtop->jtrue);
        gsym(vtop->jfalse);
    }
    vtop--;
}

static void vpush(CType *type)
{
    vset(type, VT_CONST, 0);
}

static void vpush64(int ty, unsigned long long v)
{
    vsetc(&(CType){.t = ty, .ref = NULL}, VT_CONST, &(CValue){.i = v});
}

ST_FUNC void vpushi(int v)
{
    vpush64(VT_INT, v);
}

static void vpushs(addr_t v)
{
    vpush64(VT_SIZE_T, v);
}

static inline void vpushll(long long v)
{
    vpush64(VT_LLONG, v);
}

ST_FUNC void vset(CType *type, int r, int v)
{
    vsetc(type, r, &(CValue){.i = v});
}

static void vseti(int r, int v)
{
    vset(&(CType){.t = VT_INT, .ref = NULL}, r, v);
}

ST_FUNC void vpushv(SValue *v)
{
    if (vtop >= vstack + (VSTACK_SIZE - 1))
        mcc_error("memory full (vstack)");
    vtop++;
    *vtop = *v;
}

static void vdup(void)
{
    vpushv(vtop);
}

ST_FUNC void vrotb(int n)
{
    SValue tmp;
    if (--n < 1)
        return;
    vcheck_cmp();
    tmp = vtop[-n];
    memmove(vtop - n, vtop - n + 1, sizeof *vtop * n);
    vtop[0] = tmp;
}

ST_FUNC void vrott(int n)
{
    SValue tmp;
    if (--n < 1)
        return;
    vcheck_cmp();
    tmp = vtop[0];
    memmove(vtop - n + 1, vtop - n, sizeof *vtop * n);
    vtop[-n] = tmp;
}

ST_FUNC void vrev(int n)
{
    int i;
    SValue tmp;
    vcheck_cmp();
    for (i = 0, n = -n; i > ++n; --i)
        tmp = vtop[i], vtop[i] = vtop[n], vtop[n] = tmp;
}


ST_FUNC void vset_VT_CMP(int op)
{
    vtop->r = VT_CMP;
    vtop->cmp_op = op;
    vtop->jfalse = 0;
    vtop->jtrue = 0;
}

static void vset_VT_JMP(void)
{
    int op = vtop->cmp_op;

    if (vtop->jtrue || vtop->jfalse) {
        int origt = vtop->type.t;
        int inv = op & (op < 2);
        vseti(VT_JMP+inv, gvtst(inv, 0));
        vtop->type.t |= origt & (VT_UNSIGNED | VT_DEFSIGN);
    } else {
        vtop->c.i = op;
        if (op < 2)
            vtop->r = VT_CONST;
    }
}

static void gvtst_set(int inv, int t)
{
    int *p;

    if (vtop->r != VT_CMP) {
        vpushi(0);
        gen_op(TOK_NE);
        if (vtop->r != VT_CMP)
            vset_VT_CMP(vtop->c.i != 0);
    }

    p = inv ? &vtop->jfalse : &vtop->jtrue;
    *p = gjmp_append(*p, t);
}

static int gvtst(int inv, int t)
{
    int op, x, u;

    gvtst_set(inv, t);
    t = vtop->jtrue, u = vtop->jfalse;
    if (inv)
        x = u, u = t, t = x;
    op = vtop->cmp_op;

    if (op > 1)
        t = gjmp_cond(op ^ inv, t);
    else if (op != inv)
        t = gjmp(t);
    gsym(u);

    vtop--;
    return t;
}

static void gen_test_zero(int op)
{
    if (vtop->r == VT_CMP) {
        int j;
        if (op == TOK_EQ) {
            j = vtop->jfalse;
            vtop->jfalse = vtop->jtrue;
            vtop->jtrue = j;
            vtop->cmp_op ^= 1;
        }
    } else {
        vpushi(0);
        gen_op(op);
    }
}

ST_FUNC void vpushsym(CType *type, Sym *sym)
{
    CValue cval;
    cval.i = 0;
    vsetc(type, VT_CONST | VT_SYM, &cval);
    vtop->sym = sym;
}

ST_FUNC Sym *get_sym_ref(CType *type, Section *sec, unsigned long offset, unsigned long size)
{
    int v;
    Sym *sym;

    v = anon_sym++;
    sym = sym_push(v, type, VT_CONST | VT_SYM, 0);
    sym->type.t |= VT_STATIC;
    put_extern_sym(sym, sec, offset, size);
    return sym;
}

static void vpush_ref(CType *type, Section *sec, unsigned long offset, unsigned long size)
{
    vpushsym(type, get_sym_ref(type, sec, offset, size));
}

ST_FUNC Sym *external_global_sym(int v, CType *type)
{
    Sym *s;

    s = sym_find(v);
    if (!s) {
        s = global_identifier_push(v, type->t | VT_EXTERN, 0);
        s->type.ref = type->ref;
    } else if (IS_ASM_SYM(s)) {
        s->type.t = type->t | (s->type.t & VT_EXTERN);
        s->type.ref = type->ref;
        update_storage(s);
    }
    return s;
}

ST_FUNC Sym *external_helper_sym(int v)
{
    CType ct = { VT_ASM_FUNC, NULL };
    return external_global_sym(v, &ct);
}

ST_FUNC void vpush_helper_func(int v)
{
    vpushsym(&func_old_type, external_helper_sym(v));
}

static void merge_symattr(struct SymAttr *sa, struct SymAttr *sa1)
{
    if (sa1->aligned && !sa->aligned)
      sa->aligned = sa1->aligned;
    sa->packed |= sa1->packed;
    sa->weak |= sa1->weak;
    sa->nodebug |= sa1->nodebug;
    if (sa1->visibility != STV_DEFAULT) {
	int vis = sa->visibility;
	if (vis == STV_DEFAULT
	    || vis > sa1->visibility)
	  vis = sa1->visibility;
	sa->visibility = vis;
    }
    sa->visibility_set |= sa1->visibility_set;
    sa->dllexport |= sa1->dllexport;
    sa->nodecorate |= sa1->nodecorate;
    sa->dllimport |= sa1->dllimport;
}

static void merge_funcattr(struct FuncAttr *fa, struct FuncAttr *fa1)
{
    if (fa1->func_call && !fa->func_call)
      fa->func_call = fa1->func_call;
    if (fa1->func_type && !fa->func_type)
      fa->func_type = fa1->func_type;
    if (fa1->func_args && !fa->func_args)
      fa->func_args = fa1->func_args;
    if (fa1->func_noreturn)
      fa->func_noreturn = 1;
    if (fa1->func_ctor)
      fa->func_ctor = 1;
    if (fa1->func_dtor)
      fa->func_dtor = 1;
}

static void merge_attr(AttributeDef *ad, AttributeDef *ad1)
{
    merge_symattr(&ad->a, &ad1->a);
    merge_funcattr(&ad->f, &ad1->f);

    if (ad1->section)
      ad->section = ad1->section;
    if (ad1->alias_target)
      ad->alias_target = ad1->alias_target;
    if (ad1->asm_label)
      ad->asm_label = ad1->asm_label;
    if (ad1->attr_mode)
      ad->attr_mode = ad1->attr_mode;
}

static void patch_type(Sym *sym, CType *type)
{
    if (!(type->t & VT_EXTERN) || IS_ENUM_VAL(sym->type.t)) {
        if (!(sym->type.t & VT_EXTERN))
            mcc_error("redefinition of '%s'", get_tok_str(sym->v, NULL));
        sym->type.t &= ~VT_EXTERN;
    }

    if (IS_ASM_SYM(sym)) {
        sym->type.t = type->t & (sym->type.t | ~VT_STATIC);
        sym->type.ref = type->ref;
        if ((type->t & VT_BTYPE) != VT_FUNC && !(type->t & VT_ARRAY))
            sym->r |= VT_LVAL;
    }

    if (!is_compatible_types(&sym->type, type)) {
        mcc_error("incompatible types for redefinition of '%s'",
                  get_tok_str(sym->v, NULL));

    } else if ((sym->type.t & VT_BTYPE) == VT_FUNC) {
        int static_proto = sym->type.t & VT_STATIC;
        int ft1 = sym->type.ref->f.func_type;
        int ft2 = type->ref->f.func_type;

        if ((type->t & VT_STATIC) && !static_proto
            && !((type->t | sym->type.t) & VT_INLINE))
            mcc_warning("static storage ignored for redefinition of '%s'",
                get_tok_str(sym->v, NULL));

        if ((type->t | sym->type.t) & VT_INLINE) {
            if (!((type->t ^ sym->type.t) & VT_INLINE)
             || ((type->t | sym->type.t) & VT_STATIC))
                static_proto |= VT_INLINE;
        }

        if (0 == (type->t & VT_EXTERN)) {
            struct FuncAttr f = sym->type.ref->f;
            sym->type.t = (type->t & ~(VT_STATIC|VT_INLINE)) | static_proto;
            if (ft1 != FUNC_OLD)
                type->ref->f.func_type = ft1;
            sym->type.ref = type->ref;
            merge_funcattr(&sym->type.ref->f, &f);
        } else {
            sym->type.t &= ~VT_INLINE | static_proto;
            if (ft1 == FUNC_OLD && ft2 != FUNC_OLD)
                sym->type.ref = type->ref;
        }

    } else {
        if ((sym->type.t & VT_ARRAY) && type->ref->c >= 0) {
            sym->type.ref->c = type->ref->c;
        }
        if ((type->t ^ sym->type.t) & VT_STATIC) {
            if ((type->t & VT_STATIC) && !(sym->type.t & VT_STATIC))
                mcc_error("static declaration of '%s' follows non-static "
                    "declaration", get_tok_str(sym->v, NULL));
            else if (!(type->t & (VT_STATIC|VT_EXTERN))
                     && (sym->type.t & VT_STATIC))
                mcc_error("non-static declaration of '%s' follows static "
                    "declaration", get_tok_str(sym->v, NULL));
        }
    }
}

static void patch_storage(Sym *sym, AttributeDef *ad, CType *type)
{
    if (type)
        patch_type(sym, type);

#ifdef MCC_TARGET_PE
    if (sym->a.dllimport != ad->a.dllimport)
        mcc_error("incompatible dll linkage for redefinition of '%s'",
            get_tok_str(sym->v, NULL));
#endif
    merge_symattr(&sym->a, &ad->a);
    if (ad->asm_label)
        sym->asm_label = ad->asm_label;
    update_storage(sym);
}

static Sym *sym_copy(Sym *s0, Sym **ps)
{
    Sym *s;
    s = sym_malloc(), *s = *s0;
    s->prev = *ps, *ps = s;
    if ((s->v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
        sym_link(s, 1);
    return s;
}

static void move_ref_to_global(Sym *s)
{
    Sym *l, **lp;
    int n, bt;

    bt = s->type.t & VT_BTYPE;
    if (!(bt == VT_PTR
       || bt == VT_FUNC
       || bt == VT_STRUCT
       || IS_ENUM(s->type.t)))
        return;

    for (s = s->type.ref, n = 0; s; s = s->next) {
        for (lp = &local_stack; !!(l = *lp); lp = &l->prev) {
            if (l == s) {
                *lp = s->prev;
                s->prev = global_stack, global_stack = s;
                if (n || bt == VT_PTR || bt == VT_FUNC) {
                    move_ref_to_global(s);
                } else {
                    if ((s->v & ~SYM_STRUCT) < SYM_FIRST_ANOM) {
                        s->v |= SYM_FIELD;
                        l = sym_copy(s, lp);
                        l->v &= ~SYM_FIELD;
                    }
                }
                if (bt != VT_PTR)
                    n = 1;
                break;
            }
        }
        if (n == 0)
            break;
    }
}

static Sym *external_sym(int v, CType *type, int r, AttributeDef *ad)
{
    Sym *s;

    s = sym_find(v);
    while (s && s->sym_scope)
        s = s->prev_tok;

    if (!s) {
        s = global_identifier_push(v, type->t, 0);
        s->r |= r;
        s->a = ad->a;
        s->asm_label = ad->asm_label;
        s->type.ref = type->ref;
    } else {
        patch_storage(s, ad, type);
    }
    if (local_stack) {
        move_ref_to_global(s);
        sym_copy(s, &local_stack);
    }
    return s;
}

ST_FUNC void save_regs(int n)
{
    SValue *p, *p1;
    for(p = vstack, p1 = vtop - n; p <= p1; p++)
        save_reg(p->r);
}

ST_FUNC void save_reg(int r)
{
    save_reg_upstack(r, 0);
}

ST_FUNC void save_reg_upstack(int r, int n)
{
    int l, size, align, bt, r2;
    SValue *p, *p1, sv;

    if ((r &= VT_VALMASK) >= VT_CONST)
        return;
    if (nocode_wanted)
        return;
    l = r2 = 0;
    for(p = vstack, p1 = vtop - n; p <= p1; p++) {
        if ((p->r & VT_VALMASK) == r || p->r2 == r) {
            if (!l) {
                bt = p->type.t & VT_BTYPE;
                if (bt == VT_VOID)
                    continue;
                if ((p->r & VT_LVAL) || bt == VT_FUNC)
                    bt = VT_PTR;
                sv.type.t = bt;
                size = type_size(&sv.type, &align);
                l = get_temp_local_var(size, align, &r2);
                sv.r = VT_LOCAL | VT_LVAL;
                sv.c.i = l;
		sv.sym = NULL;
                store(p->r & VT_VALMASK, &sv);
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
                if (r == TREG_ST0) {
                    o(0xd8dd);
                }
#endif
                if (p->r2 < VT_CONST && USING_TWO_WORDS(bt)) {
                    sv.c.i += PTR_SIZE;
                    store(p->r2, &sv);
                }
            }
            if (p->r & VT_LVAL) {
                p->r = (p->r & ~(VT_VALMASK | VT_BOUNDED)) | VT_LLOCAL;
            } else {
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
ST_FUNC int get_reg_ex(int rc, int rc2)
{
    int r;
    SValue *p;

    for(r=0;r<NB_REGS;r++) {
        if (reg_classes[r] & rc2) {
            int n;
            n=0;
            for(p = vstack; p <= vtop; p++) {
                if ((p->r & VT_VALMASK) == r ||
                    p->r2 == r)
                    n++;
            }
            if (n <= 1)
                return r;
        }
    }
    return get_reg(rc);
}
#endif

ST_FUNC int get_reg(int rc)
{
    int r;
    SValue *p;

    for(r=0;r<NB_REGS;r++) {
        if (reg_classes[r] & rc) {
            if (nocode_wanted)
                return r;
            for(p=vstack;p<=vtop;p++) {
                if ((p->r & VT_VALMASK) == r ||
                    p->r2 == r)
                    goto notfound;
            }
            return r;
        }
    notfound: ;
    }

    for(p=vstack;p<=vtop;p++) {
        r = p->r2;
        if (r < VT_CONST && (reg_classes[r] & rc))
            goto save_found;
        r = p->r & VT_VALMASK;
        if (r < VT_CONST && (reg_classes[r] & rc)) {
        save_found:
            save_reg(r);
            return r;
        }
    }
    return -1;
}

static int get_temp_local_var(int size,int align, int *r2)
{
    int i;
    struct temp_local_variable *temp_var;
    SValue *p;
    int r;
    unsigned used = 0;

    for (p = vstack; p <= vtop; p++) {
	r = p->r & VT_VALMASK;
	if (r == VT_LOCAL || r == VT_LLOCAL) {
	    r = p->r2 - (VT_CONST + 1);
	    if (r >= 0 && r < MAX_TEMP_LOCAL_VARIABLE_NUMBER)
	        used |= 1<<r;
	}
    }
    for (i=0;i<nb_temp_local_vars;i++) {
	temp_var=&arr_temp_local_vars[i];
	if(!(used & 1<<i)
	 && temp_var->size>=size
	 && temp_var->align>=align) {
ret_tmp:
	    *r2 = (VT_CONST + 1) + i;
	    return temp_var->location;
	}
    }
    loc = (loc - size) & -align;
    if (nb_temp_local_vars<MAX_TEMP_LOCAL_VARIABLE_NUMBER) {
	temp_var=&arr_temp_local_vars[i];
	temp_var->location=loc;
	temp_var->size=size;
	temp_var->align=align;
	nb_temp_local_vars++;
	goto ret_tmp;
    }
    *r2 = VT_CONST;
    return loc;
}

static void move_reg(int r, int s, int t)
{
    SValue sv;

    if (r != s) {
        save_reg(r);
        sv.type.t = t;
        sv.type.ref = NULL;
        sv.r = s;
        sv.c.i = 0;
        load(r, &sv);
    }
}

ST_FUNC void gaddrof(void)
{
    vtop->r &= ~VT_LVAL;
    if ((vtop->r & VT_VALMASK) == VT_LLOCAL)
        vtop->r = (vtop->r & ~VT_VALMASK) | VT_LOCAL | VT_LVAL;
}

#ifdef CONFIG_MCC_BCHECK
static void gen_bounded_ptr_add(void)
{
    int save = (vtop[-1].r & VT_VALMASK) == VT_LOCAL;
    if (save) {
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
        return;
    vtop->c.i = (cur_text_section->reloc->data_offset - sizeof(ElfW_Rel));
}

static void gen_bounded_ptr_deref(void)
{
    addr_t func;
    int size, align;
    ElfW_Rel *rel;
    Sym *sym;

    if (nocode_wanted)
        return;

    size = type_size(&vtop->type, &align);
    switch(size) {
    case  1: func = TOK___bound_ptr_indir1; break;
    case  2: func = TOK___bound_ptr_indir2; break;
    case  4: func = TOK___bound_ptr_indir4; break;
    case  8: func = TOK___bound_ptr_indir8; break;
    case 12: func = TOK___bound_ptr_indir12; break;
    case 16: func = TOK___bound_ptr_indir16; break;
    default:
        return;
    }
    sym = external_helper_sym(func);
    if (!sym->c)
        put_extern_sym(sym, NULL, 0, 0);
    rel = (ElfW_Rel *)(cur_text_section->reloc->data + vtop->c.i);
    rel->r_info = ELFW(R_INFO)(sym->c, ELFW(R_TYPE)(rel->r_info));
}

static void gbound(void)
{
    CType type1;

    vtop->r &= ~VT_MUSTBOUND;
    if (vtop->r & VT_LVAL) {
        if (!(vtop->r & VT_BOUNDED)) {
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

ST_FUNC void gbound_args(int nb_args)
{
    int i, v;
    SValue *sv;

    for (i = 1; i <= nb_args; ++i)
        if (vtop[1 - i].r & VT_MUSTBOUND) {
            vrotb(i);
            gbound();
            vrott(i);
        }

    sv = vtop - nb_args;
    if (sv->r & VT_SYM) {
        v = sv->sym->v;
        if (v == TOK_setjmp
          || v == TOK__setjmp
#ifndef MCC_TARGET_PE
          || v == TOK_sigsetjmp
          || v == TOK___sigsetjmp
#endif
          ) {
            vpush_helper_func(TOK___bound_setjmp);
            vpushv(sv + 1);
            gfunc_call(1);
            func_bound_add_epilog = 1;
        }
        if (v == TOK_alloca)
            func_bound_add_epilog = 1;
#if TARGETOS_NetBSD
        if (v == TOK_longjmp)
            sv->sym->asm_label = TOK___bound_longjmp;
#endif
    }
}

static void add_local_bounds(Sym *s, Sym *e)
{
    for (; s != e; s = s->prev) {
        if (!s->v || (s->r & VT_VALMASK) != VT_LOCAL)
          continue;
        if ((s->type.t & VT_ARRAY)
            || (s->type.t & VT_BTYPE) == VT_STRUCT
            || s->a.addrtaken) {
            int align, size = type_size(&s->type, &align);
            addr_t *bounds_ptr = section_ptr_add(lbounds_section,
                                                 2 * sizeof(addr_t));
            bounds_ptr[0] = s->c;
            bounds_ptr[1] = size;
        }
    }
}
#endif

static void mcc_debug_end_scope(Sym *b, int bounds)
{
#ifdef CONFIG_MCC_BCHECK
    if (mcc_state->do_bounds_check && bounds)
        add_local_bounds(local_stack, b);
#endif
    mcc_add_debug_info (mcc_state, local_stack, b);
}

static void incr_offset(int offset)
{
    int t = vtop->type.t;
    gaddrof();
    vtop->type.t = VT_PTRDIFF_T;
    vpushs(offset);
    gen_op('+');
    vtop->r |= VT_LVAL;
    vtop->type.t = t;
}

static void incr_bf_adr(int o)
{
    vtop->type.t = VT_BYTE | VT_UNSIGNED;
    incr_offset(o);
}

static void load_packed_bf(CType *type, int bit_pos, int bit_size)
{
    int n, o, bits;
    save_reg_upstack(vtop->r, 1);
    vpush64(type->t & VT_BTYPE, 0);
    bits = 0, o = bit_pos >> 3, bit_pos &= 7;
    do {
        vswap();
        incr_bf_adr(o);
        vdup();
        n = 8 - bit_pos;
        if (n > bit_size)
            n = bit_size;
        if (bit_pos)
            vpushi(bit_pos), gen_op(TOK_SHR), bit_pos = 0;
        if (n < 8)
            vpushi((1 << n) - 1), gen_op('&');
        gen_cast(type);
        if (bits)
            vpushi(bits), gen_op(TOK_SHL);
        vrotb(3);
        gen_op('|');
        bits += n, bit_size -= n, o = 1;
    } while (bit_size);
    vswap(), vpop();
    if (!(type->t & VT_UNSIGNED)) {
        n = ((type->t & VT_BTYPE) == VT_LLONG ? 64 : 32) - bits;
        vpushi(n), gen_op(TOK_SHL);
        vpushi(n), gen_op(TOK_SAR);
    }
}

static void store_packed_bf(int bit_pos, int bit_size)
{
    int bits, n, o, m, c;
    c = (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    vswap();
    save_reg_upstack(vtop->r, 1);
    bits = 0, o = bit_pos >> 3, bit_pos &= 7;
    do {
        incr_bf_adr(o);
        vswap();
        c ? vdup() : gv_dup();
        vrott(3);
        if (bits)
            vpushi(bits), gen_op(TOK_SHR);
        if (bit_pos)
            vpushi(bit_pos), gen_op(TOK_SHL);
        n = 8 - bit_pos;
        if (n > bit_size)
            n = bit_size;
        if (n < 8) {
            m = ((1 << n) - 1) << bit_pos;
            vpushi(m), gen_op('&');
            vpushv(vtop-1);
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

static int adjust_bf(SValue *sv, int bit_pos, int bit_size)
{
    int t;
    if (0 == sv->type.ref)
        return 0;
    t = sv->type.ref->auxtype;
    if (t != -1 && t != VT_STRUCT) {
        sv->type.t = (sv->type.t & ~(VT_BTYPE | VT_LONG)) | t;
        sv->r |= VT_LVAL;
    }
    return t;
}

ST_FUNC int gv(int rc)
{
    int r, r2, r_ok, r2_ok, rc2, bt;
    int bit_pos, bit_size, size, align;

    seqp_record_sv(vtop, SEQP_READ);

    if (vtop->type.t & VT_BITFIELD) {
        CType type;

        bit_pos = BIT_POS(vtop->type.t);
        bit_size = BIT_SIZE(vtop->type.t);
        vtop->type.t &= ~VT_STRUCT_MASK;

        type.ref = NULL;
        type.t = vtop->type.t & VT_UNSIGNED;
        if ((vtop->type.t & VT_BTYPE) == VT_BOOL)
            type.t |= VT_UNSIGNED;

        r = adjust_bf(vtop, bit_pos, bit_size);

        if ((vtop->type.t & VT_BTYPE) == VT_LLONG)
            type.t |= VT_LLONG;
        else
            type.t |= VT_INT;

        if (r == VT_STRUCT) {
            load_packed_bf(&type, bit_pos, bit_size);
        } else {
            int bits = (type.t & VT_BTYPE) == VT_LLONG ? 64 : 32;
            gen_cast(&type);
            vpushi(bits - (bit_pos + bit_size));
            gen_op(TOK_SHL);
            vpushi(bits - bit_size);
            gen_op(TOK_SAR);
        }
        r = gv(rc);
    } else {
        if (is_float(vtop->type.t) &&
            (vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
            init_params p = { .sec = rodata_section };
            unsigned long offset;
            CType ltype = vtop->type;
            ltype.t &= ~VT_TLS;
            size = type_size(&vtop->type, &align);
            if (NODATA_WANTED)
                size = 0, align = 1;
            offset = section_add(p.sec, size, align);
            vpush_ref(&ltype, p.sec, offset, size);
	    vswap();
	    init_putv(&p, &vtop->type, offset);
	    vtop->r |= VT_LVAL;
        }
#ifdef CONFIG_MCC_BCHECK
        if (vtop->r & VT_MUSTBOUND)
            gbound();
#endif

        bt = vtop->type.t & VT_BTYPE;
        if (bt == VT_VOID || bt == VT_STRUCT)
            return vtop->r;

#ifdef MCC_TARGET_RISCV64
        if (bt == VT_LDOUBLE && rc == RC_FLOAT)
          rc = RC_INT;
#endif
        rc2 = RC2_TYPE(bt, rc);

        r = vtop->r & VT_VALMASK;
        r_ok = !(vtop->r & VT_LVAL) && (r < VT_CONST) && (reg_classes[r] & rc);
        r2_ok = !rc2 || ((vtop->r2 < VT_CONST) && (reg_classes[vtop->r2] & rc2));

        if (!r_ok || !r2_ok) {

            if (!r_ok) {
                if (1
                    && r < VT_CONST
                    && (reg_classes[r] & rc)
                    && !rc2
                    )
                    save_reg_upstack(r, 1);
                else
                    r = get_reg(rc);
            }

            if (rc2) {
                int load_type = (bt == VT_QFLOAT) ? VT_DOUBLE : VT_PTRDIFF_T;
                int original_type = vtop->type.t;

                if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
                    unsigned long long ll = vtop->c.i;
                    vtop->c.i = ll;
                    load(r, vtop);
                    vtop->r = r;
                    vpushi(ll >> 32);
                } else if (vtop->r & VT_LVAL) {
                    save_reg_upstack(vtop->r, 1);
                    vtop->type.t = load_type;
                    load(r, vtop);
                    vdup();
                    vtop[-1].r = r;
                    incr_offset(PTR_SIZE);
                } else {
                    if (!r_ok)
                        load(r, vtop);
                    if (r2_ok && vtop->r2 < VT_CONST)
                        goto done;
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
            } else {
                if (vtop->r == VT_CMP)
                    vset_VT_JMP();
                load(r, vtop);
            }
        }
        vtop->r = r;
    }
    return r;
}

ST_FUNC void gv2(int rc1, int rc2)
{
    if (vtop->r != VT_CMP && rc1 <= rc2) {
        vswap();
        gv(rc1);
        vswap();
        gv(rc2);
        if ((vtop[-1].r & VT_VALMASK) >= VT_CONST) {
            vswap();
            gv(rc1);
            vswap();
        }
    } else {
        gv(rc2);
        vswap();
        gv(rc1);
        vswap();
        if ((vtop[0].r & VT_VALMASK) >= VT_CONST) {
            gv(rc2);
        }
    }
}

#if PTR_SIZE == 4
ST_FUNC void lexpand(void)
{
    int u, v;
    u = vtop->type.t & (VT_DEFSIGN | VT_UNSIGNED);
    v = vtop->r & (VT_VALMASK | VT_LVAL);
    if (v == VT_CONST) {
        vdup();
        vtop[0].c.i >>= 32;
    } else if (v == (VT_LVAL|VT_CONST) || v == (VT_LVAL|VT_LOCAL)) {
        vdup();
        vtop[0].c.i += 4;
    } else {
        gv(RC_INT);
        vdup();
        vtop[0].r = vtop[-1].r2;
        vtop[0].r2 = vtop[-1].r2 = VT_CONST;
    }
    vtop[0].type.t = vtop[-1].type.t = VT_INT | u;
}
#endif

#if PTR_SIZE == 4
static void lbuild(int t)
{
    gv2(RC_INT, RC_INT);
    vtop[-1].r2 = vtop[0].r;
    vtop[-1].type.t = t;
    vpop();
}
#endif

static void gv_dup(void)
{
    int t, rc, r;

    t = vtop->type.t;
#if PTR_SIZE == 4
    if ((t & VT_BTYPE) == VT_LLONG) {
        if (t & VT_BITFIELD) {
            gv(RC_INT);
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
    rc = RC_TYPE(t);
    gv(rc);
    r = get_reg(rc);
    vdup();
    load(r, vtop);
    vtop->r = r;
}

#if PTR_SIZE == 4
static void gen_opl(int op)
{
    int t, a, b, op1, c, i;
    int func;
    unsigned short reg_iret = REG_IRET;
    unsigned short reg_lret = REG_IRE2;
    SValue tmp;

    switch(op) {
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
        reg_iret = TREG_R2;
        reg_lret = TREG_R3;
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
        if (op == '*') {
            vpushv(vtop - 1);
            vpushv(vtop - 1);
            gen_op(TOK_UMULL);
            lexpand();
            for(i=0;i<4;i++)
                vrotb(6);
            tmp = vtop[0];
            vtop[0] = vtop[-2];
            vtop[-2] = tmp;
            gen_op('*');
            vrotb(3);
            vrotb(3);
            gen_op('*');
            gen_op('+');
            gen_op('+');
        } else if (op == '+' || op == '-') {
            if (op == '+')
                op1 = TOK_ADDC1;
            else
                op1 = TOK_SUBC1;
            gen_op(op1);
            vrotb(3);
            vrotb(3);
            gen_op(op1 + 1);
        } else {
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
        if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
            t = vtop[-1].type.t;
            vswap();
            lexpand();
            vrotb(3);
            c = (int)vtop->c.i;
            vpop();
            if (op != TOK_SHL)
                vswap();
            if (c >= 32) {
                vpop();
                if (c > 32) {
                    vpushi(c - 32);
                    gen_op(op);
                }
                if (op != TOK_SAR) {
                    vpushi(0);
                } else {
                    gv_dup();
                    vpushi(31);
                    gen_op(TOK_SAR);
                }
                vswap();
            } else {
                vswap();
                gv_dup();
                vpushi(c);
                gen_op(op);
                vswap();
                vpushi(32 - c);
                if (op == TOK_SHL)
                    gen_op(TOK_SHR);
                else
                    gen_op(TOK_SHL);
                vrotb(3);
                vpushi(c);
                if (op == TOK_SHL)
                    gen_op(TOK_SHL);
                else
                    gen_op(TOK_SHR);
                gen_op('|');
            }
            if (op != TOK_SHL)
                vswap();
            lbuild(t);
        } else {
            switch(op) {
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
        if (!cur_switch || cur_switch->bsym) {
            save_regs(4);
        }
        op1 = op;
        if (op1 == TOK_LT)
            op1 = TOK_LE;
        else if (op1 == TOK_GT)
            op1 = TOK_GE;
        else if (op1 == TOK_ULT)
            op1 = TOK_ULE;
        else if (op1 == TOK_UGT)
            op1 = TOK_UGE;
        a = 0;
        b = 0;
        gen_op(op1);
        if (op == TOK_NE) {
            b = gvtst(0, 0);
        } else {
            a = gvtst(1, 0);
            if (op != TOK_EQ) {
                vpushi(0);
                vset_VT_CMP(TOK_NE);
                b = gvtst(0, 0);
            }
        }
        op1 = op;
        if (op1 == TOK_LT)
            op1 = TOK_ULT;
        else if (op1 == TOK_LE)
            op1 = TOK_ULE;
        else if (op1 == TOK_GT)
            op1 = TOK_UGT;
        else if (op1 == TOK_GE)
            op1 = TOK_UGE;
        gen_op(op1);
#if 0
        if (op == TOK_NE) { gsym(b); break; }
        if (op == TOK_EQ) { gsym(a); break; }
#endif
        gvtst_set(1, a);
        gvtst_set(0, b);
        break;
    }
}
#endif

static uint64_t value64(uint64_t l1, int t)
{
    if ((t & VT_BTYPE) == VT_LLONG
        || (PTR_SIZE == 8 && (t & VT_BTYPE) == VT_PTR))
        return l1;
    else if (t & VT_UNSIGNED)
        return (uint32_t)l1;
    else
        return (uint32_t)l1 | -(l1 & 0x80000000);
}

static uint64_t gen_opic_sdiv(uint64_t a, uint64_t b)
{
    uint64_t x = (a >> 63 ? -a : a) / (b >> 63 ? -b : b);
    return (a ^ b) >> 63 ? -x : x;
}

static int gen_opic_lt(uint64_t a, uint64_t b)
{
    return (a ^ (uint64_t)1 << 63) < (b ^ (uint64_t)1 << 63);
}

static int pp_signed_ovf(int op, uint64_t l1, uint64_t l2)
{
    int64_t a = (int64_t)l1, b = (int64_t)l2, r;
    switch (op) {
    case '+':
        r = (int64_t)((uint64_t)a + (uint64_t)b);
        return ((a ^ r) & (b ^ r)) < 0;
    case '-':
        r = (int64_t)((uint64_t)a - (uint64_t)b);
        return ((a ^ b) & (a ^ r)) < 0;
    case '*':
        if (a == 0 || b == 0)
            return 0;
        r = (int64_t)((uint64_t)a * (uint64_t)b);
        return r / a != b
            || (a == (int64_t)0x8000000000000000ULL && b == -1)
            || (b == (int64_t)0x8000000000000000ULL && a == -1);
    }
    return 0;
}

static void gen_opic(int op)
{
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

    if (c1 && c2) {
        if (pp_expr && t1 == VT_LLONG && !(v1->type.t & VT_UNSIGNED)
            && (op == '+' || op == '-' || op == '*')
            && pp_signed_ovf(op, l1, l2)) {
            int pp_save = pp_expr;
            pp_expr = 0;
            mcc_warning("integer overflow in preprocessor expression");
            pp_expr = pp_save;
        }
        else if (!pp_expr && CONST_WANTED && !NOEVAL_WANTED
                 && !(v1->type.t & VT_UNSIGNED)
                 && (op == '+' || op == '-' || op == '*')
                 && (t1 == VT_INT || t1 == VT_LLONG)) {
            int ovf;
            if (t1 == VT_LLONG) {
                ovf = pp_signed_ovf(op, l1, l2);
            } else {
                int64_t a = (int32_t)l1, b = (int32_t)l2, r;
                switch (op) {
                case '+': r = a + b; break;
                case '-': r = a - b; break;
                default:  r = a * b; break;
                }
                ovf = r < (-2147483647LL - 1) || r > 2147483647LL;
            }
            if (ovf)
                mcc_pedantic("integer overflow in constant expression");
        }
        switch(op) {
        case '+': l1 += l2; break;
        case '-': l1 -= l2; break;
        case '&': l1 &= l2; break;
        case '^': l1 ^= l2; break;
        case '|': l1 |= l2; break;
        case '*': l1 *= l2; break;

        case TOK_PDIV:
        case '/':
        case '%':
        case TOK_UDIV:
        case TOK_UMOD:
            if (l2 == 0) {
                if (CONST_WANTED && !NOEVAL_WANTED)
                    mcc_error("division by zero in constant");
                goto general_case;
            }
            switch(op) {
            default: l1 = gen_opic_sdiv(l1, l2); break;
            case '%': l1 = l1 - l2 * gen_opic_sdiv(l1, l2); break;
            case TOK_UDIV: l1 = l1 / l2; break;
            case TOK_UMOD: l1 = l1 % l2; break;
            }
            break;
        case TOK_SHL: l1 <<= (l2 & shm); break;
        case TOK_SHR: l1 >>= (l2 & shm); break;
        case TOK_SAR:
            l1 = (l1 >> 63) ? ~(~l1 >> (l2 & shm)) : l1 >> (l2 & shm);
            break;
        case TOK_ULT: l1 = l1 < l2; break;
        case TOK_UGE: l1 = l1 >= l2; break;
        case TOK_EQ: l1 = l1 == l2; break;
        case TOK_NE: l1 = l1 != l2; break;
        case TOK_ULE: l1 = l1 <= l2; break;
        case TOK_UGT: l1 = l1 > l2; break;
        case TOK_LT: l1 = gen_opic_lt(l1, l2); break;
        case TOK_GE: l1 = !gen_opic_lt(l1, l2); break;
        case TOK_LE: l1 = !gen_opic_lt(l2, l1); break;
        case TOK_GT: l1 = gen_opic_lt(l2, l1); break;
        case TOK_LAND: l1 = l1 && l2; break;
        case TOK_LOR: l1 = l1 || l2; break;
        default:
            goto general_case;
        }
        v1->c.i = value64(l1, v1->type.t);
        v1->r |= v2->r & VT_NONCONST;
        vtop--;
    } else {
        if (c1 && (op == '+' || op == '&' || op == '^' ||
                   op == '|' || op == '*' || op == TOK_EQ || op == TOK_NE)) {
            vswap();
            c2 = c1;
            l2 = l1;
        }
        if (c1 && ((l1 == 0 &&
                    (op == TOK_SHL || op == TOK_SHR || op == TOK_SAR)) ||
                   (l1 == -1 && op == TOK_SAR))) {
            vpop();
        } else if (c2 && ((l2 == 0 && (op == '&' || op == '*')) ||
                          (op == '|' &&
                            (l2 == -1 || (l2 == 0xFFFFFFFF && t2 != VT_LLONG))) ||
                          (l2 == 1 && (op == '%' || op == TOK_UMOD)))) {
            if (l2 == 1)
                vtop->c.i = 0;
            vswap();
            vtop--;
        } else if (c2 && (((op == '*' || op == '/' || op == TOK_UDIV ||
                          op == TOK_PDIV) &&
                           l2 == 1) ||
                          ((op == '+' || op == '-' || op == '|' || op == '^' ||
                            op == TOK_SHL || op == TOK_SHR || op == TOK_SAR) &&
                           l2 == 0) ||
                          (op == '&' &&
                            (l2 == -1 || (l2 == 0xFFFFFFFF && t2 != VT_LLONG))))) {
            vtop--;
        } else if (c2 && (op == '*' || op == TOK_PDIV || op == TOK_UDIV || op == TOK_UMOD)) {
            if (l2 > 0 && (l2 & (l2 - 1)) == 0) {
                int n = -1;
                if (op == TOK_UMOD) {
                    vtop->c.i = l2 - 1;
                    op = '&';
                    goto general_case;
                }
                while (l2) {
                    l2 >>= 1;
                    n++;
                }
                vtop->c.i = n;
                if (op == '*')
                    op = TOK_SHL;
                else if (op == TOK_PDIV)
                    op = TOK_SAR;
                else
                    op = TOK_SHR;
            }
            goto general_case;
        } else if (c2 && (op == '+' || op == '-') &&
                   (r = vtop[-1].r & (VT_VALMASK | VT_LVAL | VT_SYM),
                    r == (VT_CONST | VT_SYM) || r == VT_LOCAL)) {
            if (op == '-')
                l2 = -l2;
	    l2 += vtop[-1].c.i;
	    if ((int)l2 != l2)
	        goto general_case;
            vtop--;
            vtop->c.i = l2;
        } else {
        general_case:
                if (t1 == VT_LLONG || t2 == VT_LLONG ||
                    (PTR_SIZE == 8 && (t1 == VT_PTR || t2 == VT_PTR)))
                    gen_opl(op);
                else
                    gen_opi(op);
        }
        if (vtop->r == VT_CONST)
            vtop->r |= VT_NONCONST;
    }
}

#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_I386 || defined MCC_TARGET_ARM64
# define gen_negf gen_opf
#elif defined MCC_TARGET_ARM
void gen_negf(int op)
{
    vpushi(0), vswap(), gen_op('-');
}
#else
void gen_negf(int op)
{
    int align, size, bt;
    size = type_size(&vtop->type, &align);
    bt = vtop->type.t & VT_BTYPE;
#if defined MCC_TARGET_X86_64 || defined MCC_TARGET_I386
    if (bt == VT_LDOUBLE)
        size = 10;
#endif
    if (nocode_wanted)
        goto gv2;
    save_reg(gv(RC_TYPE(bt)));
    vdup();
    incr_bf_adr(size - 1);
    vdup();
    vpushi(0x80);
    gen_op('^');
    vstore();
    vpop();
gv2:
    gv(RC_TYPE(bt));
}
#endif

static void gen_opif(int op)
{
    int c1, c2, i, bt;
    SValue *v1, *v2;
    HOST_VOLATILE_LDOUBLE long double f1, f2;

    v1 = vtop - 1;
    v2 = vtop;
    if (op == TOK_NEG)
        v1 = v2;
    bt = v1->type.t & VT_BTYPE;

    c1 = (v1->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    c2 = (v2->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
    if (CONST_WANTED)
        ice_float_op = 1;
    if (c1 && c2 && !CONST_WANTED && stdc_fenv_access(mcc_state)
        && (op == '+' || op == '-' || op == '*' || op == '/'))
        goto general_case;
    if (c1 && c2) {
        if (bt == VT_FLOAT) {
            f1 = v1->c.f;
            f2 = v2->c.f;
        } else if (bt == VT_DOUBLE) {
            f1 = v1->c.d;
            f2 = v2->c.d;
        } else {
            f1 = v1->c.ld;
            f2 = v2->c.ld;
        }
        if (!(ieee_finite(f1) || !ieee_finite(f2)) && !CONST_WANTED)
            goto general_case;
        switch(op) {
        case '+': f1 += f2; break;
        case '-': f1 -= f2; break;
        case '*': f1 *= f2; break;
        case '/':
            if (f2 == 0.0) {
                union { float f; unsigned u; } x1, x2, y;
                if (!CONST_WANTED)
                    goto general_case;
                x1.f = f1, x2.f = f2;
                if (f1 == 0.0)
                    y.u = 0x7fc00000;
                else
                    y.u = 0x7f800000;
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
        if (bt == VT_FLOAT) {
            v1->c.f = f1;
        } else if (bt == VT_DOUBLE) {
            v1->c.d = f1;
        } else {
            v1->c.ld = f1;
        }
    } else {
    general_case:
        if (op == TOK_NEG) {
            gen_negf(op);
        } else {
            gen_opf(op);
        }
    }
}

static void type_to_str(char *buf, int buf_size,
                 CType *type, const char *varstr)
{
    int bt, v, t;
    Sym *s, *sa;
    char buf1[256];
    const char *tstr;

    t = type->t;
    bt = t & VT_BTYPE;
    buf[0] = '\0';

    if (t & VT_EXTERN)
        pstrcat(buf, buf_size, "extern ");
    if (t & VT_STATIC)
        pstrcat(buf, buf_size, "static ");
    if (t & VT_TYPEDEF)
        pstrcat(buf, buf_size, "typedef ");
    if (t & VT_INLINE)
        pstrcat(buf, buf_size, "inline ");
    if (bt != VT_PTR) {
        if (t & VT_VOLATILE)
            pstrcat(buf, buf_size, "volatile ");
        if (t & VT_CONSTANT)
            pstrcat(buf, buf_size, "const ");
    }
    if (((t & VT_DEFSIGN) && bt == VT_BYTE)
        || ((t & VT_UNSIGNED)
            && (bt == VT_SHORT || bt == VT_INT || bt == VT_LLONG)
            && !IS_ENUM(t)
            ))
        pstrcat(buf, buf_size, (t & VT_UNSIGNED) ? "unsigned " : "signed ");

    buf_size -= strlen(buf);
    buf += strlen(buf);

    switch(bt) {
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
            tstr = "long";
        if (!IS_ENUM(t))
            goto add_tstr;
        tstr = "enum ";
        goto tstruct;
    case VT_FLOAT:
        tstr = "float";
        goto add_tstr;
    case VT_DOUBLE:
        tstr = "double";
        if (!(t & VT_LONG))
            goto add_tstr;
        FALLTHROUGH;
    case VT_LDOUBLE:
        tstr = "long double";
    add_tstr:
        pstrcat(buf, buf_size, tstr);
        break;
    case VT_STRUCT:
        tstr = "struct ";
        if (IS_UNION(t))
            tstr = "union ";
    tstruct:
        pstrcat(buf, buf_size, tstr);
        v = type->ref->v & ~SYM_STRUCT;
        if (v >= SYM_FIRST_ANOM)
            pstrcat(buf, buf_size, "<anonymous>");
        else
            pstrcat(buf, buf_size, get_tok_str(v, NULL));
        break;
    case VT_FUNC:
        s = type->ref;
        buf1[0]=0;
        if (varstr && '*' == *varstr) {
            pstrcat(buf1, sizeof(buf1), "(");
            pstrcat(buf1, sizeof(buf1), varstr);
            pstrcat(buf1, sizeof(buf1), ")");
        }
        pstrcat(buf1, buf_size, "(");
        sa = s->next;
        while (sa != NULL) {
            char buf2[256];
            type_to_str(buf2, sizeof(buf2), &sa->type, NULL);
            pstrcat(buf1, sizeof(buf1), buf2);
            sa = sa->next;
            if (sa)
                pstrcat(buf1, sizeof(buf1), ", ");
        }
        if (s->f.func_type == FUNC_ELLIPSIS)
            pstrcat(buf1, sizeof(buf1), ", ...");
        pstrcat(buf1, sizeof(buf1), ")");
        type_to_str(buf, buf_size, &s->type, buf1);
        goto no_var;
    case VT_PTR:
        s = type->ref;
        if (t & (VT_ARRAY|VT_VLA)) {
            if (varstr && '*' == *varstr)
                snprintf(buf1, sizeof(buf1), "(%s)[%d]", varstr, s->c);
            else
                snprintf(buf1, sizeof(buf1), "%s[%d]", varstr ? varstr : "", s->c);
            type_to_str(buf, buf_size, &s->type, buf1);
            goto no_var;
        }
        pstrcpy(buf1, sizeof(buf1), "*");
        if (t & VT_CONSTANT)
            pstrcat(buf1, buf_size, "const ");
        if (t & VT_VOLATILE)
            pstrcat(buf1, buf_size, "volatile ");
        if (varstr)
            pstrcat(buf1, sizeof(buf1), varstr);
        type_to_str(buf, buf_size, &s->type, buf1);
        goto no_var;
    }
    if (varstr) {
        pstrcat(buf, buf_size, " ");
        pstrcat(buf, buf_size, varstr);
    }
 no_var: ;
}

static void type_incompatibility_error(CType* st, CType* dt, const char* fmt)
{
    char buf1[256], buf2[256];
    type_to_str(buf1, sizeof(buf1), st, NULL);
    type_to_str(buf2, sizeof(buf2), dt, NULL);
    mcc_error(fmt, buf1, buf2);
}

static void type_incompatibility_warning(CType* st, CType* dt, const char* fmt)
{
    char buf1[256], buf2[256];
    type_to_str(buf1, sizeof(buf1), st, NULL);
    type_to_str(buf2, sizeof(buf2), dt, NULL);
    mcc_warning(fmt, buf1, buf2);
}

static int pointed_size(CType *type)
{
    int align;
    return type_size(pointed_type(type), &align);
}

static inline int is_null_pointer(SValue *p)
{
    if ((p->r & (VT_VALMASK | VT_LVAL | VT_SYM | VT_NONCONST)) != VT_CONST)
        return 0;
    return ((p->type.t & VT_BTYPE) == VT_INT && (uint32_t)p->c.i == 0) ||
        ((p->type.t & VT_BTYPE) == VT_LLONG && p->c.i == 0) ||
        ((p->type.t & VT_BTYPE) == VT_PTR &&
         (PTR_SIZE == 4 ? (uint32_t)p->c.i == 0 : p->c.i == 0) &&
         ((pointed_type(&p->type)->t & VT_BTYPE) == VT_VOID) &&
         0 == (pointed_type(&p->type)->t & VT_QUALIFY)
         );
}

static int is_compatible_func(CType *type1, CType *type2)
{
    Sym *s1, *s2;

    s1 = type1->ref;
    s2 = type2->ref;
    if (s1->f.func_call != s2->f.func_call)
        return 0;
    if (s1->f.func_type != s2->f.func_type
        && s1->f.func_type != FUNC_OLD
        && s2->f.func_type != FUNC_OLD)
        return 0;
    for (;;) {
        if (!is_compatible_unqualified_types(&s1->type, &s2->type))
            return 0;
        if (s1->f.func_type == FUNC_OLD || s2->f.func_type == FUNC_OLD )
            return 1;
        s1 = s1->next;
        s2 = s2->next;
        if (!s1)
            return !s2;
        if (!s2)
            return 0;
    }
}

static int compare_types(CType *type1, CType *type2, int unqualified)
{
    int bt1, t1, t2;

    if (IS_ENUM(type1->t)) {
        if (IS_ENUM(type2->t))
            return type1->ref == type2->ref;
        type1 = &type1->ref->type;
    } else if (IS_ENUM(type2->t))
        type2 = &type2->ref->type;

    t1 = type1->t & VT_TYPE;
    t2 = type2->t & VT_TYPE;
    if (unqualified) {
        t1 &= ~VT_QUALIFY;
        t2 &= ~VT_QUALIFY;
    }

    if ((t1 & VT_BTYPE) != VT_BYTE) {
        t1 &= ~VT_DEFSIGN;
        t2 &= ~VT_DEFSIGN;
    }
    if (t1 != t2)
        return 0;

    if ((t1 & VT_ARRAY)
        && !(type1->ref->c < 0
          || type2->ref->c < 0
          || type1->ref->c == type2->ref->c))
            return 0;

    bt1 = t1 & VT_BTYPE;
    if (bt1 == VT_PTR) {
        type1 = pointed_type(type1);
        type2 = pointed_type(type2);
        return is_compatible_types(type1, type2);
    } else if (bt1 == VT_STRUCT) {
        return (type1->ref == type2->ref);
    } else if (bt1 == VT_FUNC) {
        return is_compatible_func(type1, type2);
    } else {
        return 1;
    }
}

#define CMP_OP 'C'
#define SHIFT_OP 'S'

static int combine_types(CType *dest, SValue *op1, SValue *op2, int op)
{
    CType *type1, *type2, type;
    int t1, t2, bt1, bt2;
    int ret = 1;

    if (op == SHIFT_OP)
        op2 = op1;

    type1 = &op1->type, type2 = &op2->type;
    t1 = type1->t, t2 = type2->t;
    bt1 = t1 & VT_BTYPE, bt2 = t2 & VT_BTYPE;

    type.t = VT_VOID;
    type.ref = NULL;

    if (bt1 == VT_VOID || bt2 == VT_VOID) {
        if (op != '?')
            mcc_error("operation on void value");
        type.t = VT_VOID;
    } else if (bt1 == VT_PTR || bt2 == VT_PTR) {
        if (op == '+') {
          if (!is_integer_btype(bt1 == VT_PTR ? bt2 : bt1))
            ret = 0;
        }
        else if (is_null_pointer (op2)) type = *type1;
        else if (is_null_pointer (op1)) type = *type2;
        else if (bt1 != bt2) {
            if ((op == '?' || op == CMP_OP)
                && (is_integer_btype(bt1) || is_integer_btype(bt2)))
              mcc_warning("pointer/integer mismatch in %s",
                          op == '?' ? "conditional expression" : "comparison");
            else if (op != '-' || !is_integer_btype(bt2))
              ret = 0;
            type = *(bt1 == VT_PTR ? type1 : type2);
        } else {
            CType *pt1 = pointed_type(type1);
            CType *pt2 = pointed_type(type2);
            int pbt1 = pt1->t & VT_BTYPE;
            int pbt2 = pt2->t & VT_BTYPE;
            int newquals, copied = 0;
            if (pbt1 != VT_VOID && pbt2 != VT_VOID
                && !compare_types(pt1, pt2, 1 )) {
                if (op != '?' && op != CMP_OP)
                  ret = 0;
                else
                  type_incompatibility_warning(type1, type2,
                    op == '?'
                     ? "pointer type mismatch in conditional expression ('%s' and '%s')"
                     : "pointer type mismatch in comparison('%s' and '%s')");
            }
            if (op == '?') {
                type = *((pbt1 == VT_VOID) ? type1 : type2);
                newquals = ((pt1->t | pt2->t) & VT_QUALIFY);
                if ((~pointed_type(&type)->t & VT_QUALIFY)
                    & newquals)
                  {
                    type.ref = sym_push(SYM_FIELD, &type.ref->type,
                                        0, type.ref->c);
                    copied = 1;
                    pointed_type(&type)->t |= newquals;
                  }
                if (pt1->t & VT_ARRAY
                    && pt2->t & VT_ARRAY
                    && pointed_type(&type)->ref->c < 0
                    && (pt1->ref->c > 0 || pt2->ref->c > 0))
                  {
                    if (!copied)
                      type.ref = sym_push(SYM_FIELD, &type.ref->type,
                                          0, type.ref->c);
                    pointed_type(&type)->ref =
                        sym_push(SYM_FIELD, &pointed_type(&type)->ref->type,
                                 0, pointed_type(&type)->ref->c);
                    pointed_type(&type)->ref->c =
                        0 < pt1->ref->c ? pt1->ref->c : pt2->ref->c;
                  }
            }
        }
        if (op == CMP_OP)
          type.t = VT_SIZE_T;
    } else if (bt1 == VT_STRUCT || bt2 == VT_STRUCT) {
        if (op != '?' || !compare_types(type1, type2, 1))
          ret = 0;
        type = *type1;
    } else if (is_float(bt1) || is_float(bt2)) {
        if (bt1 == VT_LDOUBLE || bt2 == VT_LDOUBLE) {
            type.t = VT_LDOUBLE;
        } else if (bt1 == VT_DOUBLE || bt2 == VT_DOUBLE) {
            type.t = VT_DOUBLE;
        } else {
            type.t = VT_FLOAT;
        }
    } else if (bt1 == VT_LLONG || bt2 == VT_LLONG) {
        type.t = VT_LLONG | VT_LONG;
        if (bt1 == VT_LLONG)
          type.t &= t1;
        if (bt2 == VT_LLONG)
          type.t &= t2;
        if ((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED) ||
            (t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED))
          type.t |= VT_UNSIGNED;
    } else {
        type.t = VT_INT | (VT_LONG & (t1 | t2));
        if (((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED)
                && (!(t1 & VT_BITFIELD) || BIT_SIZE(t1) == 32))
         || ((t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED)
                && (!(t2 & VT_BITFIELD) || BIT_SIZE(t2) == 32)))
          type.t |= VT_UNSIGNED;
    }
    if (dest)
      *dest = type;
    return ret;
}

ST_FUNC void gen_op(int op)
{
    int t1, t2, bt1, bt2, t;
    CType type1, combtype;
    int op_class = op;

    expr_has_effect = 0;

    if (op == TOK_SHR || op == TOK_SAR || op == TOK_SHL)
        op_class = SHIFT_OP;
    else if (TOK_ISCOND(op))
        op_class = CMP_OP;

redo:
    t1 = vtop[-1].type.t;
    t2 = vtop[0].type.t;
    bt1 = t1 & VT_BTYPE;
    bt2 = t2 & VT_BTYPE;

    if (is_complex_type(&vtop[-1].type) || is_complex_type(&vtop[0].type)) {
        gen_complex_op(op);
        return;
    }

    if (bt1 == VT_FUNC || bt2 == VT_FUNC) {
	if (bt2 == VT_FUNC) {
	    mk_pointer(&vtop->type);
	    gaddrof();
	}
	if (bt1 == VT_FUNC) {
	    vswap();
	    mk_pointer(&vtop->type);
	    gaddrof();
	    vswap();
	}
	goto redo;
    } else if (!combine_types(&combtype, vtop - 1, vtop, op_class)) {
op_err:
        mcc_error("invalid operand types for binary operation");
    } else if (bt1 == VT_PTR || bt2 == VT_PTR) {
        int align;
        if (op_class == CMP_OP)
            goto std_op;
        if (op == '+' || op == '-') {
            CType *pt = bt1 == VT_PTR ? pointed_type(&vtop[-1].type)
                      : bt2 == VT_PTR ? pointed_type(&vtop[0].type) : NULL;
            if (pt && ((pt->t & VT_BTYPE) == VT_VOID
                       || (pt->t & VT_BTYPE) == VT_FUNC))
                mcc_pedantic("ISO C forbids arithmetic on a pointer to "
                             "'void' or to a function");
        }
        if (bt1 == VT_PTR && bt2 == VT_PTR) {
            if (op != '-')
                goto op_err;
            vpush_type_size(pointed_type(&vtop[-1].type), &align);
            vtop->type.t &= ~VT_UNSIGNED;
            vrott(3);
            gen_opic(op);
            vtop->type.t = VT_PTRDIFF_T;
            vswap();
            gen_op(TOK_PDIV);
        } else {
            if (op != '-' && op != '+')
                goto op_err;
            if (bt2 == VT_PTR) {
                vswap();
                t = t1, t1 = t2, t2 = t;
                bt2 = bt1;
            }
#if PTR_SIZE == 4
            if (bt2 == VT_LLONG)
                gen_cast_s(VT_INT);
#endif
            type1 = vtop[-1].type;
            vpush_type_size(pointed_type(&vtop[-1].type), &align);
            vtop->type.t &= ~VT_UNSIGNED;
            gen_op('*');
#ifdef CONFIG_MCC_BCHECK
            if (mcc_state->do_bounds_check && !CONST_WANTED) {
                if (op == '-') {
                    vpushi(0);
                    vswap();
                    gen_op('-');
                }
                gen_bounded_ptr_add();
            } else
#endif
            {
                gen_opic(op);
            }
            type1.t &= ~(VT_ARRAY|VT_VLA);
            vtop->type = type1;
        }
    } else {
        if (is_float(combtype.t)
            && op != '+' && op != '-' && op != '*' && op != '/'
            && op_class != CMP_OP) {
            goto op_err;
        }
    std_op:
        if (op_class == CMP_OP && (mcc_state->warn_sign_compare & WARN_ON)
            && !is_float(combtype.t)
            && (t1 & VT_UNSIGNED) != (t2 & VT_UNSIGNED)) {
            int ac = (vtop[-1].r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
            int bc = (vtop[0].r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
            if (!ac && !bc)
                mcc_warning_c(warn_sign_compare)(
                    "comparison of integer expressions of different signedness");
        }
        t = t2 = combtype.t;
        if (op_class == SHIFT_OP)
            t2 = VT_INT;
        if (t & VT_UNSIGNED) {
            if (op == TOK_SAR)
                op = TOK_SHR;
            else if (op == '/')
                op = TOK_UDIV;
            else if (op == '%')
                op = TOK_UMOD;
            else if (op == TOK_LT)
                op = TOK_ULT;
            else if (op == TOK_GT)
                op = TOK_UGT;
            else if (op == TOK_LE)
                op = TOK_ULE;
            else if (op == TOK_GE)
                op = TOK_UGE;
        }
        vswap();
        gen_cast_s(t);
        vswap();
        gen_cast_s(t2);
        if (is_float(t))
            gen_opif(op);
        else
            gen_opic(op);
        if (op_class == CMP_OP) {
            vtop->type.t = VT_INT;
        } else {
            vtop->type.t = t;
        }
    }
    if (vtop->r & VT_LVAL)
        gv(RC_TYPE(vtop->type.t));
}

#if defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64 || defined MCC_TARGET_ARM
#define gen_cvt_itof1 gen_cvt_itof
#else
static void gen_cvt_itof1(int t)
{
    if ((vtop->type.t & (VT_BTYPE | VT_UNSIGNED)) ==
        (VT_LLONG | VT_UNSIGNED)) {

        if (t == VT_FLOAT)
            vpush_helper_func(TOK___floatundisf);
#if LDOUBLE_SIZE != 8
        else if (t == VT_LDOUBLE)
            vpush_helper_func(TOK___floatundixf);
#endif
        else
            vpush_helper_func(TOK___floatundidf);
        vrott(2);
        gfunc_call(1);
        vpushi(0);
        PUT_R_RET(vtop, t);
    } else {
        gen_cvt_itof(t);
    }
}
#endif

#if defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64
#define gen_cvt_ftoi1 gen_cvt_ftoi
#else
static void gen_cvt_ftoi1(int t)
{
    int st;
    if (t == (VT_LLONG | VT_UNSIGNED)) {
        st = vtop->type.t & VT_BTYPE;
        if (st == VT_FLOAT)
            vpush_helper_func(TOK___fixunssfdi);
#if LDOUBLE_SIZE != 8
        else if (st == VT_LDOUBLE)
            vpush_helper_func(TOK___fixunsxfdi);
#endif
        else
            vpush_helper_func(TOK___fixunsdfdi);
        vrott(2);
        gfunc_call(1);
        vpushi(0);
        PUT_R_RET(vtop, t);
    } else {
        gen_cvt_ftoi(t);
    }
}
#endif

static void force_charshort_cast(void)
{
    int sbt = BFGET(vtop->r, VT_MUSTCAST) == 2 ? VT_LLONG : VT_INT;
    int dbt = vtop->type.t;
    vtop->r &= ~VT_MUSTCAST;
    vtop->type.t = sbt;
    gen_cast_s(dbt == VT_BOOL ? VT_BYTE|VT_UNSIGNED : dbt);
    vtop->type.t = dbt;
}

static void gen_cast_s(int t)
{
    gen_cast(&(CType){.t = t, .ref = NULL});
}

static void gen_cast(CType *type)
{
    int sbt, dbt, sf, df, c;
    int dbt_bt, sbt_bt, ds, ss, bits, trunc;

    if (!atomic_lowering && (vtop->type.t & VT_ATOMIC_BIT) && (vtop->r & VT_LVAL)
        && atomic_store_needs_libcall(vtop))
        gen_atomic_load_scalar();

    if (vtop->r & VT_MUSTCAST)
        force_charshort_cast();

    if (is_complex_type(type) || is_complex_type(&vtop->type)) {
        if (is_complex_type(type) && is_complex_type(&vtop->type)
            && (type->ref->next->type.t & (VT_BTYPE | VT_LONG))
               == (vtop->type.ref->next->type.t & (VT_BTYPE | VT_LONG)))
            return;
        gen_complex_cast(type);
        return;
    }

    if (vtop->type.t & VT_BITFIELD)
        gv(RC_INT);

    if (IS_ENUM(type->t) && type->ref->c < 0)
        mcc_error("cast to incomplete type");

    dbt = type->t & (VT_BTYPE | VT_UNSIGNED);
    sbt = vtop->type.t & (VT_BTYPE | VT_UNSIGNED);
    if (sbt == VT_FUNC)
        sbt = VT_PTR;

again:
    if (sbt != dbt) {
        sf = is_float(sbt);
        df = is_float(dbt);
        dbt_bt = dbt & VT_BTYPE;
        sbt_bt = sbt & VT_BTYPE;
        if (dbt_bt == VT_VOID) {
            goto done;
        }
        if (sbt_bt == VT_VOID) {
error:
            cast_error(&vtop->type, type);
        }

        if ((sf && dbt_bt == VT_PTR) || (df && sbt_bt == VT_PTR))
            mcc_error("cannot cast between a floating type and a pointer");

        c = (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
        if (c) {
            if (sbt == VT_FLOAT)
                vtop->c.ld = vtop->c.f;
            else if (sbt == VT_DOUBLE)
                vtop->c.ld = vtop->c.d;

            if (df) {
                if (sbt_bt == VT_LLONG) {
                    if ((sbt & VT_UNSIGNED) || !(vtop->c.i >> 63))
                        vtop->c.ld = vtop->c.i;
                    else
                        vtop->c.ld = -(long double)-vtop->c.i;
                } else if(!sf) {
                    if ((sbt & VT_UNSIGNED) || !(vtop->c.i >> 31))
                        vtop->c.ld = (uint32_t)vtop->c.i;
                    else
                        vtop->c.ld = -(long double)-(uint32_t)vtop->c.i;
                }

                if (dbt == VT_FLOAT)
                    vtop->c.f = (float)vtop->c.ld;
                else if (dbt == VT_DOUBLE)
                    vtop->c.d = (double)vtop->c.ld;
            } else if (sf && dbt == VT_BOOL) {
                vtop->c.i = (vtop->c.ld != 0);
            } else {
                if(sf) {
                    if (dbt & VT_UNSIGNED)
                        vtop->c.i = (uint64_t)vtop->c.ld;
                    else
                        vtop->c.i = (int64_t)vtop->c.ld;
                }
                else if (sbt_bt == VT_LLONG || (PTR_SIZE == 8 && sbt == VT_PTR))
                    ;
                else if (sbt & VT_UNSIGNED)
                    vtop->c.i = (uint32_t)vtop->c.i;
                else
                    vtop->c.i = ((uint32_t)vtop->c.i | -(vtop->c.i & 0x80000000));

                if (dbt_bt == VT_LLONG || (PTR_SIZE == 8 && dbt == VT_PTR))
                    ;
                else if (dbt == VT_BOOL)
                    vtop->c.i = (vtop->c.i != 0);
                else {
                    uint32_t m = dbt_bt == VT_BYTE ? 0xff :
                                 dbt_bt == VT_SHORT ? 0xffff :
                                  0xffffffff;
                    vtop->c.i &= m;
                    if (!(dbt & VT_UNSIGNED))
                        vtop->c.i |= -(vtop->c.i & ((m >> 1) + 1));
                }
            }
            goto done;

        } else if (dbt == VT_BOOL
            && (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM))
                == (VT_CONST | VT_SYM)) {
            vtop->r = VT_CONST;
            vtop->c.i = 1;
            goto done;
        }

        if (nocode_wanted & DATA_ONLY_WANTED) {
            if (df)
                vtop->r = get_reg(RC_FLOAT);
            goto done;
        }

        if (dbt == VT_BOOL) {
            gen_test_zero(TOK_NE);
            goto done;
        }

        if (sf || df) {
            if ((sf && dbt_bt == VT_PTR) || (df && sbt_bt == VT_PTR))
                mcc_error("cannot cast between a floating type and a pointer");
            if (sf && df) {
                gen_cvt_ftof(dbt);
            } else if (df) {
                gen_cvt_itof1(dbt);
            } else {
                sbt = dbt;
                if (dbt_bt != VT_LLONG && dbt_bt != VT_INT)
                    sbt = VT_INT;
                gen_cvt_ftoi1(sbt);
                goto again;
            }
            goto done;
        }

        ds = btype_size(dbt_bt);
        ss = btype_size(sbt_bt);
        if (ds == 0 || ss == 0)
            goto error;

        if (ds == ss && ds >= 4)
            goto done;
        if (dbt_bt == VT_PTR || sbt_bt == VT_PTR) {
            mcc_warning("cast between pointer and integer of different size");
            if (sbt_bt == VT_PTR) {
                vtop->type.t = (PTR_SIZE == 8 ? VT_LLONG : VT_INT);
            }
        }

        #define ALLOW_SUBTYPE_ACCESS 1

        if (ALLOW_SUBTYPE_ACCESS && (vtop->r & VT_LVAL)) {
            if (ds <= ss)
                goto done;
            if (ds <= 4 && !(dbt == (VT_SHORT | VT_UNSIGNED) && sbt == VT_BYTE)) {
                gv(RC_INT);
                goto done;
            }
        }
        gv(RC_INT);

        trunc = 0;
#if PTR_SIZE == 4
        if (ds == 8) {
            if (sbt & VT_UNSIGNED) {
                vpushi(0);
                gv(RC_INT);
            } else {
                gv_dup();
                vpushi(31);
                gen_op(TOK_SAR);
            }
            lbuild(dbt);
        } else if (ss == 8) {
            lexpand();
            vpop();
        }
        ss = 4;

#elif PTR_SIZE == 8
        if (ds == 8) {
            if (sbt & VT_UNSIGNED) {
#if defined(MCC_TARGET_RISCV64)
                trunc = 32;
#else
                goto done;
#endif
            } else {
                gen_cvt_sxtw();
                goto done;
            }
            ss = ds, ds = 4, dbt = sbt;
        } else if (ss == 8) {
#if !defined(MCC_TARGET_RISCV64)
            trunc = 32;
#endif
        } else {
            ss = 4;
        }
#endif

        if (ds >= ss)
            goto done;
#if defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64 || defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64
    if (ss == 4) {
        gen_cvt_csti(dbt);
        goto done;
    }
#endif
        bits = (ss - ds) * 8;
        vtop->type.t = (ss == 8 ? VT_LLONG : VT_INT) | (dbt & VT_UNSIGNED);
        vpushi(bits);
        gen_op(TOK_SHL);
        vpushi(bits - trunc);
        gen_op(TOK_SAR);
        vpushi(trunc);
        gen_op(TOK_SHR);
    }
done:
    vtop->type = *type;
    vtop->type.t &= ~(VT_QUALIFY | VT_ARRAY);
}

ST_FUNC int type_size(CType *type, int *a)
{
    Sym *s;
    int bt;

    bt = type->t & VT_BTYPE;
    if (bt == VT_STRUCT) {
        s = type->ref;
        *a = s->r;
        return s->c;
    } else if (bt == VT_PTR) {
        if (type->t & VT_ARRAY) {
            int ts;
            s = type->ref;
            ts = type_size(&s->type, a);
            if (s->c < 0)
                return s->c;
            return ts * s->c;
        } else {
            *a = PTR_SIZE;
            return PTR_SIZE;
        }
    } else if (IS_ENUM(type->t) && type->ref->c < 0) {
        *a = 0;
        return -1;
    } else if (bt == VT_LDOUBLE) {
        *a = LDOUBLE_ALIGN;
        return LDOUBLE_SIZE;
    } else if (bt == VT_DOUBLE || bt == VT_LLONG) {
#if (defined MCC_TARGET_I386 && !defined MCC_TARGET_PE) \
 || (defined MCC_TARGET_ARM && !defined MCC_ARM_EABI)
        *a = 4;
#else
        *a = 8;
#endif
        return 8;
    } else if (bt == VT_INT || bt == VT_FLOAT) {
        *a = 4;
        return 4;
    } else if (bt == VT_SHORT) {
        *a = 2;
        return 2;
    } else if (bt == VT_QLONG || bt == VT_QFLOAT) {
        *a = 8;
        return 16;
    } else {
        *a = 1;
        return 1;
    }
}

static void vpush_type_size(CType *type, int *a)
{
    if (type->t & VT_VLA) {
        type_size(&type->ref->type, a);
        vset(&int_type, VT_LOCAL|VT_LVAL, type->ref->c);
    } else {
        int size = type_size(type, a);
        if (size < 0)
            mcc_error("unknown type size");
        vpushs(size);
    }
}

static inline CType *pointed_type(CType *type)
{
    return &type->ref->type;
}

ST_FUNC void mk_pointer(CType *type)
{
    Sym *s;
    s = sym_push(SYM_FIELD, type, 0, -1);
    type->t = VT_PTR | (type->t & VT_STORAGE);
    type->ref = s;
}

static int is_compatible_types(CType *type1, CType *type2)
{
    return compare_types(type1,type2,0);
}

static int is_compatible_unqualified_types(CType *type1, CType *type2)
{
    return compare_types(type1,type2,1);
}

static void cast_error(CType *st, CType *dt)
{
    type_incompatibility_error(st, dt, "cannot convert '%s' to '%s'");
}

static int aggr_has_const_member(CType *type)
{
    Sym *f;
    if ((type->t & VT_BTYPE) != VT_STRUCT)
        return 0;
    for (f = type->ref->next; f; f = f->next) {
        if (f->type.t & VT_CONSTANT)
            return 1;
        if ((f->type.t & VT_BTYPE) == VT_STRUCT
            && aggr_has_const_member(&f->type))
            return 1;
    }
    return 0;
}

static void verify_assign_cast(CType *dt)
{
    CType *st, *type1, *type2;
    int dbt, sbt, qualwarn, lvl, deepqual;

    st = &vtop->type;
    dbt = dt->t & VT_BTYPE;
    sbt = st->t & VT_BTYPE;
    if (dt->t & VT_CONSTANT)
        mcc_warning("assignment of read-only location");
    else if (dbt == VT_STRUCT && aggr_has_const_member(dt))
        mcc_warning("assignment of read-only location");
    switch(dbt) {
    case VT_VOID:
        if (sbt != dbt)
            mcc_error("assignment to void expression");
        break;
    case VT_PTR:
        if (is_null_pointer(vtop))
            break;
        if (is_integer_btype(sbt)) {
            mcc_warning("assignment makes pointer from integer without a cast");
            break;
        }
        type1 = pointed_type(dt);
        if (sbt == VT_PTR)
            type2 = pointed_type(st);
        else if (sbt == VT_FUNC)
            type2 = st;
        else
            goto error;
        if (is_compatible_types(type1, type2))
            break;
        for (qualwarn = lvl = deepqual = 0;; ++lvl) {
            if (((type2->t & VT_CONSTANT) && !(type1->t & VT_CONSTANT)) ||
                ((type2->t & VT_VOLATILE) && !(type1->t & VT_VOLATILE)))
                qualwarn = 1;
            if (lvl > 0 && ((type1->t & ~type2->t) & (VT_CONSTANT|VT_VOLATILE)))
                deepqual = 1;
            dbt = type1->t & (VT_BTYPE|VT_LONG);
            sbt = type2->t & (VT_BTYPE|VT_LONG);
            if (dbt != VT_PTR || sbt != VT_PTR)
                break;
            type1 = pointed_type(type1);
            type2 = pointed_type(type2);
        }
        if (deepqual) {
            mcc_warning("assignment from incompatible pointer type");
            break;
        }
        if (!is_compatible_unqualified_types(type1, type2)) {
            if ((dbt == VT_VOID || sbt == VT_VOID) && lvl == 0) {
                if (dbt == VT_FUNC || sbt == VT_FUNC)
                    mcc_pedantic("ISO C forbids conversion between a function "
                                 "pointer and 'void *'");
            } else if (dbt == sbt
                && is_integer_btype(sbt & VT_BTYPE)
                && IS_ENUM(type1->t) + IS_ENUM(type2->t)
                    + !!((type1->t ^ type2->t) & VT_UNSIGNED) < 2) {
            } else {
                mcc_warning("assignment from incompatible pointer type");
                break;
            }
        }
        if (qualwarn)
            mcc_warning_c(warn_discarded_qualifiers)("assignment discards qualifiers from pointer target type");
        break;
    case VT_BYTE:
    case VT_SHORT:
    case VT_INT:
    case VT_LLONG:
        if (sbt == VT_PTR || sbt == VT_FUNC) {
            mcc_warning("assignment makes integer from pointer without a cast");
        } else if (sbt == VT_STRUCT) {
            if (!is_complex_type(st))
                goto case_VT_STRUCT;
        }
        break;
    case VT_STRUCT:
    case_VT_STRUCT:
        if (is_complex_type(dt)
            && (is_complex_type(st) || is_integer_btype(sbt) || is_float(st->t)))
            break;
        if (!is_compatible_unqualified_types(dt, st)) {
    error:
            cast_error(st, dt);
        }
        break;
    }
}

static void gen_assign_cast(CType *dt)
{
    verify_assign_cast(dt);
    gen_cast(dt);
}

ST_FUNC void vstore(void)
{
    int sbt, dbt, ft, r, size, align, bit_size, bit_pos, delayed_cast;

    seqp_record_sv(vtop - 1, SEQP_WRITE);

    if (!atomic_lowering && (vtop->type.t & VT_ATOMIC_BIT) && (vtop->r & VT_LVAL)
        && atomic_store_needs_generic(vtop))
        gen_atomic_load_aggregate();

    ft = vtop[-1].type.t;
    sbt = vtop->type.t & VT_BTYPE;
    dbt = ft & VT_BTYPE;
    verify_assign_cast(&vtop[-1].type);

    if (is_complex_type(&vtop[-1].type) && !is_complex_type(&vtop->type)) {
        gen_cast(&vtop[-1].type);
        sbt = vtop->type.t & VT_BTYPE;
    }
    else if (!is_complex_type(&vtop[-1].type) && is_complex_type(&vtop->type)
             && dbt != VT_STRUCT) {
        gen_cast(&vtop[-1].type);
        sbt = vtop->type.t & VT_BTYPE;
    }
    else if (is_complex_type(&vtop[-1].type) && is_complex_type(&vtop->type)
             && (vtop[-1].type.ref->next->type.t & (VT_BTYPE | VT_LONG))
                != (vtop->type.ref->next->type.t & (VT_BTYPE | VT_LONG))) {
        gen_cast(&vtop[-1].type);
        sbt = vtop->type.t & VT_BTYPE;
    }

    if (sbt == VT_STRUCT) {
        size = type_size(&vtop->type, &align);
        vpushv(vtop - 1);
#ifdef CONFIG_MCC_BCHECK
        if (vtop->r & VT_MUSTBOUND)
            gbound();
#endif
        vtop->type.t = VT_PTR;
        gaddrof();
        vswap();
#ifdef CONFIG_MCC_BCHECK
        if (vtop->r & VT_MUSTBOUND)
            gbound();
#endif
        vtop->type.t = VT_PTR;
        gaddrof();

#ifdef MCC_TARGET_NATIVE_STRUCT_COPY
        if (1
#ifdef CONFIG_MCC_BCHECK
            && !mcc_state->do_bounds_check
#endif
            ) {
            gen_struct_copy(size);
        } else
#endif
        {
            vpushi(size);
#ifdef MCC_ARM_EABI
            if(!(align & 7))
                vpush_helper_func(TOK_memmove8);
            else if(!(align & 3))
                vpush_helper_func(TOK_memmove4);
            else
#endif
            vpush_helper_func(TOK_memmove);
            vrott(4);
            gfunc_call(3);
        }

    } else if (ft & VT_BITFIELD) {

        vdup(), vtop[-1] = vtop[-2];

        bit_pos = BIT_POS(ft);
        bit_size = BIT_SIZE(ft);
        vtop[-1].type.t = ft & ~VT_STRUCT_MASK;

        if (dbt == VT_BOOL) {
            gen_cast(&vtop[-1].type);
            vtop[-1].type.t = (vtop[-1].type.t & ~VT_BTYPE) | (VT_BYTE | VT_UNSIGNED);
        }
        r = adjust_bf(vtop - 1, bit_pos, bit_size);
        if (dbt != VT_BOOL) {
            gen_cast(&vtop[-1].type);
            dbt = vtop[-1].type.t & VT_BTYPE;
        }
        if (r == VT_STRUCT) {
            store_packed_bf(bit_pos, bit_size);
        } else {
            unsigned long long mask = (1ULL << bit_size) - 1;
            if (dbt != VT_BOOL) {
                if (dbt == VT_LLONG)
                    vpushll(mask);
                else
                    vpushi((unsigned)mask);
                gen_op('&');
            }
            vpushi(bit_pos);
            gen_op(TOK_SHL);
            vswap();
            vdup();
            vrott(3);
            if (dbt == VT_LLONG)
                vpushll(~(mask << bit_pos));
            else
                vpushi(~((unsigned)mask << bit_pos));
            gen_op('&');
            gen_op('|');
            vstore();
            vpop();
        }
    } else if (dbt == VT_VOID) {
        --vtop;
    } else {
            delayed_cast = 0;
            if ((dbt == VT_BYTE || dbt == VT_SHORT)
                && is_integer_btype(sbt)
                ) {
                if ((vtop->r & VT_MUSTCAST)
                    && btype_size(dbt) > btype_size(sbt)
                    )
                    force_charshort_cast();
                delayed_cast = 1;
            } else {
                gen_cast(&vtop[-1].type);
            }

#ifdef CONFIG_MCC_BCHECK
            if (vtop[-1].r & VT_MUSTBOUND) {
                vswap();
                gbound();
                vswap();
            }
#endif
            gv(RC_TYPE(dbt));

            if (delayed_cast) {
                vtop->r |= BFVAL(VT_MUSTCAST, (sbt == VT_LLONG) + 1);
                vtop->type.t = ft & VT_TYPE;
            }

            if ((vtop[-1].r & VT_VALMASK) == VT_LLOCAL) {
                SValue sv;
                r = get_reg(RC_INT);
                sv.type.t = VT_PTRDIFF_T;
                sv.r = VT_LOCAL | VT_LVAL;
                sv.c.i = vtop[-1].c.i;
		sv.sym = NULL;
                load(r, &sv);
                vtop[-1].r = r | VT_LVAL;
            }

            r = vtop->r & VT_VALMASK;
            if (USING_TWO_WORDS(dbt)) {
                int load_type = (dbt == VT_QFLOAT) ? VT_DOUBLE : VT_PTRDIFF_T;
                vtop[-1].type.t = load_type;
                store(r, vtop - 1);
                vswap();
                incr_offset(PTR_SIZE);
                vswap();
                store(vtop->r2, vtop - 1);
            } else {
                store(r, vtop - 1);
            }
        vswap();
        vtop--;
    }
}

ST_FUNC void inc(int post, int c)
{
    test_lvalue();
    if (vtop->type.t & VT_ATOMIC_BIT) {
        if (atomic_rmw_size(vtop, '+')) {
            vpushi(c - TOK_MID);
            gen_atomic_rmw('+', !post);
            return;
        }
        if (atomic_cas_size(vtop)) {
            vpushi(c - TOK_MID);
            gen_atomic_cas_rmw('+', !post);
            return;
        }
        mcc_error("'++'/'--' on this '_Atomic' object is not supported "
                  "(only integer/pointer atomics up to a machine word)");
    }
    vdup();
    if (post) {
        gv_dup();
        vrotb(3);
        vrotb(3);
    }
    vpushi(c - TOK_MID);
    gen_op('+');
    vstore();
    if (post)
        vpop();
}

ST_FUNC CString* parse_mult_str (const char *msg)
{
    if (tok != TOK_STR)
        expect(msg);
    cstr_reset(&initstr);
    while (tok == TOK_STR) {
        cstr_cat(&initstr, tokc.str.data, -1);
        next();
    }
    cstr_ccat(&initstr, '\0');
    return &initstr;
}

ST_FUNC int exact_log2p1(int i)
{
  int ret;
  if (!i)
    return 0;
  for (ret = 1; i >= 1 << 8; ret += 8)
    i >>= 8;
  if (i >= 1 << 4)
    ret += 4, i >>= 4;
  if (i >= 1 << 2)
    ret += 2, i >>= 2;
  if (i >= 1 << 1)
    ret++;
  return ret;
}

static void parse_attribute(AttributeDef *ad)
{
    int t, n;
    char *astr;
    AttributeDef ad_tmp;

redo:
    if (tok != TOK_ATTRIBUTE1 && tok != TOK_ATTRIBUTE2)
        return;
    if (NULL == ad)
        ad = &ad_tmp;

    next();
    skip('(');
    skip('(');
    while (tok != ')') {
        if (tok < TOK_IDENT)
            expect("attribute name");
        t = tok;
        next();
        switch(t) {
	case TOK_CLEANUP1:
	case TOK_CLEANUP2:
	{
	    Sym *s;

	    skip('(');
	    s = sym_find(tok);
	    if (!s) {
	        mcc_warning_c(warn_implicit_function_declaration)(
                    "implicit declaration of function '%s'", get_tok_str(tok, &tokc));
	        s = external_global_sym(tok, &func_old_type);
            } else if ((s->type.t & VT_BTYPE) != VT_FUNC)
                mcc_error("'%s' is not declared as function", get_tok_str(tok, &tokc));
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
	    if (!strcmp (astr, "default"))
	        ad->a.visibility = STV_DEFAULT;
	    else if (!strcmp (astr, "hidden"))
	        ad->a.visibility = STV_HIDDEN;
	    else if (!strcmp (astr, "internal"))
	        ad->a.visibility = STV_INTERNAL;
	    else if (!strcmp (astr, "protected"))
	        ad->a.visibility = STV_PROTECTED;
	    else
                expect("visibility(\"default|hidden|internal|protected\")");
            ad->a.visibility_set = 1;
            skip(')');
            break;
        case TOK_ALIGNED1:
        case TOK_ALIGNED2:
            if (tok == '(') {
                next();
                n = expr_const();
                if (n <= 0 || (n & (n - 1)) != 0)
                    mcc_error("alignment must be a positive power of two");
                skip(')');
            } else {
                n = MAX_ALIGN;
            }
            ad->a.aligned = exact_log2p1(n);
	    if (n != 1 << (ad->a.aligned - 1))
	      mcc_error("alignment of %d is larger than implemented", n);
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
                n = 3;
            else if (n < 0)
                n = 0;
            if (n > 0)
                ad->f.func_call = FUNC_FASTCALL1 + n - 1;
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
            switch(tok) {
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
            if (tok == '(') {
                int parenthesis = 0;
                do {
                    if (tok == '(')
                        parenthesis++;
                    else if (tok == ')')
                        parenthesis--;
                    next();
                } while (parenthesis && tok != -1);
            }
            break;
        }
        if (tok != ',')
            break;
        next();
    }
    skip(')');
    skip(')');
    goto redo;
}

static Sym * find_field (CType *type, int v, int *cumofs)
{
    Sym *s = type->ref;
    int v1 = v | SYM_FIELD;
    if (!(v & SYM_FIELD)) {
        if ((type->t & VT_BTYPE) != VT_STRUCT)
            expect("struct or union");
        if (v < TOK_UIDENT)
            expect("field name");
        if (s->c < 0)
            mcc_error("dereferencing incomplete type '%s'",
                get_tok_str(s->v & ~SYM_STRUCT, 0));
    }
    while ((s = s->next) != NULL) {
        if (s->v == v1) {
            *cumofs = s->c;
            return s;
        }
        if ((s->type.t & VT_BTYPE) == VT_STRUCT
          && s->v >= (SYM_FIRST_ANOM | SYM_FIELD)) {
            Sym *ret = find_field (&s->type, v1, cumofs);
            if (ret) {
                *cumofs += s->c;
                return ret;
            }
        }
    }
    if (!(v & SYM_FIELD))
        mcc_error("field not found: %s", get_tok_str(v, NULL));
    return s;
}

static void check_fields (CType *type, int check)
{
    Sym *s = type->ref;

    while ((s = s->next) != NULL) {
        int v = s->v & ~SYM_FIELD;
        if (v < SYM_FIRST_ANOM) {
            TokenSym *ts = table_ident[v - TOK_IDENT];
            if (check && (ts->tok & SYM_FIELD))
                mcc_error("duplicate member '%s'", get_tok_str(v, NULL));
            ts->tok ^= SYM_FIELD;
        } else if ((s->type.t & VT_BTYPE) == VT_STRUCT)
            check_fields (&s->type, check);
    }
}

static void struct_layout(CType *type, AttributeDef *ad)
{
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


    for (f = type->ref->next; f; f = f->next) {
        if (f->type.t & VT_BITFIELD)
            bit_size = BIT_SIZE(f->type.t);
        else
            bit_size = -1;
        size = type_size(&f->type, &align);
        a = f->a.aligned ? 1 << (f->a.aligned - 1) : 0;
        packed = 0;

        if (pcc && bit_size == 0) {

        } else {
            if (pcc && (f->a.packed || ad->a.packed))
                align = packed = 1;

            if (pragma_pack) {
                packed = 1;
                if (pragma_pack < align)
                    align = pragma_pack;
                if (pcc && pragma_pack < a)
                    a = 0;
            }
        }
        if (a)
            align = a;

        if (type->ref->type.t == VT_UNION) {
	    if (pcc && bit_size >= 0)
	        size = (bit_size + 7) >> 3;
	    offset = 0;
	    if (size > c)
	        c = size;

	} else if (bit_size < 0) {
            if (pcc)
                c += (bit_pos + 7) >> 3;
	    c = (c + align - 1) & -align;
	    offset = c;
	    if (size > 0)
	        c += size;
	    bit_pos = 0;
	    prevbt = VT_STRUCT;
	    prev_bit_size = 0;

	} else {
            if (pcc) {
                if (bit_size == 0) {
            new_field:
		    c = (c + ((bit_pos + 7) >> 3) + align - 1) & -align;
		    bit_pos = 0;
                } else if (f->a.aligned) {
                    goto new_field;
                } else if (!packed) {
                    int a8 = align * 8;
	            int ofs = ((c * 8 + bit_pos) % a8 + bit_size + a8 - 1) / a8;
                    if (ofs > size / align)
                        goto new_field;
                }

                if (size == 8 && bit_size <= 32)
                    f->type.t = (f->type.t & ~VT_BTYPE) | VT_INT, size = 4;

                while (bit_pos >= align * 8)
                    c += align, bit_pos -= align * 8;
                offset = c;

		if (f->v & SYM_FIRST_ANOM
                    )
		    align = 1;

	    } else {
		bt = f->type.t & VT_BTYPE;
		if ((bit_pos + bit_size > size * 8)
                    || (bit_size > 0) == (bt != prevbt)
                    ) {
		    c = (c + align - 1) & -align;
		    offset = c;
		    bit_pos = 0;
		    if (bit_size || prev_bit_size)
		        c += size;
		}
		if (bit_size == 0 && prevbt != bt)
		    align = 1;
		prevbt = bt;
                prev_bit_size = bit_size;
	    }

	    f->type.t = (f->type.t & ~(0x3f << VT_STRUCT_SHIFT))
		        | (bit_pos << VT_STRUCT_SHIFT);
	    bit_pos += bit_size;
	}
	if (align > maxalign)
	    maxalign = align;

#ifdef BF_DEBUG
	printf("set field %s offset %-2d size %-2d align %-2d",
	       get_tok_str(f->v & ~SYM_FIELD, NULL), offset, size, align);
	if (f->type.t & VT_BITFIELD) {
	    printf(" pos %-2d bits %-2d",
                    BIT_POS(f->type.t),
                    BIT_SIZE(f->type.t)
                    );
	}
	printf("\n");
#endif

        f->c = offset;
	f->r = 0;
    }

    if (pcc)
        c += (bit_pos + 7) >> 3;

    a = bt = ad->a.aligned ? 1 << (ad->a.aligned - 1) : 1;
    if (a < maxalign)
        a = maxalign;
    type->ref->r = a;
    if (pragma_pack && pragma_pack < maxalign && 0 == pcc) {
        a = pragma_pack;
        if (a < bt)
            a = bt;
    }
    c = (c + a - 1) & -a;
    type->ref->c = c;

    if (ad->a.transp_union) {
        if (!IS_UNION(type->t))
            mcc_warning("'transparent_union' attribute ignored on non-union");
        else
            type->ref->a.transp_union = 1;
    }

#ifdef BF_DEBUG
    printf("struct size %-2d align %-2d\n\n", c, a), fflush(stdout);
#endif

    for (f = type->ref->next; f; f = f->next) {
        int s, px, cx, c0;
        CType t;

        if (0 == (f->type.t & VT_BITFIELD))
            continue;
        f->type.ref = f;
        f->auxtype = -1;
        bit_size = BIT_SIZE(f->type.t);
        if (bit_size == 0)
            continue;
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
        for (;;) {
            px = f->c * 8 + bit_pos;
            cx = (px >> 3) & -align;
            px = px - (cx << 3);
            if (c0 == cx)
                break;
            s = (px + bit_size + 7) >> 3;
            if (s > 4) {
                t.t = VT_LLONG;
            } else if (s > 2) {
                t.t = VT_INT;
            } else if (s > 1) {
                t.t = VT_SHORT;
            } else {
                t.t = VT_BYTE;
            }
            s = type_size(&t, &align);
            c0 = cx;
        }

        if (px + bit_size <= s * 8 && cx + s <= c
#ifdef MCC_TARGET_ARM
            && !(cx & (align - 1))
#endif
            ) {
            f->c = cx;
            bit_pos = px;
	    f->type.t = (f->type.t & ~(0x3f << VT_STRUCT_SHIFT))
		        | (bit_pos << VT_STRUCT_SHIFT);
            if (s != size)
                f->auxtype = t.t;
#ifdef BF_DEBUG
            printf("FIX field %s offset %-2d size %-2d align %-2d "
                "pos %-2d bits %-2d\n",
                get_tok_str(f->v & ~SYM_FIELD, NULL),
                cx, s, align, px, bit_size);
#endif
        } else {
            f->auxtype = VT_STRUCT;
#ifdef BF_DEBUG
            printf("FIX field %s : load byte-wise\n",
                 get_tok_str(f->v & ~SYM_FIELD, NULL));
#endif
        }
    }
}

static int in_range(long long n, int t)
{
    unsigned long long m = (1ULL << (btype_size(t & VT_BTYPE) * 8 - 1)) - 1;
    if (t & VT_UNSIGNED)
        return n <= (m << 1) + 1;
    return n >= -(long long)m - 1 && n <= (long long)m;
}

static void struct_decl(CType *type, int u)
{
    int v, c, size, align, flexible;
    int bit_size, bsize, bt, ut;
    Sym *s, *ss, **ps;
    AttributeDef ad, ad1;
    CType type1, btype;

    memset(&ad, 0, sizeof ad);
    next();
    parse_attribute(&ad);

    v = 0;
    if (tok >= TOK_IDENT)
        v = tok, next();

    bt = ut = 0;
    if (u == VT_ENUM) {
        ut = VT_INT;
        if (tok == ':') {
            next();
            if (!parse_btype(&btype, &ad1, 0)
             || !is_integer_btype(btype.t & VT_BTYPE))
                expect("enum type");
            bt = ut = btype.t & (VT_BTYPE|VT_LONG|VT_UNSIGNED|VT_DEFSIGN);
        }
    }

    if (v) {
        s = struct_find(v);
        if (s && (s->sym_scope == local_scope || (tok != '{' && tok != ';'))) {
            if (u == s->type.t)
                goto do_decl;
            if (u == VT_ENUM && IS_ENUM(s->type.t))
                goto do_decl;
            mcc_error("redeclaration of '%s'", get_tok_str(v, NULL));
        }
    } else {
        if (tok != '{')
            expect("struct/union/enum name");
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
        mcc_pedantic("ISO C forbids forward references to 'enum' types");

    if (tok == '{') {
        next();
        if (s->c != -1
            && !(u == VT_ENUM && s->c == 0))
            mcc_error("struct/union/enum already defined");
        s->c = -2;
        ps = &s->next;
        if (u == VT_ENUM) {
            long long ll = 0, pl = 0, nl = 0;
	    CType t;
            t.ref = s;
            s->sym_scope = local_scope;
            t.t = VT_INT|VT_STATIC|VT_ENUM_VAL;
            if (bt)
                t.t = bt|VT_STATIC|VT_ENUM_VAL;
            for(;;) {
                v = tok;
                if (v < TOK_UIDENT)
                    expect("identifier");
                next();
                if (tok == '=') {
                    next();
                    ice_float_op = ice_nonconst = 0;
		    ll = expr_const64();
                    if (ice_float_op || ice_nonconst)
                        mcc_pedantic("ISO C forbids an enumerator value that is "
                                     "not an integer constant expression");
                }
                if (bt && !in_range(ll, t.t))
		    mcc_error("enumerator '%s' out of range of its type",
			get_tok_str(v, NULL));
                if (!bt && ll != (int64_t)(int)ll)
                    mcc_pedantic("ISO C restricts enumerator values "
                                 "to the range of 'int'");
                ss = sym_push(v, &t, VT_CONST, 0);
                ss->enum_val = ll;
                *ps = ss, ps = &ss->next;
                if (ll < nl)
                    nl = ll;
                if (ll > pl)
                    pl = ll;
                if (tok != ',')
                    break;
                next();
                ll++;
                if (tok == '}')
                    break;
            }
            skip('}');

            if (bt) {
                t.t = bt;
                s->c = 2;
                goto enum_done;
            }

            t.t = VT_INT;
            if (nl >= 0) {
                if (pl != (unsigned)pl)
                    t.t = (LONG_SIZE==8 ? VT_LLONG|VT_LONG : VT_LLONG);
                t.t |= VT_UNSIGNED;
            } else if (pl != (int)pl || nl != (int)nl)
                t.t = (LONG_SIZE==8 ? VT_LLONG|VT_LONG : VT_LLONG);

            if (mcc_state->short_enums && (t.t & VT_BTYPE) == VT_INT) {
                if (t.t & VT_UNSIGNED) {
                    if (pl <= 0xff)         t.t = VT_BYTE | VT_UNSIGNED;
                    else if (pl <= 0xffff)  t.t = VT_SHORT | VT_UNSIGNED;
                } else {
                    if (nl >= -0x80 && pl <= 0x7f)          t.t = VT_BYTE;
                    else if (nl >= -0x8000 && pl <= 0x7fff)  t.t = VT_SHORT;
                }
            }

            for (ss = s->next; ss; ss = ss->next) {
                ll = ss->enum_val;
                if (ll == (int)ll)
                    continue;
                if (t.t & VT_UNSIGNED) {
                    ss->type.t |= VT_UNSIGNED;
                    if (ll == (unsigned)ll)
                        continue;
                }
                ss->type.t = (ss->type.t & ~VT_BTYPE)
                    | (LONG_SIZE==8 ? VT_LLONG|VT_LONG : VT_LLONG);
            }
            s->c = 1;
        enum_done:
            s->type.t = type->t = t.t | VT_ENUM;

        } else {
            c = 0;
            flexible = 0;
            while (tok != '}') {
                if (!parse_btype(&btype, &ad1, 0)) {
                    if (tok == TOK_STATIC_ASSERT) {
                        do_Static_assert();
                        continue;
                    }
		    skip(';');
		    continue;
		}
                while (1) {
		    if (flexible)
		        mcc_error("flexible array member '%s' not at the end of struct",
                              get_tok_str(v, NULL));
                    bit_size = -1;
                    v = 0;
                    type1 = btype;
                    if (tok != ':') {
			if (tok != ';')
                            type_decl(&type1, &ad1, &v, TYPE_DIRECT);
                        if (v == 0) {
                    	    if ((type1.t & VT_BTYPE) != VT_STRUCT) {
                    	        if (tok == ';')
                    	            mcc_warning("declaration does not declare anything");
                    	        else
                    	            expect("identifier");
                    	    } else {
				int v = btype.ref->v;
				if ((v & ~SYM_STRUCT) < SYM_FIRST_ANOM) {
				    if (mcc_state->ms_extensions == 0)
                        		mcc_warning("declaration does not "
						    "declare anything");
				} else if (mcc_state->cversion < 201112)
				    mcc_pedantic("anonymous structs/unions are a "
						 "C11 feature");
                    	    }
                        }
                        if (type_size(&type1, &align) < 0) {
			    if ((u == VT_STRUCT) && (type1.t & VT_ARRAY)) {
			        flexible = 1;
				if (!c)
				    mcc_pedantic("flexible array member in a "
						 "struct with no named members");
			    } else
			        mcc_error("field '%s' has incomplete type",
                                      get_tok_str(v, NULL));
                        }
                        if ((type1.t & VT_BTYPE) == VT_FUNC ||
			    (type1.t & VT_BTYPE) == VT_VOID ||
                            (type1.t & VT_STORAGE))
                            mcc_error("invalid type for '%s'",
                                  get_tok_str(v, NULL));
                        if (type1.t & VT_VLA)
                            mcc_pedantic("ISO C forbids a member with a "
                                         "variably modified type");
                        if (struct_has_flexible_member(&type1))
                            mcc_pedantic("ISO C forbids a member that is a "
                                         "structure with a flexible array member");
                    }
                    if (tok == ':') {
                        next();
                        ice_float_op = ice_nonconst = 0;
                        bit_size = expr_const();
                        if (ice_float_op || ice_nonconst)
                            mcc_pedantic("ISO C forbids a bit-field width that is "
                                         "not an integer constant expression");
                        if (bit_size < 0)
                            mcc_error("negative width in bit-field '%s'",
                                  get_tok_str(v, NULL));
                        if (v && bit_size == 0)
                            mcc_error("zero width for bit-field '%s'",
                                  get_tok_str(v, NULL));
			parse_attribute(&ad1);
                    }
                    if ((ad1.storage_class & 8) && bit_size >= 0)
                        mcc_error("'_Alignas' specified for a bit-field");
                    size = type_size(&type1, &align);
                    if (bit_size >= 0) {
                        bt = type1.t & VT_BTYPE;
                        if (bt != VT_INT &&
                            bt != VT_BYTE &&
                            bt != VT_SHORT &&
                            bt != VT_BOOL &&
                            bt != VT_LLONG)
                            mcc_error("bitfields must have scalar type");
                        bsize = (bt == VT_BOOL) ? 1 : size * 8;
                        if (bit_size > bsize) {
                            mcc_error("width of '%s' exceeds its type",
                                  get_tok_str(v, NULL));
                        } else if (bit_size == bsize && bt != VT_BOOL
				    && !*mcc_state->pack_stack_ptr
                                    && !ad.a.packed && !ad1.a.packed) {
                            ;
                        } else if (bit_size == 64) {
                            mcc_error("field width 64 not implemented");
                        } else {
                            type1.t = (type1.t & ~VT_STRUCT_MASK)
                                | VT_BITFIELD
                                | ((unsigned)bit_size << (VT_STRUCT_SHIFT + 6));
                        }
                    }
                    if (v != 0 || (type1.t & VT_BTYPE) == VT_STRUCT) {
			c = 1;
                    }
                    if (v == 0 &&
			((type1.t & VT_BTYPE) == VT_STRUCT ||
			 bit_size >= 0)) {
		        v = anon_sym++;
		    }
                    if (v) {
                        ss = sym_push(v | SYM_FIELD, &type1, 0, 0);
                        ss->a = ad1.a;
                        *ps = ss;
                        ps = &ss->next;
                    }
                    if (tok == ';' || tok == TOK_EOF)
                        break;
                    skip(',');
                }
                skip(';');
            }
            skip('}');
            if (!c)
                mcc_pedantic(u == VT_UNION
                    ? "ISO C forbids a union with no named members"
                    : "ISO C forbids a struct with no named members");
	    parse_attribute(&ad);
            if (ad.cleanup_func) {
                mcc_warning("attribute '__cleanup__' ignored on type");
            }
	    check_fields(type, 1);
	    check_fields(type, 0);
            struct_layout(type, &ad);
        }
        if (debug_modes)
            mcc_debug_fix_forw(mcc_state, type);
    }
}

static void sym_to_attr(AttributeDef *ad, Sym *s)
{
    merge_symattr(&ad->a, &s->a);
    merge_funcattr(&ad->f, &s->f);
}

static void parse_btype_qualify(CType *type, int qualifiers)
{
    while (type->t & VT_ARRAY) {
        type->ref = sym_push(SYM_FIELD, &type->ref->type, 0, type->ref->c);
        type = &type->ref->type;
    }
    type->t |= qualifiers;
}

static void mk_complex_type(CType *type, CType *base)
{
    static int re_tok, im_tok;
    static CType cache[4];
    int idx, bt = base->t & VT_BTYPE;
    Sym *s, *f0, *f1;
    AttributeDef ad;

    idx = bt == VT_FLOAT ? 0
        : bt == VT_DOUBLE ? ((base->t & VT_LONG) ? 3 : 1)
        : 2;
    if (cache[idx].ref) {
        *type = cache[idx];
        return;
    }
    if (!re_tok) {
        re_tok = tok_alloc_const("__real");
        im_tok = tok_alloc_const("__imag");
    }
    s = sym_push2(&global_stack, anon_sym++ | SYM_STRUCT, VT_STRUCT, -1);
    s->r = 0;
    s->a.is_complex = 1;
    f0 = sym_push2(&global_stack, re_tok | SYM_FIELD, base->t, 0);
    f0->type.ref = base->ref;
    f1 = sym_push2(&global_stack, im_tok | SYM_FIELD, base->t, 0);
    f1->type.ref = base->ref;
    s->next = f0, f0->next = f1, f1->next = NULL;
    type->t = VT_STRUCT;
    type->ref = s;
    memset(&ad, 0, sizeof ad);
    struct_layout(type, &ad);
    cache[idx] = *type;
}

static void complex_part(int imag)
{
    Sym *fre = vtop->type.ref->next;
    CType base = fre->type;
    int ofs = imag ? fre->next->c : fre->c;

    test_lvalue();
    gaddrof();
    vtop->type = char_pointer_type;
    vpushi(ofs);
    gen_op('+');
    vtop->type = base;
    vtop->r |= VT_LVAL;
}

static void cplx_local(CType *cplx, SValue *out)
{
    int align, size = type_size(cplx, &align);
    loc = (loc - size) & -align;
    out->type = *cplx;
    out->r = VT_LOCAL | VT_LVAL;
    out->r2 = VT_CONST;
    out->c.i = loc;
    out->sym = NULL;
}

static void cplx_push_part(SValue *sv, int imag)
{
    vpushv(sv);
    complex_part(imag);
    gv(RC_TYPE(vtop->type.t));
}

static void cplx_store_part(SValue *dst, int imag)
{
    vpushv(dst);
    complex_part(imag);
    vswap();
    vstore();
    vpop();
}

static void cplx_materialize(CType *cplx, CType *base, SValue *out)
{
    cplx_local(cplx, out);
    if (is_complex_type(&vtop->type)) {
        vpushv(out);
        vswap();
        vstore();
        vpop();
    } else {
        gen_cast(base);
        cplx_store_part(out, 0);
        vpushi(0);
        gen_cast(base);
        cplx_store_part(out, 1);
    }
}

static void gen_complex_call(int op, CType *cplx, CType *base, SValue *a, SValue *b)
{
    char buf[16];
    int bt = base->t & VT_BTYPE;
    char suf = bt == VT_FLOAT ? 'f' : bt == VT_LDOUBLE ? 'l' : 0;
    Sym *fsym, *p, *prev;
    CType functype, ptype;
    SValue r;
    int i;

    cplx_local(cplx, &r);

    if (suf)
        snprintf(buf, sizeof buf, "__mcc_c%s%c", op == '*' ? "mul" : "div", suf);
    else
        snprintf(buf, sizeof buf, "__mcc_c%s", op == '*' ? "mul" : "div");

    ptype = *base;
    mk_pointer(&ptype);
    fsym = sym_push2(&global_stack, SYM_FIELD, VT_VOID, 0);
    fsym->type.ref = NULL;
    fsym->f.func_call = FUNC_CDECL;
    fsym->f.func_type = FUNC_NEW;
    fsym->f.func_args = 5;
    prev = NULL;
    for (i = 0; i < 4; i++) {
        p = sym_push2(&global_stack, SYM_FIELD, base->t, 0);
        p->type.ref = base->ref;
        p->next = prev;
        prev = p;
    }
    p = sym_push2(&global_stack, SYM_FIELD, ptype.t, 0);
    p->type.ref = ptype.ref;
    p->next = prev;
    fsym->next = p;
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

static int cplx_extract_const(SValue *sv, SValue *re, SValue *im)
{
    if (is_complex_type(&sv->type)) {
        CType sb = sv->type.ref->next->type;
        int sbt = sb.t & VT_BTYPE, bsz, al;
        Section *ssec;
        ElfSym *esym;
        unsigned char *p;
        if (sbt != VT_FLOAT && sbt != VT_DOUBLE)
            return 0;
        if ((sv->r & (VT_SYM | VT_CONST | VT_LVAL)) != (VT_SYM | VT_CONST | VT_LVAL)
            || !sv->sym || sv->sym->v < SYM_FIRST_ANOM)
            return 0;
        esym = elfsym(sv->sym);
        if (!esym || esym->st_shndx == SHN_UNDEF)
            return 0;
        ssec = mcc_state->sections[esym->st_shndx];
        if (!ssec || !ssec->data || ssec->reloc)
            return 0;
        bsz = type_size(&sb, &al);
        p = ssec->data + esym->st_value + (unsigned)sv->c.i;
        memset(re, 0, sizeof *re);
        memset(im, 0, sizeof *im);
        re->type = im->type = sb;
        re->r = im->r = VT_CONST;
        if (sbt == VT_FLOAT) {
            float fr, fi;
            memcpy(&fr, p, 4); memcpy(&fi, p + bsz, 4);
            re->c.f = fr; im->c.f = fi;
        } else {
            double dr, di;
            memcpy(&dr, p, 8); memcpy(&di, p + bsz, 8);
            re->c.d = dr; im->c.d = di;
        }
        return 1;
    }
    if ((sv->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST
        && (is_float(sv->type.t) || is_integer_btype(sv->type.t & VT_BTYPE))) {
        *re = *sv;
        memset(im, 0, sizeof *im);
        im->type = sv->type;
        im->r = VT_CONST;
        return 1;
    }
    return 0;
}

static void cplx_push_cst(SValue *v, CType *base)
{
    vpushv(v);
    gen_cast(base);
}

static int float_rank(int t)
{
    int bt = t & VT_BTYPE;
    if (bt == VT_LDOUBLE) return 3;
    if (bt == VT_DOUBLE)  return (t & VT_LONG) ? 3 : 2;
    if (bt == VT_FLOAT)   return 1;
    return 0;
}

static void gen_complex_op(int op)
{
    SValue a, b, r;
    CType cplx, base;

    cplx = is_complex_type(&vtop[-1].type) ? vtop[-1].type : vtop[0].type;
    base = cplx.ref->next->type;

    {
        CType *r0 = NULL, *r1 = NULL, *wb = NULL;
        int k0, k1, kc;
        if (is_complex_type(&vtop[-1].type))
            r0 = &vtop[-1].type.ref->next->type;
        else if (is_float(vtop[-1].type.t))
            r0 = &vtop[-1].type;
        if (is_complex_type(&vtop[0].type))
            r1 = &vtop[0].type.ref->next->type;
        else if (is_float(vtop[0].type.t))
            r1 = &vtop[0].type;
        kc = float_rank(base.t);
        k0 = r0 ? float_rank(r0->t) : 0;
        k1 = r1 ? float_rank(r1->t) : 0;
        if (k0 >= k1 && k0 > kc) wb = r0;
        else if (k1 > k0 && k1 > kc) wb = r1;
        if (wb) {
            mk_complex_type(&cplx, wb);
            base = cplx.ref->next->type;
        }
    }

    {
        SValue are, aim, bre, bim;
        int ebt = base.t & VT_BTYPE;
        if ((ebt == VT_FLOAT || ebt == VT_DOUBLE)
            && CONST_WANTED
            && (op == '+' || op == '-' || op == '*' || op == '/')
            && cplx_extract_const(&vtop[-1], &are, &aim)
            && cplx_extract_const(&vtop[0], &bre, &bim)) {
            init_params pp = { .sec = rodata_section };
            unsigned long offset;
            int bsz, bal, csz, cal;
            bsz = type_size(&base, &bal);
            csz = type_size(&cplx, &cal);
            if (NODATA_WANTED) { csz = 0; cal = 1; }
            offset = section_add(pp.sec, csz, cal);
            switch (op) {
            case '+': case '-':
                cplx_push_cst(&are, &base); cplx_push_cst(&bre, &base); gen_op(op);
                break;
            case '*':
                cplx_push_cst(&are, &base); cplx_push_cst(&bre, &base); gen_op('*');
                cplx_push_cst(&aim, &base); cplx_push_cst(&bim, &base); gen_op('*');
                gen_op('-');
                break;
            default:
                cplx_push_cst(&are, &base); cplx_push_cst(&bre, &base); gen_op('*');
                cplx_push_cst(&aim, &base); cplx_push_cst(&bim, &base); gen_op('*');
                gen_op('+');
                cplx_push_cst(&bre, &base); cplx_push_cst(&bre, &base); gen_op('*');
                cplx_push_cst(&bim, &base); cplx_push_cst(&bim, &base); gen_op('*');
                gen_op('+');
                gen_op('/');
                break;
            }
            init_putv(&pp, &base, offset);
            switch (op) {
            case '+': case '-':
                cplx_push_cst(&aim, &base); cplx_push_cst(&bim, &base); gen_op(op);
                break;
            case '*':
                cplx_push_cst(&are, &base); cplx_push_cst(&bim, &base); gen_op('*');
                cplx_push_cst(&aim, &base); cplx_push_cst(&bre, &base); gen_op('*');
                gen_op('+');
                break;
            default:
                cplx_push_cst(&aim, &base); cplx_push_cst(&bre, &base); gen_op('*');
                cplx_push_cst(&are, &base); cplx_push_cst(&bim, &base); gen_op('*');
                gen_op('-');
                cplx_push_cst(&bre, &base); cplx_push_cst(&bre, &base); gen_op('*');
                cplx_push_cst(&bim, &base); cplx_push_cst(&bim, &base); gen_op('*');
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

    if (op == TOK_EQ || op == TOK_NE) {
        cplx_push_part(&a, 0); cplx_push_part(&b, 0); gen_op(TOK_EQ);
        cplx_push_part(&a, 1); cplx_push_part(&b, 1); gen_op(TOK_EQ);
        gen_op('&');
        if (op == TOK_NE) { vpushi(0); gen_op(TOK_EQ); }
        return;
    }

    if ((op == '*' || op == '/') &&
        !(mcc_state->cx_limited_range || stdc_cx_limited(mcc_state))) {
        gen_complex_call(op, &cplx, &base, &a, &b);
        return;
    }

    cplx_local(&cplx, &r);
    if (op == '+' || op == '-') {
        for (int k = 0; k < 2; k++) {
            cplx_push_part(&a, k); cplx_push_part(&b, k); gen_op(op);
            cplx_store_part(&r, k);
        }
    } else if (op == '*') {
        cplx_push_part(&a, 0); cplx_push_part(&b, 0); gen_op('*');
        cplx_push_part(&a, 1); cplx_push_part(&b, 1); gen_op('*');
        gen_op('-'); cplx_store_part(&r, 0);
        cplx_push_part(&a, 0); cplx_push_part(&b, 1); gen_op('*');
        cplx_push_part(&a, 1); cplx_push_part(&b, 0); gen_op('*');
        gen_op('+'); cplx_store_part(&r, 1);
    } else if (op == '/') {
        cplx_push_part(&a, 0); cplx_push_part(&b, 0); gen_op('*');
        cplx_push_part(&a, 1); cplx_push_part(&b, 1); gen_op('*'); gen_op('+');
        cplx_push_part(&b, 0); cplx_push_part(&b, 0); gen_op('*');
        cplx_push_part(&b, 1); cplx_push_part(&b, 1); gen_op('*'); gen_op('+');
        gen_op('/'); cplx_store_part(&r, 0);
        cplx_push_part(&a, 1); cplx_push_part(&b, 0); gen_op('*');
        cplx_push_part(&a, 0); cplx_push_part(&b, 1); gen_op('*'); gen_op('-');
        cplx_push_part(&b, 0); cplx_push_part(&b, 0); gen_op('*');
        cplx_push_part(&b, 1); cplx_push_part(&b, 1); gen_op('*'); gen_op('+');
        gen_op('/'); cplx_store_part(&r, 1);
    } else {
        mcc_error("invalid operation on complex operands");
    }
    vpushv(&r);
}

static void gen_complex_cast(CType *dt)
{
    SValue src, r;

    {
        SValue re, im;
        if (cplx_extract_const(vtop, &re, &im)) {
            if (is_complex_type(dt)) {
                CType dbase = dt->ref->next->type;
                init_params pp = { .sec = rodata_section };
                unsigned long offset;
                int bsz, bal, csz, cal;
                bsz = type_size(&dbase, &bal);
                csz = type_size(dt, &cal);
                if (NODATA_WANTED) { csz = 0; cal = 1; }
                offset = section_add(pp.sec, csz, cal);
                cplx_push_cst(&re, &dbase); init_putv(&pp, &dbase, offset);
                cplx_push_cst(&im, &dbase); init_putv(&pp, &dbase, offset + bsz);
                vtop--;
                vpush_ref(dt, pp.sec, offset, csz);
                vtop->r |= VT_LVAL;
            } else {
                vtop--;
                cplx_push_cst(&re, dt);
            }
            return;
        }
    }

    if (is_complex_type(&vtop->type)) {
        CType sbase = vtop->type.ref->next->type;
        cplx_materialize(&vtop->type, &sbase, &src);
        if (is_complex_type(dt)) {
            CType dbase = dt->ref->next->type;
            cplx_local(dt, &r);
            cplx_push_part(&src, 0); gen_cast(&dbase); cplx_store_part(&r, 0);
            cplx_push_part(&src, 1); gen_cast(&dbase); cplx_store_part(&r, 1);
            vpushv(&r);
        } else {
            vpushv(&src);
            complex_part(0);
            gen_cast(dt);
        }
    } else {
        CType dbase = dt->ref->next->type;
        cplx_local(dt, &r);
        gen_cast(&dbase);
        cplx_store_part(&r, 0);
        vpushi(0); gen_cast(&dbase);
        cplx_store_part(&r, 1);
        vpushv(&r);
    }
}

static int parse_btype(CType *type, AttributeDef *ad, int ignore_label)
{
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

    while(1) {
        switch(tok) {
        case TOK_EXTENSION:
            next();
            continue;

        case TOK_CHAR:
            u = VT_BYTE;
        basic_type:
            next();
        basic_type1:
            if (u == VT_SHORT || u == VT_LONG) {
                if (st != -1 || (bt != -1 && bt != VT_INT))
                    tmbt: mcc_error("too many basic types");
                st = u;
            } else {
                if (bt != -1 || (st != -1 && u != VT_INT))
                    goto tmbt;
                if ((t & VT_DEFSIGN) && (u == VT_VOID || u > VT_LLONG))
                    goto tmbt;
                bt = u;
            }
            if (u != VT_INT)
                t = (t & ~(VT_BTYPE|VT_LONG)) | u;
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
        case TOK_ALIGNAS:
            { int n;
              AttributeDef ad1;
              next();
              skip('(');
              memset(&ad1, 0, sizeof(AttributeDef));
              if (parse_btype(&type1, &ad1, 0)) {
                  type_decl(&type1, &ad1, &n, TYPE_ABSTRACT);
                  if (ad1.a.aligned)
                    n = 1 << (ad1.a.aligned - 1);
                  else
                    type_size(&type1, &n);
              } else {
                  n = expr_const();
                  if (n < 0 || (n & (n - 1)) != 0)
                    mcc_error("alignment must be a positive power of two");
              }
              skip(')');
              ad->a.aligned = exact_log2p1(n);
              ad->storage_class |= 8;
            }
            continue;
        case TOK_LONG:
            if ((t & VT_BTYPE) == VT_DOUBLE) {
                t = (t & ~(VT_BTYPE|VT_LONG)) | VT_LDOUBLE;
            } else if ((t & (VT_BTYPE|VT_LONG)) == VT_LONG) {
                t = (t & ~(VT_BTYPE|VT_LONG)) | VT_LLONG;
            } else {
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
            if ((t & (VT_BTYPE|VT_LONG)) == VT_LONG) {
                t = (t & ~(VT_BTYPE|VT_LONG)) | VT_LDOUBLE;
            } else {
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
                mcc_pedantic("'_Atomic' is a C11 feature");
            next();
            type->t = t;
            parse_btype_qualify(type, VT_ATOMIC);
            t = type->t;
            if (tok == '(') {
                parse_expr_type(&type1);
                if (type1.t & VT_ARRAY)
                    mcc_error("_Atomic cannot be applied to an array type");
                if ((type1.t & VT_BTYPE) == VT_FUNC)
                    mcc_error("_Atomic cannot be applied to a function type");
                if (type1.t & VT_ATOMIC_BIT)
                    mcc_error("_Atomic cannot be applied to an atomic type");
                else if (type1.t & (VT_CONSTANT | VT_VOLATILE))
                    mcc_error("_Atomic cannot be applied to a qualified type");
                type1.t &= ~(VT_STORAGE&~VT_TYPEDEF);
                if (type1.ref)
                    sym_to_attr(ad, type1.ref);
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
            if ((t & (VT_DEFSIGN|VT_UNSIGNED)) == (VT_DEFSIGN|VT_UNSIGNED))
                mcc_error("signed and unsigned modifier");
            t |= VT_DEFSIGN;
            next();
            typespec_found = 1;
            break;
        case TOK_AUTO:
        case TOK_REGISTER:
            if ((t & (VT_EXTERN|VT_STATIC|VT_TYPEDEF)) || (ad->storage_class & 3))
                mcc_error("multiple storage classes");
            ad->storage_class |= (tok == TOK_AUTO) ? 1 : 2;
            next();
            break;
        case TOK_RESTRICT1:
        case TOK_RESTRICT2:
        case TOK_RESTRICT3:
            ad->storage_class |= 4;
            next();
            break;
        case TOK_UNSIGNED:
            if ((t & (VT_DEFSIGN|VT_UNSIGNED)) == VT_DEFSIGN)
                mcc_error("signed and unsigned modifier");
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
            if ((t & (VT_EXTERN|VT_STATIC|VT_TYPEDEF) & ~g) || (ad->storage_class & 3))
                mcc_error("multiple storage classes");
            t |= g;
            next();
            break;
        case TOK_INLINE1:
        case TOK_INLINE2:
        case TOK_INLINE3:
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
            if (ad->attr_mode) {
                u = ad->attr_mode -1;
                t = (t & ~(VT_BTYPE|VT_LONG)) | u;
            }
            continue;
        case TOK_TYPEOF1:
        case TOK_TYPEOF2:
        case TOK_TYPEOF3:
            next();
            parse_expr_type(&type1);
            type1.t &= ~(VT_STORAGE&~VT_TYPEDEF);
            if (type1.ref) {
                sym_to_attr(ad, type1.ref);
                if (type1.t & VT_ARRAY)
                    type1.t |= VT_BT_ARRAY;
            }
            goto basic_type2;
        case TOK_THREAD_LOCAL:
        case TOK___thread:
            if (tok == TOK_THREAD_LOCAL && mcc_state->cversion < 201112)
                mcc_pedantic("'_Thread_local' is a C11 feature");
            if (t & VT_TLS)
                mcc_error("multiple thread-local storage specifiers");
            t |= VT_TLS;
            next();
            break;
        default:
            if (typespec_found)
                goto the_end;
            s = sym_find(tok);
            if (!s || !(s->type.t & VT_TYPEDEF))
                goto the_end;

            n = tok, next();
            if (tok == ':' && ignore_label) {
                unget_tok(n);
                goto the_end;
            }

            t &= ~(VT_BTYPE|VT_LONG);
            u = t & ~VT_QUALIFY, t ^= u;
            type->t = (s->type.t & ~VT_TYPEDEF) | u;
            type->ref = s->type.ref;
            if (t)
                parse_btype_qualify(type, t);
            t = type->t;
            if (t & VT_ARRAY)
                t |= VT_BT_ARRAY;
            sym_to_attr(ad, s);
            typespec_found = 1;
            st = bt = -2;
            break;
        }
        type_found = 1;
    }
the_end:
    if (type_found && !typespec_found)
        ad->implicit_int = 1;
    if (mcc_state->char_is_unsigned) {
        if ((t & (VT_DEFSIGN|VT_BTYPE)) == VT_BYTE)
            t |= VT_UNSIGNED;
    }
    bt = t & (VT_BTYPE|VT_LONG);
    if (bt == VT_LONG)
        t |= LONG_SIZE == 8 ? VT_LLONG : VT_INT;
#ifdef MCC_USING_DOUBLE_FOR_LDOUBLE
    if (bt == VT_LDOUBLE)
        t = (t & ~(VT_BTYPE|VT_LONG)) | (VT_DOUBLE|VT_LONG);
#endif
    if (complex_seen) {
        CType base;
        base.t = t & (VT_BTYPE | VT_LONG);
        base.ref = NULL;
        if (!is_float(base.t))
            mcc_error("_Complex requires a floating-point type");
        mk_complex_type(type, &base);
        type->t |= t & (VT_CONSTANT | VT_VOLATILE | VT_ATOMIC_BIT | VT_DEFSIGN | VT_EXTERN
                        | VT_STATIC | VT_TYPEDEF | VT_INLINE);
        return type_found;
    }
    type->t = t;
    if (ad->storage_class & 16) {
        if (t & VT_ARRAY)
            mcc_error("_Atomic cannot be applied to an array type");
        if ((t & VT_BTYPE) == VT_FUNC)
            mcc_error("_Atomic cannot be applied to a function type");
    }
    return type_found;
}

static inline void convert_parameter_type(CType *pt)
{
    pt->t &= ~VT_QUALIFY;
    pt->t &= ~(VT_ARRAY | VT_VLA);
    if ((pt->t & VT_BTYPE) == VT_FUNC) {
        mk_pointer(pt);
    }
}

ST_FUNC CString* parse_asm_str(void)
{
    skip('(');
    return parse_mult_str("string constant");
}

static int asm_label_instr(void)
{
    int v;
    char *astr;

    next();
    astr = parse_asm_str()->data;
    skip(')');
#ifdef ASM_DEBUG
    printf("asm_alias: \"%s\"\n", astr);
#endif
    v = tok_alloc_const(astr);
    return v;
}

static int post_type(CType *type, AttributeDef *ad, int storage, int td)
{
    int n, l, t1, arg_size, align;
    int star_param = 0;
    Sym **plast, *s, *first, **ps, *sr;
    AttributeDef ad1;
    CType pt;
    TokenString *vla_array_tok = NULL;
    int *vla_array_str = NULL;

    if (tok == '(') {
        next();
	if (TYPE_DIRECT == (td & (TYPE_DIRECT|TYPE_ABSTRACT)))
	  return 0;

        ps = local_stack ? &local_stack : &global_stack;
        ++local_scope;
        sr = sym_push2(ps, SYM_FIELD, 0, 0);

	if (tok == ')')
	  l = 0;
	else if (parse_btype(&pt, &ad1, 0))
	  l = FUNC_NEW;
	else if (td & (TYPE_DIRECT|TYPE_ABSTRACT)) {
            sym_pop(ps, sr->prev, 0);
            --local_scope;
	    merge_attr (ad, &ad1);
	    return 0;
	} else
	  l = FUNC_OLD;

        first = NULL;
        plast = &first;
        arg_size = 0;
        if (l) {
            for(;;) {
                if (l != FUNC_OLD) {
                    if ((pt.t & VT_BTYPE) == VT_VOID && tok == ')') {
                        if (pt.t & (VT_CONSTANT | VT_VOLATILE))
                            mcc_error("'void' as only parameter may not be qualified");
                        break;
                    }
                    type_decl(&pt, &ad1, &n, TYPE_DIRECT | TYPE_ABSTRACT | TYPE_PARAM);
                    if ((pt.t & VT_BTYPE) == VT_VOID)
                        mcc_error("parameter declared as void");
                    if ((pt.t & (VT_STATIC | VT_EXTERN | VT_TYPEDEF | VT_TLS))
                        || (ad1.storage_class & 1))
                        mcc_error("storage class specified for parameter");
                    if (ad1.storage_class & 8)
                        mcc_error("'_Alignas' specified for a function parameter");
                    if (ad1.storage_class & 32)
                        star_param = 1;
                    if ((ad1.storage_class & 64) && !(pt.t & (VT_ARRAY | VT_VLA)))
                        mcc_error("'static' or type qualifiers used in non-outermost array declarator");
                    if (n == 0 || (td & TYPE_PARAM))
                        n |= SYM_FIELD;
                } else {
                    n = tok;
                    pt.t = VT_INT | VT_EXTERN;
                    pt.ref = NULL;
                    next();
                }
                if (n < TOK_UIDENT)
                    expect("identifier");
                convert_parameter_type(&pt);
                arg_size += (type_size(&pt, &align) + PTR_SIZE - 1) / PTR_SIZE;
                s = sym_push(n, &pt, VT_LOCAL|VT_LVAL, 0);
                s->vla_inner_id = file->line_num;
                s->a.inited = 1;
                if (ad1.storage_class & 2)
                    s->a.is_register = 1;
                *plast = s;
                plast = &s->next;
                if (tok == ')')
                    break;
                skip(',');
                if (l == FUNC_NEW && tok == TOK_DOTS) {
                    l = FUNC_ELLIPSIS;
                    next();
                    break;
                }
		if (l == FUNC_NEW && !parse_btype(&pt, &ad1, 0))
		    mcc_error("invalid type");
            }
        } else
            l = FUNC_OLD;
        if (l == FUNC_OLD && first == NULL)
            mcc_warning_c(warn_strict_prototypes)(
                "function declaration isn't a prototype");
        skip(')');
        type->t &= ~VT_CONSTANT;
        if (tok == '[') {
            next();
            skip(']');
            mk_pointer(type);
        }
        ad->f.func_args = arg_size;
        ad->f.func_type = l;
        ad->f.func_star_param = star_param;
        if ((type->t & VT_BTYPE) == VT_FUNC)
            mcc_error("function cannot return a function type");
        if (type->t & VT_ARRAY)
            mcc_error("function cannot return an array type");
        sr->type = *type, s = sr;
        s->a = ad->a;
        s->f = ad->f;
        s->next = first;
        type->t = VT_FUNC;
        type->ref = s;
        sym_pop(ps, sr, 1);
        --local_scope;

    } else if (tok == '[') {
	int saved_nocode_wanted = nocode_wanted;
	int saw_static = 0;
        next();
        n = -1;
        t1 = 0;
        if (td & TYPE_PARAM) while (1) {
	    switch (tok) {
	    case TOK_RESTRICT1: case TOK_RESTRICT2: case TOK_RESTRICT3:
	    case TOK_CONST1:
	    case TOK_VOLATILE1:
	    case TOK_STATIC:
		if (td & TYPE_NEST)
		    mcc_error("'static' or type qualifiers used in non-outermost array declarator");
		if (tok == TOK_STATIC)
		    saw_static = 1;
		ad->storage_class |= 64;
		next();
		continue;
	    case '*':
		next();
		if (tok == ']')
		    ad->storage_class |= 32;
		continue;
	    default:
		break;
	    }
            if (tok != ']') {
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
                mcc_error("'static' may not be used without an array size");
            break;

	} else if (tok != ']') {
            ice_float_op = 0;
            if (!local_stack || (storage & VT_STATIC))
                vpushi(expr_const());
            else {
		nocode_wanted = 0;
		gexpr();
	    }
check:
            if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
                n = vtop->c.i;
                if (n < 0)
                    mcc_error("invalid array size");
                else if (n == 0)
                    mcc_pedantic("ISO C forbids zero-size array");
                if (ice_float_op)
                    mcc_pedantic("ISO C forbids an array size that is not an "
                                 "integer constant expression");
            } else {
                if (!is_integer_btype(vtop->type.t & VT_BTYPE))
                    mcc_error("size of variable length array should be an integer");
                n = 0;
                t1 = VT_VLA;
                mcc_warning_c(warn_vla)("ISO C90 forbids variable length array");
            }
        }
        skip(']');
        post_type(type, ad, storage, (td & ~(TYPE_DIRECT|TYPE_ABSTRACT)) | TYPE_NEST);

        if ((type->t & VT_BTYPE) == VT_FUNC)
            mcc_error("declaration of an array of functions");
        if ((type->t & VT_BTYPE) == VT_VOID
            || type_size(type, &align) < 0)
            mcc_error("declaration of an array of incomplete type elements");
        if (struct_has_flexible_member(type))
            mcc_pedantic("ISO C forbids an array of a structure with a "
                         "flexible array member");

        t1 |= type->t & VT_VLA;

        if (t1 & VT_VLA) {
            if (n < 0) {
		if  (td & TYPE_NEST)
                    mcc_error("need explicit inner array size in VLAs");
	    }
	    else {
                loc -= type_size(&int_type, &align);
                loc &= -align;
                n = loc;

                vpush_type_size(type, &align);
                gen_op('*');
                vset(&int_type, VT_LOCAL|VT_LVAL, n);
                vswap();
                vstore();
	    }
        }
        if (n != -1)
            vpop();
	nocode_wanted = saved_nocode_wanted;

        s = sym_push(SYM_FIELD, type, 0, n);
        type->t = (t1 ? VT_VLA : VT_ARRAY) | VT_PTR;
        type->ref = s;

        if (vla_array_str) {
	    if ((t1 & VT_VLA) && (td & TYPE_NEST))
	        s->vla_array_str = vla_array_str;
	    else
	        tok_str_free_str(vla_array_str);
	}
    }
    return 1;
}

static Sym *restrict_ptr_pointee[8];
static int  nb_restrict_ptr;
static int  type_decl_depth;

static CType *type_decl(CType *type, AttributeDef *ad, int *v, int td)
{
    CType *post, *ret;
    int qualifiers, restrict_q, storage, arr_nested = 0;

    if (type_decl_depth++ == 0)
        nb_restrict_ptr = 0;

    storage = type->t & VT_STORAGE;
    type->t &= ~VT_STORAGE;
    post = ret = type;

    while (tok == '*') {
        qualifiers = 0;
        restrict_q = 0;
    redo:
        next();
        switch(tok) {
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
            restrict_q = 1;
            goto redo;
	case TOK_ATTRIBUTE1:
	case TOK_ATTRIBUTE2:
	    parse_attribute(ad);
	    break;
        }
        mk_pointer(type);
        type->t |= qualifiers;
        if (restrict_q && nb_restrict_ptr < 8)
            restrict_ptr_pointee[nb_restrict_ptr++] = type->ref;
	if (ret == type)
	    ret = pointed_type(type);
    }

    if (tok == '(') {
	if (!post_type(type, ad, 0, td)) {
	    parse_attribute(ad);
	    post = type_decl(type, ad, v, td);
	    skip(')');
	    if (post != ret)
		arr_nested = 1;
	} else
	  goto abstract;
    } else if (tok >= TOK_IDENT && (td & TYPE_DIRECT)) {
	*v = tok;
	next();
    } else {
  abstract:
	if (!(td & TYPE_ABSTRACT))
	  expect("identifier");
	*v = 0;
    }
    post_type(post, ad, post != ret ? 0 : storage,
              (td & ~(TYPE_DIRECT|TYPE_ABSTRACT)) | (arr_nested ? TYPE_NEST : 0));
    parse_attribute(ad);
    type->t |= storage;
    if (--type_decl_depth == 0) {
        int bad = 0;
        while (nb_restrict_ptr)
            if ((restrict_ptr_pointee[--nb_restrict_ptr]->type.t & VT_BTYPE)
                == VT_FUNC)
                bad = 1;
        if (bad)
            mcc_error("pointer to function type may not be "
                      "'restrict'-qualified");
    }
    return ret;
}

ST_FUNC void indir(void)
{
    if ((vtop->type.t & VT_BTYPE) != VT_PTR) {
        if ((vtop->type.t & VT_BTYPE) == VT_FUNC)
            return;
        expect("pointer");
    }
    if (vtop->r & VT_LVAL)
        gv(RC_INT);
    vtop->type = *pointed_type(&vtop->type);
    if ((vtop->type.t & VT_BTYPE) == VT_VOID && !nocode_wanted)
        mcc_pedantic("dereferencing a 'void *' pointer");
    if (!(vtop->type.t & (VT_ARRAY | VT_VLA))
        && (vtop->type.t & VT_BTYPE) != VT_FUNC) {
        vtop->r |= VT_LVAL;
#ifdef CONFIG_MCC_BCHECK
        if (mcc_state->do_bounds_check)
            vtop->r |= VT_MUSTBOUND;
#endif
    }
}

#if defined MCC_TARGET_RISCV64 || defined MCC_TARGET_ARM64 \
    || (defined MCC_TARGET_X86_64 && defined MCC_TARGET_PE)
static void check_va_start_register(void)
{
    if (vtop->sym && vtop->sym->a.is_register)
        mcc_warning("undefined behavior when the second parameter of 'va_start' "
                    "is declared with 'register' storage");
}

static void check_va_start_last_param(void)
{
    if ((mcc_state->warn_varargs & WARN_ON) && cur_func_last_param
        && vtop->sym && (vtop->sym->v & ~SYM_FIELD) != cur_func_last_param)
        mcc_warning_c(warn_varargs)("second argument to 'va_start' is not the "
                                    "last named parameter");
}
#endif

static Sym *transparent_union_member(CType *type)
{
    Sym *m;
    CType *st = &vtop->type;

    if ((type->t & VT_BTYPE) != VT_STRUCT || type->ref->c < 0
        || !type->ref->a.transp_union)
        return NULL;
    if ((st->t & VT_BTYPE) == VT_STRUCT
        && is_compatible_unqualified_types(type, st))
        return NULL;
    for (m = type->ref->next; m; m = m->next) {
        if (is_compatible_unqualified_types(&m->type, st))
            return m;
        if ((m->type.t & VT_BTYPE) == VT_PTR
            && ((st->t & VT_BTYPE) == VT_PTR || is_null_pointer(vtop)))
            return m;
    }
    return NULL;
}

static void gfunc_param_typed(Sym *func, Sym *arg)
{
    int func_type;
    Sym *tu;
    CType type;

    func_type = func->f.func_type;
    if (func_type == FUNC_OLD ||
        (func_type == FUNC_ELLIPSIS && arg == NULL)) {
        if ((vtop->type.t & VT_BTYPE) == VT_FLOAT) {
            gen_cast_s(VT_DOUBLE);
        } else if (vtop->type.t & VT_BITFIELD) {
            type.t = vtop->type.t & (VT_BTYPE | VT_UNSIGNED);
	    type.ref = vtop->type.ref;
            gen_cast(&type);
        } else if (vtop->r & VT_MUSTCAST) {
            force_charshort_cast();
        }
    } else if (arg == NULL) {
        mcc_error("too many arguments to function");
    } else {
        type = arg->type;
        type.t &= ~VT_CONSTANT;
        tu = transparent_union_member(&type);
        if (tu)
            gen_cast(&tu->type);
        else
            gen_assign_cast(&type);
    }
}

static void expr_type(CType *type, void (*expr_fn)(void))
{
    nocode_wanted++;
    expr_fn();
    *type = vtop->type;
    vpop();
    nocode_wanted--;
}

static void parse_expr_type(CType *type)
{
    int n;
    AttributeDef ad;

    skip('(');
    if (parse_btype(type, &ad, 0)) {
        type_decl(type, &ad, &n, TYPE_ABSTRACT);
    } else {
        expr_type(type, gexpr);
    }
    skip(')');
}

static void parse_type(CType *type)
{
    AttributeDef ad;
    int n;

    if (!parse_btype(type, &ad, 0)) {
        expect("type");
    }
    type_decl(type, &ad, &n, TYPE_ABSTRACT);
}

static void parse_builtin_params(int nc, const char *args)
{
    char c, sep = '(';
    CType type;
    if (nc)
        nocode_wanted++;
    next();
    if (*args == 0)
	skip(sep);
    while ((c = *args++)) {
	skip(sep);
	sep = ',';
        if (c == 't') {
            parse_type(&type);
	    vpush(&type);
	    continue;
        }
        expr_eq();
        type.ref = NULL;
        type.t = 0;
	switch (c) {
	    case 'e':
		continue;
	    case 'V':
                type.t = VT_CONSTANT;
                FALLTHROUGH;
	    case 'v':
                type.t |= VT_VOID;
                mk_pointer (&type);
                break;
	    case 'S':
                type.t = VT_CONSTANT;
                FALLTHROUGH;
	    case 's':
                type.t |= char_type.t;
                mk_pointer (&type);
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
        nocode_wanted--;
}

static void parse_atomic(int atok)
{
    int size, align, arg, t, save = 0, use_generic = 0;
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
                "avm.v"
    };
    const char *template = templates[(atok - TOK___atomic_store)];

    atom = atom_ptr = NULL;
    size = 0;
    next();
    skip('(');
    for (arg = 0;;) {
        expr_eq();
        switch (template[arg]) {
        case 'a':
        case 'A':
            atom_ptr = &vtop->type;
            if ((atom_ptr->t & VT_BTYPE) != VT_PTR)
                expect("pointer");
            atom = pointed_type(atom_ptr);
            size = type_size(atom, &align);
            if (atok <= TOK___atomic_compare_exchange
                && (size > 8
                    || ((atom->t & VT_BTYPE) == VT_STRUCT
                        && (atok == TOK___atomic_load
                            || atok == TOK___atomic_exchange))))
                use_generic = 1;
            else if (size > 8
                || (size & (size - 1))
                || (atok > TOK___atomic_compare_exchange
                    && (0 == btype_size(atom->t & VT_BTYPE)
                        || (atom->t & VT_BTYPE) == VT_PTR)))
                expect("integral or integer-sized pointer target type");
            break;

        case 'p':
            if (use_generic) {
                if ((vtop->type.t & VT_BTYPE) != VT_PTR)
                    mcc_error("pointer expected in argument %d", arg + 1);
                break;
            }
            if ((vtop->type.t & VT_BTYPE) != VT_PTR
             || type_size(pointed_type(&vtop->type), &align) != size)
                mcc_error("pointer target type mismatch in argument %d", arg + 1);
            gen_assign_cast(atom_ptr);
            break;
        case 'v':
            gen_assign_cast(atom);
            break;
        case 'l':
            if (use_generic)
                break;
            indir();
            gen_assign_cast(atom);
            break;
        case 's':
            if (use_generic)
                break;
            save = 1;
            indir();
            store = *vtop;
            vpop();
            break;
        case 'm':
            gen_assign_cast(&int_type);
            break;
        case 'b':
            if (use_generic) {
                vpop();
                break;
            }
            ct.t = VT_BOOL;
            gen_assign_cast(&ct);
            break;
        }
        if ('.' == template[++arg])
            break;
        skip(',');
    }
    skip(')');

    if (use_generic) {
        int gn = arg - (atok == TOK___atomic_compare_exchange ? 1 : 0);
        vpushi(size);
        gen_cast_s(VT_SIZE_T);
        vrott(gn + 1);
        vpush_helper_func(atok);
        vrott(gn + 2);
        gfunc_call(gn + 1);
        if (atok == TOK___atomic_compare_exchange) {
            vpushi(0);
            vtop->type.t = VT_INT;
            PUT_R_RET(vtop, VT_INT);
        } else {
            ct.t = VT_VOID;
            vpush(&ct);
        }
        return;
    }

    ct.t = VT_VOID;
    switch (template[arg + 1]) {
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
    if (t == VT_BYTE || t == VT_SHORT || t == VT_BOOL) {
#ifdef PROMOTE_RET
        vtop->r |= BFVAL(VT_MUSTCAST, 1);
#else
        vtop->type.t = VT_INT;
#endif
    }
    gen_cast(&ct);
    if (save) {
        vpush(&ct);
        *vtop = store;
        vswap();
        vstore();
    }
}

static int atomic_rmw_size(SValue *sv, int op)
{
    int bt, size, align;
    if (!(sv->type.t & VT_ATOMIC_BIT) || !(sv->r & VT_LVAL))
        return 0;
    bt = sv->type.t & VT_BTYPE;
    if (bt == VT_PTR) {
        if (op != '+' && op != '-')
            return 0;
        if (type_size(pointed_type(&sv->type), &align) <= 0)
            return 0;
    } else {
        if (op != '+' && op != '-' && op != '&' && op != '|' && op != '^')
            return 0;
        if (bt != VT_INT && bt != VT_LLONG && bt != VT_BYTE
            && bt != VT_SHORT && bt != VT_BOOL)
            return 0;
    }
    size = type_size(&sv->type, &align);
    if (size < 1 || size > 8 || (size & (size - 1)))
        return 0;
    return size;
}

static void gen_atomic_rmw(int op, int ret_new)
{
    CType atomtype = vtop[-1].type;
    int size, align, bt;
    char buf[40];
    const char *base;

    atomic_lowering++;
    atomtype.t &= ~VT_QUALIFY;
    size = type_size(&atomtype, &align);
    switch (op) {
    case '+': base = ret_new ? "__atomic_add_fetch" : "__atomic_fetch_add"; break;
    case '-': base = ret_new ? "__atomic_sub_fetch" : "__atomic_fetch_sub"; break;
    case '&': base = ret_new ? "__atomic_and_fetch" : "__atomic_fetch_and"; break;
    case '|': base = ret_new ? "__atomic_or_fetch"  : "__atomic_fetch_or";  break;
    default:  base = ret_new ? "__atomic_xor_fetch" : "__atomic_fetch_xor"; break;
    }

    if ((atomtype.t & VT_BTYPE) == VT_PTR) {
        int al;
        vpush_type_size(pointed_type(&atomtype), &al);
        gen_op('*');
    } else {
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
    if (bt == VT_BYTE || bt == VT_SHORT || bt == VT_BOOL) {
#ifdef PROMOTE_RET
        vtop->r |= BFVAL(VT_MUSTCAST, 1);
#else
        vtop->type.t = VT_INT;
#endif
    }
    gen_cast(&atomtype);
    atomic_lowering--;
}

static int alloc_local_slot(int size, int align)
{
    loc = (loc - size) & -align;
    return loc;
}

static int atomic_cas_size(SValue *sv)
{
    int bt, size, align;
    if (!(sv->type.t & VT_ATOMIC_BIT) || !(sv->r & VT_LVAL))
        return 0;
    bt = sv->type.t & VT_BTYPE;
    if (bt != VT_INT && bt != VT_LLONG && bt != VT_BYTE && bt != VT_SHORT
        && bt != VT_BOOL && bt != VT_FLOAT && bt != VT_DOUBLE)
        return 0;
    size = type_size(&sv->type, &align);
    if (size < 1 || size > 8 || (size & (size - 1)))
        return 0;
    return size;
}

static void gen_atomic_cas_rmw(int op, int ret_new)
{
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
    vset(&at, VT_LOCAL | VT_LVAL, s_vr); vswap(); vstore(); vpop();

    mk_pointer(&vtop->type); gaddrof();
    vset(&pt, VT_LOCAL | VT_LVAL, s_pa); vswap(); vstore(); vpop();

    vset(&pt, VT_LOCAL | VT_LVAL, s_pa); indir();
    vset(&at, VT_LOCAL | VT_LVAL, s_vo); vswap(); vstore(); vpop();

    loop = gind();
    vset(&at, VT_LOCAL | VT_LVAL, s_vo);
    vset(&at, VT_LOCAL | VT_LVAL, s_vr);
    gen_op(op);
    gen_cast(&at);
    vset(&at, VT_LOCAL | VT_LVAL, s_vn); vswap(); vstore(); vpop();

    {
        CType it;
        it.ref = NULL;
        it.t = (size == 8) ? VT_LLONG : (size == 2) ? VT_SHORT
             : (size == 1) ? VT_BYTE : VT_INT;
        vset(&pt, VT_LOCAL | VT_LVAL, s_pa);
        vset(&at, VT_LOCAL | VT_LVAL, s_vo);
        mk_pointer(&vtop->type); gaddrof();
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

static int atomic_store_needs_libcall(SValue *sv)
{
    int size, align, bt;
    if (!(sv->type.t & VT_ATOMIC_BIT) || !(sv->r & VT_LVAL))
        return 0;
    bt = sv->type.t & VT_BTYPE;
    if (bt == VT_STRUCT || is_float(sv->type.t))
        return 0;
    size = type_size(&sv->type, &align);
    if (size <= PTR_SIZE || size > 8 || (size & (size - 1)))
        return 0;
    return size;
}

static void gen_atomic_store_scalar(void)
{
    CType at = vtop->type;
    int size, align, s_v;
    char buf[40];

    atomic_lowering++;
    at.t &= ~VT_QUALIFY;
    size = type_size(&at, &align);
    s_v = alloc_local_slot(size, align);

    mk_pointer(&vtop->type); gaddrof();
    expr_eq();
    gen_assign_cast(&at);
    vset(&at, VT_LOCAL | VT_LVAL, s_v); vswap(); vstore(); vpop();
    vset(&at, VT_LOCAL | VT_LVAL, s_v);
    vpushi(0x5);
    snprintf(buf, sizeof buf, "__atomic_store_%d", size);
    vpush_helper_func(tok_alloc_const(buf));
    vrott(4);
    gfunc_call(3);
    vset(&at, VT_LOCAL | VT_LVAL, s_v);
    atomic_lowering--;
}

static int atomic_store_needs_generic(SValue *sv)
{
    int size, align;
    if (!(sv->type.t & VT_ATOMIC_BIT) || !(sv->r & VT_LVAL))
        return 0;
    if ((sv->type.t & VT_BTYPE) == VT_STRUCT)
        return 1;
    size = type_size(&sv->type, &align);
    return size > 8;
}

static void gen_atomic_store_aggregate(void)
{
    CType at = vtop->type;
    int size, align, s_v;

    atomic_lowering++;
    at.t &= ~VT_QUALIFY;
    size = type_size(&at, &align);
    s_v = alloc_local_slot(size, align);

    mk_pointer(&vtop->type); gaddrof();
    expr_eq();
    gen_assign_cast(&at);
    vset(&at, VT_LOCAL | VT_LVAL, s_v); vswap(); vstore(); vpop();
    vset(&at, VT_LOCAL | VT_LVAL, s_v); mk_pointer(&vtop->type); gaddrof();
    vpushi(0x5);
    vpushi(size); gen_cast_s(VT_SIZE_T);
    vrott(4);
    vpush_helper_func(TOK___atomic_store);
    vrott(5);
    gfunc_call(4);
    vset(&at, VT_LOCAL | VT_LVAL, s_v);
    atomic_lowering--;
}

static void gen_atomic_load_aggregate(void)
{
    CType at = vtop->type;
    int size, align, s_v;

    atomic_lowering++;
    at.t &= ~VT_QUALIFY;
    size = type_size(&at, &align);
    s_v = alloc_local_slot(size, align);

    mk_pointer(&vtop->type); gaddrof();
    vset(&at, VT_LOCAL | VT_LVAL, s_v); mk_pointer(&vtop->type); gaddrof();
    vpushi(0x5);
    vpushi(size); gen_cast_s(VT_SIZE_T);
    vrott(4);
    vpush_helper_func(TOK___atomic_load);
    vrott(5);
    gfunc_call(4);
    vset(&at, VT_LOCAL | VT_LVAL, s_v);
    atomic_lowering--;
}

static void gen_atomic_load_scalar(void)
{
    CType at = vtop->type;
    int size, align, bt;
    char buf[40];

    atomic_lowering++;
    at.t &= ~VT_QUALIFY;
    size = type_size(&at, &align);
    mk_pointer(&vtop->type); gaddrof();
    vpushi(0x5);
    snprintf(buf, sizeof buf, "__atomic_load_%d", size);
    vpush_helper_func(tok_alloc_const(buf));
    vrott(3);
    gfunc_call(2);
    vpush(&at);
    PUT_R_RET(vtop, at.t);
    bt = at.t & VT_BTYPE;
    if (bt == VT_BYTE || bt == VT_SHORT || bt == VT_BOOL)
        vtop->type.t = VT_INT;
    atomic_lowering--;
}


static int format_func_spec(const char *name, int *is_scanf,
                            int *fmt_arg, int *first_vararg)
{
    static const struct { const char *n; char scanf, fmt, va; } tbl[] = {
        { "printf",   0, 1, 2 }, { "fprintf", 0, 2, 3 },
        { "sprintf",  0, 2, 3 }, { "snprintf",0, 3, 4 },
        { "dprintf",  0, 2, 3 },
        { "scanf",    1, 1, 2 }, { "fscanf",  1, 2, 3 },
        { "sscanf",   1, 2, 3 },
    };
    unsigned i;
    if (!name) return 0;
    for (i = 0; i < sizeof tbl / sizeof tbl[0]; i++)
        if (!strcmp(name, tbl[i].n)) {
            *is_scanf = tbl[i].scanf;
            *fmt_arg = tbl[i].fmt;
            *first_vararg = tbl[i].va;
            return 1;
        }
    return 0;
}

static const char *format_str_literal(SValue *sv, int *avail)
{
    Section *ssec;
    ElfSym *esym;
    unsigned long off;
    if (!(sv->type.t & VT_ARRAY) && (sv->type.t & VT_BTYPE) != VT_PTR)
        return NULL;
    if ((sv->r & (VT_SYM | VT_CONST)) != (VT_SYM | VT_CONST)
        || !sv->sym || sv->sym->v < SYM_FIRST_ANOM)
        return NULL;
    esym = elfsym(sv->sym);
    if (!esym || esym->st_shndx == SHN_UNDEF)
        return NULL;
    ssec = mcc_state->sections[esym->st_shndx];
    if (!ssec || !ssec->data || ssec->reloc)
        return NULL;
    off = esym->st_value + (unsigned)sv->c.i;
    if (off >= ssec->data_offset)
        return NULL;
    *avail = (int)(ssec->data_offset - off);
    return (const char *)(ssec->data + off);
}

static int format_arg_class(CType *t)
{
    int bt = t->t & VT_BTYPE;
    if (t->t & VT_ARRAY) return 2;
    if (bt == VT_PTR || bt == VT_FUNC) return 2;
    if (bt == VT_FLOAT || bt == VT_DOUBLE || bt == VT_LDOUBLE) return 1;
    if (bt == VT_BOOL || bt == VT_BYTE || bt == VT_SHORT || bt == VT_INT
        || bt == VT_LLONG)
        return 0;
    return 3;
}

static void format_check(int is_scanf, const char *fmt, int favail,
                         SValue *args, int nvar)
{
    int i = 0, used = 0;
    while (i < favail && fmt[i]) {
        int cls_want;
        char conv;
        if (fmt[i] != '%') { i++; continue; }
        i++;
        if (i < favail && fmt[i] == '%') { i++; continue; }
        while (i < favail && (fmt[i]=='-'||fmt[i]=='+'||fmt[i]==' '
                              ||fmt[i]=='#'||fmt[i]=='0')) i++;
        if (is_scanf && i < favail && fmt[i] == '*') { i++;
              }
        if (!is_scanf && i < favail && fmt[i] == '*') {
            if (used < nvar && format_arg_class(&args[used].type) != 0)
                mcc_warning_c(warn_format)("field width '*' expects an int argument");
            used++; i++;
        } else while (i < favail && fmt[i] >= '0' && fmt[i] <= '9') i++;
        if (i < favail && fmt[i] == '.') {
            i++;
            if (!is_scanf && i < favail && fmt[i] == '*') {
                if (used < nvar && format_arg_class(&args[used].type) != 0)
                    mcc_warning_c(warn_format)("precision '*' expects an int argument");
                used++; i++;
            } else while (i < favail && fmt[i] >= '0' && fmt[i] <= '9') i++;
        }
        while (i < favail && (fmt[i]=='h'||fmt[i]=='l'||fmt[i]=='L'
                              ||fmt[i]=='j'||fmt[i]=='z'||fmt[i]=='t')) i++;
        if (i >= favail || !fmt[i]) break;
        conv = fmt[i++];
        switch (conv) {
        case 'd': case 'i': case 'u': case 'o': case 'x': case 'X': case 'c':
            cls_want = 0; break;
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
        case 'a': case 'A':
            cls_want = 1; break;
        case 's': case 'p': case 'n':
            cls_want = 2; break;
        default:
            continue;
        }
        int sc_base = cls_want;
        if (is_scanf) cls_want = 2;
        if (used >= nvar) {
            mcc_warning_c(warn_format)(
                "more conversions than arguments for '%s'",
                is_scanf ? "scanf" : "printf");
            return;
        }
        {
            int got = format_arg_class(&args[used].type);
            if (got <= 2 && got != cls_want) {
                static const char *cn[] = { "an integer", "a floating",
                                            "a pointer" };
                mcc_warning_c(warn_format)(
                    "format '%%%c' expects %s argument", conv, cn[cls_want]);
            } else if (is_scanf && got == 2 && sc_base <= 1
                       && (args[used].type.t & VT_BTYPE) == VT_PTR) {
                int pc = format_arg_class(pointed_type(&args[used].type));
                if (pc <= 1 && pc != sc_base) {
                    static const char *cn2[] = { "int", "floating-point" };
                    mcc_warning_c(warn_format)(
                        "format '%%%c' expects a pointer to %s argument",
                        conv, cn2[sc_base]);
                }
            }
        }
        used++;
    }
    if (used < nvar)
        mcc_warning_c(warn_format)("too many arguments for format string");
}

static int sizeof_parsed_type;

ST_FUNC void unary(void)
{
    int n, t, align, size, r;
    CType type;
    Sym *s;
    AttributeDef ad;

    if (debug_modes)
        mcc_debug_line(mcc_state), mcc_tcov_check_line (mcc_state, 1);

    type.ref = NULL;
 tok_next:
    switch(tok) {
    case TOK_EXTENSION:
        next();
        goto tok_next;
    case TOK_U16CHAR:
        t = VT_SHORT|VT_UNSIGNED;
        goto push_tokc;
    case TOK_U32CHAR:
        t = VT_INT|VT_UNSIGNED;
        goto push_tokc;
    case TOK_LCHAR:
#ifdef MCC_TARGET_PE
        t = VT_SHORT|VT_UNSIGNED;
        goto push_tokc;
#endif
    case TOK_CINT:
    case TOK_CCHAR:
	t = VT_INT;
 push_tokc:
	type.t = t;
	vsetc(&type, VT_CONST, &tokc);
        if (tok_imaginary) {
            CType cbase, ccplx;
            SValue r;
            cbase.t = t & (VT_BTYPE | VT_LONG);
            cbase.ref = NULL;
            mk_complex_type(&ccplx, &cbase);
            cplx_local(&ccplx, &r);
            cplx_store_part(&r, 1);
            vpushi(0); gen_cast(&cbase);
            cplx_store_part(&r, 0);
            vpushv(&r);
        }
        next();
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
            goto tok_identifier;
        FALLTHROUGH;
    case TOK___FUNC__:
        if (!funcname[0])
            mcc_warning("'__func__' is not defined outside of function scope");
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
            t |= VT_CONSTANT;
        type.t = t;
        mk_pointer(&type);
        type.t |= VT_ARRAY;
        memset(&ad, 0, sizeof(AttributeDef));
        ad.section = rodata_section;
        decl_initializer_alloc(&type, &ad, VT_CONST, 2, 0, 0);
        break;
    case TOK_SOTYPE:
    case '(':
        t = tok;
        next();
        if (parse_btype(&type, &ad, 0)) {
            type_decl(&type, &ad, &n, TYPE_ABSTRACT);
            skip(')');
            if (tok == '{') {
                if (global_expr) {
                    if (local_scope)
                        mcc_error("initializer element is not constant");
                    r = VT_CONST;
                } else
                    r = VT_LOCAL;
                if (!(type.t & VT_ARRAY))
                    r |= VT_LVAL;
                memset(&ad, 0, sizeof(AttributeDef));
                decl_initializer_alloc(&type, &ad, r, 1, 0, 0);
            } else if (t == TOK_SOTYPE) {
                sizeof_parsed_type = 1;
                vpush(&type);
                return;
            } else {
                unary();
                if (type.t & (VT_ARRAY | VT_VLA))
                    mcc_error("conversion to non-scalar type requested");
                if ((type.t & VT_BTYPE) == VT_STRUCT
                    && type.ref->type.t != VT_UNION
                    && !is_complex_type(&type)
                    && !is_compatible_unqualified_types(&type, &vtop->type))
                    mcc_error("conversion to non-scalar type requested");
                gen_cast(&type);
                if ((type.t & VT_BTYPE) == VT_VOID)
                    expr_has_effect = 1;
                if ((vtop->r & VT_LVAL) && !nocode_wanted && !asm_lvalue_cast) {
                    int bt = vtop->type.t & VT_BTYPE;
                    if (bt != VT_STRUCT && bt != VT_VOID
                        && !is_complex_type(&vtop->type))
                        gv(RC_TYPE(vtop->type.t));
                }
            }
        } else if (tok == '{') {
	    int saved_nocode_wanted = nocode_wanted;
            if (CONST_WANTED && !NOEVAL_WANTED)
                expect("constant");
            if (0 == local_scope)
                mcc_error("statement expression outside of function");
            save_regs(0);
            vpushi(0), vtop->type.t = VT_VOID;
            block(STMT_EXPR);
            if (saved_nocode_wanted)
              nocode_wanted = saved_nocode_wanted;
            skip(')');
        } else {
            gexpr();
            skip(')');
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
            mcc_error("cannot take address of bit-field");
        if (vtop->sym && vtop->sym->a.is_register)
            mcc_error("address of register variable '%s' requested",
                      get_tok_str(vtop->sym->v, NULL));
        if ((vtop->type.t & VT_BTYPE) != VT_FUNC &&
            !(vtop->type.t & (VT_ARRAY | VT_VLA)))
            test_lvalue();
        if (vtop->r & VT_NONLVAL)
            mcc_error("cannot take the address of a function-call result");
        if (vtop->sym)
          vtop->sym->a.addrtaken = 1;
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
    case TOK_IMAGPART2:
        {
            int imag = (tok == TOK_IMAGPART1 || tok == TOK_IMAGPART2);
            next();
            unary();
            if (is_complex_type(&vtop->type)) {
                complex_part(imag);
            } else if (imag) {
                CType bt = vtop->type;
                vpop();
                vpushi(0);
                gen_cast(&bt);
            }
        }
        break;
    case '+':
        next();
        unary();
        if ((vtop->type.t & VT_BTYPE) == VT_PTR)
            mcc_error("pointer not accepted for unary plus");
	if (!is_float(vtop->type.t)) {
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
            tok = TOK_SOTYPE;
        expr_type(&type, unary);
        if (type.t & VT_BITFIELD)
            mcc_error("'%s' cannot be applied to a bit-field",
                      t == TOK_SIZEOF ? "sizeof" : "_Alignof");
        if (t == TOK_ALIGNOF3 && !sizeof_parsed_type)
            mcc_pedantic("ISO C does not allow '_Alignof' applied to an expression");
        if ((type.t & VT_BTYPE) == VT_FUNC)
            mcc_pedantic(t == TOK_SIZEOF
                         ? "'sizeof' applied to a function type"
                         : "'_Alignof' applied to a function type");
        if ((type.t & VT_BTYPE) == VT_VOID) {
            if (t == TOK_SIZEOF)
                mcc_pedantic("'sizeof' applied to a void type");
            else if (t == TOK_ALIGNOF3)
                mcc_error("'_Alignof' applied to a void type");
        }
        if (t == TOK_SIZEOF) {
            vpush_type_size(&type, &align);
            gen_cast_s(VT_SIZE_T);
        } else {
            if (type_size(&type, &align) < 0)
                mcc_error("'_Alignof' applied to an incomplete type");
            s = NULL;
            if (vtop[1].r & VT_SYM)
                s = vtop[1].sym;
            if (s && s->a.aligned)
                align = 1 << (s->a.aligned - 1);
            vpushs(align);
        }
        break;

    case TOK_builtin_expect:
	parse_builtin_params(0, "ee");
	vpop();
        break;
    case TOK_builtin_types_compatible_p:
	parse_builtin_params(0, "tt");
	vtop[-1].type.t &= ~VT_QUALIFY;
	vtop[0].type.t &= ~VT_QUALIFY;
	n = is_compatible_types(&vtop[-1].type, &vtop[0].type);
	vtop -= 2;
	vpushi(n);
        break;
    case TOK_builtin_choose_expr:
	{
	    int64_t c;
	    next();
	    skip('(');
	    c = expr_const64();
	    skip(',');
	    if (!c) {
		nocode_wanted++;
	    }
	    expr_eq();
	    if (!c) {
		vpop();
		nocode_wanted--;
	    }
	    skip(',');
	    if (c) {
		nocode_wanted++;
	    }
	    expr_eq();
	    if (c) {
		vpop();
		nocode_wanted--;
	    }
	    skip(')');
	}
        break;
    case TOK_builtin_complex:
	{
	    CType cbase, ccplx;
	    SValue r;
	    next();
	    skip('(');
	    expr_eq();
	    skip(',');
	    expr_eq();
	    skip(')');
	    cbase = vtop[-1].type;
	    cbase.t &= (VT_BTYPE | VT_LONG);
	    if (!is_float(cbase.t))
		cbase.t = VT_DOUBLE;
	    cbase.ref = NULL;
	    mk_complex_type(&ccplx, &cbase);
	    if ((vtop[-1].r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST
		&& (vtop[0].r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
		init_params p = { .sec = rodata_section };
		unsigned long offset;
		int bsz, bal, csz, cal;
		bsz = type_size(&cbase, &bal);
		csz = type_size(&ccplx, &cal);
		if (NODATA_WANTED) { csz = 0; cal = 1; }
		offset = section_add(p.sec, csz, cal);
		init_putv(&p, &cbase, offset + bsz);
		init_putv(&p, &cbase, offset);
		vpush_ref(&ccplx, p.sec, offset, csz);
		vtop->r |= VT_LVAL;
	    } else {
		cplx_local(&ccplx, &r);
		gen_cast(&cbase);
		cplx_store_part(&r, 1);
		gen_cast(&cbase);
		cplx_store_part(&r, 0);
		vpushv(&r);
	    }
	}
	break;
    case TOK_builtin_constant_p:
	parse_builtin_params(1, "e");
	n = 1;
	if ((vtop->r & (VT_VALMASK | VT_LVAL)) != VT_CONST
	    || ((vtop->r & VT_SYM) && vtop->sym->a.addrtaken)
	    )
	    n = 0;
	vtop--;
	vpushi(n);
        break;
    case TOK_builtin_unreachable:
	parse_builtin_params(0, "");
        type.t = VT_VOID;
        vpush(&type);
        CODE_OFF();
        break;
    case TOK_builtin_nan:    case TOK_builtin_nanf:    case TOK_builtin_nanl:
    case TOK_builtin_inf:    case TOK_builtin_inff:    case TOK_builtin_infl:
    case TOK_builtin_huge_val: case TOK_builtin_huge_valf:
    case TOK_builtin_huge_vall:
        {
            int btok = tok, fbt;
            unsigned long long bits;
            CValue cv;
            int is_nan = (btok == TOK_builtin_nan || btok == TOK_builtin_nanf
                          || btok == TOK_builtin_nanl);
            int is_float = (btok == TOK_builtin_nanf || btok == TOK_builtin_inff
                            || btok == TOK_builtin_huge_valf);
            int is_ld = (btok == TOK_builtin_nanl || btok == TOK_builtin_infl
                         || btok == TOK_builtin_huge_vall);
             
            if (is_nan)
                parse_builtin_params(1, "e");
            else
                parse_builtin_params(0, "");
            if (is_nan)
                vtop--;                  
            if (is_float) {
                union { unsigned u; float f; } x;
                x.u = is_nan ? 0x7fc00000U : 0x7f800000U;
                cv.f = x.f;
                fbt = VT_FLOAT;
            } else {
                union { unsigned long long u; double d; } x;
                x.u = is_nan ? 0x7ff8000000000000ULL : 0x7ff0000000000000ULL;
                cv.d = x.d;
                if (is_ld) { cv.ld = (long double)x.d; fbt = VT_LDOUBLE; }
                else fbt = VT_DOUBLE;
            }
            type.t = fbt; type.ref = NULL;
            vsetc(&type, VT_CONST, &cv);
            (void)bits;
        }
        break;
    case TOK_builtin_signbit: case TOK_builtin_signbitf:
    case TOK_builtin_signbitl:
        /* real builtin (gcc-compatible): inspects the sign bit only, never
           raises FP exceptions -- an FP compare would trap on NaN input */
        {
            int btok = tok, bt;
            parse_builtin_params(0, "e");
            bt = vtop->type.t & VT_BTYPE;
            if (bt != VT_FLOAT && bt != VT_DOUBLE && bt != VT_LDOUBLE)
                mcc_error("non-floating-point argument in call to function '%s'",
                          get_tok_str(btok, NULL));
            if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
                int sb;
                if (bt == VT_FLOAT) {
                    uint32_t u;
                    memcpy(&u, &vtop->c.f, 4);
                    sb = u >> 31;
                } else if (bt == VT_DOUBLE) {
                    uint64_t u;
                    memcpy(&u, &vtop->c.d, 8);
                    sb = (int)(u >> 63);
                } else {
                    unsigned char lb[16];
                    memset(lb, 0, sizeof lb);
                    write_ldouble(lb, &vtop->c.ld);
#if defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64
                    /* x87 80-bit: sign byte 9, above is padding */
                    sb = lb[LDOUBLE_SIZE > 8 ? 9 : 7] >> 7;
#else
                    sb = lb[LDOUBLE_SIZE - 1] >> 7;
#endif
                }
                vpop();
                vpushi(sb);
            } else {
                vpush_helper_func(tok_alloc_const(
                    bt == VT_FLOAT ? "__mcc_signbitf"
                    : bt == VT_LDOUBLE ? "__mcc_signbitl" : "__mcc_signbit"));
                vrott(2);
                gfunc_call(1);
                vpushi(0);
                vtop->type.t = VT_INT;
                PUT_R_RET(vtop, VT_INT);
            }
        }
        break;
    case TOK_builtin_ffs:    case TOK_builtin_ffsl:    case TOK_builtin_ffsll:
    case TOK_builtin_clz:    case TOK_builtin_clzl:    case TOK_builtin_clzll:
    case TOK_builtin_ctz:    case TOK_builtin_ctzl:    case TOK_builtin_ctzll:
    case TOK_builtin_clrsb:  case TOK_builtin_clrsbl:  case TOK_builtin_clrsbll:
    case TOK_builtin_popcount: case TOK_builtin_popcountl: case TOK_builtin_popcountll:
    case TOK_builtin_parity: case TOK_builtin_parityl: case TOK_builtin_parityll:
        {
            int bop, bw, op_unsigned;
            int btok = tok;
            CType at;
            switch (btok) {
            case TOK_builtin_ffs: case TOK_builtin_ffsl: case TOK_builtin_ffsll:
                bop = BB_FFS; op_unsigned = 0; break;
            case TOK_builtin_clz: case TOK_builtin_clzl: case TOK_builtin_clzll:
                bop = BB_CLZ; op_unsigned = 1; break;
            case TOK_builtin_ctz: case TOK_builtin_ctzl: case TOK_builtin_ctzll:
                bop = BB_CTZ; op_unsigned = 1; break;
            case TOK_builtin_clrsb: case TOK_builtin_clrsbl: case TOK_builtin_clrsbll:
                bop = BB_CLRSB; op_unsigned = 0; break;
            case TOK_builtin_popcount: case TOK_builtin_popcountl: case TOK_builtin_popcountll:
                bop = BB_POPCOUNT; op_unsigned = 1; break;
            default:
                bop = BB_PARITY; op_unsigned = 1; break;
            }
            switch (btok) {
            case TOK_builtin_ffsll: case TOK_builtin_clzll: case TOK_builtin_ctzll:
            case TOK_builtin_clrsbll: case TOK_builtin_popcountll: case TOK_builtin_parityll:
                bw = 64; at.t = VT_LLONG; break;
            case TOK_builtin_ffsl: case TOK_builtin_clzl: case TOK_builtin_ctzl:
            case TOK_builtin_clrsbl: case TOK_builtin_popcountl: case TOK_builtin_parityl:
                bw = LONG_SIZE * 8; at.t = (LONG_SIZE == 8 ? VT_LLONG|VT_LONG : VT_INT); break;
            default:
                bw = 32; at.t = VT_INT; break;
            }
            at.ref = NULL;
            if (op_unsigned)
                at.t |= VT_UNSIGNED;
            parse_builtin_params(0, "e");
            gen_assign_cast(&at);
            if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
                int64_t cv = vtop->c.i;
                vpop();
                vpushi(fold_bit_builtin(bop, bw, cv));
            } else {
                vpush_helper_func(btok);
                vrott(2);
                gfunc_call(1);
                vpushi(0);
                vtop->type.t = VT_INT;
                PUT_R_RET(vtop, VT_INT);
            }
        }
        break;
    case TOK_builtin_bswap16:
    case TOK_builtin_bswap32:
    case TOK_builtin_bswap64:
        {
            int btok = tok;
            CType at;
            int rt = (btok == TOK_builtin_bswap16) ? (VT_SHORT|VT_UNSIGNED)
                   : (btok == TOK_builtin_bswap32) ? (VT_INT|VT_UNSIGNED)
                   : (VT_LLONG|VT_UNSIGNED);
            at.ref = NULL;
            at.t = rt;
            parse_builtin_params(0, "e");
            gen_assign_cast(&at);
            if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
                uint64_t u = (uint64_t)vtop->c.i, r2 = 0;
                int i, nb = (btok == TOK_builtin_bswap16) ? 2
                          : (btok == TOK_builtin_bswap32) ? 4 : 8;
                for (i = 0; i < nb; i++)
                    r2 |= ((u >> (i * 8)) & 0xff) << ((nb - 1 - i) * 8);
                vtop->c.i = r2;
                vtop->r = VT_CONST;
            } else {
                vpush_helper_func(btok);
                vrott(2);
                gfunc_call(1);
                vpush(&at);
                PUT_R_RET(vtop, at.t);
            }
        }
        break;
    case TOK_builtin_frame_address:
    case TOK_builtin_return_address:
        {
            int tok1 = tok;
            int level;
            next();
            skip('(');
            level = expr_const();
            if (level < 0)
                mcc_error("%s only takes positive integers", get_tok_str(tok1, 0));
            skip(')');
            type.t = VT_VOID;
            mk_pointer(&type);
            vset(&type, VT_LOCAL, 0);
            while (level--) {
#ifdef MCC_TARGET_RISCV64
                vpushi(2*PTR_SIZE);
                gen_op('-');
#endif
                mk_pointer(&vtop->type);
                indir();
            }
            if (tok1 == TOK_builtin_return_address) {
#ifdef MCC_TARGET_ARM
                vpushi(2*PTR_SIZE);
                gen_op('+');
#elif defined MCC_TARGET_RISCV64
                vpushi(PTR_SIZE);
                gen_op('-');
#else
                vpushi(PTR_SIZE);
                gen_op('+');
#endif
                mk_pointer(&vtop->type);
                indir();
            }
        }
        break;
#ifdef MCC_TARGET_RISCV64
    case TOK_builtin_va_start:
        parse_builtin_params(0, "ee");
        check_va_start_register();
        check_va_start_last_param();
        r = vtop->r & VT_VALMASK;
        if (r == VT_LLOCAL)
            r = VT_LOCAL;
        if (r != VT_LOCAL)
            mcc_error("__builtin_va_start expects a local variable");
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
            r = VT_LOCAL;
        if (r != VT_LOCAL)
            mcc_error("__builtin_va_start expects a local variable");
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
            mcc_error("pointer not accepted for unary minus");
	if (is_float(vtop->type.t)) {
            gen_opif(TOK_NEG);
	} else {
            vpushi(0);
            vswap();
            gen_op('-');
        }
        break;
    case TOK_LAND:
        if (!gnu_ext)
            goto tok_identifier;
        next();
        if (tok < TOK_UIDENT)
            expect("label identifier");
        s = label_find(tok);
        if (!s) {
            s = label_push(&global_label_stack, tok, LABEL_FORWARD);
        } else {
            if (s->r == LABEL_DECLARED)
                s->r = LABEL_FORWARD;
        }
        if ((s->type.t & VT_BTYPE) != VT_PTR) {
            s->type.t = VT_VOID;
            mk_pointer(&s->type);
            s->type.t |= VT_STATIC;
        }
        vpushsym(&s->type, s);
        next();
        break;

    case TOK_GENERIC:
    {
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
	convert_parameter_type (&controlling_type);

        nocode_wanted = saved_nocode_wanted;

        for (;;) {
	    learn = 0;
	    skip(',');
	    if (tok == TOK_DEFAULT) {
		if (has_default)
		    mcc_error("too many 'default'");
		has_default = 1;
		if (!has_match)
		    learn = 1;
		next();
	    } else {
		int v, i;
		parse_btype(&type, &ad, 0);
		type_decl(&type, &ad, &v, TYPE_ABSTRACT);
		if (type.t & VT_VLA)
		    mcc_error("'_Generic' association has a variably modified type");
		else if ((type.t & VT_BTYPE) == VT_FUNC)
		    mcc_pedantic("ISO C forbids a '_Generic' association with a "
				 "function type");
		else {
		    int gsz, galign;
		    gsz = type_size(&type, &galign);
		    if (gsz < 0 || (type.t & VT_BTYPE) == VT_VOID)
			mcc_pedantic("ISO C forbids a '_Generic' association "
				     "with an incomplete type");
		}
		for (i = 0; i < nb_assoc; i++)
		    if (compare_types(&assoc_types[i], &type, 0))
			mcc_error("_Generic specifies two compatible types");
		assoc_types = mcc_realloc(assoc_types,
					  (nb_assoc + 1) * sizeof(CType));
		assoc_types[nb_assoc++] = type;
		if (compare_types(&controlling_type, &type, 0)) {
		    if (has_match) {
		      mcc_error("type match twice");
		    }
		    has_match = 1;
		    learn = 1;
		}
	    }
	    skip(':');
	    if (learn) {
		if (str)
		    tok_str_free(str);
		skip_or_save_block(&str);
	    } else {
		skip_or_save_block(NULL);
	    }
	    if (tok == ')')
		break;
	}
	if (!str) {
	    char buf[60];
	    type_to_str(buf, sizeof buf, &controlling_type, NULL);
	    mcc_error("type '%s' does not match any association", buf);
	}
	mcc_free(assoc_types);
	begin_macro(str, 1);
	next();
	expr_eq();
	if (tok != TOK_EOF)
	    expect(",");
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
            mcc_error("expression expected before '%s'", get_tok_str(tok, &tokc));
        t = tok;
        next();
        s = sym_find(t);
        if (!s || IS_ASM_SYM(s)) {
            const char *name = get_tok_str(t, NULL);
            if (tok != '(')
                mcc_error("'%s' undeclared", name);
            mcc_warning_c(warn_implicit_function_declaration)(
                "implicit declaration of function '%s'", name);
            s = external_global_sym(t, &func_old_type);
        }

        s->a.used = 1;

        r = s->r;
        if ((r & VT_VALMASK) < VT_CONST)
            r = (r & ~VT_VALMASK) | VT_LOCAL;

        vset(&s->type, r, s->c);
	vtop->sym = s;

        if (r & VT_SYM) {
            vtop->c.i = 0;
#ifdef MCC_TARGET_PE
            if (s->a.dllimport) {
                mk_pointer(&vtop->type);
                vtop->r |= VT_LVAL;
                indir();
            }
#endif
        } else if (r == VT_CONST && IS_ENUM_VAL(s->type.t)) {
            vtop->c.i = s->enum_val;
        }
        break;
    }

    while (1) {
        if (tok == TOK_INC || tok == TOK_DEC) {
            inc(1, tok);
            expr_has_effect = 1;
            next();
        } else if (tok == '.' || tok == TOK_ARROW) {
            int qualifiers, cumofs, base_nonlval;
            if (tok == TOK_ARROW)
                indir();
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
                parse_btype_qualify(&vtop->type, qualifiers);
            if (!(vtop->type.t & VT_ARRAY)) {
                vtop->r |= VT_LVAL | base_nonlval;
#ifdef CONFIG_MCC_BCHECK
                if (mcc_state->do_bounds_check)
                    vtop->r |= VT_MUSTBOUND;
#endif
            }
            next();
        } else if (tok == '[') {
            next();
            gexpr();
            gen_op('+');
            indir();
            skip(']');
        } else if (tok == '(') {
            SValue ret;
            Sym *sa;
            int nb_args, ret_nregs, ret_align, regsize, variadic;
            TokenString *p, *p2;
            const char *fmt_fname = NULL;

            if ((mcc_state->warn_format & WARN_ON)
                && (vtop->r & VT_SYM) && vtop->sym)
                fmt_fname = get_tok_str(vtop->sym->v, NULL);

            if ((vtop->type.t & VT_BTYPE) != VT_FUNC) {
                if ((vtop->type.t & (VT_BTYPE | VT_ARRAY)) == VT_PTR) {
                    vtop->type = *pointed_type(&vtop->type);
                    if ((vtop->type.t & VT_BTYPE) != VT_FUNC)
                        goto error_func;
                } else {
                error_func:
                    expect("function pointer");
                }
            } else {
                vtop->r &= ~VT_LVAL;
            }
            s = vtop->type.ref;
            next();
            sa = s->next;
            nb_args = regsize = 0;
            ret.r2 = VT_CONST;
            if ((s->type.t & VT_BTYPE) == VT_STRUCT) {
                variadic = (s->f.func_type == FUNC_ELLIPSIS);
                ret_nregs = gfunc_sret(&s->type, variadic, &ret.type,
                                       &ret_align, &regsize);
                if (ret_nregs <= 0) {
                    size = type_size(&s->type, &align);
#ifdef MCC_TARGET_ARM64
                if (size < 16)
                    while (size & (size - 1))
                        size = (size | (size - 1)) + 1;
#endif
                    loc = (loc - size) & -align;
                    ret.type = s->type;
                    ret.r = VT_LOCAL | VT_LVAL;
                    vseti(VT_LOCAL, loc);
#ifdef CONFIG_MCC_BCHECK
                    if (mcc_state->do_bounds_check)
                        --loc;
#endif
                    ret.c = vtop->c;
                    if (ret_nregs < 0)
                      vtop--;
                    else
                      nb_args++;
                }
            } else {
                ret_nregs = 1;
                ret.type = s->type;
            }

            if (ret_nregs > 0) {
                ret.c.i = 0;
                PUT_R_RET(&ret, ret.type.t);
            }

            p = NULL;
            if (tok != ')') {
                r = mcc_state->reverse_funcargs;
                for(;;) {
                    if (r) {
                        skip_or_save_block(&p2);
                        p2->prev = p, p = p2;
                    } else {
                        expr_eq();
                        gfunc_param_typed(s, sa);
                        seqp_flush();
                    }
                    nb_args++;
                    if (sa)
                        sa = sa->next;
                    if (tok == ')')
                        break;
                    skip(',');
                }
            }
            if (sa)
                mcc_error("too few arguments to function");

            if (p) {
                for (n = 0; p; p = p2, ++n) {
                    p2 = p, sa = s;
                    do {
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

            if (fmt_fname && !mcc_state->reverse_funcargs
                && (s->type.t & VT_BTYPE) != VT_STRUCT) {
                int is_scanf, fa, fv;
                if (format_func_spec(fmt_fname, &is_scanf, &fa, &fv)
                    && nb_args >= fa) {
                    int favail;
                    const char *fstr =
                        format_str_literal(vtop - (nb_args - fa), &favail);
                    if (fstr) {
                        int nvar = nb_args - fv + 1;
                        if (nvar < 0) nvar = 0;
                        format_check(is_scanf, fstr, favail,
                                     vtop - (nb_args - fv), nvar);
                    }
                }
            }

            next();
            vcheck_cmp();
            gfunc_call(nb_args);
            expr_has_effect = 1;

            if (ret_nregs < 0) {
                vsetc(&ret.type, ret.r, &ret.c);
#if defined(MCC_TARGET_RISCV64) || (defined(MCC_TARGET_X86_64) && !defined(MCC_TARGET_PE))
                arch_transfer_ret_regs(1);
#endif
            } else {
                n = ret_nregs;
                while (n > 1) {
                    int rc = reg_classes[ret.r] & ~(RC_INT | RC_FLOAT);
                    rc <<= --n;
                    for (r = 0; r < NB_REGS; ++r)
                        if (reg_classes[r] & rc)
                            break;
                    vsetc(&ret.type, r, &ret.c);
                }
                vsetc(&ret.type, ret.r, &ret.c);
                vtop->r2 = ret.r2;

                if (((s->type.t & VT_BTYPE) == VT_STRUCT) && ret_nregs) {
                    int addr, offset;

                    size = type_size(&s->type, &align);
                    size = (size + regsize - 1) & -regsize;
                    if (ret_align > align)
                        align = ret_align;
                    loc = (loc - size) & -align;
                    addr = loc;
                    offset = 0;
                    for (;;) {
                        vset(&ret.type, VT_LOCAL | VT_LVAL, addr + offset);
                        vswap();
                        vstore();
                        vtop--;
                        if (--ret_nregs == 0)
                          break;
                        offset += regsize;
                    }
                    vset(&s->type, VT_LOCAL | VT_LVAL, addr);
                }

                t = s->type.t & VT_BTYPE;
                if (t == VT_BYTE || t == VT_SHORT || t == VT_BOOL) {
#ifdef PROMOTE_RET
                    vtop->r |= BFVAL(VT_MUSTCAST, 1);
#else
                    vtop->type.t = VT_INT;
#endif
                }
            }
            if ((s->type.t & VT_BTYPE) == VT_STRUCT)
                vtop->r |= VT_NONLVAL;
            if (s->f.func_noreturn) {
                if (debug_modes)
	            mcc_tcov_block_end(mcc_state, -1);
                CODE_OFF();
	    }
        } else {
            break;
        }
    }
}

#ifndef precedence_parser

static void expr_prod(void)
{
    int t;

    unary();
    while ((t = tok) == '*' || t == '/' || t == '%') {
        next();
        unary();
        gen_op(t);
    }
}

static void expr_sum(void)
{
    int t;

    expr_prod();
    while ((t = tok) == '+' || t == '-') {
        next();
        expr_prod();
        gen_op(t);
    }
}

static void expr_shift(void)
{
    int t;

    expr_sum();
    while ((t = tok) == TOK_SHL || t == TOK_SAR) {
        next();
        expr_sum();
        gen_op(t);
    }
}

static void expr_cmp(void)
{
    int t;

    expr_shift();
    while (((t = tok) >= TOK_ULE && t <= TOK_GT) ||
           t == TOK_ULT || t == TOK_UGE) {
        next();
        expr_shift();
        gen_op(t);
    }
}

static void expr_cmpeq(void)
{
    int t;

    expr_cmp();
    while ((t = tok) == TOK_EQ || t == TOK_NE) {
        next();
        expr_cmp();
        gen_op(t);
    }
}

static void expr_and(void)
{
    expr_cmpeq();
    while (tok == '&') {
        next();
        expr_cmpeq();
        gen_op('&');
    }
}

static void expr_xor(void)
{
    expr_and();
    while (tok == '^') {
        next();
        expr_and();
        gen_op('^');
    }
}

static void expr_or(void)
{
    expr_xor();
    while (tok == '|') {
        next();
        expr_xor();
        gen_op('|');
    }
}

static void expr_landor(int op);

static void expr_land(void)
{
    expr_or();
    if (tok == TOK_LAND)
        expr_landor(tok);
}

static void expr_lor(void)
{
    expr_land();
    if (tok == TOK_LOR)
        expr_landor(tok);
}

# define expr_landor_next(op) op == TOK_LAND ? expr_or() : expr_land()
#else
# define expr_landor_next(op) unary(), expr_infix(precedence(op) + 1)
# define expr_lor() unary(), expr_infix(1)

static int precedence(int tok)
{
    switch (tok) {
        case TOK_LOR: return 1;
        case TOK_LAND: return 2;
	case '|': return 3;
	case '^': return 4;
	case '&': return 5;
	case TOK_EQ: case TOK_NE: return 6;
 relat: case TOK_ULT: case TOK_UGE: return 7;
	case TOK_SHL: case TOK_SAR: return 8;
	case '+': case '-': return 9;
	case '*': case '/': case '%': return 10;
	default:
	    if (tok >= TOK_ULE && tok <= TOK_GT)
	        goto relat;
	    return 0;
    }
}
static unsigned char prec[256];
static void init_prec(void)
{
    for (int i = 0; i < 256; i++)
	prec[i] = precedence(i);
}
#define precedence(i) ((unsigned)i < 256 ? prec[i] : 0)

static void expr_landor(int op);

static void expr_infix(int p)
{
    int t = tok, p2;
    while ((p2 = precedence(t)) >= p) {
        if (t == TOK_LOR || t == TOK_LAND) {
            expr_landor(t);
        } else {
            next();
            unary();
            if (precedence(tok) > p2)
              expr_infix(p2 + 1);
            gen_op(t);
        }
        t = tok;
    }
}
#endif

static int condition_3way(void)
{
    int c = -1;
    if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST &&
	(!(vtop->r & VT_SYM) || !vtop->sym->a.weak)) {
	vdup();
        gen_cast_s(VT_BOOL);
	c = vtop->c.i;
	vpop();
    }
    return c;
}

static void expr_landor(int op)
{
    int t = 0, cc = 1, f = 0, i = op == TOK_LAND, c;
    for(;;) {
        c = f ? i : condition_3way();
        if (c < 0)
            save_regs(1), cc = 0;
        else if (c != i)
            nocode_wanted++, f = 1;
        if (tok != op)
            break;
        if (c < 0)
            t = gvtst(i, t);
        else
            vpop();
        next();
        seqp_flush();
        expr_landor_next(op);
    }
    if (cc || f) {
        vpop();
        vpushi(i ^ f);
        gsym(t);
        nocode_wanted -= f;
    } else {
        gvtst_set(i, t);
    }
}

static int is_cond_bool(SValue *sv)
{
    if ((sv->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST
        && (sv->type.t & VT_BTYPE) == VT_INT)
        return (unsigned)sv->c.i < 2;
    if (sv->r == VT_CMP)
        return 1;
    return 0;
}

static void expr_cond(void)
{
    int tt, u, r1, r2, rc, t1, t2, islv, c, g;
    SValue sv;
    CType type;

    expr_lor();
    if (tok == '?') {
        next();
	c = condition_3way();
        seqp_flush();
        g = (tok == ':' && gnu_ext);
        tt = 0;
        if (!g) {
            if (c < 0) {
                save_regs(1);
                tt = gvtst(1, 0);
            } else {
                vpop();
            }
        } else if (c < 0) {
            save_regs(1);
            gv_dup();
            tt = gvtst(0, 0);
        }

        if (c == 0)
          nocode_wanted++;
        if (!g)
          gexpr();

        if ((vtop->type.t & VT_BTYPE) == VT_FUNC)
          mk_pointer(&vtop->type);
        sv = *vtop;
        vtop--;

        if (g) {
            u = tt;
        } else if (c < 0) {
            u = gjmp(0);
            gsym(tt);
        } else
          u = 0;

        if (c == 0)
          nocode_wanted--;
        if (c == 1)
          nocode_wanted++;
        skip(':');
        expr_cond();

        if ((vtop->type.t & VT_BTYPE) == VT_FUNC)
          mk_pointer(&vtop->type);

        if (!combine_types(&type, &sv, vtop, '?'))
          type_incompatibility_error(&sv.type, &vtop->type,
            "type mismatch in conditional expression (have '%s' and '%s')");

        if (((sv.type.t & VT_BTYPE) == VT_VOID)
            != ((vtop->type.t & VT_BTYPE) == VT_VOID))
          mcc_pedantic("ISO C forbids conditional expression with "
                       "only one void operand");

        if (CONST_WANTED && c >= 0) {
            SValue *dead = (c == 1) ? vtop : &sv;
            if ((dead->r & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
                ice_nonconst = 1;
        }

        if (c < 0 && is_cond_bool(vtop) && is_cond_bool(&sv)) {
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

        if (c != 1) {
            gen_cast(&type);
            if (islv) {
                mk_pointer(&vtop->type);
                gaddrof();
            }
        }

        rc = RC_TYPE(type.t);
        if (USING_TWO_WORDS(type.t))
          rc = RC_RET(type.t);

        tt = r2 = 0;
        if (c < 0) {
            if (type.t != VT_VOID)
                r2 = gv(rc);
            tt = gjmp(0);
        }
        gsym(u);
        if (c == 1)
          nocode_wanted--;

        if (c != 0) {
            *vtop = sv;
            gen_cast(&type);
            if (islv) {
                mk_pointer(&vtop->type);
                gaddrof();
            }
        }

        if (c < 0) {
            if (type.t != VT_VOID) {
                r1 = gv(rc);
                move_reg(r2, r1, islv ? VT_PTR : type.t);
                vtop->r = r2;
            }
            gsym(tt);
        }

        if (islv)
          indir();
    }
}

static void expr_eq(void)
{
    int t;
    int was_assign = 0;

    expr_cond();
    if ((t = tok) == '=' || TOK_ASSIGN(t)) {
        was_assign = 1;
        test_lvalue();
        if (vtop->r & VT_NONLVAL)
            mcc_error("expression is not assignable (function-call result)");
        if (t != '=' && (vtop->type.t & VT_ATOMIC_BIT)) {
            int op = TOK_ASSIGN_OP(t);
            if (atomic_rmw_size(vtop, op)) {
                next();
                expr_eq();
                gen_atomic_rmw(op, 1);
                return;
            }
            if (atomic_cas_size(vtop)) {
                next();
                expr_eq();
                gen_atomic_cas_rmw(op, 1);
                return;
            }
            mcc_error("this compound assignment to an '_Atomic' object is not "
                      "supported (float, or larger than a machine word)");
        }
        if (t == '=' && atomic_store_needs_libcall(vtop)) {
            next();
            gen_atomic_store_scalar();
            return;
        }
        if (t == '=' && atomic_store_needs_generic(vtop)) {
            next();
            gen_atomic_store_aggregate();
            return;
        }
        next();
        if (t == '=') {
            expr_eq();
        } else {
            vdup();
            expr_eq();
            gen_op(TOK_ASSIGN_OP(t));
        }
        vstore();
    }
    expr_was_assign = was_assign;
    if (was_assign)
        expr_has_effect = 1;
}

ST_FUNC void gexpr(void)
{
    expr_eq();
    if (tok == ',') {
        if (CONST_WANTED && !NOEVAL_WANTED)
            mcc_pedantic("ISO C forbids a comma operator "
                         "in a constant expression");
        do {
            vpop();
            next();
            seqp_flush();
            expr_eq();
        } while (tok == ',');

        convert_parameter_type(&vtop->type);

        if ((vtop->r & VT_LVAL) && !nocode_wanted) {
            int bt = vtop->type.t & VT_BTYPE;
            if (bt != VT_STRUCT && bt != VT_VOID && bt != VT_FUNC
                && !(vtop->type.t & (VT_ARRAY | VT_VLA))
                && !is_complex_type(&vtop->type))
                gv(RC_TYPE(vtop->type.t));
        }

        if ((vtop->r & VT_VALMASK) == VT_CONST && nocode_wanted && !CONST_WANTED)
            if (vtop->type.t != VT_VOID && (vtop->type.t & VT_BTYPE) != VT_STRUCT)
                gv(RC_TYPE(vtop->type.t));
    }
}

static void expr_const1(void)
{
    nocode_wanted += CONST_WANTED_BIT;
    expr_cond();
    nocode_wanted -= CONST_WANTED_BIT;
}

static inline int64_t expr_const64(void)
{
    int64_t c;
    expr_const1();
    if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM | VT_NONCONST)) != VT_CONST)
        expect("constant expression");
    if (is_float(vtop->type.t))
        mcc_error("integer constant expression must have integer type");
    c = vtop->c.i;
    vpop();
    return c;
}

ST_FUNC int expr_const(void)
{
    int c;
    int64_t wc = expr_const64();
    c = wc;
    if (c != wc && (unsigned)c != wc)
        mcc_error("constant exceeds 32 bit");
    return c;
}


#ifndef MCC_TARGET_ARM64
static void gfunc_return(CType *func_type)
{
    if ((func_type->t & VT_BTYPE) == VT_STRUCT) {
        CType type, ret_type;
        int ret_align, ret_nregs, regsize;
        ret_nregs = gfunc_sret(func_type, func_var, &ret_type,
                               &ret_align, &regsize);
        if (ret_nregs < 0) {
#if defined(MCC_TARGET_RISCV64) || (defined(MCC_TARGET_X86_64) && !defined(MCC_TARGET_PE))
            arch_transfer_ret_regs(0);
#endif
        } else if (0 == ret_nregs) {
            type = *func_type;
            mk_pointer(&type);
            vset(&type, VT_LOCAL | VT_LVAL, func_vc);
            indir();
            vswap();
            vstore();
        } else {
            int size, addr, align, rc, n;
            size = type_size(func_type,&align);
            if (ret_nregs * regsize > size ||
		((align & (ret_align - 1))
                 && ((vtop->r & VT_VALMASK) < VT_CONST
                     || (vtop->c.i & (ret_align - 1))
                     ))) {
		if (ret_nregs * regsize > size)
		    size = ret_nregs * regsize;
		if (ret_align > align)
		    align = ret_align;
                loc = (loc - size) & -align;
                addr = loc;
                type = *func_type;
                vset(&type, VT_LOCAL | VT_LVAL, addr);
                vswap();
                vstore();
                vpop();
                vset(&ret_type, VT_LOCAL | VT_LVAL, addr);
            }
            vtop->type = ret_type;
            rc = RC_RET(ret_type.t);
            for (n = ret_nregs; --n > 0;) {
                vdup();
                gv(rc);
                vswap();
                incr_offset(regsize);
                rc <<= 1;
            }
            gv(rc);
            vtop -= ret_nregs - 1;
        }
    } else {
        gv(RC_RET(func_type->t));
    }
    vtop--;
}
#endif

static void check_func_return(void)
{
    if ((func_vt.t & VT_BTYPE) == VT_VOID)
        return;
    if ((!strcmp(funcname, "main") || func_old)
        && (func_vt.t & VT_BTYPE) == VT_INT) {
        vpushi(0);
        gfunc_return(&func_vt);
    } else {
        mcc_warning("function might return no value: '%s'", funcname);
    }
}


static int case_cmp(uint64_t a, uint64_t b)
{
    if (cur_switch->sv.type.t & VT_UNSIGNED)
        return a < b ? -1 : a > b;
    else
        return (int64_t)a < (int64_t)b ? -1 : (int64_t)a > (int64_t)b;
}

static int case_cmp_qs(const void *pa, const void *pb)
{
    return case_cmp((*(struct case_t**)pa)->v1, (*(struct case_t**)pb)->v1);
}

static void case_sort(struct switch_t *sw)
{
    struct case_t **p;
    if (sw->n < 2)
        return;
    qsort(sw->p, sw->n, sizeof *sw->p, case_cmp_qs);
    p = sw->p;
    while (p < sw->p + sw->n - 1) {
        if (case_cmp(p[0]->v2, p[1]->v1) >= 0) {
            int l1 = p[0]->line, l2 = p[1]->line;
            mcc_error("%i:duplicate case value", l1 > l2 ? l1 : l2);
        } else if (p[0]->v2 + 1 == p[1]->v1 && p[0]->ind == p[1]->ind) {
            p[1]->v1 = p[0]->v1;
            mcc_free(p[0]);
            memmove(p, p + 1, (--sw->n - (p - sw->p)) * sizeof *p);
        } else
            ++p;
    }
}

static int gcase(struct case_t **base, int len, int dsym)
{
    struct case_t *p;
    int t, l2, e;

    t = vtop->type.t & VT_BTYPE;
    if (t != VT_LLONG)
        t = VT_INT;
    while (len) {
        l2 = len > 8 ? len/2 : 0;
        p = base[l2];
        vdup(), vpush64(t, p->v2);
        if (l2 == 0 && p->v1 == p->v2) {
            gen_op(TOK_EQ);
            gsym_addr(gvtst(0, 0), p->ind);
        } else {
            gen_op(TOK_GT);
            if (len == 1)
                dsym = gvtst(0, dsym), e = 0;
            else
                e = gvtst(0, 0);
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

static void end_switch(void)
{
    struct switch_t *sw = cur_switch;
    dynarray_reset(&sw->p, &sw->n);
    cur_switch = sw->prev;
    mcc_free(sw);
}


static void save_lvalues(void)
{
    SValue *sv = vtop;
    while (sv >= vstack) {
        if (sv->sym && (sv->r & VT_LVAL)) {
            int align, size = type_size(&sv->type, &align);
            int r2, l = get_temp_local_var(size, align, &r2);
            vset(&sv->type, VT_LOCAL | VT_LVAL, l), vtop->r2 = r2;
            vpushv(sv), *sv = vtop[-1], vstore(), --vtop;
        }
        --sv;
    }
}

static void try_call_scope_cleanup(Sym *stop)
{
    Sym *cls = cur_scope->cl.s;
    for (; cls != stop; cls = cls->next) {
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


static void try_call_cleanup_goto(Sym *cleanupstate)
{
    Sym *oc, *cc;
    int ocd, ccd;

    if (!cur_scope->cl.s)
	return;

    ocd = cleanupstate ? cleanupstate->v & ~SYM_FIELD : 0;
    for (ccd = cur_scope->cl.n, oc = cleanupstate; ocd > ccd; --ocd, oc = oc->next)
      ;
    for (cc = cur_scope->cl.s; ccd > ocd; --ccd, cc = cc->next)
      ;
    for (; cc != oc; cc = cc->next, oc = oc->next, --ccd)
      ;

    try_call_scope_cleanup(cc);
}

static void block_cleanup(struct scope *o)
{
    int jmp = 0;
    Sym *g, **pg;
    for (pg = &pending_gotos; (g = *pg) && g->c > o->cl.n;) {
        if (g->cleanup_label->r & LABEL_FORWARD) {
            Sym *pcl = g->next;
            if (!jmp)
                jmp = gjmp(0);
            gsym(pcl->jnext);
            try_call_scope_cleanup(o->cl.s);
            pcl->jnext = gjmp(0);
            if (!o->cl.n)
                goto remove_pending;
            g->c = o->cl.n;
            pg = &g->prev;
        } else {
    remove_pending:
            *pg = g->prev;
            sym_free(g);
        }
    }
    gsym(jmp);
    try_call_scope_cleanup(o->cl.s);
}


static void vla_restore(int loc)
{
    if (loc)
        gen_vla_sp_restore(loc);
}

static int vla_scope_open(int id)
{
    int i;
    if (id == 0)
        return 1;
    if (vla_track_ovf)
        return 1;
    for (i = 0; i < nb_vla_open; i++)
        if (vla_open_birth[i] == id)
            return 1;
    return 0;
}

static int vla_inner_scope(void)
{
    return nb_vla_open ? vla_open_birth[nb_vla_open - 1] : 0;
}

static void vla_leave(struct scope *o)
{
    struct scope *c = cur_scope, *v = NULL;
    for (; c != o && c; c = c->prev)
      if (c->vla.num)
        v = c;
    if (v)
      vla_restore(v->vla.locorig);
}


static void new_scope(struct scope *o)
{
    *o = *cur_scope;
    o->prev = cur_scope;
    cur_scope = o;
    cur_scope->vla.num = 0;
    cur_scope->vla_diag = 0;

    o->stdc_fp_contract = mcc_state->stdc_fp_contract;
    o->stdc_fenv_access = mcc_state->stdc_fenv_access;
    o->stdc_cx_limited  = mcc_state->stdc_cx_limited;

    o->lstk = local_stack;
    o->llstk = local_label_stack;
    ++local_scope;
}

static void prev_scope(struct scope *o, int is_expr)
{
    if (o->vla_diag) {
        nb_vla_open -= o->vla_diag;
        if (nb_vla_open < 0)
            nb_vla_open = 0;
    }

    vla_leave(o->prev);

    if (o->cl.s != o->prev->cl.s)
        block_cleanup(o->prev);

    if (debug_modes)
        mcc_debug_end_scope(o->lstk, !is_expr);

    label_pop(&local_label_stack, o->llstk, is_expr);

    if ((mcc_state->warn_unused_variable & WARN_ON) && !is_expr) {
        Sym *sm;
        for (sm = local_stack; sm && sm != o->lstk; sm = sm->prev) {
            if ((sm->r & VT_VALMASK) == VT_LOCAL && !sm->a.used
                && sm->v >= TOK_IDENT && sm->v < SYM_FIRST_ANOM
                && !(sm->type.t & VT_TYPEDEF))
                mcc_warning_c(warn_unused_variable)(
                    "%i:unused variable '%s'",
                    sm->vla_inner_id, get_tok_str(sm->v, NULL));
        }
    }

    sym_pop(&local_stack, o->lstk, is_expr);

    mcc_state->stdc_fp_contract = o->stdc_fp_contract;
    mcc_state->stdc_fenv_access = o->stdc_fenv_access;
    mcc_state->stdc_cx_limited  = o->stdc_cx_limited;

    cur_scope = o->prev;
    --local_scope;
}

static void leave_scope(struct scope *o)
{
    if (!o)
        return;
    try_call_scope_cleanup(o->cl.s);
    vla_leave(o);
}

static void new_scope_s(struct scope *o)
{
    o->lstk = local_stack;
    ++local_scope;
}

static void prev_scope_s(struct scope *o)
{
    sym_pop(&local_stack, o->lstk, 0);
    --local_scope;
}


static void lblock(int *bsym, int *csym)
{
    struct scope *lo = loop_scope, *co = cur_scope;
    int *b = co->bsym, *c = co->csym;
    if (csym) {
        co->csym = csym;
        loop_scope = co;
    }
    co->bsym = bsym;
    block(0);
    co->bsym = b;
    if (csym) {
        co->csym = c;
        loop_scope = lo;
    }
}

static void gexpr_decl(void)
{
    int v = decl(VT_JMP);
    if (v > 1 && tok != ';') {
        Sym *s = sym_find(v);
        vset(&s->type, s->r, (s->r & VT_SYM) ? 0 : s->c);
        vtop->sym = s;
    } else {
        if (v)
            skip(';');
        gexpr();
    }
}

static int tok_starts_declspec(void)
{
    switch (tok) {
    case TOK_CHAR: case TOK_VOID: case TOK_SHORT: case TOK_INT:
    case TOK_LONG: case TOK_BOOL: case TOK_COMPLEX: case TOK_IMAGINARY:
    case TOK_FLOAT: case TOK_DOUBLE: case TOK_ENUM: case TOK_STRUCT:
    case TOK_UNION: case TOK__Atomic: case TOK_UNSIGNED:
    case TOK_CONST1: case TOK_CONST2: case TOK_CONST3:
    case TOK_VOLATILE1: case TOK_VOLATILE2: case TOK_VOLATILE3:
    case TOK_SIGNED1: case TOK_SIGNED2: case TOK_SIGNED3:
    case TOK_RESTRICT1: case TOK_RESTRICT2: case TOK_RESTRICT3:
    case TOK_AUTO: case TOK_REGISTER: case TOK_EXTERN: case TOK_STATIC:
    case TOK_TYPEDEF: case TOK_INLINE1: case TOK_INLINE2: case TOK_INLINE3:
    case TOK_NORETURN3: case TOK_THREAD_LOCAL: case TOK_ALIGNAS:
    case TOK_STATIC_ASSERT: case TOK_EXTENSION:
        return 1;
    default:
        if (tok >= TOK_IDENT) {
            Sym *s = sym_find(tok);
            return s && (s->type.t & VT_TYPEDEF);
        }
        return 0;
    }
}

static void block(int flags)
{
    int a, b, c, d, e, t;
    struct scope o;
    Sym *s;
    unsigned char stdc_save_fp, stdc_save_fenv, stdc_save_cx;

again:
    t = tok;
    if (TOK_HAS_VALUE(t))
        goto expr;
    stdc_save_fp = mcc_state->stdc_fp_contract;
    stdc_save_fenv = mcc_state->stdc_fenv_access;
    stdc_save_cx = mcc_state->stdc_cx_limited;
    next();

    seqp_reset();

    if (debug_modes)
        mcc_tcov_check_line (mcc_state, 0), mcc_tcov_block_begin (mcc_state);

    if (t == TOK_IF) {
        new_scope_s(&o);
        skip('(');
        gexpr_decl();
        if (expr_was_assign)
            mcc_warning_c(warn_parentheses)("suggest parentheses around "
                "assignment used as a truth value");
        seqp_check();
        a = gvtst(1, 0);
        skip(')');
        block(0);
        if (tok == TOK_ELSE) {
            d = gjmp(0);
            gsym(a);
            next();
            block(0);
            gsym(d);
        } else {
            gsym(a);
        }
        prev_scope_s(&o);

    } else if (t == TOK_WHILE) {
        new_scope_s(&o);
        d = gind();
        skip('(');
        gexpr();
        if (expr_was_assign)
            mcc_warning_c(warn_parentheses)("suggest parentheses around "
                "assignment used as a truth value");
        seqp_check();
        a = gvtst(1, 0);
        skip(')');
        b = 0;
        lblock(&a, &b);
        gjmp_addr(d);
        gsym_addr(b, d);
        gsym(a);
        prev_scope_s(&o);

    } else if (t == '{') {
        if (debug_modes)
            mcc_debug_stabn(mcc_state, N_LBRAC, ind - func_ind);
        new_scope(&o);
        o.stdc_fp_contract = stdc_save_fp;
        o.stdc_fenv_access = stdc_save_fenv;
        o.stdc_cx_limited  = stdc_save_cx;

        while (tok == TOK_LABEL) {
            do {
                next();
                if (tok < TOK_UIDENT)
                    expect("label identifier");
                label_push(&local_label_stack, tok, LABEL_DECLARED);
                next();
            } while (tok == ',');
            skip(';');
        }

        while (tok != '}') {
	    decl(VT_LOCAL);
            if (tok != '}') {
                block(flags | STMT_COMPOUND);
            }
        }

        prev_scope(&o, flags & STMT_EXPR);
        if (debug_modes)
            mcc_debug_stabn(mcc_state, N_RBRAC, ind - func_ind);
        if (local_scope)
            next();
        else if (!nocode_wanted)
            check_func_return();

    } else if (t == TOK_RETURN) {
        if (cur_func_noreturn)
            mcc_warning("function declared 'noreturn' has a 'return' statement");
        b = (func_vt.t & VT_BTYPE) != VT_VOID;
        if (tok != ';') {
            gexpr();
            seqp_check();
            if (b) {
                gen_assign_cast(&func_vt);
            } else {
                if (vtop->type.t != VT_VOID)
                    mcc_warning("void function returns a value");
                vtop--;
            }
        } else if (b && func_old && (func_vt.t & VT_BTYPE) == VT_INT) {
            vpushi(0);
        } else if (b) {
            mcc_warning("'return' with no value");
            b = 0;
        }
        leave_scope(root_scope);
        if (b)
            gfunc_return(&func_vt);
        skip(';');
        if (tok != '}' || local_scope != 1)
            rsym = gjmp(rsym);
        if (debug_modes)
	    mcc_tcov_block_end (mcc_state, -1);
        CODE_OFF();

    } else if (t == TOK_BREAK) {
        if (!cur_scope->bsym)
            mcc_error("cannot break");
        if (cur_switch && cur_scope->bsym == cur_switch->bsym)
            leave_scope(cur_switch->scope);
        else
            leave_scope(loop_scope);
        *cur_scope->bsym = gjmp(*cur_scope->bsym);
        skip(';');

    } else if (t == TOK_CONTINUE) {
        if (!cur_scope->csym)
            mcc_error("cannot continue");
        leave_scope(loop_scope);
        *cur_scope->csym = gjmp(*cur_scope->csym);
        skip(';');

    } else if (t == TOK_FOR) {
        new_scope(&o);
        o.stdc_fp_contract = stdc_save_fp;
        o.stdc_fenv_access = stdc_save_fenv;
        o.stdc_cx_limited  = stdc_save_cx;

        skip('(');
        if (tok != ';') {
            in_for_init = 1;
            if (!decl(VT_JMP)) {
                gexpr();
                vpop();
            }
            in_for_init = 0;
        }
        seqp_flush();
        skip(';');
        a = b = 0;
        c = d = gind();
        if (tok != ';') {
            gexpr();
            if (expr_was_assign)
                mcc_warning_c(warn_parentheses)("suggest parentheses around "
                    "assignment used as a truth value");
            a = gvtst(1, 0);
        }
        seqp_flush();
        skip(';');
        if (tok != ')') {
            e = gjmp(0);
            d = gind();
            gexpr();
            seqp_check();
            vpop();
            gjmp_addr(c);
            gsym(e);
        }
        skip(')');
        lblock(&a, &b);
        gjmp_addr(d);
        gsym_addr(b, d);
        gsym(a);
        prev_scope(&o, 0);

    } else if (t == TOK_DO) {
        new_scope_s(&o);
        a = b = 0;
        d = gind();
        lblock(&a, &b);
        gsym(b);
        skip(TOK_WHILE);
        skip('(');
	gexpr();
        if (expr_was_assign)
            mcc_warning_c(warn_parentheses)("suggest parentheses around "
                "assignment used as a truth value");
        seqp_check();
        c = gvtst(0, 0);
        skip(')');
        skip(';');
	gsym_addr(c, d);
        gsym(a);
        prev_scope_s(&o);

    } else if (t == TOK_SWITCH) {
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
            mcc_error("switch value not an integer");
        skip(')');
        sw->sv = *vtop--;
        a = 0;
        b = gjmp(0);
        lblock(&a, NULL);
        a = gjmp(a);
        gsym(b);
        prev_scope_s(&o);
        if ((mcc_state->warn_switch & WARN_ON)
            && IS_ENUM(sw->sv.type.t) && !sw->def_sym) {
            Sym *ev;
            for (ev = sw->sv.type.ref->next; ev; ev = ev->next) {
                int64_t val = ev->enum_val;
                int i, covered = 0;
                for (i = 0; i < sw->n; i++)
                    if (val >= sw->p[i]->v1 && val <= sw->p[i]->v2) {
                        covered = 1;
                        break;
                    }
                if (!covered)
                    mcc_warning_c(warn_switch)(
                        "enumeration value '%s' not handled in switch",
                        get_tok_str(ev->v & ~SYM_FIELD, NULL));
            }
        }
        if (sw->nocode_wanted)
            goto skip_switch;
        case_sort(sw);
        sw->bsym = NULL;
        vpushv(&sw->sv);
        gv(RC_INT);
        d = gcase(sw->p, sw->n, 0);
        vpop();
        if (sw->def_sym)
            gsym_addr(d, sw->def_sym);
        else
            gsym(d);
    skip_switch:
        gsym(a);
        end_switch();

    } else if (t == TOK_CASE) {
        struct case_t *cr;
        if (!cur_switch)
            expect("switch");
        cr = mcc_malloc(sizeof(struct case_t));
        dynarray_add(&cur_switch->p, &cur_switch->n, cr);
        t = cur_switch->sv.type.t;
        ice_float_op = ice_nonconst = 0;
        cr->v1 = cr->v2 = value64(expr_const64(), t);
        if (ice_float_op || ice_nonconst)
            mcc_pedantic("ISO C forbids a case label that is not an integer "
                         "constant expression");
        if (tok == TOK_DOTS && gnu_ext) {
            next();
            cr->v2 = value64(expr_const64(), t);
            if (case_cmp(cr->v2, cr->v1) < 0)
                mcc_warning("empty case range");
        }
        if (!cur_switch->nocode_wanted)
            cr->ind = gind();
        cr->line = file->line_num;
        skip(':');
        if (cur_switch->vla_gpp < vla_inner_scope())
            mcc_error("switch jumps into the scope of a variably modified declaration");
        goto block_after_label;

    } else if (t == TOK_DEFAULT) {
        if (!cur_switch)
            expect("switch");
        if (cur_switch->def_sym)
            mcc_error("too many 'default'");
        cur_switch->def_sym = cur_switch->nocode_wanted ? -1 : gind();
        skip(':');
        if (cur_switch->vla_gpp < vla_inner_scope())
            mcc_error("switch jumps into the scope of a variably modified declaration");
        goto block_after_label;

    } else if (t == TOK_GOTO) {
        vla_restore(cur_scope->vla.locorig);
        if (tok == '*' && gnu_ext) {
            next();
            gexpr();
            if ((vtop->type.t & VT_BTYPE) != VT_PTR)
                expect("pointer");
            ggoto();

        } else if (tok >= TOK_UIDENT) {
	    s = label_find(tok);
            if (!s)
              s = label_push(&global_label_stack, tok, LABEL_FORWARD);
            else if (s->r == LABEL_DECLARED)
              s->r = LABEL_FORWARD;

	    if (s->r & LABEL_FORWARD) {
		if (vla_seq < s->vla_min_goto_gpp)
		    s->vla_min_goto_gpp = vla_seq;
		if (cur_scope->cl.s && !nocode_wanted) {
                    sym_push2(&pending_gotos, SYM_FIELD, 0, cur_scope->cl.n);
                    pending_gotos->cleanup_label = s;
                    s = sym_push2(&s->next, SYM_FIELD, 0, 0);
                    pending_gotos->next = s;
                }
		s->jnext = gjmp(s->jnext);
	    } else {
		if (!vla_scope_open(s->vla_inner_id))
		    mcc_error("goto jumps into the scope of a variably modified declaration");
		try_call_cleanup_goto(s->cleanupstate);
		gjmp_addr(s->jind);
	    }
	    next();

        } else {
            expect("label identifier");
        }
        skip(';');

    } else if (t == TOK_ASM1 || t == TOK_ASM2 || t == TOK_ASM3) {
#ifdef CONFIG_MCC_ASM
        asm_instr();
#else
        mcc_error("inline assembler not supported (built without CONFIG_MCC_ASM)");
#endif

    } else {
        if (tok == ':' && t >= TOK_UIDENT) {
	    next();
            s = label_find(t);
            if (s) {
                if (s->r == LABEL_DEFINED)
                    mcc_error("duplicate label '%s'", get_tok_str(s->v, NULL));
                s->r = LABEL_DEFINED;
		if (s->next) {
		    Sym *pcl;
		    for (pcl = s->next; pcl; pcl = pcl->prev)
		      gsym(pcl->jnext);
		    sym_pop(&s->next, NULL, 0);
		} else
		  gsym(s->jnext);
            } else {
                s = label_push(&global_label_stack, t, LABEL_DEFINED);
            }
            s->jind = gind();
            s->cleanupstate = cur_scope->cl.s;
            s->vla_inner_id = vla_inner_scope();
            if (s->vla_min_goto_gpp < s->vla_inner_id)
                mcc_error("goto jumps into the scope of a variably modified declaration");

    block_after_label:
            parse_attribute(NULL);

            if (tok != '}' && mcc_state->cversion < 202311
                && tok_starts_declspec())
                mcc_pedantic("a label can only be part of a statement and a "
                             "declaration is not a statement");

            if (debug_modes)
                mcc_tcov_reset_ind(mcc_state);
            vla_restore(cur_scope->vla.loc);

            if (tok != '}') {
                if (0 == (flags & STMT_COMPOUND))
                    goto again;
            } else {
                if (mcc_state->cversion < 202311
                    && (mcc_state->warn_pedantic || mcc_state->pedantic_errors))
                    mcc_pedantic("label at end of compound statement is a "
                                 "C23 feature");
                else
                    mcc_warning_c(warn_all)("deprecated use of label at end of "
                                            "compound statement");
            }
        } else {
            if (t != ';') {
                unget_tok(t);
    expr:
                seqp_reset();
                if (flags & STMT_EXPR) {
                    vpop();
                    gexpr();
                } else {
                    expr_has_effect = 0;
                    gexpr();
                    if ((mcc_state->warn_unused_value & WARN_ON)
                        && !expr_has_effect
                        && !(vtop->type.t & VT_VOLATILE))
                        mcc_warning_c(warn_unused_value)(
                            "value computed is not used");
                    vpop();
                }
                seqp_check();
                skip(';');
            }
        }
    }

    if (debug_modes)
        mcc_tcov_check_line (mcc_state, 0), mcc_tcov_block_end (mcc_state, 0);
}

static void skip_or_save_block(TokenString **str)
{
    int braces = tok == '{';
    int level = 0;
    if (str)
      *str = tok_str_alloc();

    while (1) {
	int t = tok;
        if (level == 0
            && (t == ','
             || t == ';'
             || t == '}'
             || t == ')'
             || t == ']'))
             break;
	if (t == TOK_EOF) {
	     if (str || level > 0)
	       mcc_error("unexpected end of file");
	     else
	       break;
	}
	if (str)
	  tok_str_add_tok(*str);
	next();
	if (t == '{' || t == '(' || t == '[') {
	    level++;
	} else if (t == '}' || t == ')' || t == ']') {
	    level--;
	    if (level == 0 && braces && t == '}')
	      break;
	}
    }
    if (str)
	tok_str_add(*str, TOK_EOF);
}

#define EXPR_CONST 1
#define EXPR_ANY   2

static void parse_init_elem(int expr_type)
{
    int saved_global_expr;
    switch(expr_type) {
    case EXPR_CONST:
        saved_global_expr = global_expr;
        global_expr = 1;
        expr_const1();
        global_expr = saved_global_expr;
        if (((vtop->r & (VT_VALMASK | VT_LVAL)) != VT_CONST
             && ((vtop->r & (VT_SYM|VT_LVAL)) != (VT_SYM|VT_LVAL)
                 || vtop->sym->v < SYM_FIRST_ANOM))
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

#if 1
static void init_assert(init_params *p, int offset)
{
    if (p->sec ? !NODATA_WANTED && offset > p->sec->data_offset
               : !nocode_wanted && offset > p->local_offset)
        mcc_internal_error("initializer overflow");
}
#else
#define init_assert(sec, offset)
#endif

static void init_putz(init_params *p, unsigned long c, int size)
{
    init_assert(p, c + size);
    if (p->sec) {
    } else {
        vpush_helper_func(TOK_memset);
        vseti(VT_LOCAL, c);
        vpushi(0);
        vpushs(size);
#if defined MCC_TARGET_ARM && defined MCC_ARM_EABI
        vswap();
#endif
        gfunc_call(3);
    }
}

#define DIF_FIRST     1
#define DIF_SIZE_ONLY 2
#define DIF_HAVE_ELEM 4
#define DIF_CLEAR     8

static void decl_design_delrels(Section *sec, int c, int size)
{
    ElfW_Rel *rel, *rel2, *rel_end;
    if (!sec || !sec->reloc)
        return;
    rel = rel2 = (ElfW_Rel*)sec->reloc->data;
    rel_end = (ElfW_Rel*)(sec->reloc->data + sec->reloc->data_offset);
    while (rel < rel_end) {
        if (rel->r_offset >= c && rel->r_offset < c + size) {
            sec->reloc->data_offset -= sizeof *rel;
        } else {
            if (rel2 != rel)
                memcpy(rel2, rel, sizeof *rel);
            ++rel2;
        }
        ++rel;
    }
}

static void decl_design_flex(init_params *p, Sym *ref, int index)
{
    if (ref == p->flex_array_ref) {
        if (p->flex_is_member && index >= 0 && !p->flex_warned) {
            p->flex_warned = 1;
            mcc_pedantic("initialization of a flexible array member is not "
                         "allowed in ISO C");
        }
        if (index >= ref->c)
            ref->c = index + 1;
    } else if (ref->c < 0 && index >= 0)
        mcc_error("flexible array has zero size in this context");
}

static int decl_designator(init_params *p, CType *type, unsigned long c,
                           Sym **cur_field, int flags, int al)
{
    Sym *s, *f;
    int index, index_last, align, l, nb_elems, elem_size, routed = 0;
    unsigned long corig = c;

    elem_size = 0;
    nb_elems = 1;

    if (flags & DIF_HAVE_ELEM)
        goto no_designator;

    if (gnu_ext && tok >= TOK_UIDENT) {
        l = tok, next();
        if (tok == ':')
            goto struct_field;
        unget_tok(l);
    }

    if ((tok == '[' || tok == '.') && mcc_state->cversion < 199901)
        mcc_pedantic("designated initializers are a C99 feature");

    while (nb_elems == 1 && (tok == '[' || tok == '.')) {
        if (tok == '[') {
            if (!(type->t & VT_ARRAY))
                expect("array type");
            next();
            index = index_last = expr_const();
            if (tok == TOK_DOTS && gnu_ext) {
                next();
                index_last = expr_const();
            }
            skip(']');
            s = type->ref;
            decl_design_flex(p, s, index_last);
            if (index < 0 || index_last >= s->c || index_last < index)
	        mcc_error("index exceeds array bounds or range is empty");
            if (cur_field)
		(*cur_field)->c = index_last;
            type = pointed_type(type);
            elem_size = type_size(type, &align);
            c += index * elem_size;
            nb_elems = index_last - index + 1;
        } else {
            int cumofs;
            next();
            l = tok;
        struct_field:
            next();
	    f = find_field(type, l, &cumofs);
            if (cur_field)
                *cur_field = f;
	    type = &f->type;
            c += cumofs;
        }
        if (((type->t & VT_ARRAY) || (type->t & VT_BTYPE) == VT_STRUCT)
            && (tok == '[' || tok == '.')) {
            cur_field = NULL;
            routed = 1;
            break;
        }
        cur_field = NULL;
    }
    if (!cur_field) {
        if (tok == '=') {
            next();
        } else if (!gnu_ext) {
	    expect("=");
        }
    } else {
    no_designator:
        if (type->t & VT_ARRAY) {
	    index = (*cur_field)->c;
            s = type->ref;
            decl_design_flex(p, s, index);
            if (index >= s->c)
                mcc_error("too many initializers");
            type = pointed_type(type);
            elem_size = type_size(type, &align);
            c += index * elem_size;
        } else {
            f = *cur_field;
	    while (f && (f->v & SYM_FIRST_ANOM) &&
		   is_integer_btype(f->type.t & VT_BTYPE))
	        *cur_field = f = f->next;
            if (!f)
                mcc_error("too many initializers");
	    type = &f->type;
            c += f->c;
        }
    }

    if (!elem_size)
        elem_size = type_size(type, &align);

    if (!routed && !(flags & DIF_SIZE_ONLY) && c - corig < al) {
        decl_design_delrels(p->sec, c, elem_size * nb_elems);
        flags &= ~DIF_CLEAR;
    }

    decl_initializer(p, type, c, flags & ~DIF_FIRST);

    if (!(flags & DIF_SIZE_ONLY) && nb_elems > 1) {
        Sym aref = {0};
        CType t1;
        if (p->sec || (type->t & VT_ARRAY)) {
            aref.c = elem_size;
            t1.t = VT_STRUCT, t1.ref = &aref;
            type = &t1;
        }
        if (p->sec)
            vpush_ref(type, p->sec, c, elem_size);
        else
	    vset(type, VT_LOCAL|VT_LVAL, c);
        for (int i = 1; i < nb_elems; i++) {
            vdup();
            init_putv(p, type, c + elem_size * i);
	}
        vpop();
    }

    c += nb_elems * elem_size;
    if (c - corig > al)
      al = c - corig;
    return al;
}

static void write_ldouble(unsigned char *d, void *s)
{
#ifdef MCC_CROSS_TEST
    if (LDOUBLE_SIZE >= 10) {
        double b = *(long double*)s;
        s = &b;
#else
    if (sizeof (long double) == 8 && LDOUBLE_SIZE >= 10) {
#endif
        uint64_t m = *(uint64_t*)s;
        int e = m >> 48;
        int f = e >> 4 & 0x7FF;
        m <<= 11;
        if (0 == f) {
            if (0 == m)
                goto set;
            for (f = 1; !(m & 1ULL<<63); --f)
                m <<= 1;
        }
        if (f == 0x7ff)
            f = 0x43FF;
        e = (e & 0x8000) | (f + 0x3C00);
        m |= 1ULL<<63;
    set:
    #if (defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64)
        write64le(d, m);
        write16le(d+8, e);
    #elif LDOUBLE_SIZE == 16
        write64le(d+6, m << 1);
        write16le(d+14, e);
    #endif
        ;
    } else {
    #if LDOUBLE_SIZE == 8
        double b = *(long double*)s;
        memcpy(d, &b, 8);
    #elif (__i386__ || __x86_64__) && (defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64)
        memcpy(d, s, 10);
    #elif (__i386__ || __x86_64__) && (defined MCC_TARGET_ARM64 || defined MCC_TARGET_RISCV64)
        uint64_t m = *(uint64_t*)s;
        int e = *(uint16_t*)((char*)s + 8);
        write64le(d+6, m << 1);
        write16le(d+14, e);
    #elif (__aarch64__ || __riscv) && (defined MCC_TARGET_I386 || defined MCC_TARGET_X86_64)
        uint64_t m = read64le((unsigned char*)s + 6);
        int e = read16le((unsigned char*)s + 14);
         



        if ((e & 0x7fff) && (m & 1) && 0 == ++m)
            ++e;
        write64le(d, m >> 1 | ((e & 0x7fff) ? 1ULL<<63 : 0));
        write16le(d+8, e);
    #else
        if (sizeof (long double) == LDOUBLE_SIZE)
            memcpy(d, s, LDOUBLE_SIZE);
    #endif
    }
}

static void init_putv(init_params *p, CType *type, unsigned long c)
{
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
        size = (BIT_POS(type->t) + BIT_SIZE(type->t) + 7) / 8;
    init_assert(p, c + size);

    if (sec) {
        if (is_complex_type(type) && !is_complex_type(&vtop->type)) {
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

        if ((vtop->r & VT_SYM)
            && bt != VT_PTR
            && (bt != (PTR_SIZE == 8 ? VT_LLONG : VT_INT)
                || (type->t & VT_BITFIELD))
            && !((vtop->r & VT_CONST) && vtop->sym->v >= SYM_FIRST_ANOM)
            )
            mcc_error("initializer element is not computable at load time");

        if (NODATA_WANTED) {
            vtop--;
            return;
        }

        ptr = sec->data + c;
        val = vtop->c.i;

	if ((vtop->r & (VT_SYM|VT_CONST)) == (VT_SYM|VT_CONST)
            && vtop->sym->v >= SYM_FIRST_ANOM
            && ((vtop->r & VT_LVAL)
                || bt == VT_STRUCT
                )) {
	    Section *ssec;
	    ElfSym *esym;
	    ElfW_Rel *rel;
	    esym = elfsym(vtop->sym);
	    ssec = mcc_state->sections[esym->st_shndx];
	    memmove (ptr, ssec->data + esym->st_value + (int)vtop->c.i, size);
	    if (ssec->reloc) {
                unsigned long relofs = ssec->reloc->data_offset;
		while (relofs >= sizeof(*rel)) {
                    relofs -= sizeof(*rel);
                    rel = (ElfW_Rel*)(ssec->reloc->data + relofs);
		    if (rel->r_offset >= esym->st_value + size)
		      continue;
		    if (rel->r_offset < esym->st_value)
		      break;
		    put_elf_reloca(symtab_section, sec,
				   c + rel->r_offset - esym->st_value,
				   ELFW(R_TYPE)(rel->r_info),
				   ELFW(R_SYM)(rel->r_info),
#if PTR_SIZE == 8
				   rel->r_addend
#else
				   0
#endif
				  );
		}
	    }
	} else {
            if (type->t & VT_BITFIELD) {
                int bit_pos, bit_size, bits, n;
                unsigned char *p, v, m;
                bit_pos = BIT_POS(vtop->type.t);
                bit_size = BIT_SIZE(vtop->type.t);
                p = (unsigned char*)ptr + (bit_pos >> 3);
                bit_pos &= 7, bits = 0;
                while (bit_size) {
                    n = 8 - bit_pos;
                    if (n > bit_size)
                        n = bit_size;
                    v = val >> bits << bit_pos;
                    m = ((1 << n) - 1) << bit_pos;
                    *p = (*p & ~m) | (v & m);
                    bits += n, bit_size -= n, bit_pos = 0, ++p;
                }
            } else
            switch(bt) {
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

#if PTR_SIZE == 8
	    case VT_LLONG:
	    case VT_PTR:
	        if (vtop->r & VT_SYM)
	          greloca(sec, vtop->sym, c, R_DATA_PTR, val);
	        else
	          write64le(ptr, val);
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
	          greloc(sec, vtop->sym, c, R_DATA_PTR);
	        write32le(ptr, val);
	        break;
#endif
	    default:
                break;
	    }
	}
        vtop--;
    } else {
        vset(&dtype, VT_LOCAL|VT_LVAL, c);
        vswap();
        vstore();
        vpop();
    }
}

static void decl_initializer(init_params *p, CType *type, unsigned long c, int flags)
{
    int len, n, no_oblock;
    int size1, align1;
    Sym *s, *f;
    Sym indexsym;
    CType *t1;

    if (debug_modes && !(flags & DIF_SIZE_ONLY) && !p->sec)
        mcc_debug_line(mcc_state), mcc_tcov_check_line (mcc_state, 1);

    if (!(flags & DIF_HAVE_ELEM) && tok != '{' &&
	tok != TOK_LSTR && tok != TOK_STR && tok != TOK_U32STR && tok != TOK_U16STR &&
	tok != TOK_U8STR &&
	!(((type->t & VT_ARRAY) || (type->t & VT_BTYPE) == VT_STRUCT)
	  && (tok == '[' || tok == '.')) &&
	(!(flags & DIF_SIZE_ONLY)
            || (type->t & VT_BTYPE) == VT_STRUCT)
        ) {
        int ncw_prev = nocode_wanted;
        if ((flags & DIF_SIZE_ONLY) && !p->sec)
            ++nocode_wanted;
	parse_init_elem(!p->sec ? EXPR_ANY : EXPR_CONST);
        nocode_wanted = ncw_prev;
        flags |= DIF_HAVE_ELEM;
    }

    if (type->t & VT_ARRAY) {
        no_oblock = 1;
        if (((flags & DIF_FIRST) && tok != TOK_LSTR && tok != TOK_STR
             && tok != TOK_U32STR && tok != TOK_U16STR && tok != TOK_U8STR) ||
            tok == '{') {
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
            ) || (tok == TOK_U32STR && (t1->t & VT_BTYPE) == VT_INT)
              || (tok == TOK_U16STR && (t1->t & VT_BTYPE) == VT_SHORT
                  && (t1->t & VT_UNSIGNED))
              || ((tok == TOK_STR || tok == TOK_U8STR)
                  && (t1->t & VT_BTYPE) == VT_BYTE)) {
	    len = 0;
            cstr_reset(&initstr);
            {
              int merge_kind = (tok == TOK_STR) ? 0 : tok;
              while (tok == TOK_STR || tok == TOK_LSTR
                     || tok == TOK_U16STR || tok == TOK_U32STR
                     || tok == TOK_U8STR) {
                int toksz = tok == TOK_STR || tok == TOK_U8STR ? 1
                          : tok == TOK_U16STR ? 2
                          : tok == TOK_U32STR ? 4 : sizeof(nwchar_t);
                int nch = tokc.str.size / toksz, i;
                if (tok != TOK_STR) {
                    if (merge_kind && merge_kind != tok)
                        mcc_error("unsupported concatenation of string literals "
                                  "with different encoding prefixes");
                    merge_kind = tok;
                }
                if (toksz > size1)
                    mcc_error("a wide string literal cannot follow a narrower "
                              "string literal in a concatenation");
                if (initstr.size)
                    initstr.size -= size1;
                len += nch - 1;
                if (toksz == size1) {
                    cstr_cat(&initstr, tokc.str.data, tokc.str.size);
                } else {
                    for (i = 0; i < nch; i++) {
                        unsigned int ch =
                            toksz == 1 ? ((unsigned char *)tokc.str.data)[i]
                          : toksz == 2 ? ((unsigned short *)tokc.str.data)[i]
                          : ((unsigned int *)tokc.str.data)[i];
                        if (size1 == 1) { unsigned char v = ch; cstr_cat(&initstr, (char *)&v, 1); }
                        else if (size1 == 2) { unsigned short v = ch; cstr_cat(&initstr, (char *)&v, 2); }
                        else { unsigned int v = ch; cstr_cat(&initstr, (char *)&v, 4); }
                    }
                }
                next();
              }
            }
            if (tok != ')' && tok != '}' && tok != ',' && tok != ';'
                && tok != TOK_EOF) {
                unget_tok(size1 == 1 ? TOK_STR
                          : size1 == 2 ? TOK_U16STR
                          : size1 == 4 ? TOK_U32STR : TOK_LSTR);
                tokc.str.size = initstr.size;
                tokc.str.data = initstr.data;
                goto do_init_array;
            }

            decl_design_flex(p, s, len);
            if (!(flags & DIF_SIZE_ONLY)) {
                int nb = n, ch;
                if (len < nb)
                    nb = len;
                if (len > nb)
                  mcc_warning("initializer-string for array is too long");
                if (p->sec && size1 == 1) {
                    init_assert(p, c + nb);
                    if (!NODATA_WANTED)
                      memcpy(p->sec->data + c, initstr.data, nb);
                } else {
                    for(int i=0;i<n;i++) {
                        if (i >= nb) {
                          if (flags & DIF_CLEAR)
                            break;
                          if (n - i >= 4) {
                            init_putz(p, c + i * size1, (n - i) * size1);
                            break;
                          }
                          ch = 0;
                        } else if (size1 == 1)
                          ch = ((unsigned char *)initstr.data)[i];
                        else if (size1 == 2)
                          ch = ((unsigned short *)initstr.data)[i];
                        else
                          ch = ((unsigned int *)initstr.data)[i];
                        vpushi(ch);
                        init_putv(p, t1, c + i * size1);
                    }
                }
            }
            if (tok == ',' && !no_oblock)
                next();
        } else if (no_oblock && !(t1->t & VT_ARRAY)
                   && (tok == TOK_STR || tok == TOK_LSTR
                       || tok == TOK_U16STR || tok == TOK_U32STR
                       || tok == TOK_U8STR)) {
            char buf[64];
            type_to_str(buf, sizeof(buf), t1, NULL);
            mcc_error("cannot initialize array of '%s' from a string literal "
                      "of a different character type", buf);
        } else {

          do_init_array:
	    indexsym.c = 0;
	    f = &indexsym;

          do_init_list:
            if (!(flags & (DIF_CLEAR | DIF_SIZE_ONLY))) {
                init_putz(p, c, n*size1);
                flags |= DIF_CLEAR;
            }

	    len = 0;
            decl_design_flex(p, s, len - 1);
	    {
	    int sublist_comma = 0;
	    while (tok != '}' || (flags & DIF_HAVE_ELEM)) {
		if (no_oblock && sublist_comma && !(flags & DIF_HAVE_ELEM)) {
		    int climb = 0;
		    if (tok == '[' && !(type->t & VT_ARRAY))
			climb = 1;
		    else if (tok == '.') {
			if (type->t & VT_ARRAY) {
			    climb = 1;
			} else {
			    int cumofs;
			    next();
			    if (tok >= TOK_UIDENT
				&& !find_field(type, tok | SYM_FIELD, &cumofs))
				climb = 1;
			    unget_tok('.');
			}
		    }
		    if (climb) {
			unget_tok(',');
			break;
		    }
		}
		len = decl_designator(p, type, c, &f, flags, len);
		flags &= ~DIF_HAVE_ELEM;
		if (type->t & VT_ARRAY) {
		    ++indexsym.c;
		    if (no_oblock && len >= n*size1)
		        break;
		} else {
		    if (s->type.t == VT_UNION)
		        f = NULL;
		    else
		        f = f->next;
		    if (no_oblock && f == NULL)
		        break;
		}

		if (tok == '}')
		    break;
		skip(',');
		sublist_comma = 1;
		seqp_flush();
	    }
	    }
        }
        if (!no_oblock)
            skip('}');

    } else if ((flags & DIF_HAVE_ELEM)
            && (is_compatible_unqualified_types(type, &vtop->type)
                || (is_complex_type(type)
                    && (is_complex_type(&vtop->type)
                        || is_integer_btype(vtop->type.t & VT_BTYPE)
                        || is_float(vtop->type.t))))) {
        goto one_elem;

    } else if ((type->t & VT_BTYPE) == VT_STRUCT) {
        no_oblock = 1;
        if ((flags & DIF_FIRST) || tok == '{') {
            skip('{');
            no_oblock = 0;
        }
        s = type->ref;
        f = s->next;
        n = s->c;
        size1 = 1;
	goto do_init_list;

    } else if (tok == '{') {
        if (flags & DIF_HAVE_ELEM)
          skip(';');
        next();
        if (tok == '{') {
            if (mcc_state->warn_pedantic || mcc_state->pedantic_errors)
                mcc_pedantic("too many braces around scalar initializer");
            else
                mcc_warning_c(warn_all)(
                    "too many braces around scalar initializer");
        }
        decl_initializer(p, type, c, flags & ~DIF_HAVE_ELEM);
        skip('}');

    } else one_elem: if ((flags & DIF_SIZE_ONLY)) {
        if (flags & DIF_HAVE_ELEM)
            vpop();
        else
            skip_or_save_block(NULL);

    } else {
	if (!(flags & DIF_HAVE_ELEM)) {
	    if (tok != TOK_STR && tok != TOK_LSTR && tok != TOK_U32STR
		&& tok != TOK_U16STR && tok != TOK_U8STR)
	      expect("string constant");
	    parse_init_elem(!p->sec ? EXPR_ANY : EXPR_CONST);
	}
        if (!p->sec && (flags & DIF_CLEAR)
            && (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST
            && vtop->c.i == 0
            && btype_size(type->t & VT_BTYPE)
            )
            vpop();
        else
            init_putv(p, type, c);
    }
}

static TokenString *gather_string_run(CType *type, int has_init)
{
#define MCC_STRSZ(k) ((k)==TOK_STR||(k)==TOK_U8STR?1:(k)==TOK_U16STR?2:(k)==TOK_U32STR?4:(int)sizeof(nwchar_t))
    TokenString *istr = tok_str_alloc();
    CString *parts = NULL;
    int *pkinds = NULL, nparts = 0, firstsz = 0, wsz = 0, wkind = TOK_STR;
    while (tok == TOK_STR || tok == TOK_LSTR
           || tok == TOK_U32STR || tok == TOK_U16STR || tok == TOK_U8STR) {
        int sz = MCC_STRSZ(tok);
        tok_str_add_tok(istr);
        parts = mcc_realloc(parts, (nparts + 1) * sizeof(CString));
        pkinds = mcc_realloc(pkinds, (nparts + 1) * sizeof(int));
        cstr_new(&parts[nparts]);
        cstr_cat(&parts[nparts], tokc.str.data, tokc.str.size);
        pkinds[nparts] = tok;
        if (!firstsz) firstsz = sz;
        if (sz > wsz) { wsz = sz; wkind = tok; }
        nparts++;
        next();
    }
    tok_str_add(istr, TOK_EOF);
    if (firstsz && wsz > firstsz) {
        int wet = wkind == TOK_U16STR ? (VT_SHORT | VT_UNSIGNED)
                : wkind == TOK_U32STR ? (VT_INT | VT_UNSIGNED)
#ifdef MCC_TARGET_PE
                : (VT_SHORT | VT_UNSIGNED);
#else
                : VT_INT;
#endif
        CString cc;
        int merge_kind = 0, j, i, save_tok = tok;
        CValue save_tokc = tokc;
        cstr_new(&cc);
        for (j = 0; j < nparts; j++) {
            int tk = pkinds[j], toksz = MCC_STRSZ(tk);
            int nch = parts[j].size / toksz;
            if (tk != TOK_STR) {
                if (merge_kind && merge_kind != tk)
                    mcc_error("unsupported concatenation of string "
                              "literals with different encoding prefixes");
                merge_kind = tk;
            }
            for (i = 0; i < nch - 1; i++) {
                unsigned int ch =
                    toksz == 1 ? ((unsigned char *)parts[j].data)[i]
                  : toksz == 2 ? ((unsigned short *)parts[j].data)[i]
                  : ((unsigned int *)parts[j].data)[i];
                if (wsz == 2) { unsigned short v = ch; cstr_cat(&cc, (char *)&v, 2); }
                else { unsigned int v = ch; cstr_cat(&cc, (char *)&v, 4); }
            }
        }
        { unsigned int z = 0; cstr_cat(&cc, (char *)&z, wsz); }
        tok_str_free(istr);
        istr = tok_str_alloc();
        tok = wkind; tokc.str.size = cc.size; tokc.str.data = cc.data;
        tok_str_add_tok(istr);
        tok = save_tok; tokc = save_tokc;
        tok_str_add(istr, TOK_EOF);
        cstr_free(&cc);
        if (has_init == 2 && (type->t & VT_ARRAY))
            type->ref->type.t = wet | (type->ref->type.t & VT_CONSTANT);
    }
    for (int j = 0; j < nparts; j++)
        cstr_free(&parts[j]);
    mcc_free(parts);
    mcc_free(pkinds);
    return istr;
#undef MCC_STRSZ
}

static void decl_initializer_alloc(CType *type, AttributeDef *ad, int r,
                                   int has_init, int v, int scope)
{
    int size, align, addr;
    TokenString *init_str = NULL;

    Section *sec;
    Sym *flexible_array;
    Sym *sym = NULL;
    int saved_nocode_wanted = nocode_wanted;
#ifdef CONFIG_MCC_BCHECK
    int bcheck = mcc_state->do_bounds_check && !NODATA_WANTED;
#endif
    init_params p = {0};

    if (scope == VT_CONST) {
        sym = sym_find(v);
        if (sym) {
            patch_storage(sym, ad, type);
            if (!has_init && sym->c && elfsym(sym)->st_shndx != SHN_UNDEF)
                return;
            type = &sym->type;
        }
    }

    if (v && (r & VT_VALMASK) == VT_CONST)
        nocode_wanted |= DATA_ONLY_WANTED;

    flexible_array = NULL;
    size = type_size(type, &align);


    if (size < 0) {
        if (!(type->t & VT_ARRAY))
            mcc_error("initialization of incomplete type");
        if (IS_BT_ARRAY(type->t))
            type->ref = sym_push(SYM_FIELD, &type->ref->type, 0, type->ref->c);
        p.flex_array_ref = type->ref;

    } else if (has_init && (type->t & VT_BTYPE) == VT_STRUCT) {
        Sym *field = type->ref->next;
        if (field) {
            while (field->next)
                field = field->next;
            if (field->type.t & VT_ARRAY && field->type.ref->c < 0) {
                flexible_array = field;
                p.flex_array_ref = field->type.ref;
                p.flex_is_member = 1;
                size = -1;
            }
        }
    }

    if (size < 0) {
        if (!has_init)
            goto err_size;
        if (has_init == 2
            || tok == TOK_STR || tok == TOK_LSTR
            || tok == TOK_U16STR || tok == TOK_U32STR || tok == TOK_U8STR) {
            init_str = gather_string_run(type, has_init);
        } else
            skip_or_save_block(&init_str);
        unget_tok(0);

        begin_macro(init_str, 1);
        next();
        decl_initializer(&p, type, 0, DIF_FIRST | DIF_SIZE_ONLY);
        macro_ptr = init_str->str;
        next();

        size = type_size(type, &align);
        if (size < 0)
    err_size:
            mcc_error("unknown type size");

        if (flexible_array && flexible_array->type.ref->c > 0)
            size += flexible_array->type.ref->c
                    * pointed_size(&flexible_array->type);
    }

    if (ad->a.aligned) {
	int speca = 1 << (ad->a.aligned - 1);
        if (speca > align)
            align = speca;
    } else if (ad->a.packed) {
        align = 1;
    }

    if (!v && NODATA_WANTED)
        size = 0, align = 1;

    if ((r & VT_VALMASK) == VT_LOCAL) {
        sec = NULL;
#ifdef CONFIG_MCC_BCHECK
        if (bcheck && v) {
            loc -= align;
        }
#endif
        loc = (loc - size) & -align;
        addr = loc;
        p.local_offset = addr + size;
#ifdef CONFIG_MCC_BCHECK
        if (bcheck && v) {
            loc -= align;
        }
#endif
        if (v) {
#ifdef CONFIG_MCC_ASM
	    if (ad->asm_label) {
		int reg = asm_parse_regvar(ad->asm_label);
		if (reg >= 0)
		    r = (r & ~VT_VALMASK) | reg;
	    }
#endif
            sym = sym_push(v, type, r, addr);
	    if (ad->cleanup_func) {
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
                sym->a.inited = 1;
        } else {
            vset(type, r, addr);
        }
    } else {
        sec = ad->section;
        if (!sec) {
            CType *tp = type;
            while ((tp->t & (VT_BTYPE|VT_ARRAY)) == (VT_PTR|VT_ARRAY))
                tp = &tp->ref->type;
            if (type->t & VT_TLS) {
                if (has_init)
                    sec = tdata_section;
                else
                    sec = tbss_section;
            } else if (tp->t & VT_CONSTANT) {
		sec = rodata_section;
            } else if (has_init) {
		sec = data_section;
            } else if (mcc_state->nocommon)
                sec = bss_section;
        }

        if (sec) {
	    addr = section_add(sec, size, align);
#ifdef CONFIG_MCC_BCHECK
            if (bcheck)
                section_add(sec, 1, 1);
#endif
        } else {
            addr = align;
	    sec = common_section;
        }

        if (v) {
            if (!sym) {
                sym = sym_push(v, type, r | VT_SYM, 0);
                patch_storage(sym, ad, NULL);
            }
	    put_extern_sym(sym, sec, addr, size);
        } else {
            vpush_ref(type, sec, addr, size);
            sym = vtop->sym;
	    vtop->r |= r;
        }

#ifdef CONFIG_MCC_BCHECK
        if (bcheck) {
            addr_t *bounds_ptr;

            greloca(bounds_section, sym, bounds_section->data_offset, R_DATA_PTR, 0);
            bounds_ptr = section_ptr_add(bounds_section, 2 * sizeof(addr_t));
            bounds_ptr[0] = 0;
            bounds_ptr[1] = size;
        }
#endif
    }

    if (!(type->t & VT_VLA) && type_is_vm(type)) {
        if (nb_vla_open < VLA_TRACK_MAX)
            vla_open_birth[nb_vla_open++] = ++vla_seq;
        else
            vla_track_ovf = 1;
        cur_scope->vla_diag++;
    }

    if (type->t & VT_VLA) {
        int a;

        if (has_init)
            mcc_error("variable length array cannot be initialized");

        if (nb_vla_open < VLA_TRACK_MAX)
            vla_open_birth[nb_vla_open++] = ++vla_seq;
        else
            vla_track_ovf = 1;
        cur_scope->vla_diag++;

        if (NODATA_WANTED)
            goto no_alloc;

        if (cur_scope->vla.num == 0) {
            if (cur_scope->prev && cur_scope->prev->vla.num) {
                cur_scope->vla.locorig = cur_scope->prev->vla.loc;
            } else {
                gen_vla_sp_save(loc -= PTR_SIZE);
                cur_scope->vla.locorig = loc;
            }
        }

        vpush_type_size(type, &a);
        gen_vla_alloc(type, a);
#if defined MCC_TARGET_PE && defined MCC_TARGET_X86_64
        gen_vla_result(addr), addr = (loc -= PTR_SIZE);
#endif
        gen_vla_sp_save(addr);
        cur_scope->vla.loc = addr;
        cur_scope->vla.num++;
    } else if (has_init) {
        p.sec = sec;
        if (!init_str && size >= 0 && (type->t & VT_ARRAY)
            && (tok == TOK_STR || tok == TOK_LSTR
                || tok == TOK_U16STR || tok == TOK_U32STR || tok == TOK_U8STR)) {
            int el_al, el_sz = type_size(pointed_type(type), &el_al);
            int firstsz = tok == TOK_STR || tok == TOK_U8STR ? 1
                        : tok == TOK_U16STR ? 2
                        : tok == TOK_U32STR ? 4 : (int)sizeof(nwchar_t);
            if (el_sz > firstsz) {
                init_str = gather_string_run(type, has_init);
                unget_tok(0);
                begin_macro(init_str, 1);
                next();
            }
        }
        seqp_reset();
        decl_initializer(&p, type, addr, DIF_FIRST);
        seqp_check();
        if (flexible_array)
            flexible_array->type.ref->c = -1;
    }

 no_alloc:
    if (init_str) {
        end_macro();
        next();
    }

    nocode_wanted = saved_nocode_wanted;
}

static void func_vla_arg_code(Sym *arg)
{
    int align;
    TokenString *vla_array_tok = NULL;

    if (arg->type.ref)
        func_vla_arg_code(arg->type.ref);

    if ((arg->type.t & VT_VLA) && arg->type.ref->vla_array_str) {
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
	vset(&int_type, VT_LOCAL|VT_LVAL, arg->type.ref->c);
	vswap();
	vstore();
	vpop();
    }
}

static void func_vla_arg(Sym *sym)
{
    Sym *arg;

    for (arg = sym->type.ref->next; arg; arg = arg->next)
        if ((arg->type.t & VT_BTYPE) == VT_PTR && (arg->type.ref->type.t & VT_VLA))
            func_vla_arg_code(arg->type.ref);
}

ST_FUNC Sym *gfunc_set_param(Sym *s, int c, int byref)
{
    s = sym_find(s->v);
    if (!s)
        return NULL;
    s->c = c;
    if (byref)
        s->r = VT_LLOCAL | VT_LVAL;
    return s;
}

static void sym_push_params(Sym *ref)
{
    Sym *s = ref;
    while (s->next)
        s = s->next;
    while (s != ref) {
        if ((s->v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
            sym_copy(s, &local_stack);
        s = s->prev;
    }
}

static void gen_function(Sym *sym)
{
    struct scope f = { 0 };

    cur_scope = root_scope = &f;
    nocode_wanted = 0;

    ind = cur_text_section->data_offset;
    if (sym->a.aligned) {
	size_t newoff = section_add(cur_text_section, 0,
				    1 << (sym->a.aligned - 1));
	gen_fill_nops(newoff - ind);
    }

    funcname = get_tok_str(sym->v, NULL);
    func_ind = ind;
    func_vt = sym->type.ref->type;
    func_var = sym->type.ref->f.func_type == FUNC_ELLIPSIS;
    cur_func_last_param = 0;
    if (func_var) {
        Sym *pa;
        for (pa = sym->type.ref->next; pa; pa = pa->next)
            cur_func_last_param = pa->v & ~SYM_FIELD;
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
        add_array (mcc_state, ".init_array", sym->c);
    if (sym->type.ref->f.func_dtor)
        add_array (mcc_state, ".fini_array", sym->c);

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
    block(0);
    if ((mcc_state->warn_unused_parameter & WARN_ON)
        && sym->type.ref->f.func_type != FUNC_OLD) {
        Sym *pc;
        for (pc = local_stack; pc; pc = pc->prev) {
            if ((pc->r & VT_VALMASK) == VT_LOCAL && !pc->a.used
                && pc->v >= TOK_IDENT && pc->v < SYM_FIRST_ANOM
                && !(pc->type.t & VT_TYPEDEF))
                mcc_warning_c(warn_unused_parameter)(
                    "%i:unused parameter '%s'",
                    pc->vla_inner_id, get_tok_str(pc->v, NULL));
        }
    }
    cur_func_inline_extern = 0;
    gsym(rsym);
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

static void gen_inline_functions(MCCState *s)
{
    Sym *sym;
    int inline_generated;
    struct InlineFunc *fn;

    mcc_open_bf(s, ":inline:", 0);
    do {
        inline_generated = 0;
        for (int i = 0; i < s->nb_inline_fns; ++i) {
            fn = s->inline_fns[i];
            sym = fn->sym;
            if (sym && (sym->c || !(sym->type.t & VT_INLINE))) {
                fn->sym = NULL;
                mccpp_putfile(fn->filename);
                begin_macro(fn->func_str, 1);
                next();
                cur_text_section = text_section;
                gen_function(sym);
                end_macro();

                inline_generated = 1;
            }
        }
    } while (inline_generated);
    mcc_close();
}

static void free_inline_functions(MCCState *s)
{
    for (int i = 0; i < s->nb_inline_fns; ++i) {
        struct InlineFunc *fn = s->inline_fns[i];
        if (fn->sym)
            tok_str_free(fn->func_str);
    }
    dynarray_reset(&s->inline_fns, &s->nb_inline_fns);
}

static void do_Static_assert(void)
{
    int c;
    const char *msg;

    if (mcc_state->cversion < 201112)
        mcc_pedantic("ISO C does not support '_Static_assert' before C11");
    next();
    skip('(');
    c = expr_const();
    msg = "_Static_assert fail";
    if (tok == ',') {
        next();
        msg = parse_mult_str("string constant")->data;
    }
    skip(')');
    if (c == 0)
        mcc_error("%s", msg);
    skip(';');
}

#ifdef MCC_TARGET_PE
static void pe_check_linkage(CType *type, AttributeDef *ad)
{
    if (!ad->a.dllimport && !ad->a.dllexport)
        return;
    if (type->t & VT_STATIC)
        mcc_error("cannot have dll linkage with static");
    if (type->t & VT_TYPEDEF) {
        const char *m = ad->a.dllimport ? "im" : "ex";
        mcc_warning("'dll%sport' attribute ignored for typedef", m);
        ad->a.dllimport = 0;
        ad->a.dllexport = 0;
    } else if (ad->a.dllimport) {
        if ((type->t & VT_BTYPE) == VT_FUNC)
            ad->a.dllimport = 0;
        else
            type->t |= VT_EXTERN;
    }
}
#endif

static int decl(int l)
{
    int v, has_init, r, oldint;
    CType type, btype;
    Sym *sym, *sa;
    AttributeDef ad, adbase;
    ElfSym *esym;

    while (1) {

        oldint = 0;
        if (parse_btype(&btype, &adbase, l == VT_LOCAL)) {
            if (adbase.implicit_int)
                mcc_warning_c(warn_implicit_int)("type defaults to 'int' in declaration");
        } else {
            if (l == VT_JMP)
                return 0;
            if (tok == ';' && l != VT_CMP) {
                if (l == VT_CONST)
                    mcc_pedantic("ISO C does not allow an empty declaration");
                next();
                continue;
            }
            if (tok == TOK_STATIC_ASSERT) {
                do_Static_assert();
                continue;
            }
            if (l != VT_CONST)
                break;
            if (tok == TOK_ASM1 || tok == TOK_ASM2 || tok == TOK_ASM3) {
#ifdef CONFIG_MCC_ASM
                asm_global_instr();
                continue;
#else
                mcc_error("assembler not supported (built without CONFIG_MCC_ASM)");
#endif
            }
            if (tok >= TOK_UIDENT) {
                btype.t = VT_INT;
                oldint = 1;
            } else {
                if (tok != TOK_EOF)
                    expect("declaration");
                break;
            }
        }

        if (tok == ';') {
            if ((btype.t & VT_BTYPE) == VT_STRUCT
                && (btype.ref->v & ~SYM_STRUCT) < SYM_FIRST_ANOM)
                ;
            else if (IS_ENUM(btype.t))
                ;
            else
                mcc_warning("useless type defines no instances");
            if (l == VT_JMP)
                return 1;
            next();
            continue;
        }

        while (1) {
            type = btype;
	    ad = adbase;
            type_decl(&type, &ad, &v, l == VT_CMP ? TYPE_DIRECT | TYPE_PARAM : TYPE_DIRECT);
            if ((type.t & VT_BTYPE) == VT_FUNC) {
                if (oldint)
                    mcc_warning_c(warn_implicit_int)("return type defaults to 'int'");
                if ((type.t & VT_STATIC) && (l != VT_CONST))
                    mcc_error("function without file scope cannot be static");
                sym = type.ref;
                if (sym->f.func_type == FUNC_OLD && l == VT_CONST) {
                    func_vt = type;
		    ++local_scope;
                    decl(VT_CMP);
		    --local_scope;
                }
                if ((type.t & (VT_EXTERN|VT_INLINE)) == (VT_EXTERN|VT_INLINE)) {
                    if (mcc_state->gnu89_inline || sym->f.func_alwinl)
                        type.t = (type.t & ~VT_EXTERN) | VT_STATIC;
                    else
                        type.t &= ~VT_INLINE;
                }

            } else if (oldint) {
                mcc_warning("type defaults to int");
            }

            if (in_for_init && (type.t & (VT_STATIC | VT_EXTERN | VT_TYPEDEF)))
                mcc_pedantic("ISO C forbids a 'static', 'extern' or 'typedef' "
                             "declaration in a 'for' loop initializer");

            if (l == VT_CONST && (ad.storage_class & 1))
                mcc_error("file-scope declaration of '%s' specifies 'auto'",
                          get_tok_str(v, NULL));
            if (l == VT_CONST && (ad.storage_class & 2))
                mcc_error("file-scope declaration of '%s' specifies 'register'",
                          get_tok_str(v, NULL));
            if ((ad.storage_class & 4) && !(type.t & VT_TYPEDEF)
                && ((type.t & VT_BTYPE) != VT_PTR || (type.t & VT_ARRAY)))
                mcc_error("'restrict' requires a pointer type");
            if ((ad.storage_class & 8) && ad.a.aligned) {
                int nat;
                if (type.t & VT_TYPEDEF)
                    mcc_error("'_Alignas' specified for a typedef");
                else if ((type.t & VT_BTYPE) == VT_FUNC)
                    mcc_error("'_Alignas' specified for a function");
                else if (ad.storage_class & 2)
                    mcc_error("'_Alignas' specified for a 'register' object");
                else if ((type_size(&type, &nat), nat > 0)
                         && (1 << (ad.a.aligned - 1)) < nat)
                    mcc_error("requested alignment is less than the minimum "
                              "alignment of the type");
            }
            if ((type.t & VT_BTYPE) != VT_FUNC && (type.t & VT_INLINE))
                mcc_error("'inline' used outside of a function declaration");
            if ((type.t & VT_VLA) && (type.t & VT_EXTERN)
                && (type.t & VT_BTYPE) != VT_FUNC)
                mcc_error("object with variably modified type must have no linkage");
            if ((ad.storage_class & 128) && (type.t & VT_BTYPE) != VT_FUNC)
                mcc_pedantic("'_Noreturn' used outside of a function declaration");
            if (type.t & VT_TLS) {
                if (type.t & VT_TYPEDEF)
                    mcc_error("'_Thread_local' used with 'typedef'");
                else if ((type.t & VT_BTYPE) == VT_FUNC)
                    mcc_error("'_Thread_local' applied to a function");
                else if (l != VT_CONST && !(type.t & (VT_STATIC | VT_EXTERN)))
                    mcc_error("'_Thread_local' at block scope requires "
                              "'static' or 'extern'");
            }

            if (l != VT_CONST && (type.t & VT_BTYPE) == VT_FUNC
                && (ad.storage_class & 3))
                mcc_error("invalid storage class for function '%s'",
                          get_tok_str(v, NULL));

            if (gnu_ext && (tok == TOK_ASM1 || tok == TOK_ASM2 || tok == TOK_ASM3)) {
                ad.asm_label = asm_label_instr();
                parse_attribute(&ad);
            #if 0
                if (tok == '{')
                    expect(";");
            #endif
            }

#ifdef MCC_TARGET_PE
            pe_check_linkage(&type, &ad);
#endif
            if (tok == '{') {
                if (l != VT_CONST)
                    mcc_error("cannot use local functions");
                if (type.t & VT_TYPEDEF)
                    mcc_error("function definition declared 'typedef'");
                if ((type.t & VT_BTYPE) != VT_FUNC)
                    expect("function definition");
                if (type.ref->f.func_star_param)
                    mcc_error("'[*]' not allowed in a function definition");

                if (ad.f.func_type == 0)
                    mcc_error("function definition declared with a typedef'd function type");
                merge_funcattr(&type.ref->f, &ad.f);
                type.t &= ~VT_EXTERN;
                sym = external_sym(v, &type, 0, &ad);

                {
                    CType *rt = &sym->type.ref->type;
                    if ((rt->t & VT_BTYPE) != VT_VOID
                        && ((rt->t & VT_BTYPE) == VT_STRUCT || IS_ENUM(rt->t))
                        && rt->ref->c < 0)
                        mcc_error("return type is an incomplete type");
                }

                for (sa = sym->type.ref; (sa = sa->next) != NULL;) {
                    if (!(sa->v & ~SYM_FIELD))
                        expect("identifier");
                    if (((sa->type.t & VT_BTYPE) == VT_STRUCT || IS_ENUM(sa->type.t))
                        && sa->type.ref->c < 0)
                        mcc_error("parameter '%s' has incomplete type",
                                  get_tok_str(sa->v & ~SYM_FIELD, NULL));
                    if (sym->type.ref->f.func_type == FUNC_OLD) {
                        if (sa->type.t & VT_EXTERN)
                            mcc_warning_c(warn_implicit_int)("type of '%s' defaults to 'int'",
                                        get_tok_str(sa->v & ~SYM_FIELD, NULL));
                        if (sa->type.t == VT_FLOAT)
                            sa->type.t = VT_DOUBLE;
                    }
                }

                if (sym->type.t & VT_INLINE) {
                    struct InlineFunc *fn;
                    fn = mcc_malloc(sizeof *fn + strlen(file->filename));
                    strcpy(fn->filename, file->filename);
                    fn->sym = sym;
                    dynarray_add(&mcc_state->inline_fns,
				 &mcc_state->nb_inline_fns, fn);
                    skip_or_save_block(&fn->func_str);
                } else {
                    cur_text_section = ad.section;
                    if (!cur_text_section)
                        cur_text_section = text_section;
                    else if (cur_text_section->sh_num > bss_section->sh_num)
                        cur_text_section->sh_flags = text_section->sh_flags;
                    gen_function(sym);
                }
                break;
            } else {
                has_init = 0;
                if ((type.t & VT_BTYPE) == VT_FUNC
                    && type.ref->f.func_type == FUNC_OLD
                    && type.ref->next != NULL)
                    mcc_error("parameter names (without types) in function declaration");
		if (l == VT_CMP) {
		    for (sym = func_vt.ref->next; sym; sym = sym->next)
			if ((sym->v & ~SYM_FIELD) == v)
			    goto found;
		    mcc_error("declaration for parameter '%s' but no such parameter",
			      get_tok_str(v, NULL));
                found:
		    if (type.t & VT_STORAGE)
		        mcc_error("storage class specified for '%s'",
				  get_tok_str(v, NULL));
		    if (!(sym->type.t & VT_EXTERN))
		        mcc_error("redefinition of parameter '%s'",
				  get_tok_str(v, NULL));
		    convert_parameter_type(&type);
		    sym->type = type;
		} else if (type.t & VT_TYPEDEF) {
                    sym = sym_find(v);
                    if (sym && sym->sym_scope == local_scope) {
                        if (!is_compatible_types(&sym->type, &type)
                            || !(sym->type.t & VT_TYPEDEF))
                            mcc_error("incompatible redefinition of '%s'",
                                get_tok_str(v, NULL));
                        else if ((sym->type.t & VT_VLA) || (type.t & VT_VLA))
                            mcc_error("redefinition of variably modified typedef '%s'",
                                get_tok_str(v, NULL));
                        else if (mcc_state->cversion < 201112)
                            mcc_pedantic("redefinition of typedef is a C11 feature");
                        sym->type = type;
                    } else {
                        sym = sym_push(v, &type, 0, 0);
                    }
                    sym->a = ad.a;
                    if ((type.t & VT_BTYPE) == VT_FUNC)
                      merge_funcattr(&sym->type.ref->f, &ad.f);
                    if (debug_modes)
                        mcc_debug_typedef (mcc_state, sym);
		} else if ((type.t & VT_BTYPE) == VT_VOID
			   && !(type.t & VT_EXTERN)) {
		    mcc_error("declaration of void object");
                } else {
                    {
                        Sym *prev = sym_find(v);
                        if (prev && (prev->type.t & VT_TYPEDEF)
                            && prev->sym_scope == local_scope)
                            mcc_error("'%s' redeclared as different kind of symbol",
                                      get_tok_str(v, NULL));
                        else if ((mcc_state->warn_shadow & WARN_ON)
                                 && local_scope > 0 && prev
                                 && prev->sym_scope != local_scope
                                 && v >= TOK_IDENT && v < SYM_FIRST_ANOM
                                 && (type.t & VT_BTYPE) != VT_FUNC
                                 && !(type.t & (VT_TYPEDEF | VT_EXTERN))
                                 && (prev->type.t & VT_BTYPE) != VT_FUNC
                                 && !(prev->type.t & VT_TYPEDEF)
                                 && !IS_ENUM_VAL(prev->type.t)) {
                            if ((prev->r & VT_VALMASK) == VT_LOCAL)
                                mcc_warning_c(warn_shadow)(
                                    "declaration of '%s' shadows a previous local",
                                    get_tok_str(v, NULL));
                            else
                                mcc_warning_c(warn_shadow)(
                                    "declaration of '%s' shadows a global declaration",
                                    get_tok_str(v, NULL));
                        }
                    }
                    r = 0;
                    if ((type.t & VT_BTYPE) == VT_FUNC) {
                        merge_funcattr(&type.ref->f, &ad.f);
                    } else if (!(type.t & VT_ARRAY)) {
                        r |= VT_LVAL;
                    }

                    if (tok == '=')
                        has_init = 1;

                    if (((type.t & VT_EXTERN) && (!has_init || l != VT_CONST))
		        || (type.t & VT_BTYPE) == VT_FUNC
                        || ((type.t & VT_ARRAY) && !has_init
                            && l == VT_CONST && type.ref->c < 0)
                        ) {
                        int was_tentative_flex =
                            !(type.t & VT_EXTERN)
                            && (type.t & VT_ARRAY) && !has_init
                            && l == VT_CONST && type.ref->c < 0;
                        type.t |= VT_EXTERN;
                        sym = external_sym(v, &type, r, &ad);
                        if (was_tentative_flex && sym)
                            sym->a.tentative_array = 1;
                    } else {
                        if (cur_func_inline_extern && l != VT_CONST
                            && (type.t & VT_STATIC)
                            && !(type.t & VT_CONSTANT) && v)
                            mcc_warning("'%s' is static but declared in inline "
                                "function '%s' which is not static",
                                get_tok_str(v, NULL), funcname);
                        if (l == VT_CONST || (type.t & VT_STATIC))
                            r |= VT_CONST;
                        else
                            r |= VT_LOCAL;
                        type.t &= ~VT_EXTERN;
                        if (has_init)
                            next();
                        else if (l == VT_CONST) {
                            if (!(type.t & VT_STATIC)) {
                                Sym *pe = sym_find(v);
                                if (pe && (pe->type.t & VT_STATIC))
                                    mcc_error("non-static declaration of '%s' "
                                        "follows static declaration",
                                        get_tok_str(v, NULL));
                            }
                            type.t |= VT_EXTERN;
                        }
                        decl_initializer_alloc(&type, &ad, r, has_init, v, l);
                    }

                    if ((ad.storage_class & 2) && v) {
                        Sym *rs = sym_find(v);
                        if (rs)
                            rs->a.is_register = 1;
                    }

                    if (ad.alias_target && l == VT_CONST) {
                        esym = elfsym(sym_find(ad.alias_target));
                        if (!esym)
                            mcc_error("unsupported forward __alias__ attribute");
                        put_extern_sym2(sym_find(v), esym->st_shndx,
                                        esym->st_value, esym->st_size, 1);
                    }
                }
                if (tok != ',') {
                    if (l == VT_JMP)
                        return has_init ? v : 1;
                    skip(';');
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
