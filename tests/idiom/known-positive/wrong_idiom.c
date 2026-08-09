#ifdef MCC_CONFIG_PIE
int pie_on(void) { return 1; }
#endif

#if MCC_CONFIG_STATIC
int static_on(void) { return 1; }
#endif

#if defined(MCC_CONFIG_MUSL)
int musl_on(void) { return 1; }
#endif
