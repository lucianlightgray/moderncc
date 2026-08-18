void f(int n) {
	void *p = &&L;
	goto *p;
	{
		int vla[n];
	L:
		vla[0] = 1;
	}
}
