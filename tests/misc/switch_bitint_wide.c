#ifdef __SIZEOF_INT128__

static int pick(_BitInt(128) x) {
	switch (x) {
	case 3:
		return 30;
	case 100:
		return 100;
	case -5:
		return 55;
	case ((_BitInt(128))1 << 70):
		return 70;
	default:
		return 9;
	}
}

static int pick100(_BitInt(100) x) {
	switch (x) {
	case 5:
		return 5;
	case -1:
		return 99;
	case ((_BitInt(100))1 << 80):
		return 80;
	default:
		return 0;
	}
}

static unsigned upick(unsigned _BitInt(128) x) {
	switch (x) {
	case 0:
		return 1;
	case ((unsigned _BitInt(128))1 << 127):
		return 127;
	default:
		return 0;
	}
}

int main(void) {
	if (pick(3) != 30) return 1;
	if (pick(100) != 100) return 2;
	if (pick(-5) != 55) return 3;
	if (pick((_BitInt(128))1 << 70) != 70) return 4;
	if (pick(7) != 9) return 5;

	if (pick100(5) != 5) return 6;
	if (pick100(-1) != 99) return 7;
	if (pick100((_BitInt(100))1 << 80) != 80) return 8;

	if (upick(0) != 1) return 9;
	if (upick((unsigned _BitInt(128))1 << 127) != 127) return 10;

	return 0;
}

#else
int main(void) { return 0; }
#endif
