#include <stdio.h>

static int g(int x) {
	return x + 1;
}

int main(void) {

	int x = 1, y = 2, z = 3;
	int *ap[3] = {&x, &y, &z};
	printf("arr_of_ptr: %d %d %d\n", *ap[0], *ap[1], *ap[2]);

	int grid[2][3] = {{10, 20, 30}, {40, 50, 60}};
	int (*rp)[3] = grid;
	printf("ptr_to_arr: %d %d\n", rp[1][2], (*rp)[0]);

	int (*fp)(int) = g;
	printf("ptr_to_func: %d\n", fp(41));

	int (*fa[2])(int) = {g, g};
	printf("arr_of_func: %d\n", fa[0](10) + fa[1](20));

	int v = 7;
	int *p = &v;
	int **pp = &p;
	**pp = 70;
	printf("ptr_ptr: %d\n", v);

	int (*const crp)[3] = grid;
	printf("const_chain: %d\n", crp[0][1]);
	return 0;
}
