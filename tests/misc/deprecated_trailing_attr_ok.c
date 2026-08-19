struct Q { int x; } __attribute__((deprecated));
struct R { int y; };
int main(void) { struct R r; r.y = 0; return r.y; }
