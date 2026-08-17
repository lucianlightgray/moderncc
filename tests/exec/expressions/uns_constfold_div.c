#include <stdio.h>

unsigned long long ud(void) { return -9223372036854775808ULL / 2ULL; }
unsigned long long um(void) { return -9223372036854775807ULL % 10ULL; }
unsigned long long ud2(void) { return -9223372036854775807ULL / 3ULL; }
unsigned long long um2(void) { return -100000000000000000ULL % 7ULL; }

long long sd(void) { return -9223372036854775807LL / 3LL; }
long long sm(void) { return -9223372036854775807LL % 10LL; }

int main(void) {
	printf("ud=%llu\n", ud());
	printf("um=%llu\n", um());
	printf("ud2=%llu\n", ud2());
	printf("um2=%llu\n", um2());
	printf("sd=%lld\n", sd());
	printf("sm=%lld\n", sm());
	return 0;
}
