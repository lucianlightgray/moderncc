#ifndef MCCRIR_H
#define MCCRIR_H

#if defined(MCC_INTERNAL)

enum {
	RIR_R_NONE = 0,
	RIR_R_IF,
	RIR_R_THEN,
	RIR_R_ELSE,
	RIR_R_WHILE,
	RIR_R_DO,
	RIR_R_FOR,
	RIR_R_SWITCH,
	RIR_R_TERNARY,
	RIR_R_LANDOR,
	RIR_R_CALL,
	RIR_R_COND,
	RIR_R_BODY,
	RIR_R_INCR,
	RIR_R_SYNTH,
	RIR_R_INC,
	RIR_R_MEMBER,
	RIR_R_TARM,
	RIR_R_LSUP,
	RIR_R_LOPND,
	RIR_R_VSTORE,
	RIR_R_VLA,
	RIR_R_CPLX,
	RIR_R_CVT,
	RIR_R_ACAS,
	RIR_R_CPLXB,
	RIR_R_COUNT
};

extern int rir_env;
extern int rir_try_active;
extern int rir_active;
extern int rir_c2_active;
extern int rir_body_loc_sv;
extern int rir_started;
extern int rir_prod_env;
extern int rir_prod_low_env;

#define RIR_LOW_NCLASS 8
#define RIR_LOW_NLEVEL 3
#define AST_LOW_MIN_REGION 3
#define AST_LOW_BIG_REGION 16
void rir_low_set(long nodes, const long *clean, const long *why, int nwhy);
void rir_low_regions(const long *regions, const long *big, const long *huge);

struct AstArena;
struct AstArena *rir_prod_take(void);
void rir_arena_normalise(struct AstArena *a);
void rir_teardown(void);
void rir_prod_replay_begin(void);
void rir_prod_replay_end(void);
void rir_prod_note(const char *verdict);
void rir_prod_body_set(long bytes);
void rir_prod_why_set(const char *why);
void rir_prod_fn_begin(void);
void rir_prod_fn_end(long bytes);
void rir_prod_reemit(long bytes);
void rir_prod_span(int first, int end, int body_len, int new_len);
extern const char *rir_unfaithful_why;

void rir_snap_types(SValue *sv, int n);
void rir_loc_record(int loc_in);
int rir_loc_replay(int *loc_out);
void rir_slot_record(int loc_in);
int rir_slot_replay(int *loc_out);
void rir_tvar_record(int loc_in, int r2);
int rir_tvar_replay(int *loc_out, int *r2_out);
void rir_configure(void);
void rir_reset(void);
void rir_verify(void);
enum {
	RIR_M_RETURN = 1,
	RIR_M_JUMP,
	RIR_M_LOAD,
	RIR_M_CONVERT,
	RIR_M_LABEL,
	RIR_M_RETJMP,
	RIR_M_IRETURN,
	RIR_M_OPASSIGN,
	RIR_M_GOTO,
	RIR_M_CASE,
	RIR_M_DEFAULT,
	RIR_M_CMPINV,
	RIR_M_RETEXPR,
	RIR_M_CASTGV,
	RIR_M_NORETURN,
	RIR_M_VLA,
	RIR_M_VLARESTORE,
	RIR_M_ARGCAST,
	RIR_M_WHILECOND,
	RIR_M_BFGV,
	RIR_M_TERNHOLD,
	RIR_M_TERNPICK,
	RIR_M_CLGOTO,
	RIR_M_CLTHUNK,
	RIR_M_CLJMP,
	RIR_M_ADDRLATE,
	RIR_M_ASMOPS,
	RIR_M_CASTT,
	RIR_M_COUNT
};

void rir_rbegin(int kind);
void rir_rbegin_val(int kind, int val);
void rir_rend_to(int kind);
void rir_rend_to_val(int kind, int val);
void rir_rcond_done(void);
void rir_mark_pt(int kind);
void rir_mark_val(int kind, int val);
void rir_mark_val2(int kind, long long a, long long b);
void rir_mark_vla(int t, uint64_t ref, int addr, int new_save, int locorig,
									int align, int result);
void rir_vla_begin(void);

