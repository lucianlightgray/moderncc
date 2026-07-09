#include <stdio.h>

int calls;
int note(int v) {
	calls++;
	return v;
}

int main(void) {

	printf("sel: %d %d\n", (1 ? 10 : 20), (0 ? 10 : 20));

	calls = 0;
	int a = (1 ? note(7) : note(9));
	printf("true_branch: a=%d calls=%d\n", a, calls);
	calls = 0;
	int b = (0 ? note(7) : note(9));
	printf("false_branch: b=%d calls=%d\n", b, calls);

	int cond = 0;
	double r = cond ? 1 : 2.5;
	printf("commontype: %d\n", (int)(r * 2));

	for (int x = -1; x <= 1; x++) {
		const char *s = x < 0 ? "neg" : x == 0 ? "zero"
																					 : "pos";
		printf("sign(%d)=%s\n", x, s);
	}

	int m = 3 > 2 ? 100 : 0;
	printf("use: %d\n", m + 1);
	return 0;
}
