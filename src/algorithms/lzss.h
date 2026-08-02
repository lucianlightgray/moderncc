#ifndef MCC_ALG_LZSS_H
#define MCC_ALG_LZSS_H

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
				flags |= (unsigned char)(1u << b);
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
			if (flags & (1u << b)) {
				if (o >= cap)
					return -1;
				d[o++] = s[i++];
			} else {
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
