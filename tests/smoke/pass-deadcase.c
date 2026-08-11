extern int printf(const char *, ...);

static volatile int smp_zero = 0;

static int smp_c1(int i)
{
	int r = 0;
	switch (i) {
		if (0) {
		case 41:
			r = 1;
		}
	}
	return r;
}

static int smp_c2(int i)
{
	int r = 0;
	switch (i) {
		if (1) {
			r = 2;
		} else {
		case 7:
			r = 3;
		}
	}
	return r;
}

static int smp_c3(int i)
{
	int r = 0;
	switch (i) {
		while (0) {
		case 9:
			r = 4;
		}
	}
	return r;
}

static int smp_c4(int i)
{
	int r = 0;
	switch (i) {
		if (0) {
			if (0) {
			case 11:
				r = 5;
			}
		}
	}
	return r;
}

static int smp_c5(int n)
{
	int r = 0, i = 0;
	switch (n % 4) {
	case 0:
		do {
			r += 10;
		case 3:
			r += 1;
			i++;
		} while (i < 1);
	}
	return r;
}

static int smp_g1(void)
{
	int r = 0;
	if (0) {
		goto L;
	}
	r = 6;
	return r;
L:
	return 7;
}

static int smp_d1(int i)
{
	int r = 0;
	switch (i) {
		if (smp_zero) {
		case 13:
			r = 8;
		}
	}
	return r;
}

int main(void)
{
	printf("deadcase %d %d %d %d %d %d %d %d %d %d\n", smp_c1(41), smp_c1(0),
				 smp_c2(7), smp_c3(9), smp_c4(11), smp_c5(3), smp_c5(0), smp_g1(),
				 smp_d1(13), smp_d1(0));
	return 0;
}