void rir_hook_if_begin(void);
void rir_hook_if_gvtst_done(void);
void rir_hook_if_else(void);
void rir_hook_if_end(void);
void rir_hook_while_cond_start(void);
void rir_hook_while_begin(void);
void rir_hook_while_end(void);
void rir_hook_do_begin(void);
void rir_hook_do_body_end(void);
void rir_hook_do_cond(void);
void rir_hook_do_end(void);
void rir_hook_for_begin(void);
void rir_hook_for_cond(void);
void rir_hook_for_incr_begin(void);
void rir_hook_for_incr_end(void);
void rir_hook_for_no_incr(void);
void rir_hook_for_body_begin(void);
void rir_hook_for_end(void);
void rir_hook_switch_begin(void);
void rir_hook_switch_end(void);
void rir_hook_case(long long v1, long long v2);
void rir_hook_default(void);
void rir_hook_label(int v);
void rir_hook_goto(int v);
void rir_hook_break_continue(int is_continue, int nc_pre);
void rir_hook_call_begin(void);
void rir_hook_call_end(void);
void rir_hook_call_argcast(int pre_seq);
void rir_hook_convert(void);
extern int rir_cast_seq;
void rir_hook_call_noreturn(void);
void rir_hook_call_effect_end(void);
void rir_hook_vstore(void);
void rir_hook_vstore_end(void);
void rir_hook_ret_expr_done(void);
void rir_hook_return(int has_val);
void rir_hook_return_jmp(int jumps);
void rir_hook_implicit_return(void);
void rir_hook_synth_begin(void);
void rir_hook_synth_end(void);
void rir_hook_castsynth_end(struct CType *type, int ds, int ss);
void rir_hook_castlower_begin(struct CType *type);
void rir_hook_castlower_end(void);
void rir_hook_cast_type(struct CType *type, int src_t);
void rir_hook_member_begin(int is_arrow);
void rir_hook_member_end(int cumofs, int nonlval);
void rir_hook_builtin_complex_lower(void);
void rir_hook_builtin_complex_end(void);
void rir_hook_body_begin(void);
void rir_hook_bail(void);
void rir_hook_cleanup_call_begin(void);
void rir_hook_cleanup_call_end(void);
int rir_dbg_on(void);
int rir_capture_live(void);
int rir_hook_slot_replay(void);
void rir_hook_slot_record(void);
void rir_hook_fconst_record(int c, int cplx, const unsigned char *key);
int rir_hook_fconst_reuse(int cplx, const unsigned char *key);
void rir_hook_ternary_begin(int c, int g);
void rir_hook_ternary_branch(int which);
void rir_hook_ternary_branch_done(int which);
void rir_hook_ternary_pick(void);
void rir_hook_ternary_end(void);
void rir_hook_landor_operand(int op, int c, int first);
void rir_hook_landor_next(void);
void rir_hook_landor_end(int materialized);
void rir_hook_cplx_begin(void);
void rir_hook_cplx_end(void);
void rir_hook_acas_begin(int val);
void rir_hook_acas_end(int val);
void rir_hook_vla_alloc_begin(void);
void rir_hook_vla_alloc_end(struct CType *type, int addr, int new_save,
														int locorig, int align, int result);
void rir_hook_vla_restore(int loc);
void rir_hook_store_addr_late(void);
void rir_hook_inc(int post, int c);
void rir_hook_inc_end(void);
void rir_hook_vdup(void);
void rir_hook_indir(void);
void rir_hook_bfgv(int tt);
void rir_hook_cmp_invert(void);
void rir_hook_cast_gv(void);
void rir_hook_cast_const(int dbt, int sbt, uint64_t pre, uint64_t post);
void rir_hook_cleanup_goto(void *pcl);
void rir_hook_cleanup_thunk(void *pcl, int v, int end);
void rir_hook_asm_operands(int nb_operands, uint64_t gvmask);

#else

