#include <stdio.h>

#define N_OUTER 4
#define N_INNER 7

int sink;
int zero_v;
int hundred_v = 100;
int thousand_v = 1000;

static int esc(int n);

int main(void) {
	int i, j, k;

	for (i = 0; i < 5; i++)
		sink += i;

	i = 0;
	while (i < 3) {
		sink += i;
		i++;
	}

	k = 0;
	do {
		sink += k;
		k++;
	} while (k < 1);

	for (i = 0; i < N_OUTER; i++)
		for (j = 0; j < N_INNER; j++)
			sink += j;

	for (i = 0; i < hundred_v; i++) {
		if (i == 9)
			break;
		sink += i;
	}

	for (i = 0; i < thousand_v; i++)
		sink += 1;

	for (i = 0; i < zero_v; i++)
		sink += 1;

	sink += esc(10);

	printf("%d\n", sink);
	return 0;
}

static int esc(int n) {
	int i;
	for (i = 0; i < n; i++) {
		if (i == 2)
			return i;
	}
	return -1;
}
