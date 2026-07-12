#ifndef MCC_ALG_LZW_H
#define MCC_ALG_LZW_H

/*
 * LZW (Lempel–Ziv–Welch) — the canonical smallest feature-complete adaptive
 * compressor (compress(1), GIF, PDF/LZWDecode): both sides build the same
 * dictionary as they go, so nothing but the code stream is transmitted. Codes are
 * 16-bit little-endian; the dictionary grows to LZW_MAX entries then freezes.
 * Header-only, fully inlined, no libc, no heap. Handles the KwKwK self-referential
 * code. Returns the output length, or -1 on overflow / corrupt input.
 */

#include <stddef.h>

#define LZW_MAX 4096
#define LZW_HASH 8192

static inline long lzw_compress(const unsigned char *s, long n, unsigned char *d,
																long cap) {
	long htk[LZW_HASH];
	int htv[LZW_HASH];
	long i, o = 0;
	int next = 256, w, c;
	unsigned h;
	for (i = 0; i < LZW_HASH; i++)
		htk[i] = 0; /* 0 = empty; stored key is key+1 */
	if (n == 0)
		return 0;
	w = s[0];
	for (i = 1; i < n; i++) {
		long key = ((long)w << 8) | (c = s[i]);
		int found = -1;
		h = (unsigned)((unsigned long)key * 2654435761UL) & (LZW_HASH - 1);
		while (htk[h] != 0) {
			if (htk[h] == key + 1) {
				found = htv[h];
				break;
			}
			h = (h + 1) & (LZW_HASH - 1);
		}
		if (found >= 0) {
			w = found;
		} else {
			if (o + 2 > cap)
				return -1;
			d[o++] = (unsigned char)(w & 0xff);
			d[o++] = (unsigned char)((w >> 8) & 0xff);
			if (next < LZW_MAX) { /* h is the empty slot */
				htk[h] = key + 1;
				htv[h] = next++;
			}
			w = c;
		}
	}
	if (o + 2 > cap)
		return -1;
	d[o++] = (unsigned char)(w & 0xff);
	d[o++] = (unsigned char)((w >> 8) & 0xff);
	return o;
}

static inline long lzw_decompress(const unsigned char *s, long n, unsigned char *d,
																	long cap) {
	int pre[LZW_MAX];
	unsigned char byt[LZW_MAX], stack[LZW_MAX];
	int next = 256, oldc, oldfirst, newc, c, sp;
	long i, o = 0;
	if (n < 2)
		return 0;
	oldc = s[0] | (s[1] << 8);
	if (oldc >= 256)
		return -1;
	if (o >= cap)
		return -1;
	d[o++] = (unsigned char)oldc;
	oldfirst = oldc;
	for (i = 2; i + 1 < n; i += 2) {
		newc = s[i] | (s[i + 1] << 8);
		sp = 0;
		if (newc >= next) { /* KwKwK: emit translate(old) + oldfirst */
			stack[sp++] = (unsigned char)oldfirst;
			c = oldc;
		} else {
			c = newc;
		}
		while (c >= 256) {
			if (c >= LZW_MAX || sp >= LZW_MAX - 1)
				return -1;
			stack[sp++] = byt[c];
			c = pre[c];
		}
		stack[sp++] = (unsigned char)c;
		if (o + sp > cap)
			return -1;
		while (sp)
			d[o++] = stack[--sp];
		if (next < LZW_MAX) { /* dict[next] = translate(old) + firstchar(current) */
			pre[next] = oldc;
			byt[next] = (unsigned char)c;
			next++;
		}
		oldfirst = c;
		oldc = newc;
	}
	return o;
}

#endif
