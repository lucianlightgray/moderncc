struct S {
	int v;
};

int f(unsigned long long *p, int i) {
	return ((struct S *)(unsigned long)p[i])->v;
}
