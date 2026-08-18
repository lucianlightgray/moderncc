static int f(_Bool ok) {
	switch (ok) {
	case 3:
		return 30;
	default:
		return 99;
	}
}

static int g(_Bool x) {
	switch (x) {
	case 0:
		return 0;
	case 1:
		return 1;
	case 2:
		return 2;
	default:
		return 9;
	}
}

static int h(_Bool b) {
	int r = 0;
	switch (b) {
	case 5:
		r += 100;
	case 1:
		r += 1;
		break;
	case 0:
		r += 10;
		break;
	}
	return r;
}

int main(void) {
	if (f(1) != 99) return 1;
	if (f(0) != 99) return 2;
	if (g(0) != 0) return 3;
	if (g(1) != 1) return 4;
	if (h(1) != 1) return 5;
	if (h(0) != 10) return 6;

	char c = 2;
	switch (c) {
	case 2:
		break;
	default:
		return 7;
	}

	short s = 300;
	switch (s) {
	case 300:
		break;
	default:
		return 8;
	}

	return 0;
}
