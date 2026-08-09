int printf(const char *, ...);

unsigned int foo(unsigned int x) { return x; }

unsigned long long lo(unsigned long long *x) { return foo(*x >> 32); }

int main(void)
{
	unsigned long long l = 0xfeedbea800000000ULL;
	printf("lo=%llx\n", lo(&l));
	return 0;
}
