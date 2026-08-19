__attribute__((sentinel)) void s(int first, ...) { (void)first; }

void use(void) { s(1, 2, 3); }
