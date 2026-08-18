#include <stdio.h>

extern unsigned long long bin_q;
extern unsigned int bin_l;
extern unsigned char bin_b;
int bin_imm(void);

int main(void) {
	if (bin_q != 1023ULL) { printf("FAIL q=%llu\n", bin_q); return 1; }
	if (bin_l != 257u) { printf("FAIL l=%u\n", bin_l); return 2; }
	if (bin_b != 12u) { printf("FAIL b=%u\n", (unsigned)bin_b); return 3; }
	if (bin_imm() != 42) { printf("FAIL imm=%d\n", bin_imm()); return 4; }
	printf("OK\n");
	return 0;
}
