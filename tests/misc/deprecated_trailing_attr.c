struct Q { int x; } __attribute__((deprecated));
int use(void) { struct Q q; q.x = 1; return q.x; }
