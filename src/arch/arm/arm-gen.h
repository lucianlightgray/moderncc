#ifndef MCC_ARM_GEN_H
#define MCC_ARM_GEN_H

#if defined(MCC_ARM_EABI) && !defined(MCC_ARM_VFP)
#error "Currently ModernCC only supports float computation with VFP instructions"
#endif

#ifdef MCC_ARM_VFP
#define NB_REGS 13
#else
#define NB_REGS 9
#endif

#ifndef CONFIG_MCC_CPUVER
#define CONFIG_MCC_CPUVER 5
#endif

#define RC_INT 0x0001
#define RC_FLOAT 0x0002
#define RC_R0 0x0004
#define RC_R1 0x0008
#define RC_R2 0x0010
#define RC_R3 0x0020
#define RC_R12 0x0040
#define RC_F0 0x0080
#define RC_F1 0x0100
#define RC_F2 0x0200
#define RC_F3 0x0400
#ifdef MCC_ARM_VFP
#define RC_F4 0x0800
#define RC_F5 0x1000
#define RC_F6 0x2000
#define RC_F7 0x4000
#endif
#define RC_IRET RC_R0
#define RC_IRE2 RC_R1
#define RC_FRET RC_F0

enum {
	TREG_R0 = 0,
	TREG_R1,
	TREG_R2,
	TREG_R3,
	TREG_R12,
	TREG_F0,
	TREG_F1,
	TREG_F2,
	TREG_F3,
#ifdef MCC_ARM_VFP
	TREG_F4,
	TREG_F5,
	TREG_F6,
	TREG_F7,
#endif
	TREG_SP = 13,
	TREG_LR,
};

#ifdef MCC_ARM_VFP
#define T2CPR(t) (((t) & VT_BTYPE) != VT_FLOAT ? 0x100 : 0)
#endif

#define REG_IRET TREG_R0
#define REG_IRE2 TREG_R1
#define REG_FRET TREG_F0

#ifdef MCC_ARM_EABI
#define TOK___divdi3 TOK___aeabi_ldivmod
#define TOK___moddi3 TOK___aeabi_ldivmod
#define TOK___udivdi3 TOK___aeabi_uldivmod
#define TOK___umoddi3 TOK___aeabi_uldivmod
#endif

#define INVERT_FUNC_PARAMS

#define PTR_SIZE 4

#ifdef MCC_ARM_VFP
#define LDOUBLE_SIZE 8
#endif

#ifndef LDOUBLE_SIZE
#define LDOUBLE_SIZE 8
#endif

#ifdef MCC_ARM_EABI
#define LDOUBLE_ALIGN 8
#else
#define LDOUBLE_ALIGN 4
#endif

#if LDOUBLE_SIZE == 8
#define MCC_USING_DOUBLE_FOR_LDOUBLE 1
#endif

#define MAX_ALIGN 8

#define CHAR_IS_UNSIGNED

#define ARM_SOFTFP_FLOAT 0
#define ARM_HARD_FLOAT 1

#endif
