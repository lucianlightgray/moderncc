#ifdef MCC_CONFIG_NOT_A_REAL_KNOB
int unregistered_name(void) { return 1; }
#endif

#ifdef MCC_CONFIG_TRIPLET
int a_registered_name_so_the_run_has_a_subject(void) { return 1; }
#endif
