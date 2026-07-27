#include <stdio.h>

static int churn(int s)
{
	int a = s, b = s + 1, c = s + 2, d = s + 3;
	int e = s + 4, f = s + 5, g = s + 6, h = s + 7;
	int i = s + 8, j = s + 9, k = s + 10, l = s + 11;
	int t = 0;
	int n;

	for (n = 0; n < 8; n++) {
		a = (a + b) & 255;
		b = (b + c) & 255;
		c = (c + d) & 255;
		d = (d + e) & 255;
		e = (e + f) & 255;
		f = (f + g) & 255;
		g = (g + h) & 255;
		h = (h + i) & 255;
		i = (i + j) & 255;
		j = (j + k) & 255;
		k = (k + l) & 255;
		l = (l + a) & 255;
		t += a + b + c + d + e + f + g + h + i + j + k + l;
	}
	return t;
}

int main(void)
{
	printf("color=%d\n", churn(1) + churn(2) + churn(3));
	return 0;
}
