#ifndef MCC_ARM64_GEN_H
#define MCC_ARM64_GEN_H

#define MCC_NB_REGS 38

#define MCC_TREG_R(x) (x)
#define MCC_TREG_R30 19
#define MCC_TREG_F(x) (x + 20)

/* Callee-saved integer registers x19..x28, exposed only to the Tier-3
   register-promotion pool (reg_classes[] entry 0, so the general allocator
   never picks them).  intr() maps these indices to their physical encoding.
   Indices 28..37 sit above the float block (20..27); the 64-bit ast_pinned_regs
   mask makes indices >31 representable. */
#define MCC_TREG_SAVED(x) (28 + (x))
#define MCC_NB_SAVED 10

#define MCC_RC_INT (1 << 0)
#define MCC_RC_FLOAT (1 << 1)
#define MCC_RC_R(x) (1 << (2 + (x)))
#define MCC_RC_R30 (1 << 21)
#define MCC_RC_F(x) (1 << (22 + (x)))

#define MCC_RC_IRET (MCC_RC_R(0))
#define MCC_RC_FRET (MCC_RC_F(0))

#define REG_IRET (MCC_TREG_R(0))
#define REG_FRET (MCC_TREG_F(0))

#define MCC_PTR_SIZE 8

#define MCC_LDOUBLE_SIZE 16
#define MCC_LDOUBLE_ALIGN 16

#define MCC_MAX_ALIGN 16

#if !defined(MCC_TARGET_MACHO) && !defined(MCC_TARGET_PE)
#define MCC_CHAR_IS_UNSIGNED
#endif

#define MCC_RET_PROMOTES_INT

#endif
