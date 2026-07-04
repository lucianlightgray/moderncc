#include <stdio.h>

int main(void) {
	int i = 5;
	printf("post: %d\n", i++);
	printf("after: %d\n", i);
	printf("pre: %d\n", ++i);
	printf("after: %d\n", i);

	int j = 5;
	printf("postdec: %d\n", j--);
	printf("predec: %d\n", --j);

	int a[5] = {0, 1, 2, 3, 4};
	int *p = a;
	int s = 0;
	while (p < a + 5)
		s += *p++;
	printf("walk: %d\n", s);

	int *q = a;
	printf("preptr: %d\n", *++q);

	int k = 0;
	int picked = a[k++];
	printf("index: picked=%d k=%d\n", picked, k);
	return 0;
}