#define RIR_R_IF 0
#define RIR_R_THEN 0
#define RIR_R_ELSE 0
#define RIR_R_WHILE 0
#define RIR_R_DO 0
#define RIR_R_FOR 0
#define RIR_R_SWITCH 0
#define RIR_R_TERNARY 0
#define RIR_R_LANDOR 0
#define RIR_R_CALL 0
#define RIR_R_COND 0
#define RIR_R_BODY 0
#define RIR_R_INCR 0
#define RIR_R_SYNTH 0
#define RIR_R_INC 0
#define RIR_R_MEMBER 0
#define RIR_R_TARM 0
#define RIR_R_LSUP 0
#define RIR_R_LOPND 0
#define RIR_R_VSTORE 0
#define RIR_R_VLA 0
#define RIR_R_CPLX 0
#define RIR_R_CVT 0
#define RIR_R_ACAS 0
#define RIR_R_CPLXB 0
#define RIR_M_RETURN 0
#define RIR_M_JUMP 0
#define RIR_M_LOAD 0
#define RIR_M_CONVERT 0
#define RIR_M_LABEL 0
#define RIR_M_RETJMP 0
#define RIR_M_IRETURN 0
#define RIR_M_OPASSIGN 0
#define RIR_M_GOTO 0
#define RIR_M_CASE 0
#define RIR_M_DEFAULT 0
#define RIR_M_CMPINV 0
#define RIR_M_RETEXPR 0
#define RIR_M_CASTGV 0
#define RIR_M_NORETURN 0
#define RIR_M_VLA 0
#define RIR_M_VLARESTORE 0
#define RIR_M_ARGCAST 0
#define RIR_M_WHILECOND 0
#define RIR_M_BFGV 0
#define RIR_M_TERNHOLD 0
#define RIR_M_TERNPICK 0
#define RIR_M_CLGOTO 0
#define RIR_M_CLTHUNK 0
#define RIR_M_CLJMP 0
#define RIR_M_ADDRLATE 0
#define RIR_M_ASMOPS 0
#define RIR_M_CASTT 0
#define rir_rbegin(k) ((void)0)
#define rir_rbegin_val(k, v) ((void)0)
#define rir_rend_to(k) ((void)0)
#define rir_rend_to_val(k, v) ((void)0)
#define rir_rcond_done() ((void)0)
#define rir_mark_pt(k) ((void)0)
#define rir_mark_val(k, v) ((void)0)
#define rir_mark_val2(k, a, b) ((void)0)
#define rir_mark_vla(t, r, a, n, l, al, rs) ((void)0)
#define rir_vla_begin() ((void)0)
#define rir_env 0
#define rir_try_active 0
#define rir_c2_active 0
#define rir_prod_env 0
#define rir_prod_low_env 0
#define RIR_LOW_NCLASS 8
#define RIR_LOW_NLEVEL 3
#define AST_LOW_MIN_REGION 3
#define AST_LOW_BIG_REGION 16
#define rir_low_set(n, c, w, k) ((void)0)
#define rir_low_regions(r, b, h) ((void)0)
#define rir_prod_take() NULL
#define rir_arena_normalise(a) ((void)0)
#define rir_teardown() ((void)0)
#define rir_prod_replay_begin() ((void)0)
#define rir_prod_replay_end() ((void)0)
#define rir_prod_note(v) ((void)0)
#define rir_prod_body_set(n) ((void)0)
#define rir_prod_why_set(w) ((void)0)
#define rir_prod_fn_begin() ((void)0)
#define rir_prod_fn_end(n) ((void)0)
#define rir_prod_reemit(n) ((void)0)
#define rir_prod_span(a, b, c, d) ((void)0)
#define rir_snap_types(sv, n) ((void)0)
#define rir_loc_record(l) ((void)0)
#define rir_loc_replay(p) 0
#define rir_slot_record(l) ((void)0)
#define rir_slot_replay(p) 0
#define rir_tvar_record(l, r) ((void)0)
#define rir_tvar_replay(p, q) 0
#define rir_hook_if_begin() ((void)0)
#define rir_hook_if_gvtst_done() ((void)0)
#define rir_hook_if_else() ((void)0)
#define rir_hook_if_end() ((void)0)
#define rir_hook_while_cond_start() ((void)0)
#define rir_hook_while_begin() ((void)0)
#define rir_hook_while_end() ((void)0)
#define rir_hook_do_begin() ((void)0)
#define rir_hook_do_body_end() ((void)0)
#define rir_hook_do_cond() ((void)0)
#define rir_hook_do_end() ((void)0)
#define rir_hook_for_begin() ((void)0)
#define rir_hook_for_cond() ((void)0)
#define rir_hook_for_incr_begin() ((void)0)
#define rir_hook_for_incr_end() ((void)0)
#define rir_hook_for_no_incr() ((void)0)
#define rir_hook_for_body_begin() ((void)0)
#define rir_hook_for_end() ((void)0)
#define rir_hook_switch_begin() ((void)0)
#define rir_hook_switch_end() ((void)0)
#define rir_hook_case(a, b) ((void)0)
#define rir_hook_default() ((void)0)
#define rir_hook_label(v) ((void)0)
#define rir_hook_goto(v) ((void)0)
#define rir_hook_break_continue(c, n) ((void)0)
#define rir_hook_call_begin() ((void)0)
#define rir_hook_call_end() ((void)0)
#define rir_hook_call_argcast(p) ((void)0)
#define rir_hook_convert() ((void)0)
#define rir_cast_seq 0
#define rir_hook_call_noreturn() ((void)0)
#define rir_hook_call_effect_end() ((void)0)
#define rir_hook_vstore() ((void)0)
#define rir_hook_vstore_end() ((void)0)
#define rir_hook_ret_expr_done() ((void)0)
#define rir_hook_return(v) ((void)0)
#define rir_hook_return_jmp(j) ((void)0)
#define rir_hook_implicit_return() ((void)0)
#define rir_hook_synth_begin() ((void)0)
#define rir_hook_castsynth_end(t, d, s) ((void)0)
#define rir_hook_synth_end() ((void)0)
#define rir_hook_castlower_begin(t) ((void)0)
#define rir_hook_castlower_end() ((void)0)
#define rir_hook_cast_type(t, s) ((void)0)
#define rir_hook_member_begin(a) ((void)0)
#define rir_hook_member_end(c, n) ((void)0)
#define rir_hook_builtin_complex_lower() ((void)0)
#define rir_hook_builtin_complex_end() ((void)0)
#define rir_hook_body_begin() ((void)0)
#define rir_hook_bail() ((void)0)
#define rir_hook_cleanup_call_begin() ((void)0)
#define rir_hook_cleanup_call_end() ((void)0)
#define rir_dbg_on() 0
#define rir_capture_live() 0
#define rir_hook_slot_replay() 0
#define rir_hook_slot_record() ((void)0)
#define rir_hook_fconst_record(c, cplx, key) ((void)0)
#define rir_hook_fconst_reuse(cplx, key) (-1)
#define rir_configure() ((void)0)
#define rir_hook_ternary_begin(c, g) ((void)0)
#define rir_hook_ternary_branch(w) ((void)0)
#define rir_hook_ternary_branch_done(w) ((void)0)
#define rir_hook_ternary_pick() ((void)0)
#define rir_hook_ternary_end() ((void)0)
#define rir_hook_landor_operand(o, c, f) ((void)0)
#define rir_hook_landor_next() ((void)0)
#define rir_hook_landor_end(m) ((void)0)
#define rir_hook_cplx_begin() ((void)0)
#define rir_hook_cplx_end() ((void)0)
#define rir_hook_acas_begin(v) ((void)0)
#define rir_hook_acas_end(v) ((void)0)
#define rir_hook_vla_alloc_begin() ((void)0)
#define rir_hook_vla_alloc_end(t, a, n, l, g, r) ((void)0)
#define rir_hook_vla_restore(l) ((void)0)
#define rir_hook_store_addr_late() ((void)0)
#define rir_hook_inc(p, c) ((void)0)
#define rir_hook_inc_end() ((void)0)
#define rir_hook_vdup() ((void)0)
#define rir_hook_indir() ((void)0)
#define rir_hook_bfgv(t) ((void)0)
#define rir_hook_cmp_invert() ((void)0)
#define rir_hook_cast_gv() ((void)0)
#define rir_hook_cast_const(d, s, p, q) ((void)0)
#define rir_hook_cleanup_goto(p) ((void)0)
#define rir_hook_cleanup_thunk(p, v, e) ((void)0)
#define rir_hook_asm_operands(n, m) ((void)0)

#endif

#endif
