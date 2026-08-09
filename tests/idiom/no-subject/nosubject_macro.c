#if MCC_CONFIG_UCLIBC
int uclibc_has_no_conditional_by_registration(void) { return 1; }
#endif

#if MCC_CONFIG_JIT
int jit_is_a_cmake_cache_variable(void) { return 1; }
#endif
