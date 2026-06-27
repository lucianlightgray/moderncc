/* Compiled by tcc; linked with abi_main.c (gcc) to check tcc's _Complex calling
 * convention against the platform ABI. Parameterised by base type via
 * -DABI_BASE=float|double|"long double" (default double) so the runner can test
 * each (target, base) cell independently. See R3 in TODO.md. */
#ifndef ABI_BASE
#define ABI_BASE double
#endif
typedef ABI_BASE _Complex C;

extern C gcc_op(C, C);

C tcc_op(C a, C b){ return a + b; }                 /* tcc callee: arg+return */
C tcc_calls_gcc(C a, C b){ return gcc_op(a, b); }   /* tcc caller of gcc */
C tcc_mix(int k, C z, ABI_BASE r){ return (k + r) + z; } /* mixed-arg interleave */
