#ifndef MCC_ARM_GEN_H
#define MCC_ARM_GEN_H

#if defined(MCC_ARM_EABI) && !defined(MCC_ARM_VFP)
#error "Currently ModernCC only supports float computation with VFP instructions"
#endif

#ifdef MCC_ARM_VFP
#define MCC_NB_REGS 13
#else
#define MCC_NB_REGS 9
#endif

#ifndef MCC_CONFIG_CPUVER
#define MCC_CONFIG_CPUVER 5
#endif

#define MCC_RC_INT 0x0001
#define MCC_RC_FLOAT 0x0002
#define MCC_RC_R0 0x0004
#define MCC_RC_R1 0x0008
#define MCC_RC_R2 0x0010
#define MCC_RC_R3 0x0020
#define MCC_RC_R12 0x0040
#define MCC_RC_F0 0x0080
#define MCC_RC_F1 0x0100
#define MCC_RC_F2 0x0200
#define MCC_RC_F3 0x0400
#ifdef MCC_ARM_VFP
#define MCC_RC_F4 0x0800
#define MCC_RC_F5 0x1000
#define MCC_RC_F6 0x2000
#define MCC_RC_F7 0x4000
#endif
#define MCC_RC_IRET MCC_RC_R0
#define MCC_RC_IRE2 MCC_RC_R1
#define MCC_RC_FRET MCC_RC_F0

enum {
	MCC_TREG_R0 = 0,
	MCC_TREG_R1,
	MCC_TREG_R2,
	MCC_TREG_R3,
	MCC_TREG_R12,
	MCC_TREG_F0,
	MCC_TREG_F1,
	MCC_TREG_F2,
	MCC_TREG_F3,
#ifdef MCC_ARM_VFP
	MCC_TREG_F4,
	MCC_TREG_F5,
	MCC_TREG_F6,
	MCC_TREG_F7,
#endif
	MCC_TREG_SP = 13,
	MCC_TREG_LR,
};

#ifdef MCC_ARM_VFP
#define T2CPR(t) (((t) & VT_BTYPE) != VT_FLOAT ? 0x100 : 0)
#endif

#define REG_IRET MCC_TREG_R0
#define REG_IRE2 MCC_TREG_R1
#define REG_FRET MCC_TREG_F0

#ifdef MCC_ARM_EABI
#define TOK___divdi3 TOK___aeabi_ldivmod
#define TOK___moddi3 TOK___aeabi_ldivmod
#define TOK___udivdi3 TOK___aeabi_uldivmod
#define TOK___umoddi3 TOK___aeabi_uldivmod
#endif

#define INVERT_FUNC_PARAMS

#define MCC_PTR_SIZE 4

#ifdef MCC_ARM_VFP
#define MCC_LDOUBLE_SIZE 8
#endif

#ifndef MCC_LDOUBLE_SIZE
#define MCC_LDOUBLE_SIZE 8
#endif

#ifdef MCC_ARM_EABI
#define MCC_LDOUBLE_ALIGN 8
#else
#define MCC_LDOUBLE_ALIGN 4
#endif

#if MCC_LDOUBLE_SIZE == 8
#define MCC_USING_DOUBLE_FOR_LDOUBLE 1
#endif

#define MCC_MAX_ALIGN 8

#define MCC_CHAR_IS_UNSIGNED

#define ARM_SOFTFP_FLOAT 0
#define ARM_HARD_FLOAT 1

#endif
