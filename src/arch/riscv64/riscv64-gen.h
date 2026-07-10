#ifndef MCC_RISCV64_GEN_H
#define MCC_RISCV64_GEN_H

#define NB_REGS 19
#ifndef MCC_DISABLE_ASM
#define CONFIG_MCC_ASM
#endif

#define TREG_R(x) (x)
#define TREG_F(x) (x + 8)

#define RC_INT (1 << 0)
#define RC_FLOAT (1 << 1)
#define RC_R(x) (1 << (2 + (x)))
#define RC_F(x) (1 << (10 + (x)))

#define RC_IRET (RC_R(0))
#define RC_IRE2 (RC_R(1))
#define RC_FRET (RC_F(0))

#define REG_IRET (TREG_R(0))
#define REG_IRE2 (TREG_R(1))
#define REG_FRET (TREG_F(0))

#define PTR_SIZE 8

#define LDOUBLE_SIZE 16
#define LDOUBLE_ALIGN 16

#define MAX_ALIGN 16

#define CHAR_IS_UNSIGNED

#define PROMOTE_RET

#endif
