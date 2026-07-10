#ifndef MCC_I386_GEN_H
#define MCC_I386_GEN_H

#define NB_REGS 5
#define NB_ASM_REGS 8
#ifndef MCC_DISABLE_ASM
#define CONFIG_MCC_ASM
#endif

#define RC_INT 0x0001
#define RC_FLOAT 0x0002
#define RC_EAX 0x0004
#define RC_EDX 0x0008
#define RC_ECX 0x0010
#define RC_EBX 0x0020
#define RC_ST0 0x0040

#define RC_IRET RC_EAX
#define RC_IRE2 RC_EDX
#define RC_FRET RC_ST0

enum {
	TREG_EAX = 0,
	TREG_ECX,
	TREG_EDX,
	TREG_EBX,
	TREG_ST0,
	TREG_ESP = 4,
	TREG_MEM = 0x20
};

#define REG_VALUE(reg) ((reg) & 7)

#define REG_IRET TREG_EAX
#define REG_IRE2 TREG_EDX
#define REG_FRET TREG_ST0

#define INVERT_FUNC_PARAMS

#define PTR_SIZE 4

#define LDOUBLE_SIZE 12
#define LDOUBLE_ALIGN 4
#define MAX_ALIGN 8

#define PROMOTE_RET

#endif
