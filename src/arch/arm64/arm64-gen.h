#ifndef MCC_ARM64_GEN_H
#define MCC_ARM64_GEN_H

#define NB_REGS 28

#define TREG_R(x) (x)
#define TREG_R30 19
#define TREG_F(x) (x + 20)

#define RC_INT (1 << 0)
#define RC_FLOAT (1 << 1)
#define RC_R(x) (1 << (2 + (x)))
#define RC_R30 (1 << 21)
#define RC_F(x) (1 << (22 + (x)))

#define RC_IRET (RC_R(0))
#define RC_FRET (RC_F(0))

#define REG_IRET (TREG_R(0))
#define REG_FRET (TREG_F(0))

#define PTR_SIZE 8

#define LDOUBLE_SIZE 16
#define LDOUBLE_ALIGN 16

#define MAX_ALIGN 16

#if !defined(MCC_TARGET_MACHO) && !defined(MCC_TARGET_PE)
#define CHAR_IS_UNSIGNED
#endif

#define PROMOTE_RET

#endif
