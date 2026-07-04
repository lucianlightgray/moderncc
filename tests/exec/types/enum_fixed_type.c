#include <stdio.h>

enum small : unsigned char { A = 1,
							 B = 200 };
enum wide : long long { C = 1,
						D = 1LL << 40 };

int main(void) {
	printf("%zu %zu\n", sizeof(enum small), sizeof(enum wide));
	printf("%d %d %lld\n", A, B, (long long)D);
	return 0;
}
