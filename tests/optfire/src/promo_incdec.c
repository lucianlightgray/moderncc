#include <stdio.h>

static int count(int n)
{
	int i;
	int j = 0;
	int k = n;
	int hits = 0;

	for (i = 0; i < n; i++) {
		j++;
		j++;
		k--;
		if (((i + j) & 3) == 0)
			hits++;
	}
	while (k > 0) {
		k--;
		hits++;
	}
	return hits + j + k;
}

int main(void)
{
	printf("promo_incdec=%d\n", count(9) + count(20));
	return 0;
}
