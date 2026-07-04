#include <stdio.h>

int main(void) {

	int pairs[][2] = {{7, 2}, {-7, 2}, {7, -2}, {-7, -2}, {1, 3}, {-1, 3}};
	for (int i = 0; i < (int)(sizeof pairs / sizeof pairs[0]); i++) {
		int a = pairs[i][0], b = pairs[i][1];
		int q = a / b, r = a % b;
		printf("%d/%d=%d %d%%%d=%d ident=%d\n",
			   a, b, q, a, b, r, (q * b + r) == a);
	}

	unsigned u = 0u;
	u = u - 1u;
	printf("wrap: %u\n", u + 1u);

	unsigned x = 1u;
	printf("shl: %u\n", x << 4);
	printf("shr: %u\n", 0xF0u >> 4);

	int s = 256;
	printf("sshr: %d\n", s >> 3);

	printf("wide: %llu\n", (unsigned long long)1 << 40);
	return 0;
}
