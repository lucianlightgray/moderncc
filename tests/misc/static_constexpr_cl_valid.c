int main(void) {
	int *a = (static int[]){ 10, 20 };
	const int *b = (constexpr int[]){ 1, 2, 3 };
	int c = (constexpr int){ 42 };
	if (a[0] + a[1] != 30) return 1;
	if (b[0] + b[1] + b[2] != 6) return 2;
	if (c != 42) return 3;
	return 0;
}
