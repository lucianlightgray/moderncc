#if MCC_CONFIG_SYSROOT
int sysroot_as_value(void) { return 1; }
#elif MCC_CONFIG_CROSSPREFIX
int crossprefix_as_value(void) { return 1; }
#endif

#if MCC_CONFIG_CRTPREFIX
int crtprefix_as_value(void) { return 1; }
#endif

#if MCC_CONFIG_LIBPATHS
int libpaths_as_value(void) { return 1; }
#endif

#if MCC_CONFIG_SYSINCLUDEPATHS
int sysincludepaths_as_value(void) { return 1; }
#endif

#if MCC_CONFIG_ELFINTERP
int elfinterp_as_value(void) { return 1; }
#endif

#if MCC_CONFIG_ELFINTERP_ARMHF
int elfinterp_armhf_as_value(void) { return 1; }
#endif
