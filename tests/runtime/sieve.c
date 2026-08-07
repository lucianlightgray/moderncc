#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char flags[1 << 22];

int main(int argc, char **argv)
{
	int n = argc > 1 ? atoi(argv[1]) : 1000000;
	int reps = argc > 2 ? atoi(argv[2]) : 1;
	long total = 0;
	unsigned sum = 0;
	int r, i, j;

	if (n > (1 << 22))
		n = 1 << 22;
	for (r = 0; r < reps; r++) {
		memset(flags, 1, (size_t)n);
		flags[0] = flags[1] = 0;
		for (i = 2; (long)i * i < (long)n; i++) {
			if (!flags[i])
				continue;
			for (j = i * i; j < n; j += i)
				flags[j] = 0;
		}
		total = 0;
		sum = 0;
		for (i = 2; i < n; i++) {
			if (flags[i]) {
				total++;
				sum = sum * 31u + (unsigned)i;
			}
		}
	}
	printf("sieve %d %ld %u\n", n, total, sum);
	return 0;
}
