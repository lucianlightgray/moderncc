#include <stdio.h>

static int classify(int x) {
	int acc = 0;
	switch (x) {
	case 1:
		acc += 1;
	case 2:
		acc += 2;
	case 3:
		acc += 3;
		break;
	default:
		acc = -1;
		break;
	case 9:
		acc = 99;
		break;
	}
	return acc;
}

static void copy_n(const char *src, char *dst, int n) {
	int count = (n + 3) / 4;
	int i = 0;
	switch (n % 4) {
	case 0:
		do {
			dst[i] = src[i];
			i++;
		case 3:
			dst[i] = src[i];
			i++;
		case 2:
			dst[i] = src[i];
			i++;
		case 1:
			dst[i] = src[i];
			i++;
		} while (--count > 0);
	}
}

int main(void) {
	printf("classify: %d %d %d %d %d\n",
				 classify(1), classify(2), classify(3), classify(5), classify(9));

	char dst[16] = {0};
	copy_n("abcdefg", dst, 7);
	printf("duff: %s\n", dst);

	int hit = 0;
	switch (42) {
	case 0:
		hit = 1;
		break;
	default:
		hit = 2;
		break;
	}
	printf("defonly: %d\n", hit);
	return 0;
}
