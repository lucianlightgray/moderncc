#include <stdio.h>

static int bump(int i, int a[i++]) { return i; }

static int twice(int i, int a[i += 2]) { return i; }

static int plain(int i, int a[i + 0]) { return i; }

static int nested(int i, int j, int a[i++][j++]) { return i * 100 + j; }

int main(void) {
	static int room[64];
	static int grid[8][8];
	printf("%d %d %d %d\n", bump(10, room), twice(5, room), plain(7, room),
				 nested(3, 4, grid));
	return 0;
}
