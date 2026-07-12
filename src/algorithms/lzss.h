#ifndef MCC_ALG_LZSS_H
#define MCC_ALG_LZSS_H

/*
 * LZSS (Storer–Szymanski variant of LZ77) — the smallest feature-complete
 * general-purpose dictionary compressor: a sliding window over the already-emitted
 * bytes, a flag bit per token (literal byte vs. back-reference), matches of 3..18
 * bytes within a 4096-byte window. Header-only, fully inlined, no libc, no heap.
 *
 * Token stream: one flag byte carries 8 tokens (bit set = literal, next 1 byte;
 * bit clear = match, next 2 bytes = 12-bit distance-1 and 4-bit length-3). Returns
 * the output length, or -1 on output-buffer overflow. Decompress stops when the
 * input is exhausted, bounds-checking every read.
 */

#include <stddef.h>

#define LZSS_WINDOW 4096
#define LZSS_MINMATCH 3
#define LZSS_MAXMATCH 18

static inline long lzss_compress(const unsigned char *s, long n, unsigned char *d,
																 long cap) {
	long i = 0, o = 0;
	while (i < n) {
		long flagpos = o++;
		unsigned char flags = 0;
		int b;
		if (flagpos >= cap)
			return -1;
		for (b = 0; b < 8 && i < n; b++) {
			long best_len = 0, best_dist = 0, start = i - LZSS_WINDOW, j;
			if (start < 0)
				start = 0;
			for (j = start; j < i; j++) {
				long l = 0;
				while (l < LZSS_MAXMATCH && i + l < n && s[j + l] == s[i + l])
					l++;
				if (l > best_len) {
					best_len = l;
					best_dist = i - j;
				}
			}
			if (best_len >= LZSS_MINMATCH) {
				long dd = best_dist - 1, ll = best_len - LZSS_MINMATCH;
				if (o + 2 > cap)
					return -1;
				d[o++] = (unsigned char)(dd >> 4);
				d[o++] = (unsigned char)(((dd & 0xf) << 4) | ll);
				i += best_len;
			} else {
				flags |= (unsigned char)(1u << b); /* literal */
				if (o + 1 > cap)
					return -1;
				d[o++] = s[i++];
			}
		}
		d[flagpos] = flags;
	}
	return o;
}

static inline long lzss_decompress(const unsigned char *s, long n,
																	 unsigned char *d, long cap) {
	long i = 0, o = 0;
	while (i < n) {
		unsigned char flags = s[i++];
		int b;
		for (b = 0; b < 8; b++) {
			if (i >= n)
				return o;
			if (flags & (1u << b)) { /* literal */
				if (o >= cap)
					return -1;
				d[o++] = s[i++];
			} else { /* match */
				long dist, len, k;
				int b0, b1;
				if (i + 1 >= n)
					return o;
				b0 = s[i++];
				b1 = s[i++];
				dist = ((b0 << 4) | (b1 >> 4)) + 1;
				len = (b1 & 0xf) + LZSS_MINMATCH;
				if (dist > o || o + len > cap)
					return -1;
				for (k = 0; k < len; k++, o++)
					d[o] = d[o - dist];
			}
		}
	}
	return o;
}

#endif
