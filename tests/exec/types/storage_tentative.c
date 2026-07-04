#include <stdio.h>

int tent;
int tent;

static int zero_static;
static int zarr[4];

static long counter_seed = 41;

int next_id(void) {

	static int id = 0;
	return ++id;
}

int main(void) {
	printf("tentative: %d\n", tent);
	printf("zero: %d %d\n", zero_static, zarr[3]);

	int id1 = next_id(), id2 = next_id(), id3 = next_id();
	printf("ids: %d %d %d\n", id1, id2, id3);

	register int r = 7;
	int sum = 0;
	for (int i = 0; i < r; i++)
		sum += i;
	printf("regsum: %d\n", sum);

	printf("seed: %ld\n", counter_seed + 1);
	return 0;
}
