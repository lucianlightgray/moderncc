typedef struct __attribute__((deprecated)) S { int x; } T;
T t;
int use(void) {
	struct S s;
	s.x = t.x;
	return s.x;
}
