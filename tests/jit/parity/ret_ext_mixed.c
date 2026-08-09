int printf(const char *, ...);

unsigned int m_ui(long long a, double b, int c)
{
	return (unsigned int)(a * 3 + (long long)b + c);
}

int m_si(long long a, double b, int c)
{
	return (int)(a * 3 + (long long)b + c);
}

unsigned short m_us(long long a, double b, int c)
{
	return (unsigned short)(a * 3 + (long long)b + c);
}

double m_d(long long a, double b, int c)
{
	return (double)(a & 0xffff) * b + (double)c;
}

unsigned long long w_ui(long long *p) { return (unsigned long long)m_ui(*p, 2.5, 3); }
unsigned long long w_si(long long *p) { return (unsigned long long)(long long)m_si(*p, 2.5, 3); }
unsigned long long w_us(long long *p) { return (unsigned long long)m_us(*p, 2.5, 3); }
double w_d(long long *p) { return m_d(*p, 2.5, 3); }

int main(void)
{
	long long v[2];
	int i;
	v[0] = (long long)0xfeedbea8ffffcd35ULL;
	v[1] = 0x000000005555aaaaLL;
	for (i = 0; i < 2; i++)
		printf("ui=%llx si=%llx us=%llx d=%.4f\n", w_ui(&v[i]), w_si(&v[i]),
					 w_us(&v[i]), w_d(&v[i]));
	return 0;
}
