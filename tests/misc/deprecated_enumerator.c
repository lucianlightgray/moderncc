enum E { A __attribute__((deprecated)), B };
int use(void) { return (int)A + (int)B; }
