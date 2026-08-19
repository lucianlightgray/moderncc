__attribute__((warn_unused_result)) int f(void) { return 1; }
__attribute__((nonnull(1))) int g(void *p) { return p != (void *)0; }
__attribute__((nonnull)) int h(void *a, void *b) {
	return (a != (void *)0) + (b != (void *)0);
}
__attribute__((sentinel)) int s(int first, ...) { return first; }

int main(void) {
	int r = 0, x = 0;
	r += f();
	if (f())
		r += 1;
	r += g(&x);
	r += h(&x, &x);
	r += s(1, &x, (void *)0);
	return r > 0 ? 0 : 1;
}
