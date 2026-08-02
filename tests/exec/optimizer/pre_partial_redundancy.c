extern int printf(const char *, ...);
int f(int c, int a, int b) {
	int t;
	if (c) { t = a + b; } else { t = 0; }
	int u = a + b;
	return t + u;
}
int g(int c, int a, int b) {
	int t;
	if (c) { t = 0; } else { t = a * b; }
	int u = a * b;
	return t - u;
}
int main(void) { printf("%d %d\n", f(1,3,4), g(0,3,4)); return 0; }
