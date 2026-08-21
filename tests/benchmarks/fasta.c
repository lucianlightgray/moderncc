/*
 * fasta — deterministic DNA/protein sequence generator (checksum only).
 *
 * Ported from The Computer Language Benchmarks Game fasta problem
 * (vendor/plb/bench/algorithm/fasta/, C-family logic mirrored from 1.py by Ian
 * Osgood / Heinrich Acker / Justin Peel). The original prints three FASTA
 * blocks: a repeat of the ALU sequence, then two random blocks drawn from the
 * IUB and Homo-sapiens frequency tables via a linear-congruential PRNG.
 *
 * That PRNG is inherently sequential — seed[i+1] = (seed[i]*IA + IC) % IM, and
 * every character consumes the next draw in order — so this port stays
 * single-threaded. <threads.h> is still included and the NT macro honored (the
 * runner builds a -DNT=1 serial reference of every kernel), but no threads are
 * spawned: splitting the stream would change the draw order and the output.
 *
 * Rather than emit a multi-megabyte stream, this port folds an FNV-1a checksum
 * over the generated bases in order (see mandelbrot.c) and prints that plus the
 * total base count. The character sequence — and therefore the checksum — is
 * identical for any NT (serial -DNT=1 included).
 *
 * Work scale: N = argv[1] (default 2000); the base counts are N*100 scaled by
 * the CLBG 2/3/5 factors, so 10*N*100 = N*1000 bases total (default 2,000,000).
 *
 * <threads.h> leads and libc is hand-declared rather than #included: mcc's
 * cooperative backend (-DMCC_THREADS_COOP) defines its own once_flag, which
 * collides with the one glibc's <stdlib.h> pulls in.
 */
#include <threads.h>

extern int atoi(const char *);
extern int printf(const char *, ...);

#ifndef NT
#define NT 4
#endif

#define IA 3877
#define IC 29573
#define IM 139968

static const char alu[] =
	"GGCCGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGG"
	"GAGGCCGAGGCGGGCGGATCACCTGAGGTCAGGAGTTCGAGA"
	"CCAGCCTGGCCAACATGGTGAAACCCCGTCTCTACTAAAAAT"
	"ACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCA"
	"GCTACTCGGGAGGCTGAGGCAGGAGAATCGCTTGAACCCGGG"
	"AGGCGGAGGTTGCAGTGAGCCGAGATCGCGCCACTGCACTCC"
	"AGCCTGGGCGACAGAGCGAGACTCCGTCTCAAAAA";

static const char iub_chars[] = "acgtBDHKMNRSVWY";
static const double iub_probs[] = {0.27, 0.12, 0.12, 0.27, 0.02, 0.02,
				   0.02, 0.02, 0.02, 0.02, 0.02, 0.02,
				   0.02, 0.02, 0.02};

static const char hs_chars[] = "acgt";
static const double hs_probs[] = {0.3029549426680, 0.1979883004921,
				  0.1975473066391, 0.3015094502008};

static unsigned seed = 42;

static double next_random(void) {
	seed = (seed * IA + IC) % IM;
	return (double)seed / (double)IM;
}

static unsigned long long fnv;
static unsigned long long total;

static void emit(char c) {
	fnv = (fnv ^ (unsigned char)c) * 1099511628211ULL;
	total++;
}

static void repeat_fasta(const char *src, int len, long count) {
	int i = 0;
	for (long k = 0; k < count; k++) {
		emit(src[i]);
		if (++i == len)
			i = 0;
	}
}

static void random_fasta(const char *chars, const double *probs, int nsym,
			 long count) {
	double cum[64];
	double c = 0.0;
	for (int i = 0; i < nsym; i++) {
		c += probs[i];
		cum[i] = c;
	}
	for (long k = 0; k < count; k++) {
		double r = next_random();
		int idx = 0;
		while (idx < nsym - 1 && r >= cum[idx])
			idx++;
		emit(chars[idx]);
	}
}

int main(int argc, char **argv) {
	int n = argc > 1 ? atoi(argv[1]) : 2000;
	if (n < 1)
		n = 1;
	long base = (long)n * 100;

	fnv = 1469598103934665603ULL;
	total = 0;

	int alulen = 0;
	while (alu[alulen])
		alulen++;

	repeat_fasta(alu, alulen, base * 2);
	random_fasta(iub_chars, iub_probs, 15, base * 3);
	random_fasta(hs_chars, hs_probs, 4, base * 5);

	printf("fasta bases=%llu checksum=%llu\n", total, fnv);
	return 0;
}
