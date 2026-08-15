struct plain {
	char a;
	int b : 3;
	char c;
};

struct mixed {
	unsigned short a : 3;
	unsigned short b : 9;
	unsigned int c : 7;
	char d;
};

struct shared {
	unsigned char a : 7;
	unsigned char b : 1;
	unsigned short w;
};

#ifdef BFABI_WRITER
struct plain g_plain = {1, -2, 3};
struct mixed g_mixed = {5, 300, 77, 9};
struct shared g_shared = {100, 1, 4242};

unsigned long bfabi_writer_sizes(void)
{
	return (unsigned long)sizeof(struct plain) |
				 ((unsigned long)sizeof(struct mixed) << 8) |
				 ((unsigned long)sizeof(struct shared) << 16);
}
#else
extern struct plain g_plain;
extern struct mixed g_mixed;
extern struct shared g_shared;
extern unsigned long bfabi_writer_sizes(void);
extern int printf(const char *, ...);

int main(void)
{
	unsigned long ws = bfabi_writer_sizes();
	unsigned long rs = (unsigned long)sizeof(struct plain) |
										 ((unsigned long)sizeof(struct mixed) << 8) |
										 ((unsigned long)sizeof(struct shared) << 16);
	int bad = 0;
	if (ws != rs) {
		printf("SIZE MISMATCH writer=%lx reader=%lx\n", ws, rs);
		bad = 1;
	}
	if (g_plain.a != 1 || g_plain.b != -2 || g_plain.c != 3) {
		printf("PLAIN FIELDS SCRAMBLED a=%d b=%d c=%d\n", g_plain.a, g_plain.b,
					 g_plain.c);
		bad = 1;
	}
	if (g_mixed.a != 5 || g_mixed.b != 300 || g_mixed.c != 77 ||
			g_mixed.d != 9) {
		printf("MIXED FIELDS SCRAMBLED a=%u b=%u c=%u d=%d\n", g_mixed.a,
					 g_mixed.b, g_mixed.c, g_mixed.d);
		bad = 1;
	}
	if (g_shared.a != 100 || g_shared.b != 1 || g_shared.w != 4242) {
		printf("SHARED FIELDS SCRAMBLED a=%u b=%u w=%u\n", g_shared.a,
					 g_shared.b, g_shared.w);
		bad = 1;
	}
	if (bad)
		return 1;
	printf("bitfield-abi OK sizes=%lx\n", rs);
	return 0;
}
#endif
