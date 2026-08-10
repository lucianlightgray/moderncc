static int dup_helper(int n) {
	int a = n, i;
	for (i = 0; i < 4; i++)
		a = a * 11 + 5;
	return a & 0x3ff;
}

int other_hot(int n) {
	int s = 0, i;
	for (i = 0; i < n; i++)
		s += dup_helper(i);
	return s;
}
