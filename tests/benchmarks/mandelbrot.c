/*
 * mandelbrot — escape-time Mandelbrot set, row-parallel.
 *
 * Ported from The Computer Language Benchmarks Game mandelbrot problem
 * (vendor/plb/bench/algorithm/mandelbrot/). The plb C entry (1-ffi.c) pulls in
 * OpenSSL just to MD5 the output image; this self-contained port drops that and
 * emits a deterministic checksum of the 1-bit-per-pixel PBM bitmap instead.
 *
 * The image rows are data-independent, so each of NT C11 <threads.h> workers
 * fills a disjoint band of rows (fork-join); the checksum is folded over the
 * finished bitmap in row order after the join, so the printed value is identical
 * for any NT (serial -DNT=1 included).
 *
 * Work scale: image is N x N (N = argv[1], run.sh default 2000 → 4M pixels x 50
 * iterations), comparable in cost to the spectral-norm kernels at the same N.
 */
#include <threads.h>

extern void *malloc(__SIZE_TYPE__);
extern void free(void *);
extern int atoi(const char *);
extern int printf(const char *, ...);

#ifndef NT
#define NT 4
#endif

#define MAXITER 50

struct band {
	unsigned char *bits;
	int w, h;
	int rowbytes;
	int lo, hi;
};

static int mbrot_worker(void *arg) {
	struct band *b = arg;
	int w = b->w, h = b->h, rowbytes = b->rowbytes;
	for (int y = b->lo; y < b->hi; y++) {
		double ci = 2.0 * y / h - 1.0;
		unsigned char *row = b->bits + (long)y * rowbytes;
		for (int x = 0; x < w; x++) {
			double cr = 2.0 * x / w - 1.5;
			double zr = 0, zi = 0;
			int i = 0;
			while (i < MAXITER && zr * zr + zi * zi <= 4.0) {
				double t = zr * zr - zi * zi + cr;
				zi = 2.0 * zr * zi + ci;
				zr = t;
				i++;
			}
			if (zr * zr + zi * zi <= 4.0)
				row[x >> 3] |= (unsigned char)(0x80 >> (x & 7));
		}
	}
	return 0;
}

int main(int argc, char **argv) {
	int n = argc > 1 ? atoi(argv[1]) : 2000;
	if (n < 1)
		n = 1;
	int w = n, h = n;
	int rowbytes = (w + 7) / 8;
	long total = (long)rowbytes * h;
	unsigned char *bits = malloc(total);
	for (long i = 0; i < total; i++)
		bits[i] = 0;

	struct band bands[NT];
	thrd_t th[NT];
	int nt = NT < h ? NT : h;
	if (nt < 1)
		nt = 1;
	int per = (h + nt - 1) / nt;
	for (int k = 0; k < nt; k++) {
		bands[k].bits = bits;
		bands[k].w = w;
		bands[k].h = h;
		bands[k].rowbytes = rowbytes;
		bands[k].lo = k * per;
		bands[k].hi = (k + 1) * per < h ? (k + 1) * per : h;
	}
	for (int k = 0; k < nt; k++)
		thrd_create(&th[k], mbrot_worker, &bands[k]);
	for (int k = 0; k < nt; k++)
		thrd_join(th[k], 0);

	unsigned long long sum = 1469598103934665603ULL;
	unsigned long long setbits = 0;
	for (long i = 0; i < total; i++) {
		unsigned char c = bits[i];
		sum = (sum ^ c) * 1099511628211ULL;
		for (int bt = 0; bt < 8; bt++)
			setbits += (c >> bt) & 1u;
	}
	free(bits);
	printf("mandelbrot %dx%d checksum=%llu setbits=%llu\n", w, h, sum, setbits);
	return 0;
}
