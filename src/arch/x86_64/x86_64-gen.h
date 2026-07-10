#ifndef MCC_X86_64_GEN_H
#define MCC_X86_64_GEN_H

#define NB_REGS 25
#define NB_ASM_REGS 16
#ifndef MCC_DISABLE_ASM
#define CONFIG_MCC_ASM
#endif

#define RC_INT 0x0001
#define RC_FLOAT 0x0002
#define RC_RAX 0x0004
#define RC_RDX 0x0008
#define RC_RCX 0x0010
#define RC_RSI 0x0020
#define RC_RDI 0x0040
#define RC_ST0 0x0080
#define RC_R8 0x0100
#define RC_R9 0x0200
#define RC_R10 0x0400
#define RC_R11 0x0800
#define RC_XMM0 0x1000
#define RC_XMM1 0x2000
#define RC_XMM2 0x4000
#define RC_XMM3 0x8000
#define RC_XMM4 0x10000
#define RC_XMM5 0x20000
#define RC_XMM6 0x40000
#define RC_XMM7 0x80000
#define RC_IRET RC_RAX
#define RC_IRE2 RC_RDX
#define RC_FRET RC_XMM0
#define RC_FRE2 RC_XMM1

enum {
	TREG_RAX = 0,
	TREG_RCX = 1,
	TREG_RDX = 2,
	TREG_RSP = 4,
	TREG_RSI = 6,
	TREG_RDI = 7,

	TREG_R8 = 8,
	TREG_R9 = 9,
	TREG_R10 = 10,
	TREG_R11 = 11,

	TREG_XMM0 = 16,
	TREG_XMM1 = 17,
	TREG_XMM2 = 18,
	TREG_XMM3 = 19,
	TREG_XMM4 = 20,
	TREG_XMM5 = 21,
	TREG_XMM6 = 22,
	TREG_XMM7 = 23,

	TREG_ST0 = 24,

	TREG_MEM = 0x20
};

#define REX_BASE(reg) (((reg) >> 3) & 1)
#define REG_VALUE(reg) ((reg) & 7)

#define REG_IRET TREG_RAX
#define REG_IRE2 TREG_RDX
#define REG_FRET TREG_XMM0
#define REG_FRE2 TREG_XMM1

#define INVERT_FUNC_PARAMS

#define PTR_SIZE 8

#define LDOUBLE_SIZE 16
#define LDOUBLE_ALIGN 16
#define MAX_ALIGN 16

#define PROMOTE_RET

#define MCC_TARGET_NATIVE_STRUCT_COPY
ST_FUNC void gen_struct_copy(int size);

#endif
