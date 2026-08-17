#ifndef MCC_WIDE512_ARITH_H
#define MCC_WIDE512_ARITH_H

/* Compiler-side 8-limb (512-bit) instantiation of wide256_arith.h, exposing
 * mcc_w512_* names so a translation unit that already holds the 4-limb
 * mcc_w256_* set (via wide256_slice.h) can also const-fold 512-bit values.
 * The kernel has no file guard, so we include it a second time under a macro
 * shim that (a) forces the limb count to 8 and (b) renames every symbol.
 * Include AFTER wide256_slice.h. */

#ifdef MCC_W256_LIMBS
#undef MCC_W256_LIMBS
#endif
#define MCC_W256_LIMBS 8

#define mcc_w256_add mcc_w512_add
#define mcc_w256_and mcc_w512_and
#define mcc_w256_bit mcc_w512_bit
#define mcc_w256_copy mcc_w512_copy
#define mcc_w256_from_i64 mcc_w512_from_i64
#define mcc_w256_from_u64 mcc_w512_from_u64
#define mcc_w256_is_zero mcc_w512_is_zero
#define mcc_w256_limb mcc_w512_limb
#define mcc_w256_mul mcc_w512_mul
#define mcc_w256_neg mcc_w512_neg
#define mcc_w256_not mcc_w512_not
#define mcc_w256_or mcc_w512_or
#define mcc_w256_sar mcc_w512_sar
#define mcc_w256_scmp mcc_w512_scmp
#define mcc_w256_sdivmod mcc_w512_sdivmod
#define mcc_w256_setbit mcc_w512_setbit
#define mcc_w256_shl mcc_w512_shl
#define mcc_w256_shr mcc_w512_shr
#define mcc_w256_sign mcc_w512_sign
#define mcc_w256_sub mcc_w512_sub
#define mcc_w256_ucmp mcc_w512_ucmp
#define mcc_w256_udivmod mcc_w512_udivmod
#define mcc_w256_xor mcc_w512_xor
#define mcc_w256_zero mcc_w512_zero

#include "wide256_arith.h"

#undef MCC_W256_LIMBS
#define MCC_W256_LIMBS MCC_WIDE256_LIMBS

#undef mcc_w256_add
#undef mcc_w256_and
#undef mcc_w256_bit
#undef mcc_w256_copy
#undef mcc_w256_from_i64
#undef mcc_w256_from_u64
#undef mcc_w256_is_zero
#undef mcc_w256_limb
#undef mcc_w256_mul
#undef mcc_w256_neg
#undef mcc_w256_not
#undef mcc_w256_or
#undef mcc_w256_sar
#undef mcc_w256_scmp
#undef mcc_w256_sdivmod
#undef mcc_w256_setbit
#undef mcc_w256_shl
#undef mcc_w256_shr
#undef mcc_w256_sign
#undef mcc_w256_sub
#undef mcc_w256_ucmp
#undef mcc_w256_udivmod
#undef mcc_w256_xor
#undef mcc_w256_zero

#endif /* MCC_WIDE512_ARITH_H */
