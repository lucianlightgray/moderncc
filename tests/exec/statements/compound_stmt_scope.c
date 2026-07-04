#include <stdio.h>

int main(void) {
	int x = 1;
	printf("outer: %d\n", x);
	{
		int x = 2;
		printf("inner: %d\n", x);
		{
			int x = 3;
			printf("inner2: %d\n", x);
		}
		printf("back2: %d\n", x);
	}
	printf("back1: %d\n", x);

	int sum = 0;
	for (int i = 0; i < 4; i++)
		sum += i;
	int i = 99;
	printf("loopscope: sum=%d i=%d\n", sum, i);

	int totals = 0;
	for (int k = 0; k < 3; k++) {
		int fresh = 10;
		fresh += k;
		totals += fresh;
	}
	printf("fresh: %d\n", totals);

	long x_long;
	x_long = (long)x + 41;
	printf("mixed: %ld\n", x_long);
	return 0;
}
