#ifndef MCC_RISCV64_GEN_H
#define MCC_RISCV64_GEN_H

#define MCC_NB_REGS 19
#ifndef MCC_CONFIG_ASM
#define MCC_CONFIG_ASM 1
#endif

#define MCC_TREG_R(x) (x)
#define MCC_TREG_F(x) (x + 8)

#define MCC_RC_INT (1 << 0)
#define MCC_RC_FLOAT (1 << 1)
#define MCC_RC_R(x) (1 << (2 + (x)))
#define MCC_RC_F(x) (1 << (10 + (x)))

#define MCC_RC_IRET (MCC_RC_R(0))
#define MCC_RC_IRE2 (MCC_RC_R(1))
#define MCC_RC_FRET (MCC_RC_F(0))

#define REG_IRET (MCC_TREG_R(0))
#define REG_IRE2 (MCC_TREG_R(1))
#define REG_FRET (MCC_TREG_F(0))

#define MCC_PTR_SIZE 8

#define MCC_LDOUBLE_SIZE 16
#define MCC_LDOUBLE_ALIGN 16

#define MCC_MAX_ALIGN 16

#define MCC_CHAR_IS_UNSIGNED

#define MCC_RET_PROMOTES_INT

#endif
