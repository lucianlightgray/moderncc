/* An inline-asm node is host machine text and can never be lowered. */
int f(int a, int b) {
	int r = a;
	__asm__ volatile("" : "+r"(r) : "r"(b));
	return r;
}
