#include <stdio.h>

int imin(int a, int b) { return a < b ? a : b; }
int imax(int a, int b) { return a > b ? a : b; }
long lmin(long a, long b) { return a < b ? a : b; }
long lmax(long a, long b) { return a > b ? a : b; }
unsigned umin(unsigned a, unsigned b) { return a < b ? a : b; }
int isign(int x) { return x < 0 ? -1 : 1; }
int iabs(int x) { return x < 0 ? -x : x; }
int clampz(int x) { return x < 0 ? 0 : x; }
int selxor(int a, int b) { return a < b ? a ^ b : a & b; }

int main(void) {
	int fails = 0;
	for (int a = -6; a <= 6; a++) {
		for (int b = -6; b <= 6; b++) {
			if (imin(a, b) != (a < b ? a : b)) fails++;
			if (imax(a, b) != (a > b ? a : b)) fails++;
			if (lmin(a, b) != (long)(a < b ? a : b)) fails++;
			if (lmax(a, b) != (long)(a > b ? a : b)) fails++;
			if (selxor(a, b) != (a < b ? (a ^ b) : (a & b))) fails++;
		}
	}
	for (unsigned u = 0; u < 8; u++) {
		for (unsigned v = 0; v < 8; v++) {
			if (umin(u, v) != (u < v ? u : v)) fails++;
		}
	}
	for (int x = -6; x <= 6; x++) {
		if (isign(x) != (x < 0 ? -1 : 1)) fails++;
		if (iabs(x) != (x < 0 ? -x : x)) fails++;
		if (clampz(x) != (x < 0 ? 0 : x)) fails++;
	}
	printf(fails ? "FAIL %d\n" : "OK\n", fails);
	return fails != 0;
}
