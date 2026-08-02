#ifndef MCC_ALG_RLE_H
#define MCC_ALG_RLE_H

#include <stddef.h>

static inline long rle_compress(const unsigned char *s, long n, unsigned char *d,
																long cap) {
	long i = 0, o = 0;
	while (i < n) {
		long run = 1;
		while (i + run < n && run < 128 && s[i + run] == s[i])
			run++;
		if (run >= 2) {
			if (o + 2 > cap)
				return -1;
			d[o++] = (unsigned char)(257 - run);
			d[o++] = s[i];
			i += run;
		} else {
			long lit = i + 1;
			while (lit < n && lit - i < 128 &&
						 !(lit + 1 < n && s[lit] == s[lit + 1]))
				lit++;
			long cnt = lit - i;
			if (o + 1 + cnt > cap)
				return -1;
			d[o++] = (unsigned char)(cnt - 1);
			for (long k = 0; k < cnt; k++)
				d[o++] = s[i + k];
			i = lit;
		}
	}
	return o;
}

static inline long rle_decompress(const unsigned char *s, long n, unsigned char *d,
																	long cap) {
	long i = 0, o = 0;
	while (i < n) {
		int c = s[i++];
		if (c < 128) {
			long cnt = c + 1;
			if (i + cnt > n || o + cnt > cap)
				return -1;
			while (cnt--)
				d[o++] = s[i++];
		} else if (c > 128) {
			long cnt = 257 - c;
			if (i >= n || o + cnt > cap)
				return -1;
			unsigned char b = s[i++];
			while (cnt--)
				d[o++] = b;
		}
	}
	return o;
}

#endif
