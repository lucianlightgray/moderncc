#ifdef MCC_CONFIG_TRACE
int trace_ifdef(void) { return 1; }
#endif

#ifndef MCC_CONFIG_TRACE
int trace_ifndef_without_default(void) { return 1; }
#endif

#if defined(MCC_CONFIG_CPUVER)
int cpuver_defined(void) { return 1; }
#endif

#ifdef MCC_CONFIG_DWARF_VERSION
int dwarf_version_ifdef(void) { return 1; }
#endif

#if defined MCC_CONFIG_SEMLOCK
int semlock_defined(void) { return 1; }
#endif

#ifdef MCC_CONFIG_RUNMEM_RO
int runmem_ro_ifdef(void) { return 1; }
#endif

#if defined(MCC_CONFIG_AUTO_MCCDIR)
int auto_mccdir_defined(void) { return 1; }
#endif
