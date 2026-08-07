#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 4000000;
	int i;
	long acc = 0;
	unsigned uacc = 0;
	long long lacc = 0;
	for (i = 1; i <= n; i++) {
		unsigned u = (unsigned)i;
		acc += i / 7 + i % 13;
		acc -= i / 100 - i % 1000;
		uacc += u / 3u + u % 255u;
		uacc ^= u / 65537u;
		lacc += (long long)i * 3 / 11 + (long long)i % 97;
		if ((i & 0xffff) == 0)
			acc += (long)i / (long)(i & 0x3ff ? (i & 0x3ff) : 1);
	}
	printf("divmod %ld %u %lld\n", acc, uacc, lacc);
	return 0;
}
