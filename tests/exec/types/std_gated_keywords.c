extern int printf(const char *, ...);

enum big { B0 = 1, B1 = 4294967295u };

static int width(int x) {
	switch (x) {
	case 1 ... 4:
		return 1;
	case 5 ... 9:
		return 2;
	default:
		return 0;
	}
}

int main(void) {
	int ok = 1;

	if (alignof(int[]) != alignof(int))
		ok = 0;
	if (alignof(int[][1]) != alignof(int))
		ok = 0;
	if (B1 != 4294967295u)
		ok = 0;
	if (sizeof(B1) < 4)
		ok = 0;
	if (width(3) != 1 || width(7) != 2 || width(20) != 0)
		ok = 0;
	printf("%s\n", ok ? "OK" : "FAIL");
	return 0;
}
