#ifndef MCCRIR_H
#define MCCRIR_H

#if MCC_REPLAY_IR && defined(MCC_INTERNAL)

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
	RIR_R_COUNT
};

extern int rir_env;
extern int rir_active;
extern int rir_c2_active;
extern int rir_body_loc_sv;
extern int rir_started;

void rir_snap_types(SValue *sv, int n);
void rir_loc_record(int loc_in);
int rir_loc_replay(int *loc_out);
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
void rir_mark_vla(int t, uint64_t ref, int addr, int new_save, int locorig);
void rir_vla_begin(void);

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
#define rir_rbegin(k) ((void)0)
#define rir_rbegin_val(k, v) ((void)0)
#define rir_rend_to(k) ((void)0)
#define rir_rend_to_val(k, v) ((void)0)
#define rir_rcond_done() ((void)0)
#define rir_mark_pt(k) ((void)0)
#define rir_mark_val(k, v) ((void)0)
#define rir_mark_val2(k, a, b) ((void)0)
#define rir_mark_vla(t, r, a, n, l) ((void)0)
#define rir_vla_begin() ((void)0)
#define rir_env 0
#define rir_c2_active 0
#define rir_snap_types(sv, n) ((void)0)
#define rir_loc_record(l) ((void)0)
#define rir_loc_replay(p) 0

#endif

#endif
