int classify(int x) {
	if (x < 0) return -1;
	if (x == 0) return 0;
	if (x < 10) return 1;
	if (x < 100) return 2;
	return 3;
}
int loopsum(int n) {
	int s = 0;
	for (int i = 0; i < n; i++) {
		if (i & 1) s += i;
		else s -= i;
	}
	return s;
}
int logic(int a, int b, int c) { return (a && b) || (b && c) || (a && c); }
int sw(int x) {
	switch (x) {
	case 0: return 10;
	case 1: return 20;
	case 2: return 30;
	case 3: return 40;
	default: return 0;
	}
}
