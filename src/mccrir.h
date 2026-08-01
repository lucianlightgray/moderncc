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
	RIR_R_COUNT
};

extern int rir_env;
extern int rir_active;
extern int rir_started;

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
#define rir_rbegin(k) ((void)0)
#define rir_rbegin_val(k, v) ((void)0)
#define rir_rend_to(k) ((void)0)
#define rir_rend_to_val(k, v) ((void)0)
#define rir_rcond_done() ((void)0)
#define rir_mark_pt(k) ((void)0)
#define rir_mark_val(k, v) ((void)0)
#define rir_mark_val2(k, a, b) ((void)0)
#define rir_env 0

#endif

#endif
