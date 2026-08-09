int printf(const char *, ...);

signed char r_sc(unsigned long long x) { return (signed char)(x * 3u + 1u); }
unsigned char r_uc(unsigned long long x) { return (unsigned char)(x * 3u + 1u); }
short r_ss(unsigned long long x) { return (short)(x * 3u + 1u); }
unsigned short r_us(unsigned long long x) { return (unsigned short)(x * 3u + 1u); }
int r_si(unsigned long long x) { return (int)(x * 3u + 1u); }
unsigned int r_ui(unsigned long long x) { return (unsigned int)(x * 3u + 1u); }
long long r_sl(unsigned long long x) { return (long long)(x * 3u + 1u); }
unsigned long long r_ul(unsigned long long x) { return x * 3u + 1u; }
_Bool r_bl(unsigned long long x) { return (_Bool)(x & 0x80000000u); }

unsigned long long w_sc(unsigned long long *p) { return (unsigned long long)(long long)r_sc(*p); }
unsigned long long w_uc(unsigned long long *p) { return (unsigned long long)r_uc(*p); }
unsigned long long w_ss(unsigned long long *p) { return (unsigned long long)(long long)r_ss(*p); }
unsigned long long w_us(unsigned long long *p) { return (unsigned long long)r_us(*p); }
unsigned long long w_si(unsigned long long *p) { return (unsigned long long)(long long)r_si(*p); }
unsigned long long w_ui(unsigned long long *p) { return (unsigned long long)r_ui(*p); }
unsigned long long w_sl(unsigned long long *p) { return (unsigned long long)r_sl(*p); }
unsigned long long w_ul(unsigned long long *p) { return r_ul(*p); }
unsigned long long w_bl(unsigned long long *p) { return (unsigned long long)r_bl(*p); }

int main(void)
{
	unsigned long long v[3];
	int i;
	v[0] = 0xfeedbea8ffffcd35ULL;
	v[1] = 0x000000005555aaaaULL;
	v[2] = 0xffffffffffffffffULL;
	for (i = 0; i < 3; i++)
		printf("sc=%llx uc=%llx ss=%llx us=%llx si=%llx ui=%llx sl=%llx ul=%llx bl=%llx\n",
					 w_sc(&v[i]), w_uc(&v[i]), w_ss(&v[i]), w_us(&v[i]), w_si(&v[i]),
					 w_ui(&v[i]), w_sl(&v[i]), w_ul(&v[i]), w_bl(&v[i]));
	return 0;
}
