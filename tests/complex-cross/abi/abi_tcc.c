



#ifndef ABI_BASE
#define ABI_BASE double
#endif
typedef ABI_BASE _Complex C;

extern C gcc_op(C, C);

C tcc_op(C a, C b){ return a + b; }
C tcc_calls_gcc(C a, C b){ return gcc_op(a, b); }
C tcc_mix(int k, C z, ABI_BASE r){ return (k + r) + z; }
