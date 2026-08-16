static int helper(int x) { return x * 3 + 1; }
extern int libcall(int);
int process(int *data, int n) {
	int total = 0;
	for (int i = 0; i < n; i++) {
		int v = data[i];
		if (v > 0)
			total += helper(v);
		else if (v < 0)
			total += libcall(v);
		else
			total += helper(libcall(v));
	}
	return total;
}
