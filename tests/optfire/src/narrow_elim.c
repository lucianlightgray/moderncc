#include <stdio.h>

static int guarded(int x)
{
	int s = 0;
	if (x > 0 && x < 100) {
		signed char c = (signed char)x;
		s += c;
	}
	return s;
}

static int guarded2(int x)
{
	int s = 0;
	if (x >= 0 && x < 200) {
		unsigned char c = (unsigned char)x;
		s += c;
	}
	return s;
}

int main(void)
{
	printf("narrow_elim=%d\n", guarded(42) + guarded2(150));
	return 0;
}
