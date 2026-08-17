/* dg-error: ARM ISA identity predefines present */
/* T-mac-30029 slice-1.  mcc emitted only __AARCH64EL__ on arm64; both gcc and
 * clang emit the ARM ISA-identity set.  On arm64 this asserts the seven values
 * match the reference compilers via a guarded #error (the expected diagnostic);
 * on non-arm64 hosts it passes trivially (the identity set is arch-specific). */
#if defined(__aarch64__) || defined(__arm64__)
#if __ARM_ARCH == 8 && __ARM_ARCH_ISA_A64 == 1 && __ARM_64BIT_STATE == 1 \
 && __ARM_ARCH_PROFILE == 'A' && __ARM_PCS_AAPCS64 == 1 \
 && __ARM_SIZEOF_MINIMAL_ENUM == 4 && __ARM_SIZEOF_WCHAR_T == 4
#error "ARM ISA identity predefines present and correct"
#endif
#else
#error "ARM ISA identity predefines present and correct"
#endif
int x;
