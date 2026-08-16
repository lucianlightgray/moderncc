int load(int *p) { return *p; }
void store(int *p, int v) { *p = v; }
int loadstore(int *p, int *q) {
	*p = *q + 1;
	return *p + *q;
}
struct S { int a; int b; int c; };
int field(struct S *s) { return s->a + s->b + s->c; }
void setfield(struct S *s, int v) {
	s->a = v;
	s->b = v + 1;
	s->c = s->a + s->b;
}
int walk(int *a, int n) {
	int t = 0;
	for (int i = 0; i < n; i++)
		t += a[i];
	return t;
}
