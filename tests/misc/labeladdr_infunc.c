int main(void) {
	static void *tab[] = { &&A, &&B, &&C };
	int i = 1, r = 0;
	void *p = tab[i];
	goto *p;
A:
	r += 10;
	goto done;
B:
	r += 20;
	goto done;
C:
	r += 30;
	goto done;
done:
	return r == 20 ? 0 : 1;
}
