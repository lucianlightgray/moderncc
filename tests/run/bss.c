#include <stdint.h>
#include <stdio.h>

static int32_t big[262144];
static char zeros[65536];
static int32_t small_bss[16];

extern int32_t aux_big[131072];
extern char aux_pad[65536];
extern int32_t aux_probe(int32_t i);

int main(void) {
	int i;
	int32_t s = 0, nz = 0;
	for (i = 0; i < 262144; i++)
		if (big[i] != 0)
			nz++;
	printf("bsszero=%d\n", nz);
	for (i = 0; i < 262144; i += 1024)
		big[i] = i / 1024;
	for (i = 0; i < 262144; i += 1024)
		s += big[i];
	printf("bsssum=%d\n", s);
	for (i = 0; i < 65536; i++)
		if (zeros[i])
			nz++;
	for (i = 0; i < 65536; i++)
		if (aux_pad[i])
			nz++;
	printf("charzero=%d\n", nz);
	for (i = 0; i < 16; i++)
		nz += small_bss[i];
	printf("smallzero=%d\n", nz);
	aux_big[131071] = 1234;
	aux_big[0] = 55;
	printf("auxbig=%d\n", aux_probe(131071));
	printf("auxbig0=%d\n", aux_probe(0));
	printf("auxbigmid=%d\n", aux_probe(65536));
	return 0;
}
