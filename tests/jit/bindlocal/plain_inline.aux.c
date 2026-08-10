int inline_helper(int n) {
	int a = n, i;
	for (i = 0; i < 5; i++)
		a = a * 5 + 2;
	return a & 0x7ff;
}
