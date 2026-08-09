int printf(const char *, ...);

unsigned int a1(unsigned int a) { return a * 3u + 1u; }
unsigned int a2(unsigned int a, unsigned long long b) { return a * 3u + (unsigned int)b; }
unsigned int a3(unsigned int a, unsigned long long b, short c)
{
	return a * 3u + (unsigned int)b + (unsigned int)c;
}
unsigned int a4(unsigned int a, unsigned long long b, short c, unsigned char d)
{
	return a * 3u + (unsigned int)b + (unsigned int)c + d;
}
unsigned int a5(unsigned int a, unsigned long long b, short c, unsigned char d,
								int e)
{
	return a * 3u + (unsigned int)b + (unsigned int)c + d + (unsigned int)e;
}
unsigned int a6(unsigned int a, unsigned long long b, short c, unsigned char d,
								int e, long long f)
{
	return a * 3u + (unsigned int)b + (unsigned int)c + d + (unsigned int)e +
				 (unsigned int)f;
}

unsigned long long w1(unsigned long long *p) { return a1((unsigned int)p[0]); }
unsigned long long w2(unsigned long long *p) { return a2((unsigned int)p[0], p[1]); }
unsigned long long w3(unsigned long long *p) { return a3((unsigned int)p[0], p[1], (short)p[2]); }
unsigned long long w4(unsigned long long *p)
{
	return a4((unsigned int)p[0], p[1], (short)p[2], (unsigned char)p[3]);
}
unsigned long long w5(unsigned long long *p)
{
	return a5((unsigned int)p[0], p[1], (short)p[2], (unsigned char)p[3],
						(int)p[4]);
}
unsigned long long w6(unsigned long long *p)
{
	return a6((unsigned int)p[0], p[1], (short)p[2], (unsigned char)p[3],
						(int)p[4], (long long)p[5]);
}

int main(void)
{
	unsigned long long v[6];
	v[0] = 0xfeedbea8ffffcd35ULL;
	v[1] = 0x123456789abcdef0ULL;
	v[2] = 0xffffffffffff8001ULL;
	v[3] = 0x00000000000000feULL;
	v[4] = 0xfffffffff0000001ULL;
	v[5] = 0x8000000000000003ULL;
	printf("a1=%llx a2=%llx a3=%llx a4=%llx a5=%llx a6=%llx\n", w1(v), w2(v),
				 w3(v), w4(v), w5(v), w6(v));
	return 0;
}
