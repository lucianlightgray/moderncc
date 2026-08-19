__attribute__((nonnull(1))) void g(void *p) { (void)p; }

void use(void) { g(0); }
