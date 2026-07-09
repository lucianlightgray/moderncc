#include <stdio.h>

int main(void) {

	printf("bases: %d %d %d\n", 255, 0377, 0xff);
	printf("octal0: %d\n", 0);

	printf("widths: %d %d %d %d %d\n",
				 (int)sizeof(1), (int)sizeof(1U), (int)sizeof(1L),
				 (int)sizeof(1UL), (int)sizeof(1LL));

	printf("dec_overflow_size: %d\n", (int)sizeof(2147483648));
	printf("hex_unsigned_size: %d\n", (int)sizeof(0x80000000));
	printf("hex_big_size: %d\n", (int)sizeof(0xFFFFFFFFFFFFFFFF));

	printf("signedness: %d %d %d\n",
				 (0x80000000 > 0), (-1 < 1U), (-1 < 1));

	printf("wrap: %u %u\n", 0u - 1u, 0xFFFFFFFFu + 1u);

	printf("suffixed: %llu\n", 0x100000000ULL);
	return 0;
}
