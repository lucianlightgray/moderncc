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
	RIR_R_COUNT
};

extern int rir_env;
extern int rir_active;
extern int rir_started;

void rir_configure(void);
void rir_reset(void);
void rir_verify(void);
void rir_rbegin(int kind);
void rir_rend_to(int kind);

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
#define rir_rbegin(k) ((void)0)
#define rir_rend_to(k) ((void)0)
#define rir_env 0

#endif

#endif
