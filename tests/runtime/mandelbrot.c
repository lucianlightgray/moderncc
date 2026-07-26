#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	int n = argc > 1 ? atoi(argv[1]) : 200;
	int x, y, i, bits = 0, nbits = 0;
	unsigned long checksum = 0;
	double inv = 2.0 / n;
	for (y = 0; y < n; y++) {
		double ci = y * inv - 1.0;
		for (x = 0; x < n; x++) {
			double cr = x * inv - 1.5;
			double zr = 0.0, zi = 0.0, tr = 0.0, ti = 0.0;
			for (i = 0; i < 50 && tr + ti <= 4.0; i++) {
				zi = 2.0 * zr * zi + ci;
				zr = tr - ti + cr;
				tr = zr * zr;
				ti = zi * zi;
			}
			bits = (bits << 1) | (tr + ti <= 4.0);
			if (++nbits == 8) {
				checksum = checksum * 31u + (unsigned char)bits;
				bits = 0;
				nbits = 0;
			}
		}
		if (nbits) {
			checksum = checksum * 31u + (unsigned char)(bits << (8 - nbits));
			bits = 0;
			nbits = 0;
		}
	}
	printf("mandelbrot %d %lu\n", n, checksum);
	return 0;
}
