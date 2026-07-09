enum color { RED,
						 GREEN = 5,
						 BLUE };

int global_with_init = 42;

int decl_kinds(int a, int b) {
	int arr[3] = {1, 2, 3};
	int c = (int)a;
	enum color k = BLUE;
retry:
	if (c > 0) {
		c = c - 1;
		goto retry;
	}
	return arr[0] + b + k;
}
