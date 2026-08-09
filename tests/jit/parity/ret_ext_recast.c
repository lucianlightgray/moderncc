int printf(const char *, ...);

int c_si(int x) { return x; }
unsigned int c_ui(int x) { return (unsigned int)x; }
short c_ss(int x) { return (short)x; }
unsigned short c_us(int x) { return (unsigned short)x; }
signed char c_sc(int x) { return (signed char)x; }
unsigned char c_uc(int x) { return (unsigned char)x; }

unsigned int r1(int x) { return c_si(x + 6); }
int r2(int x) { return c_ui(x + 6); }
unsigned short r3(int x) { return c_ss(x + 6); }
short r4(int x) { return c_us(x + 6); }
unsigned char r5(int x) { return c_sc(x + 6); }
signed char r6(int x) { return c_uc(x + 6); }
unsigned long long r7(int x) { return c_si(x + 6); }
long long r8(int x) { return c_ui(x + 6); }

int main(void)
{
	printf("r1=%llx r2=%llx r3=%llx r4=%llx\n", (unsigned long long)r1(-10),
				 (unsigned long long)(long long)r2(-10), (unsigned long long)r3(-10),
				 (unsigned long long)(long long)r4(-10));
	printf("r5=%llx r6=%llx r7=%llx r8=%llx\n", (unsigned long long)r5(-10),
				 (unsigned long long)(long long)r6(-10), r7(-10),
				 (unsigned long long)r8(-10));
	return 0;
}
