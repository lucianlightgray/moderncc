/* Round-trip + never-corrupt self-test for the src/algorithms compressors.
 * Build: cc -I src/algorithms src/algorithms/selftest.c -o selftest && ./selftest */
#include "lzss.h"
#include "lzw.h"
#include "rle.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails, checks;

static void roundtrip(const char *codec, const char *name,
											long (*enc)(const unsigned char *, long, unsigned char *, long),
											long (*dec)(const unsigned char *, long, unsigned char *, long),
											const unsigned char *in, long n) {
	unsigned char *comp = malloc(n * 2 + 64), *out = malloc(n + 64);
	long clen, olen;
	checks++;
	clen = enc(in, n, comp, n * 2 + 64);
	if (clen < 0) {
		printf("FAIL %s/%s: compress overflow\n", codec, name);
		fails++;
	} else {
		olen = dec(comp, clen, out, n + 64);
		if (olen != n || memcmp(in, out, n) != 0) {
			printf("FAIL %s/%s: round-trip mismatch (n=%ld clen=%ld olen=%ld)\n", codec,
						 name, n, clen, olen);
			fails++;
		} else {
			printf("ok   %-5s %-10s %6ld -> %6ld bytes (%.0f%%)\n", codec, name, n,
						 clen, n ? 100.0 * clen / n : 0);
		}
	}
	free(comp);
	free(out);
}

int main(void) {
	static unsigned char buf[70000];
	long i;
	struct {
		const char *codec;
		long (*enc)(const unsigned char *, long, unsigned char *, long);
		long (*dec)(const unsigned char *, long, unsigned char *, long);
	} codecs[] = {
			{"rle", rle_compress, rle_decompress},
			{"lzss", lzss_compress, lzss_decompress},
			{"lzw", lzw_compress, lzw_decompress},
	};
	int ci;
	for (ci = 0; ci < 3; ci++) {
		roundtrip(codecs[ci].codec, "empty", codecs[ci].enc, codecs[ci].dec, buf, 0);
		buf[0] = 42;
		roundtrip(codecs[ci].codec, "one", codecs[ci].enc, codecs[ci].dec, buf, 1);
		for (i = 0; i < 40000; i++)
			buf[i] = 0xAB;
		roundtrip(codecs[ci].codec, "runs", codecs[ci].enc, codecs[ci].dec, buf, 40000);
		for (i = 0; i < 40000; i++)
			buf[i] = "ABCABCDABCDE this is highly repetitive text. "[i % 45];
		roundtrip(codecs[ci].codec, "text", codecs[ci].enc, codecs[ci].dec, buf, 40000);
		{
			unsigned long r = 0x9e3779b9UL;
			for (i = 0; i < 40000; i++) {
				r = r * 1103515245UL + 12345UL;
				buf[i] = (unsigned char)(r >> 16);
			}
		}
		roundtrip(codecs[ci].codec, "random", codecs[ci].enc, codecs[ci].dec, buf, 40000);
		/* KwKwK stressor: repeated short cycles that trigger self-referential codes */
		for (i = 0; i < 40000; i++)
			buf[i] = (unsigned char)('a' + (i % 3));
		roundtrip(codecs[ci].codec, "cycle", codecs[ci].enc, codecs[ci].dec, buf, 40000);
	}
	printf("\n%d checks, %d failures\n", checks, fails);
	return fails ? 1 : 0;
}
