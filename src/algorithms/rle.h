#ifndef MCC_ALG_RLE_H
#define MCC_ALG_RLE_H

/*
 * PackBits run-length codec (Apple/TIFF/PDF) — the smallest feature-complete RLE:
 * round-trips any byte stream, never expands by more than 1 byte per 128. Control
 * byte c: 0..127 => copy c+1 literal bytes; 129..255 => repeat the next byte
 * (257-c) times; 128 => no-op. Header-only, fully inlined, no libc.
 *
 * Each function returns the output length, or -1 on output-buffer overflow.
 */

#include <stddef.h>

static inline long rle_compress(const unsigned char *s, long n, unsigned char *d,
																long cap) {
	long i = 0, o = 0;
	while (i < n) {
		long run = 1;
		while (i + run < n && run < 128 && s[i + run] == s[i])
			run++;
		if (run >= 2) { /* encode a run */
			if (o + 2 > cap)
				return -1;
			d[o++] = (unsigned char)(257 - run);
			d[o++] = s[i];
			i += run;
		} else { /* gather literals until a >=2 run starts (or 128) */
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
		if (c < 128) { /* literal run of c+1 */
			long cnt = c + 1;
			if (i + cnt > n || o + cnt > cap)
				return -1;
			while (cnt--)
				d[o++] = s[i++];
		} else if (c > 128) { /* repeat next byte 257-c times */
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
